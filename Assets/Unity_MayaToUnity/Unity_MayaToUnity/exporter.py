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

from constants import guid_from_string
from coords import transform_to_unity_local
from instance_detector import analyze, strip_trailing_digits
from hierarchy import build_hierarchy
from material_mapper import collect_materials
import yaml_writer
import debug

# 世界单位矩阵：导出前把临时物体摆到原点，让 FBX 只存对象空间顶点
_IDENTITY_MATRIX = [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1]
# 固定临时导出名，避免与场景里已有 SM_Bucket_Small_2 等重名被 Maya 改成 _6
_TEMP_EXPORT_NAME = "__MTU_Mesh__"


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
        'FBXExportInputConnections -v false',
        'FBXExportInstances -v false',
        'FBXExportUseSceneName -v false',
    ):
        _mel_quiet(cmd)


def _ensure_unique_mesh(transform):
    """若 transform 仍是实例（shape 被多个父节点共用），转成独立几何体。

    直接导出实例时，Maya FBX 会把共享同一 shape 的所有 transform 一起展开；
    必须先变成独立物体，临时副本才能单独导出。
    """
    shapes = cmds.listRelatives(transform, shapes=True, noIntermediate=True, fullPath=True) or []
    shared = False
    for shape in shapes:
        if len(cmds.listRelatives(shape, allParents=True) or []) > 1:
            shared = True
            break
    if not shared:
        return transform
    cmds.select(transform, r=True)
    # Maya 2018+：Modify > Convert > Instance to Object
    try:
        mel.eval('catchQuiet(`ConvertInstanceToObject`)')
    except Exception:
        mel.eval('catchQuiet(`convertInstanceToObject`)')
    sel = cmds.ls(selection=True, type='transform', long=True) or []
    return sel[0] if sel else transform


def _export_fbx(node, out_path, export_name=None):
    """把单个 transform 导出为对象空间 FBX，不改动原场景节点。

    Maya 导出选中时会把世界变换烘焙进顶点。若带着旋转/缩放导出，FBX 顶点
    就成了世界坐标，场景再按世界矩阵摆放会双重叠加。实例还会把共享 shape
    的所有成员一起展开进 FBX。

    做法：复制临时物体 → 转成独立几何 → 解父到世界 → 世界矩阵归零 → 导出 →
    删除临时物体。不依赖 undo（cmds.file 导出常会冲掉 undo 队列）。

    临时物体固定命名为 __MTU_Mesh__（与场景物体不冲突），返回该短名供
    .meta 的 fileIDToRecycleName 匹配。export_name 参数保留兼容，已忽略。
    """
    orig = cmds.ls(selection=True, long=True)
    tmp = None
    used_name = _TEMP_EXPORT_NAME
    try:
        # 清掉上次残留
        if cmds.objExists(used_name):
            cmds.delete(used_name)
        tmp = cmds.duplicate(node, returnRootsOnly=True)[0]
        tmp = cmds.ls(tmp, long=True)[0]
        if cmds.listRelatives(tmp, parent=True):
            tmp = cmds.parent(tmp, world=True)[0]
            tmp = cmds.ls(tmp, long=True)[0]
        tmp = _ensure_unique_mesh(tmp)
        tmp = cmds.rename(tmp, used_name)
        tmp = cmds.ls(tmp, long=True)[0]
        used_name = tmp.split('|')[-1]
        # 归零世界变换 + 枢轴，确保导出烘焙后顶点仍是对象空间
        cmds.xform(tmp, ws=True, matrix=_IDENTITY_MATRIX)
        try:
            cmds.xform(tmp, ws=True, pivots=(0, 0, 0))
        except Exception:
            pass
        _configure_fbx_export()
        cmds.select(tmp, r=True)
        # FBXExport 的 -s 是纯标志位，经 cmds 传布尔值会报语法错误；改用 file 命令
        cmds.file(out_path, force=True, exportSelected=True, type="FBX export")
        return used_name
    finally:
        if tmp and cmds.objExists(tmp):
            cmds.delete(tmp)
        # 再扫一遍，防止 rename 后路径变化漏删
        if cmds.objExists(_TEMP_EXPORT_NAME):
            cmds.delete(_TEMP_EXPORT_NAME)
        if orig:
            alive = [n for n in orig if cmds.objExists(n)]
            if alive:
                cmds.select(alive, r=True)
            else:
                cmds.select(clear=True)
        else:
            cmds.select(clear=True)


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


def _shape_to_material(materials):
    """建立 shape 短名 -> Material 的映射（通过 shadingEngine 成员反查）。"""
    result = {}
    mat_by_name = {m.name: m for m in materials}
    for se in cmds.ls(type='shadingEngine'):
        mats = cmds.listConnections(se + '.surfaceShader') or []
        if not mats:
            continue
        leaf = mats[0].split('|')[-1]
        mat = mat_by_name.get(leaf)
        members = cmds.sets(se, q=True) or []
        debug.log("SG %s -> material=%s, members=%s" % (se, leaf, members))
        if mat is None:
            debug.log("  material %s not in collected list %s" % (leaf, sorted(mat_by_name.keys())))
            continue
        for member in members:
            # member 可能是 "transform.f[0:N]"（Maya 省略 shape 名，前缀是 transform），
            # 也可能是 "shape.f[0:N]" 或纯 "shape"；统一解析出 shape 短名
            node = member.split('.')[0].split('|')[-1]
            node_type = cmds.nodeType(node)
            if node_type == 'mesh':
                shape_names = [node]
            elif node_type == 'transform':
                shape_names = [s.split('|')[-1] for s in (
                    cmds.listRelatives(node, shapes=True, noIntermediate=True) or [])]
            else:
                continue
            for shape_name in shape_names:
                result[shape_name] = mat
                debug.log("  mapped %s -> %s" % (shape_name, leaf))
    return result


def _long_name(node):
    """统一为 Maya long name。"""
    longs = cmds.ls(node, long=True) or []
    return longs[0] if longs else node


def _make_item(transform, shape, prefab_name, used):
    """构造导出项，并对重名 Prefab 追加序号。"""
    name = prefab_name
    if name in used:
        used[name] += 1
        name = "%s_%d" % (prefab_name, used[name])
    else:
        used[name] = 1
    return {"transform": transform, "shape": shape, "prefab_name": name}


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

    shape_mat = _shape_to_material(materials)

    # 2. 实例分析
    groups, singletons = analyze(selection)
    debug.log("Instance groups: %d, singletons: %d" % (len(groups), len(singletons)))

    # 3. 组装导出项（去重 Prefab 名）
    used = {}
    export_items = []
    for g in groups:
        export_items.append(_make_item(
            _long_name(g.base), g.shape, g.prefab_name, used))
    for s in singletons:
        s_long = _long_name(s)
        leaf = s_long.split('|')[-1]
        shape = (cmds.listRelatives(s_long, shapes=True, noIntermediate=True) or [None])[0]
        export_items.append(_make_item(s_long, shape, strip_trailing_digits(leaf), used))

    if not export_items:
        cmds.warning("Unity_MayaToUnity: no mesh to export.")
        return None

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
    debug.log("Hierarchy nodes: %d (groups+meshes)" % len(hierarchy))
    for hn in hierarchy:
        debug.log("  [%s] %s parent=%s children=%s" % (
            hn.kind, hn.name, hn.parent_path, [c.split('|')[-1] for c in hn.children]))

    # 4. 每个导出项：FBX + meta + prefab + prefab meta
    prefab_guid_by_transform = {}
    generated = []

    # 把 export item 的 transform 确保为 long name（上面已规范）
    for item in export_items:
        item["transform"] = _long_name(item["transform"])

    for item in export_items:
        t = item["transform"]
        shape = item["shape"]
        prefab_name = item["prefab_name"]

        fbx_guid = guid_from_string("fbx:" + prefab_name)
        prefab_guid = guid_from_string("prefab:" + prefab_name)

        debug.log("Item: transform=%s shape=%s prefab=%s" % (t, shape, prefab_name))

        fbx_path = os.path.join(models_dir, prefab_name + ".fbx")
        # 临时物体固定名 __MTU_Mesh__，避免与场景节点重名
        exported_name = _export_fbx(t, fbx_path)

        parsed_mesh = _read_fbx_mesh_name(fbx_path)
        if parsed_mesh is None:
            debug.log("FBX is binary; use export name as mesh name: %s" % exported_name)
        mesh_name = parsed_mesh or exported_name or _TEMP_EXPORT_NAME
        debug.log("  FBX mesh name: %s" % mesh_name)
        _write_text(fbx_path + ".meta", yaml_writer.fbx_meta(fbx_guid, mesh_name))

        shape_key = shape.split('|')[-1] if shape else None
        material = shape_mat.get(shape_key)
        material_guid = material.guid if material else None
        debug.log("  fbx_guid=%s prefab_guid=%s material=%s" % (
            fbx_guid, prefab_guid, material.name if material else "(none)"))

        prefab_path = os.path.join(prefabs_dir, prefab_name + ".prefab")
        _write_text(prefab_path, yaml_writer.prefab_file(prefab_name, fbx_guid, material_guid))
        _write_text(prefab_path + ".meta", yaml_writer.prefab_meta(prefab_guid))

        prefab_guid_by_transform[t] = prefab_guid
        generated.append(prefab_path)

    # 5. 材质文件（去重）
    written_mats = set()
    for item in export_items:
        shape = item["shape"]
        shape_key = shape.split('|')[-1] if shape else None
        material = shape_mat.get(shape_key)
        if material is None or material.name in written_mats:
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
                cmds.warning("Unity_MayaToUnity: failed to copy texture %s: %s" % (tex.src_path, e))
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

    # 7. 场景：保留组嵌套，本地 TRS 相对父节点
    # fileID：空组 GO 5000+ / Transform 5000000+；PrefabInstance 1000+ / stripped 1000000+
    go_id = 5000
    group_tr_id = 5000000
    instance_id = 1000
    mesh_tr_id = 1000000

    # path -> {kind, tr_id, go_id/instance_id, root_order}
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
    # 先写空组（父先于子已由 hierarchy 排序保证）
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
            cmds.warning("Unity_MayaToUnity: no prefab guid for %s, skip." % hn.path)
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

    scene_path = os.path.join(output_dir, scene_name + ".unity")
    _write_text(scene_path, yaml_writer.scene_file("".join(blocks)))
    generated.append(scene_path)

    # 写诊断报告（始终写入，排查问题有据可查）
    report_path = os.path.join(output_dir, "export_report.log")
    _write_text(report_path, debug.get_report())
    generated.append(report_path)

    debug.log("Export done, %d files" % len(generated))

    return generated
