# -*- coding: utf-8 -*-
"""导出主流程。

整体流程：
  1. 收集材质 / 贴图（material_mapper）
  2. 识别实例组（instance_detector）
  3. 每个实例组的 base（及每个唯一网格）导出一个 FBX
  4. 解析 FBX 内 mesh 名 → 写 FBX .meta（fileIDToRecycleName 固定 fileID）
  5. 生成 Prefab（.prefab + .meta）、材质（.mat + .meta）、贴图（复制 + .meta）
  6. 生成场景（.unity：保留组嵌套，Prefab 挂在父 Transform 下）
"""
from __future__ import print_function

import os
import re
import shutil

import maya.cmds as cmds
import maya.mel as mel

from constants import guid_from_string, to_maya_ui
from coords import transform_to_unity_local, transform_to_unity_local_light
from instance_detector import analyze
from hierarchy import build_hierarchy, merge_lights_into_hierarchy
from material_mapper import collect_materials
from light_mapper import map_lights, ambient_color_from
import yaml_writer
import debug

# 世界单位矩阵：导出前把临时物体摆到原点，让 FBX 只存对象空间顶点
_IDENTITY_MATRIX = [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1]
# 导出副本临时名前缀（不改动场景中的源物体）
_TEMP_EXPORT_PREFIX = "__MTU_Exp_"


def _write_text(path, text):
    """写 UTF-8 文本（Python 2 下 str/unicode 统一按字节写）。"""
    if isinstance(text, unicode):
        text = text.encode("utf-8")
    with open(path, "wb") as fh:
        fh.write(text)


def _load_fbx_plugin():
    if not cmds.pluginInfo('fbxmaya', q=True, loaded=True):
        cmds.loadPlugin('fbxmaya')


def _mel_quiet(cmd):
    """执行 MEL，过程不存在时不刷红字（Maya mel 错误常不抛 Python 异常）。"""
    mel.eval('catchQuiet(`%s`)' % cmd.replace('`', ''))


def _configure_fbx_export():
    """固定 FBX 导出选项，避免轴向/单位把网格再次弄歪。"""
    for cmd in (
        'FBXResetExport',
        'FBXExportUpAxis y',
        'FBXExportScaleFactor 1.0',
        'FBXExportConvertUnitString "cm"',
        'FBXExportBakeComplexAnimation -v false',
        # 不导出上游连接，避免把整棵 shading network 打进 FBX；
        # 多材质槽依赖 mesh 上的 shadingEngine 面分配（LayerElementMaterial）。
        'FBXExportInputConnections -v false',
        'FBXExportInstances -v false',
        'FBXExportUseSceneName -v false',
        # Binary：体积约为 ASCII 的 1/5；mesh 名靠导出前节点名 + .meta fileIDToRecycleName
        'FBXExportInAscii -v false',
        'FBXExportEmbeddedTextures -v false',
    ):
        _mel_quiet(cmd)


def _shape_is_shared(transform):
    """shape 是否被多个 transform 共用（Maya 实例）。"""
    shapes = cmds.listRelatives(
        transform, shapes=True, noIntermediate=True, fullPath=True) or []
    for shape in shapes:
        if len(cmds.listRelatives(shape, allParents=True) or []) > 1:
            return True
    return False


def _ensure_unique_mesh(transform):
    """若 transform 仍是实例（shape 被多个父节点共用），转成独立几何体。

    直接导出实例时，Maya FBX 会把共享同一 shape 的所有 transform 一起展开；
    必须先变成独立物体，临时副本才能单独导出。
    """
    if not _shape_is_shared(transform):
        return transform
    cmds.select(transform, r=True)
    # Maya 2018+：Modify > Convert > Instance to Object
    try:
        mel.eval('catchQuiet(`ConvertInstanceToObject`)')
    except Exception:
        mel.eval('catchQuiet(`convertInstanceToObject`)')
    sel = cmds.ls(selection=True, type='transform', long=True) or []
    return sel[0] if sel else transform


def _cleanup_temp_exports():
    """清掉上次崩溃/延迟删除残留的临时导出节点。"""
    leftovers = cmds.ls(_TEMP_EXPORT_PREFIX + "*", long=True) or []
    if not leftovers:
        return
    try:
        cmds.select(clear=True)
    except Exception:
        pass
    try:
        cmds.delete(leftovers)
    except Exception:
        for n in leftovers:
            if cmds.objExists(n):
                try:
                    cmds.delete(n)
                except Exception:
                    pass


def _deferred_delete(nodes):
    """空闲时再删节点，避免 Outliner 仍握着 Qt 指针时立即 delete 崩溃。"""
    names = [n for n in (nodes or []) if n]

    def _do():
        try:
            cmds.select(clear=True)
        except Exception:
            pass
        for n in names:
            if cmds.objExists(n):
                try:
                    cmds.delete(n)
                except Exception:
                    pass
        _cleanup_temp_exports()

    try:
        cmds.evalDeferred(_do)
    except Exception:
        _do()


def _finish_fbx_mesh_name(out_path, desired, fallback):
    """确定写入 .meta 的 mesh 名。

    Binary FBX 无法文本改名：直接用 desired（与 fileIDToRecycleName 对齐）。
    ASCII 时仍尝试强制改 Geometry 名（兼容旧文件）。
    """
    desired = _sanitize_maya_name(desired)
    parsed = _read_fbx_mesh_name(out_path)
    if parsed is None:
        # Binary 或读不到：meta 用目标名
        debug.log("FBX mesh name (binary/meta): %s" % desired)
        return desired
    if parsed == desired:
        return desired
    forced = _force_fbx_mesh_name(out_path, desired)
    if forced:
        return forced
    debug.log("FBX mesh keep %s (want %s)" % (parsed, desired))
    return parsed or fallback or desired


def _unique_short_name(base):
    """生成场景内可用的短名。"""
    if not cmds.objExists(base):
        return base
    i = 1
    while cmds.objExists("%s_%d" % (base, i)):
        i += 1
    return "%s_%d" % (base, i)


def _sanitize_maya_name(name):
    """去掉命名空间，得到可用短名。"""
    if not name:
        return "Mesh"
    name = name.split("|")[-1]
    if ":" in name:
        name = name.split(":")[-1]
    return name or "Mesh"


def _list_fbx_mesh_names(fbx_path):
    """列出 ASCII FBX 中所有 Geometry:: Mesh 名。"""
    try:
        with open(fbx_path, "rb") as fh:
            data = fh.read()
    except Exception:
        return []
    if b"Kaydara FBX Binary" in data[:64]:
        return []
    try:
        text = data.decode("utf-8", "ignore")
    except Exception:
        return []
    names = re.findall(r'"Geometry::([^"]+)"\s*,\s*"Mesh"', text)
    # 保序去重
    seen = set()
    out = []
    for n in names:
        if n not in seen:
            seen.add(n)
            out.append(n)
    return out


def _force_fbx_mesh_name(fbx_path, desired):
    """把 ASCII FBX 内所有 Mesh Geometry / 对应 Model 名统一成 desired。

    Maya 复制后 shape 常变成 SM_Bucket_Small_2 这类名字，不能只替换临时 transform 名。
    """
    desired = _sanitize_maya_name(desired)
    try:
        with open(fbx_path, "rb") as fh:
            data = fh.read()
    except Exception:
        return None
    if b"Kaydara FBX Binary" in data[:64]:
        debug.log("FBX is binary; cannot rename mesh to %s" % desired)
        return None
    try:
        text = data.decode("utf-8", "ignore")
    except Exception:
        return None

    mesh_names = re.findall(r'"Geometry::([^"]+)"\s*,\s*"Mesh"', text)
    # 也收集 Model:: 节点名（与 Geometry 常成对）
    model_names = re.findall(r'"Model::([^"]+)"\s*,\s*"Mesh"', text)
    to_replace = []
    seen = set()
    for n in mesh_names + model_names:
        if n and n != desired and n not in seen:
            seen.add(n)
            to_replace.append(n)

    if not to_replace:
        debug.log("FBX mesh already named %s" % desired)
        return desired

    text2 = text
    for old in to_replace:
        text2 = text2.replace('"' + old + '"', '"' + desired + '"')
        text2 = text2.replace("::" + old, "::" + desired)
        # shape 后缀
        if old.endswith("Shape"):
            continue
        shape_old = old + "Shape"
        if shape_old in text2 and shape_old != desired:
            text2 = text2.replace('"' + shape_old + '"', '"' + desired + '"')
            text2 = text2.replace("::" + shape_old, "::" + desired)

    if text2 == text:
        debug.log("FBX force rename: no change (%s)" % ", ".join(to_replace))
        return to_replace[0]

    try:
        with open(fbx_path, "wb") as fh:
            fh.write(text2.encode("utf-8"))
    except Exception as e:
        debug.log("FBX force rename write failed: %s" % e)
        return to_replace[0]

    debug.log("FBX mesh forced %s -> %s" % (", ".join(to_replace), desired))
    return desired


def _export_fbx_inplace(src_long, out_path, desired):
    """非实例：临时把世界矩阵归零后导出，再还原。无 duplicate/delete。"""
    world_mat = cmds.xform(src_long, q=True, ws=True, matrix=True)
    try:
        pivots = cmds.xform(src_long, q=True, ws=True, pivots=True)
    except Exception:
        pivots = None
    try:
        cmds.xform(src_long, ws=True, matrix=_IDENTITY_MATRIX)
        try:
            cmds.xform(src_long, ws=True, pivots=(0, 0, 0))
        except Exception:
            pass
        _configure_fbx_export()
        cmds.select(src_long, r=True)
        cmds.file(out_path, force=True, exportSelected=True, type="FBX export")
        return _finish_fbx_mesh_name(
            out_path, desired, _sanitize_maya_name(src_long))
    finally:
        try:
            cmds.xform(src_long, ws=True, matrix=world_mat)
        except Exception:
            pass
        if pivots is not None:
            try:
                cmds.xform(src_long, ws=True, pivots=tuple(pivots[:3]))
            except Exception:
                pass
        try:
            cmds.select(clear=True)
        except Exception:
            pass


def _export_fbx_via_temp(src_long, out_path, desired):
    """实例：必须 duplicate + ConvertInstanceToObject；删除延后到 idle。

    尽量把临时物体命名为 desired，使 Binary FBX 内 mesh 名正确。
    """
    tmp = None
    desired = _sanitize_maya_name(desired)
    if cmds.objExists(desired):
        temp_name = _unique_short_name(_TEMP_EXPORT_PREFIX + "0")
    else:
        temp_name = desired
    used_name = temp_name
    try:
        tmp = cmds.duplicate(src_long, returnRootsOnly=True)[0]
        tmp = cmds.ls(tmp, long=True)[0]
        if cmds.listRelatives(tmp, parent=True):
            tmp = cmds.parent(tmp, world=True)[0]
            tmp = cmds.ls(tmp, long=True)[0]
        tmp = _ensure_unique_mesh(tmp)
        tmp = cmds.rename(tmp, temp_name)
        tmp = cmds.ls(tmp, long=True)[0]
        used_name = tmp.split("|")[-1]

        cmds.xform(tmp, ws=True, matrix=_IDENTITY_MATRIX)
        try:
            cmds.xform(tmp, ws=True, pivots=(0, 0, 0))
        except Exception:
            pass
        _configure_fbx_export()
        cmds.select(tmp, r=True)
        cmds.file(out_path, force=True, exportSelected=True, type="FBX export")
        return _finish_fbx_mesh_name(out_path, desired, used_name)
    finally:
        # 先藏起来并清空选择，再延迟删除（同步 delete 易触发 Outliner/Qt 崩溃）
        to_del = []
        if tmp and cmds.objExists(tmp):
            to_del.append(tmp)
        elif cmds.objExists(temp_name):
            to_del.append(temp_name)
        try:
            cmds.select(clear=True)
        except Exception:
            pass
        for n in to_del:
            try:
                cmds.hide(n)
            except Exception:
                pass
        _deferred_delete(to_del)


def _export_fbx(node, out_path, export_name=None):
    """把单个 transform 导出为对象空间 FBX。

    - 非实例：就地归零世界矩阵 → 导出 → 还原（不 duplicate/delete，避免 Qt 崩溃）
    - 实例：临时副本 + ConvertInstanceToObject → 导出 → 延迟删除
    返回最终 mesh 短名，供 .meta 的 fileIDToRecycleName 匹配。
    """
    desired = _sanitize_maya_name(export_name or node)
    src_long = cmds.ls(node, long=True)[0]
    try:
        cmds.select(clear=True)
    except Exception:
        pass
    if _shape_is_shared(src_long):
        debug.log("FBX via temp (instanced): %s -> %s" % (src_long, desired))
        return _export_fbx_via_temp(src_long, out_path, desired)
    debug.log("FBX inplace (unique): %s -> %s" % (src_long, desired))
    return _export_fbx_inplace(src_long, out_path, desired)


def _read_fbx_mesh_name(fbx_path):
    """从 ASCII FBX 里提取主 Mesh 节点名；失败返回 None。"""
    try:
        with open(fbx_path, "rb") as fh:
            data = fh.read(300000)
    except Exception:
        return None
    try:
        text = data.decode("utf-8", "ignore")
    except Exception:
        text = data
    names = re.findall(r'"Geometry::([^"]+)"\s*,\s*"Mesh"', text)
    if not names:
        return None
    for n in names:
        if "_LOD" not in n:
            return n
    return names[0]


def _member_min_face(member):
    """从 sets 成员名解析最小面号；整物体赋值（无 .f[]）返回 -1。"""
    m = re.search(r'\.f\[(\d+)(?::(\d+))?\]', member)
    if not m:
        return -1
    return int(m.group(1))


def _shape_to_materials(materials):
    """建立 shape 短名 -> [Material, ...]（按最小面号排序，对应 Unity submesh 槽序）。

    同一 mesh 上多个 shadingEngine / 按面赋材质时，返回多个材质；单材质仍为长度 1。
    """
    mat_by_name = {m.name: m for m in materials}
    # shape -> [(sort_key, mat_name, Material)]
    buckets = {}

    for se in cmds.ls(type='shadingEngine'):
        if se == 'initialShadingGroup':
            continue
        mats = cmds.listConnections(se + '.surfaceShader') or []
        if not mats:
            continue
        leaf = mats[0].split('|')[-1]
        mat = mat_by_name.get(leaf)
        members = cmds.sets(se, q=True) or []
        debug.log("SG %s -> material=%s, members=%s" % (se, leaf, members))
        if mat is None:
            debug.log("  material %s not in collected list %s" % (
                leaf, sorted(mat_by_name.keys())))
            continue

        # 本 SG 影响到的 shape -> 最小面号
        shape_faces = {}
        for member in members:
            node = member.split('.')[0].split('|')[-1]
            if not cmds.objExists(node):
                continue
            node_type = cmds.nodeType(node)
            if node_type == 'mesh':
                shape_names = [node]
            elif node_type == 'transform':
                shape_names = [
                    s.split('|')[-1]
                    for s in (cmds.listRelatives(node, shapes=True, noIntermediate=True) or [])
                ]
            else:
                continue
            fmin = _member_min_face(member)
            for shape_name in shape_names:
                if shape_name not in shape_faces:
                    shape_faces[shape_name] = fmin
                else:
                    prev = shape_faces[shape_name]
                    # -1 表示整物体，保持优先；否则取更小面号
                    if prev == -1 or fmin == -1:
                        shape_faces[shape_name] = -1
                    else:
                        shape_faces[shape_name] = min(prev, fmin)

        for shape_name, fmin in shape_faces.items():
            buckets.setdefault(shape_name, []).append((fmin, leaf, mat))
            debug.log("  mapped %s faceMin=%s -> %s" % (shape_name, fmin, leaf))

    result = {}
    for shape_name, items in buckets.items():
        items.sort(key=lambda x: (x[0], x[1]))
        ordered = []
        seen = set()
        for _fmin, leaf, mat in items:
            if leaf in seen:
                continue
            seen.add(leaf)
            ordered.append(mat)
        result[shape_name] = ordered
        debug.log("shape %s -> %d material slot(s): %s" % (
            shape_name, len(ordered), [m.name for m in ordered]))
    return result


def _long_name(node):
    """统一为 Maya long name。"""
    longs = cmds.ls(node, long=True) or []
    return longs[0] if longs else node


def _make_item(transform, shape, prefab_name, used, mesh_name=None):
    """构造导出项，并对重名 Prefab 追加序号。

    mesh_name：写入 FBX Geometry 的网格名；默认与 prefab_name 相同。
    非实例化应传入 Maya 原始短名，避免去尾数字或重名序号改写 mesh 名。
    """
    name = prefab_name
    if name in used:
        used[name] += 1
        name = "%s_%d" % (prefab_name, used[name])
    else:
        used[name] = 1
    return {
        "transform": transform,
        "shape": shape,
        "prefab_name": name,
        "mesh_name": mesh_name or prefab_name,
    }


def export(output_dir, selection=None, scene_name="MayaToUnity"):
    """导出整个场景（或选中对象）为 Unity 项目资源。

    返回生成的文件路径列表；没有可导出的网格时返回 None。
    """
    if not os.path.isdir(output_dir):
        os.makedirs(output_dir)

    materials_dir = os.path.join(output_dir, "Materials")
    textures_dir = os.path.join(output_dir, "Textures")
    models_dir = os.path.join(output_dir, "Models")
    prefabs_dir = os.path.join(output_dir, "Prefabs")
    for d in (materials_dir, textures_dir, models_dir, prefabs_dir):
        if not os.path.isdir(d):
            os.makedirs(d)

    _load_fbx_plugin()
    _cleanup_temp_exports()

    debug.clear_log()
    debug.log("Export started -> %s" % output_dir)
    debug.log("Scope: %s" % ("selection" if selection else "whole scene"))
    debug.log("Debug mode: %s" % ("on" if debug.DEBUG else "off"))

    # 1. 材质 / 贴图
    materials, textures = collect_materials()
    for m in materials:
        m.guid = guid_from_string("mat:" + m.name)
    for t in textures:
        t.guid = guid_from_string("tex:" + t.name)

    debug.log("Materials: %d, Textures: %d" % (len(materials), len(textures)))

    shape_mats = _shape_to_materials(materials)

    # 2. 实例 / 相同网格分析（每组只导一份 FBX）
    groups, singletons = analyze(selection)
    debug.log("Shared-mesh groups: %d, singletons: %d" % (len(groups), len(singletons)))
    for g in groups:
        debug.log(
            "  [Group kind=%s] prefab=%s base=%s members=%d"
            % (getattr(g, "kind", "instance"), g.prefab_name,
               g.base, len(g.members))
        )
        for m in g.members:
            debug.log("      - %s%s" % (
                m, " (base/FBX)" if _long_name(m) == _long_name(g.base) else ""))

    # 3. 组装导出项（去重 Prefab 名）——组员不单独进 export_items
    used = {}
    export_items = []
    for g in groups:
        export_items.append(_make_item(
            _long_name(g.base), g.shape, g.prefab_name, used,
            mesh_name=g.prefab_name))
    for s in singletons:
        s_long = _long_name(s)
        leaf = _sanitize_maya_name(s_long)
        shape = (cmds.listRelatives(s_long, shapes=True, noIntermediate=True) or [None])[0]
        export_items.append(_make_item(s_long, shape, leaf, used, mesh_name=leaf))

    if not export_items:
        cmds.warning(to_maya_ui("Unity_MayaToUnity: no mesh to export."))
        return None

    # 清掉「真 Instance」组员残留的旧 FBX/Prefab（不含独立模型）
    for g in groups:
        if getattr(g, "kind", "instance") != "instance":
            continue
        base_short = _sanitize_maya_name(g.base)
        keep = set([base_short, g.prefab_name])
        # 旧版无后缀的文件名也纳入保留/对照
        if g.prefab_name.endswith("_Inst"):
            keep.add(g.prefab_name[:-5])
        for m in g.members:
            leaf = _sanitize_maya_name(m)
            if leaf in keep:
                continue
            # 仅当该短名不在 singletons 里时才删，防止误删独立模型
            singleton_names = set(
                _sanitize_maya_name(s) for s in singletons)
            if leaf in singleton_names:
                continue
            for folder, ext in (
                (models_dir, ".fbx"),
                (models_dir, ".fbx.meta"),
                (prefabs_dir, ".prefab"),
                (prefabs_dir, ".prefab.meta"),
            ):
                obsolete = os.path.join(folder, leaf + ext)
                if os.path.isfile(obsolete):
                    try:
                        os.remove(obsolete)
                        debug.log("Removed obsolete instance member asset: %s" % obsolete)
                    except Exception as exc:
                        debug.log("Failed to remove %s: %s" % (obsolete, exc), warn=True)

    # 在任何导出操作前缓存变换；mesh 用 long name 作 key
    mesh_paths = []
    for g in groups:
        for member in g.members:
            mesh_paths.append(_long_name(member))
    for s in singletons:
        mesh_paths.append(_long_name(s))

    # 去重且保序
    seen = set()
    ordered_meshes = []
    for p in mesh_paths:
        if p not in seen:
            seen.add(p)
            ordered_meshes.append(p)

    hierarchy = build_hierarchy(ordered_meshes)
    scene_lights, ambients = map_lights(selection)
    light_by_path = dict((L["path"], L) for L in scene_lights)
    if scene_lights:
        hierarchy = merge_lights_into_hierarchy(
            hierarchy, [L["path"] for L in scene_lights])
    debug.log("Hierarchy nodes: %d (groups+meshes+lights)" % len(hierarchy))
    debug.log(
        "Lights: %d scene + %d ambient"
        % (len(scene_lights), len(ambients))
    )
    for hn in hierarchy:
        debug.log("  [%s] %s parent=%s children=%s" % (
            hn.kind, hn.name, hn.parent_path, [c.split('|')[-1] for c in hn.children]))

    # 4. 每个导出项：FBX + meta + prefab + prefab meta
    prefab_guid_by_transform = {}
    generated = []
    written_mats = set()

    # 把 export item 的 transform 确保为 long name（上面已规范）
    for item in export_items:
        item["transform"] = _long_name(item["transform"])

    # 导出期间暂停 UI 刷新，减轻 Outliner 在 duplicate/delete 时崩溃
    try:
        cmds.refresh(suspend=True)
    except Exception:
        pass
    try:
        for item in export_items:
            t = item["transform"]
            shape = item["shape"]
            prefab_name = item["prefab_name"]
            mesh_want = item.get("mesh_name") or prefab_name

            fbx_guid = guid_from_string("fbx:" + prefab_name)
            prefab_guid = guid_from_string("prefab:" + prefab_name)

            debug.log("Item: transform=%s shape=%s prefab=%s mesh=%s" % (
                t, shape, prefab_name, mesh_want))

            fbx_path = os.path.join(models_dir, prefab_name + ".fbx")
            # 非实例化 mesh 用原模型名；实例组用共享 prefab 名
            exported_name = _export_fbx(t, fbx_path, export_name=mesh_want)

            parsed_mesh = _read_fbx_mesh_name(fbx_path)
            if parsed_mesh is None:
                debug.log("FBX is binary; use export name as mesh name: %s" % exported_name)
            mesh_name = parsed_mesh or exported_name or mesh_want
            debug.log("  FBX mesh name: %s (want %s)" % (mesh_name, mesh_want))

            shape_key = shape.split('|')[-1] if shape else None
            # 防御：若 shape 误存成 uuid:/node: 身份键，改从 transform 解析
            if (not shape_key) or shape_key.startswith("uuid:") or shape_key.startswith("node:"):
                real_shapes = cmds.listRelatives(
                    t, shapes=True, noIntermediate=True) or []
                for s in real_shapes:
                    if cmds.nodeType(s) == 'mesh':
                        shape_key = s.split('|')[-1]
                        break
            mats_for_shape = shape_mats.get(shape_key) or []
            material_guids = [m.guid for m in mats_for_shape]
            debug.log("  fbx_guid=%s prefab_guid=%s shape_key=%s materials=%s" % (
                fbx_guid, prefab_guid, shape_key,
                [m.name for m in mats_for_shape] if mats_for_shape else "(none)"))

            # 先写 .mat，再写 FBX .meta（含 Remapped Materials 引用）
            for material in mats_for_shape:
                if material.name in written_mats:
                    continue
                written_mats.add(material.name)
                mat_path = os.path.join(materials_dir, material.name + ".mat")
                _write_text(mat_path, yaml_writer.material_file(material))
                _write_text(mat_path + ".meta", yaml_writer.material_meta(material.guid))
                generated.append(mat_path)

            _write_text(
                fbx_path + ".meta",
                yaml_writer.fbx_meta(fbx_guid, mesh_name, mats_for_shape),
            )

            prefab_path = os.path.join(prefabs_dir, prefab_name + ".prefab")
            _write_text(prefab_path, yaml_writer.prefab_file(prefab_name, fbx_guid, material_guids))
            _write_text(prefab_path + ".meta", yaml_writer.prefab_meta(prefab_guid))

            prefab_guid_by_transform[t] = prefab_guid
            generated.append(prefab_path)
    finally:
        try:
            cmds.refresh(suspend=False)
        except Exception:
            pass
        try:
            cmds.select(clear=True)
        except Exception:
            pass
        # 不要 force refresh：Outliner 立刻重建时若叠鼠标事件会 Qt SharedPointer 崩溃

    # 5. 补写尚未写出的材质（理论上上面循环已覆盖）
    for item in export_items:
        shape = item["shape"]
        shape_key = shape.split('|')[-1] if shape else None
        for material in (shape_mats.get(shape_key) or []):
            if material.name in written_mats:
                continue
            written_mats.add(material.name)
            mat_path = os.path.join(materials_dir, material.name + ".mat")
            _write_text(mat_path, yaml_writer.material_file(material))
            _write_text(mat_path + ".meta", yaml_writer.material_meta(material.guid))
            generated.append(mat_path)

    # 6. 贴图文件（复制 + meta）
    for tex in textures:
        dst = os.path.join(textures_dir, tex.name)
        if os.path.isfile(tex.src_path) and not os.path.isfile(dst):
            try:
                shutil.copyfile(tex.src_path, dst)
            except Exception as e:
                cmds.warning(to_maya_ui(
                    "Unity_MayaToUnity: failed to copy texture %s: %s" % (tex.src_path, e)
                ))
        _write_text(
            dst + ".meta",
            yaml_writer.texture_meta(
                tex.guid,
                srgb=getattr(tex, "srgb", True),
                normal=getattr(tex, "is_normal", False),
            ),
        )
        generated.append(dst)

    # 实例组的每个成员都引用 base 的 prefab
    for g in groups:
        base = _long_name(g.base)
        base_guid = prefab_guid_by_transform[base]
        for member in g.members:
            prefab_guid_by_transform[_long_name(member)] = base_guid

    # 7. 场景：保留组嵌套，本地 TRS 相对父节点；灯光写成 Light GameObject
    # fileID：空组 GO 5000+ / Transform 5000000+；
    #         PrefabInstance 1000+ / stripped 1000000+；
    #         灯光 GO 3000+ / Light 300000+ / Transform 310000+
    go_id = 5000
    group_tr_id = 5000000
    instance_id = 1000
    mesh_tr_id = 1000000
    light_go_id = 3000
    light_comp_id = 300000
    light_tr_id = 310000

    # path -> {kind, tr_id, go_id/instance_id/light_id, root_order, props?}
    id_map = {}
    # 先算每个节点在兄弟中的 root_order（场景根从 2 起，避开相机/灯）
    root_order_of = {}
    roots = [x.path for x in hierarchy if x.parent_path is None]
    by_path = dict((x.path, x) for x in hierarchy)
    for hn in hierarchy:
        if hn.parent_path and hn.parent_path in by_path:
            siblings = by_path[hn.parent_path].children
            root_order_of[hn.path] = siblings.index(hn.path) if hn.path in siblings else 0
        else:
            root_order_of[hn.path] = 2 + roots.index(hn.path) if hn.path in roots else 2

    for hn in hierarchy:
        if hn.kind == 'group':
            id_map[hn.path] = {
                'kind': 'group',
                'go_id': go_id,
                'tr_id': group_tr_id,
                'root_order': root_order_of.get(hn.path, 0),
            }
            go_id += 1
            group_tr_id += 1
        elif hn.kind == 'light':
            id_map[hn.path] = {
                'kind': 'light',
                'go_id': light_go_id,
                'light_id': light_comp_id,
                'tr_id': light_tr_id,
                'root_order': root_order_of.get(hn.path, 0),
                'props': light_by_path.get(hn.path),
            }
            light_go_id += 1
            light_comp_id += 1
            light_tr_id += 1
        else:
            id_map[hn.path] = {
                'kind': 'mesh',
                'instance_id': instance_id,
                'tr_id': mesh_tr_id,
                'root_order': root_order_of.get(hn.path, 0),
            }
            instance_id += 1
            mesh_tr_id += 1

    blocks = []
    # 先写空组（父先于子已由 hierarchy 排序保证）；灯光节点不当空组写
    for hn in hierarchy:
        if hn.kind != 'group':
            continue
        info = id_map[hn.path]
        parent_tr = 0
        if hn.parent_path and hn.parent_path in id_map:
            parent_tr = id_map[hn.parent_path]['tr_id']
        pos, rot, scale = transform_to_unity_local(hn.path, hn.parent_path)
        child_trs = []
        for cp in hn.children:
            if cp in id_map:
                child_trs.append(id_map[cp]['tr_id'])
        debug.log("Group %s local pos=%s parent_tr=%s" % (hn.name, pos, parent_tr))
        blocks.append(yaml_writer.empty_gameobject_block(
            info['go_id'], info['tr_id'], hn.name, parent_tr,
            pos, rot, scale, child_trs, info['root_order'],
        ))

    # 灯光 GameObject
    sun_light_id = 0
    for hn in hierarchy:
        if hn.kind != 'light':
            continue
        info = id_map[hn.path]
        props = info.get('props') or light_by_path.get(hn.path)
        if not props:
            cmds.warning(to_maya_ui(
                "Unity_MayaToUnity: missing light props for %s, skip." % hn.path
            ))
            continue
        parent_tr = 0
        if hn.parent_path and hn.parent_path in id_map:
            parent_tr = id_map[hn.parent_path]['tr_id']
        pos, rot, scale = transform_to_unity_local_light(hn.path, hn.parent_path)
        child_trs = []
        for cp in hn.children:
            if cp in id_map:
                child_trs.append(id_map[cp]['tr_id'])
        debug.log(
            "Light %s type=%s intensity=%s parent_tr=%s"
            % (hn.name, props.get("maya_type"), props.get("intensity"), parent_tr)
        )
        blocks.append(yaml_writer.light_gameobject_block(
            info['go_id'], info['light_id'], info['tr_id'], hn.name, props,
            parent_tr, pos, rot, scale, child_trs, info['root_order'],
        ))
        if sun_light_id == 0 and props.get("unity_type") == 1:
            sun_light_id = info['light_id']

    # 再写 PrefabInstance
    for hn in hierarchy:
        if hn.kind != 'mesh':
            continue
        info = id_map[hn.path]
        parent_tr = 0
        if hn.parent_path and hn.parent_path in id_map:
            parent_tr = id_map[hn.parent_path]['tr_id']
        guid = prefab_guid_by_transform.get(hn.path)
        if guid is None:
            # 短名回退
            for k, v in prefab_guid_by_transform.items():
                if k.split('|')[-1] == hn.name or _long_name(k) == hn.path:
                    guid = v
                    break
        if guid is None:
            cmds.warning(to_maya_ui(
                "Unity_MayaToUnity: no prefab guid for %s, skip." % hn.path
            ))
            continue
        pos, rot, scale = transform_to_unity_local(hn.path, hn.parent_path)
        debug.log("Mesh %s local pos=%s parent_tr=%s" % (hn.name, pos, parent_tr))
        blocks.append(yaml_writer.prefab_instance_block(
            info['instance_id'], info['tr_id'], guid, hn.name,
            pos, rot, scale,
            parent_transform_id=parent_tr,
            root_order=info['root_order'],
        ))

    debug.log("Scene hierarchy nodes written: %d" % len(hierarchy))

    ambient_col = ambient_color_from(ambients)
    disable_default = bool(scene_lights)
    scene_path = os.path.join(output_dir, scene_name + ".unity")
    _write_text(
        scene_path,
        yaml_writer.scene_file(
            "".join(blocks),
            disable_default_light=disable_default,
            ambient_color=ambient_col,
            sun_light_id=sun_light_id,
        ),
    )
    generated.append(scene_path)

    # 写诊断报告（始终写入，排查问题有据可查）
    report_path = os.path.join(output_dir, "export_report.log")
    _write_text(report_path, debug.get_report())
    generated.append(report_path)

    debug.log("Export done, %d files" % len(generated))

    return generated
