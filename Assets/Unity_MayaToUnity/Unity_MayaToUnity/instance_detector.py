# -*- coding: utf-8 -*-
"""Maya 实例识别。

仅识别真正的 Maya Instance（多个 transform 共享同一 mesh shape）。
普通 Duplicate 出的独立网格各自导出，不做几何指纹合并（避免误合并）。

导出时每个实例组只生成一个 Prefab/FBX（base），组内其它 transform
在场景里作为该 Prefab 的实例摆放。
"""
from __future__ import print_function

import re

import maya.cmds as cmds

from constants import (
    INSTANCE_BASE_SUFFIX,
    MTU_PREFAB_NAME_ATTR,
    to_unicode,
)


class InstanceGroup(object):
    """一个实例组：共享同一 shape 的一组 transform。"""

    def __init__(self, shape, base, members, prefab_name, kind="instance"):
        self.shape = shape
        self.base = base
        self.members = members
        self.prefab_name = prefab_name
        self.kind = kind  # 仅 "instance"


def strip_trailing_digits(name):
    """去掉名字末尾数字：SM_Bucket_Small_2 -> SM_Bucket_Small。"""
    m = re.match(r'^(.*?)\d+$', name)
    if m and m.group(1):
        return m.group(1).rstrip('_ .-')
    return name


def _short(node):
    name = (node or "").split("|")[-1]
    if ":" in name:
        name = name.split(":")[-1]
    return name


def _exc_text(exc):
    """异常原文（保留中文）。Python 2 的 str(异常) 会按 ascii 编码而再次报错。"""
    if isinstance(exc, UnicodeError):
        obj = getattr(exc, "object", u"")
        if isinstance(obj, basestring):
            return u"%s: %s" % (exc.__class__.__name__, to_unicode(obj))
    args = getattr(exc, "args", ()) or ()
    if args and isinstance(args[0], basestring):
        return to_unicode(args[0])
    try:
        return to_unicode(repr(exc))
    except Exception:
        return u"%s" % exc.__class__.__name__


def _api_names(name):
    """OpenMaya 1.0 只收字节串。中文名依次试 utf-8 / 系统编码。"""
    if not isinstance(name, unicode):
        return [name]
    out = []
    for enc in ("utf-8", "mbcs", "gbk"):
        try:
            raw = name.encode(enc)
        except Exception:
            continue
        if raw not in out:
            out.append(raw)
    return out


def _dag_path_of(om, name):
    last = None
    for cand in _api_names(name):
        try:
            sel = om.MSelectionList()
            sel.add(cand)
            dag = om.MDagPath()
            sel.getDagPath(0, dag)
            return dag
        except Exception as exc:
            last = exc
    if last is not None:
        raise last
    raise RuntimeError(u"empty node name")


def _long(node):
    longs = cmds.ls(node, long=True) or []
    return longs[0] if longs else node


def default_prefab_name(base):
    """由 base transform 推导默认导出 Prefab/FBX 名。"""
    leaf = _short(base)
    return strip_trailing_digits(leaf) + INSTANCE_BASE_SUFFIX


def get_prefab_name_override(node):
    """读取节点上的自定义导出名；无则返回空串。"""
    if not node or not cmds.objExists(node):
        return ""
    if not cmds.attributeQuery(MTU_PREFAB_NAME_ATTR, node=node, exists=True):
        return ""
    try:
        val = cmds.getAttr("%s.%s" % (node, MTU_PREFAB_NAME_ATTR))
    except Exception:
        return ""
    if val is None:
        return ""
    return (val or "").strip()


def set_prefab_name_override(node, name):
    """把自定义导出名写到节点（通常是共享 mesh shape）。"""
    if not node or not cmds.objExists(node):
        return False
    name = (name or "").strip()
    if not name:
        return False
    plug = "%s.%s" % (node, MTU_PREFAB_NAME_ATTR)
    if not cmds.attributeQuery(MTU_PREFAB_NAME_ATTR, node=node, exists=True):
        try:
            cmds.addAttr(node, ln=MTU_PREFAB_NAME_ATTR, dt="string")
        except Exception:
            return False
    try:
        cmds.setAttr(plug, name, type="string")
        return True
    except Exception:
        return False


def resolve_prefab_name(base, shape=None):
    """实例组最终导出名：优先 shape/base 上的覆盖，否则默认规则。"""
    shape = shape or _mesh_shape(base)
    for n in (shape, base):
        override = get_prefab_name_override(n)
        if override:
            return override
    return default_prefab_name(base)


def _as_name_list(value):
    """把 cmds 返回值收成名字列表（兼容偶发单字符串）。"""
    if not value:
        return []
    if isinstance(value, (list, tuple)):
        out = []
        for item in value:
            if item:
                out.append(item)
        return out
    return [value]


def _node_type(node):
    """安全读节点类型；Maya cmds 对异常参数会抛 argument list must be a tuple。"""
    if not node or isinstance(node, (list, tuple, dict, set)):
        return ""
    try:
        return cmds.nodeType(node) or ""
    except Exception:
        return ""


def _mesh_shapes_of(node):
    """transform 下的非 intermediate mesh shape（long path）。"""
    if not node:
        return []
    try:
        shapes = cmds.listRelatives(
            node,
            shapes=True,
            noIntermediate=True,
            fullPath=True,
            type="mesh",
        )
    except Exception:
        shapes = None
    return _as_name_list(shapes)


def _is_mesh_transform(node):
    if _node_type(node) != "transform":
        return False
    return bool(_mesh_shapes_of(node))


def collect_mesh_transforms(selection=None):
    """收集要导出的 mesh transform。"""
    if not selection:
        result = []
        for t in _as_name_list(cmds.ls(type="transform", long=True)):
            if _is_mesh_transform(t):
                result.append(t)
        return result

    result = []
    for obj in selection:
        ntype = _node_type(obj)
        if ntype == "mesh":
            for p in _as_name_list(
                    cmds.listRelatives(obj, parent=True, fullPath=True)):
                if p not in result:
                    result.append(p)
        elif ntype == "transform":
            if _is_mesh_transform(obj):
                if obj not in result:
                    result.append(obj)
            else:
                children = _as_name_list(cmds.listRelatives(
                    obj, ad=True, type="transform", fullPath=True))
                for child in children:
                    if _is_mesh_transform(child) and child not in result:
                        result.append(child)
    return result


def _mesh_shape(transform):
    """返回 transform 下第一个非 intermediate mesh shape（long name）。"""
    shapes = _mesh_shapes_of(transform)
    if not shapes:
        return None
    return _long(shapes[0])


def _shape_identity(transform):
    """同一 Maya Instance 共享同一 shape 节点身份（用 UUID，不用 DAG 路径）。"""
    sh = _mesh_shape(transform)
    if not sh:
        return None
    try:
        uuids = cmds.ls(sh, uuid=True) or []
        if uuids:
            return "uuid:" + uuids[0]
    except Exception:
        pass
    return "node:" + _short(sh)


def _pick_base(members):
    """选导出 base：优先根名（无尾缀数字）、更短的名字。"""
    def score(t):
        leaf = _short(t)
        root = strip_trailing_digits(leaf)
        is_root = 0 if leaf == root else 1
        return (is_root, len(leaf), leaf.lower())

    return sorted(members, key=score)[0]


def _trailing_int(leaf):
    m = re.search(r'(\d+)$', leaf or "")
    if not m:
        return -1
    try:
        return int(m.group(1))
    except Exception:
        return -1


def _claim_rank(group, raw_name):
    """同名实例组里，谁保留不带编号的基础名。数值越小越优先。"""
    override = ""
    for node in (group.shape, group.base):
        override = get_prefab_name_override(node)
        if override:
            break
    has_override = 0 if override == raw_name else 1
    root = raw_name
    if raw_name.endswith(INSTANCE_BASE_SUFFIX):
        root = raw_name[:-len(INSTANCE_BASE_SUFFIX)]
    exact = 1
    for member in group.members:
        if _short(member) == root:
            exact = 0
            break
    return (
        has_override,
        exact,
        _trailing_int(_short(group.base)),
        (_short(group.base) or "").lower(),
    )


def _next_prefab_suffix(raw_name, used):
    """同名时插编号：SM_ArchWall_01a_Inst -> SM_ArchWall_01a_1_Inst。"""
    root = raw_name or "Inst"
    tail = ""
    if root.endswith(INSTANCE_BASE_SUFFIX) and len(root) > len(INSTANCE_BASE_SUFFIX):
        tail = INSTANCE_BASE_SUFFIX
        root = root[:-len(INSTANCE_BASE_SUFFIX)]
    n = 1
    while True:
        cand = "%s_%d%s" % (root, n, tail)
        if cand not in used:
            return cand
        n += 1


def _disambiguate_prefab_names(groups):
    """不同 shape 的实例组不要共用同一个基础模型名。

    先出现、名字更像源模型的那组保留原名，后面复制再实例化的组在 _Inst 前加 _1、_2。
    """
    buckets = {}
    order = []
    for group in groups:
        raw = group.prefab_name or ""
        if raw not in buckets:
            buckets[raw] = []
            order.append(raw)
        buckets[raw].append(group)
    used = set()
    for raw in order:
        claimants = sorted(buckets[raw], key=lambda g: _claim_rank(g, raw))
        for index, group in enumerate(claimants):
            if index == 0 and raw and raw not in used:
                name = raw
            else:
                name = _next_prefab_suffix(raw or "Inst", used)
            used.add(name)
            group.prefab_name = name


def _collect_instance_groups(selection=None):
    """按共享 shape 分组。prefab 名尚未做同名编号。"""
    transforms = []
    for t in collect_mesh_transforms(selection):
        transforms.append(_long(t))

    shape_map = {}
    shape_order = []
    for t in transforms:
        key = _shape_identity(t)
        if not key:
            continue
        if key not in shape_map:
            shape_map[key] = []
            shape_order.append(key)
        if t not in shape_map[key]:
            shape_map[key].append(t)

    groups = []
    singletons = []
    for key in shape_order:
        members = shape_map[key]
        if len(members) >= 2:
            base = _pick_base(members)
            # InstanceGroup.shape 必须是真实 mesh shape 路径，供材质映射；
            # 身份键 key（uuid:...）只用于分组，不能写进 shape。
            shape_node = _mesh_shape(base)
            prefab = resolve_prefab_name(base, shape_node)
            groups.append(InstanceGroup(
                shape_node, base, members, prefab, kind="instance",
            ))
        else:
            singletons.append(members[0])
    return groups, singletons


def analyze(selection=None):
    """分析场景，返回 (groups, singletons)。

    groups：真·Maya Instance 组（共享 shape，成员 >= 2），每组只导一份 FBX。
    singletons：独立网格（含普通 Duplicate 的副本），各自导出。
    不同 shape 算出同一个基础名时，复制出来的那组会在 _Inst 前加上 _1、_2。
    """
    groups, singletons = _collect_instance_groups(selection)
    if selection:
        scene_groups, _scene_singles = _collect_instance_groups(None)
        _disambiguate_prefab_names(scene_groups)
        names = {}
        for group in scene_groups:
            ident = _shape_identity(group.base)
            if ident:
                names[ident] = group.prefab_name
        for group in groups:
            ident = _shape_identity(group.base)
            if ident and ident in names:
                group.prefab_name = names[ident]
    else:
        _disambiguate_prefab_names(groups)
    return groups, singletons


_GEOM_EPS = 1.0e-4


def _mesh_shapes(transform):
    return _mesh_shapes_of(transform)


def _uv_set_names(shape):
    try:
        names = cmds.polyUVSet(shape, query=True, allUVSets=True) or []
    except Exception:
        names = []
    return list(names)


def _read_mesh_geom(transform):
    """读取物体空间顶点与 UV，供实例化前对比。"""
    import maya.OpenMaya as om

    shapes = _mesh_shapes(transform)
    info = {
        "transform": _long(transform),
        "name": _short(transform),
        "shapes": shapes,
        "shape": shapes[0] if len(shapes) == 1 else None,
    }
    if len(shapes) != 1:
        return info
    shape = shapes[0]
    dag = _dag_path_of(om, shape)
    fn = om.MFnMesh(dag)
    points = om.MFloatPointArray()
    fn.getPoints(points, om.MSpace.kObject)
    info["verts"] = fn.numVertices()
    info["faces"] = fn.numPolygons()
    info["fverts"] = fn.numFaceVertices()
    info["points"] = points
    uvs = []
    for uv_name in _uv_set_names(shape):
        u_arr = om.MFloatArray()
        v_arr = om.MFloatArray()
        try:
            fn.getUVs(u_arr, v_arr, uv_name)
        except Exception:
            loaded = False
            for cand in _api_names(uv_name):
                try:
                    u_arr = om.MFloatArray()
                    v_arr = om.MFloatArray()
                    fn.getUVs(u_arr, v_arr, cand)
                    loaded = True
                    break
                except Exception:
                    continue
            if not loaded:
                u_arr = om.MFloatArray()
                v_arr = om.MFloatArray()
        uvs.append((uv_name, u_arr, v_arr))
    info["uvs"] = uvs
    return info


def _array_mismatch(a, b, eps):
    """返回第一个超出误差的下标，相同则 None。"""
    n = a.length()
    if n != b.length():
        return 0
    i = 0
    while i < n:
        if abs(a[i] - b[i]) > eps:
            return i
        i += 1
    return None


def _points_mismatch_indices(a, b, limit=500):
    """收集物体空间顶点不一致的下标。返回 (indexes, total)。"""
    n = a.length()
    if n != b.length():
        return [0], 0
    indexes = []
    total = 0
    i = 0
    while i < n:
        pa = a[i]
        pb = b[i]
        if (abs(pa.x - pb.x) > _GEOM_EPS
                or abs(pa.y - pb.y) > _GEOM_EPS
                or abs(pa.z - pb.z) > _GEOM_EPS):
            total += 1
            if len(indexes) < limit:
                indexes.append(i)
        i += 1
    return indexes, total


def compare_mesh_geom(base_info, other_info):
    """对比两个模型。返回差异码列表，空列表表示几何一致。"""
    if len(base_info.get("shapes") or []) != 1 or len(other_info.get("shapes") or []) != 1:
        return [{"code": "shapes"}]
    reasons = []
    if base_info["verts"] != other_info["verts"]:
        reasons.append({
            "code": "verts",
            "a": base_info["verts"],
            "b": other_info["verts"],
        })
    if base_info["faces"] != other_info["faces"]:
        reasons.append({
            "code": "faces",
            "a": base_info["faces"],
            "b": other_info["faces"],
        })
    if base_info["fverts"] != other_info["fverts"]:
        reasons.append({
            "code": "fverts",
            "a": base_info["fverts"],
            "b": other_info["fverts"],
        })
    if reasons:
        return reasons
    indexes, total = _points_mismatch_indices(
        base_info["points"], other_info["points"])
    if indexes:
        reasons.append({
            "code": "points",
            "index": indexes[0],
            "indices": indexes,
            "count": total,
        })
        return reasons
    base_uvs = base_info.get("uvs") or []
    other_uvs = other_info.get("uvs") or []
    if len(base_uvs) != len(other_uvs):
        reasons.append({
            "code": "uv_sets",
            "a": len(base_uvs),
            "b": len(other_uvs),
        })
        return reasons
    set_i = 0
    while set_i < len(base_uvs):
        name_a, ua, va = base_uvs[set_i]
        name_b, ub, vb = other_uvs[set_i]
        if ua.length() != ub.length() or va.length() != vb.length():
            reasons.append({
                "code": "uv_len",
                "set": set_i,
                "name_a": name_a,
                "name_b": name_b,
                "a": ua.length(),
                "b": ub.length(),
            })
            return reasons
        ui = _array_mismatch(ua, ub, _GEOM_EPS)
        vi = _array_mismatch(va, vb, _GEOM_EPS)
        hit = ui if ui is not None else vi
        if hit is not None:
            reasons.append({
                "code": "uv",
                "set": set_i,
                "name_a": name_a,
                "name_b": name_b,
                "index": hit,
            })
            return reasons
        set_i += 1
    return reasons


def _points_mismatch(a, b):
    indexes, _total = _points_mismatch_indices(a, b, limit=1)
    return indexes[0] if indexes else None


def highlight_point_mismatches(result):
    """在视口选中不一致的顶点（基准 + 对比模型），并框选聚焦。

    返回 {"count": int, "meshes": [dag, ...]}。显示顶点只是临时打开，
    用 clear_point_mismatch_display 关掉。
    """
    base_path = result.get("base_path") or ""
    comps = []
    meshes = []
    for item in result.get("diffs") or []:
        path = item.get("path") or ""
        for reason in item.get("reasons") or []:
            if reason.get("code") != "points":
                continue
            indexes = list(reason.get("indices") or [])
            if not indexes and reason.get("index") is not None:
                indexes = [int(reason.get("index") or 0)]
            for mesh in (base_path, path):
                if not mesh or not cmds.objExists(mesh):
                    continue
                if mesh not in meshes:
                    meshes.append(mesh)
                for idx in indexes:
                    comps.append("%s.vtx[%d]" % (mesh, int(idx)))
    if not comps:
        return {"count": 0, "meshes": []}
    for mesh in meshes:
        try:
            _set_vertex_display(mesh, True)
        except Exception:
            pass
        try:
            cmds.hilite(mesh, replace=False)
        except Exception:
            pass
    try:
        cmds.selectMode(component=True)
        cmds.selectType(vertex=True, alc=False)
        cmds.select(comps, replace=True)
    except Exception:
        try:
            cmds.select(comps, replace=True)
        except Exception:
            return {"count": 0, "meshes": meshes}
    try:
        cmds.viewFit(fitFactor=0.8)
    except Exception:
        pass
    return {"count": len(comps), "meshes": meshes}


def _mesh_shapes_for_display(node):
    """顶点显示写在 mesh shape 上；实例共用同一个 shape。"""
    if not node or not cmds.objExists(node):
        return []
    try:
        if cmds.nodeType(node) == "mesh":
            return [node]
    except Exception:
        return []
    return _mesh_shapes_of(node)


def _set_vertex_display(node, enabled):
    """绝对开关顶点显示。不要用 polyOptions 的 relative，那个是翻转，关不掉。"""
    flag = 1 if enabled else 0
    shapes = _mesh_shapes_for_display(node)
    if not shapes and node and cmds.objExists(node):
        shapes = [node]
    for shape in shapes:
        try:
            cmds.polyOptions(shape, displayVertex=bool(enabled))
        except Exception:
            pass
        plug = shape + ".displayVertices"
        try:
            if cmds.objExists(plug):
                cmds.setAttr(plug, flag)
        except Exception:
            pass


def clear_point_mismatch_display(meshes=None):
    """关掉临时顶点显示，并退出 hilite。不传 meshes 时清掉场景里所有打开的顶点显示。"""
    targets = list(meshes or [])
    if not targets:
        try:
            targets = cmds.ls(type="mesh", long=True) or []
        except Exception:
            targets = []
    seen = set()
    for mesh in targets:
        if not mesh or mesh in seen or not cmds.objExists(mesh):
            continue
        seen.add(mesh)
        _set_vertex_display(mesh, False)
        try:
            cmds.hilite(mesh, unHilite=True)
        except Exception:
            pass
    try:
        cmds.selectMode(object=True)
    except Exception:
        pass


def _replace_with_instance(base, target):
    """用 base 的网格实例替换 target。名字、父级和世界位置保持不变。"""
    parent = cmds.listRelatives(target, parent=True, fullPath=True) or []
    world_mat = cmds.xform(target, query=True, matrix=True, worldSpace=True)
    try:
        rotate_order = cmds.getAttr(target + ".rotateOrder")
    except Exception:
        rotate_order = None
    leaf = target.split("|")[-1]
    parked = _long(cmds.rename(target, leaf + "__mtuSwap"))
    kids = []
    for kid in cmds.listRelatives(
            parked, children=True, fullPath=True, type="transform") or []:
        try:
            kid_mat = cmds.xform(kid, query=True, matrix=True, worldSpace=True)
        except Exception:
            kid_mat = None
        kids.append((kid, kid_mat))
    try:
        new_node = _long(cmds.instance(base, name=leaf)[0])
    except Exception:
        new_node = _long(cmds.instance(base)[0])
    cur_parent = cmds.listRelatives(new_node, parent=True, fullPath=True) or []
    if parent:
        if not cur_parent or cur_parent[0] != parent[0]:
            new_node = _long(cmds.parent(new_node, parent[0])[0])
    elif cur_parent:
        new_node = _long(cmds.parent(new_node, world=True)[0])
    if rotate_order is not None:
        try:
            cmds.setAttr(new_node + ".rotateOrder", rotate_order)
        except Exception:
            pass
    cmds.xform(new_node, matrix=world_mat, worldSpace=True)
    for kid, kid_mat in kids:
        if not cmds.objExists(kid):
            continue
        moved = cmds.parent(kid, new_node)[0]
        if kid_mat is not None:
            try:
                cmds.xform(moved, matrix=kid_mat, worldSpace=True)
            except Exception:
                pass
    if new_node.split("|")[-1] != leaf:
        new_node = _long(cmds.rename(new_node, leaf))
    if cmds.objExists(parked):
        cmds.delete(parked)
    return _long(new_node)


def convert_selection_to_instances(selection=None):
    """把当前选择收成同一网格的实例。

    以选择顺序中的第一个模型为基准。几何或 UV 不一致则不修改场景。
    返回 dict：ok / error / base / diffs / converted / already。
    """
    if selection is None:
        selection = cmds.ls(selection=True, long=True) or []
    transforms = []
    seen = set()
    for t in collect_mesh_transforms(selection):
        t = _long(t)
        if t in seen:
            continue
        seen.add(t)
        transforms.append(t)
    if len(transforms) < 2:
        return {"ok": False, "error": "need_two"}

    base = transforms[0]
    try:
        base_info = _read_mesh_geom(base)
    except Exception as exc:
        return {"ok": False, "error": "read", "detail": _exc_text(exc), "name": _short(base)}

    diffs = []
    infos = {base: base_info}
    for other in transforms[1:]:
        try:
            info = _read_mesh_geom(other)
        except Exception as exc:
            diffs.append({
                "name": _short(other),
                "reasons": [{"code": "read", "detail": _exc_text(exc)}],
            })
            continue
        infos[other] = info
        reasons = compare_mesh_geom(base_info, info)
        if reasons:
            diffs.append({
                "name": info["name"],
                "path": info["transform"],
                "reasons": reasons,
            })
    if diffs:
        return {
            "ok": False,
            "error": "mismatch",
            "base": base_info["name"],
            "base_path": base_info["transform"],
            "diffs": diffs,
        }

    converted = 0
    already = 0
    failed = None
    failed_name = ""
    chunk_name = "mtuMakeInstances"
    chunk_open = False
    new_paths = [base]
    try:
        cmds.undoInfo(openChunk=True, chunkName=chunk_name)
        chunk_open = True
        # 锚点让撤销块一定存在，失败时不会误撤用户刚才的选择
        anchor = cmds.createNode(
            "transform", name="mtuUndoAnchor", skipSelect=True)
        for other in transforms[1:]:
            if _shape_identity(other) == _shape_identity(base):
                already += 1
                new_paths.append(_long(other))
                continue
            failed_name = _short(other)
            new_paths.append(_replace_with_instance(base, other))
            converted += 1
        if cmds.objExists(anchor):
            cmds.delete(anchor)
    except Exception as exc:
        failed = exc
    finally:
        if chunk_open:
            try:
                cmds.undoInfo(closeChunk=True)
            except Exception:
                pass
    if failed is not None:
        try:
            undo_name = cmds.undoInfo(query=True, undoName=True) or ""
            if chunk_name in undo_name:
                cmds.undo()
        except Exception:
            pass
        detail = _exc_text(failed)
        if failed_name:
            detail = u"%s: %s" % (failed_name, detail)
        return {
            "ok": False,
            "error": "fail",
            "detail": detail,
            "base": base_info["name"],
        }

    try:
        cmds.select(new_paths, replace=True)
    except Exception:
        pass
    return {
        "ok": True,
        "base": base_info["name"],
        "converted": converted,
        "already": already,
    }


def _bbox_diagonal(points):
    n = points.length()
    if n <= 0:
        return 1.0
    p0 = points[0]
    mn = [p0.x, p0.y, p0.z]
    mx = [p0.x, p0.y, p0.z]
    i = 1
    while i < n:
        p = points[i]
        if p.x < mn[0]:
            mn[0] = p.x
        if p.y < mn[1]:
            mn[1] = p.y
        if p.z < mn[2]:
            mn[2] = p.z
        if p.x > mx[0]:
            mx[0] = p.x
        if p.y > mx[1]:
            mx[1] = p.y
        if p.z > mx[2]:
            mx[2] = p.z
        i += 1
    dx = mx[0] - mn[0]
    dy = mx[1] - mn[1]
    dz = mx[2] - mn[2]
    diag = (dx * dx + dy * dy + dz * dz) ** 0.5
    if diag < 1.0e-8:
        return 1.0
    return diag


def score_mesh_similarity(base_info, other_info, mark_limit=500):
    """几何相似度 0-100（物体空间顶点）。拓扑不同则为 0。

    分数 = 100 * (1 - 平均顶点距离 / 包围盒对角线)，再钳到 0-100。
    不比较 UV（模糊实例化会直接用源模型网格）。
    """
    if len(base_info.get("shapes") or []) != 1 or len(other_info.get("shapes") or []) != 1:
        return {
            "score": 0.0,
            "reasons": [{"code": "shapes"}],
            "indices": [],
            "count": 0,
        }
    va = int(base_info.get("verts") or 0)
    vb = int(other_info.get("verts") or 0)
    fa = int(base_info.get("faces") or 0)
    fb = int(other_info.get("faces") or 0)
    if va != vb or fa != fb or va <= 0:
        return {
            "score": 0.0,
            "reasons": [{
                "code": "topo",
                "va": va,
                "vb": vb,
                "fa": fa,
                "fb": fb,
            }],
            "indices": [],
            "count": 0,
        }
    pa = base_info["points"]
    pb = other_info["points"]
    diag = _bbox_diagonal(pa)
    total_d = 0.0
    indexes = []
    bad_count = 0
    # 相对包围盒 0.1% 以上视为“可见差异”，用于视口标记
    mark_eps = diag * 0.001
    if mark_eps < _GEOM_EPS:
        mark_eps = _GEOM_EPS
    i = 0
    while i < va:
        a = pa[i]
        b = pb[i]
        dx = a.x - b.x
        dy = a.y - b.y
        dz = a.z - b.z
        dist = (dx * dx + dy * dy + dz * dz) ** 0.5
        total_d += dist
        if dist > mark_eps:
            bad_count += 1
            if len(indexes) < mark_limit:
                indexes.append(i)
        i += 1
    mean_rel = (total_d / float(va)) / diag
    score = 100.0 * (1.0 - mean_rel)
    if score < 0.0:
        score = 0.0
    if score > 100.0:
        score = 100.0
    # 完全重合时浮点噪声仍可能 <100，贴到 100
    if mean_rel <= (_GEOM_EPS / diag):
        score = 100.0
    reasons = []
    if indexes:
        reasons.append({
            "code": "points",
            "index": indexes[0],
            "indices": indexes,
            "count": bad_count,
        })
    return {
        "score": round(score, 1),
        "reasons": reasons,
        "indices": indexes,
        "count": bad_count,
        "mean_rel": mean_rel,
    }


def convert_selection_to_fuzzy_instances(selection=None, threshold=90):
    """按相似度阈值做实例替换。

    选择顺序：第一个为源，其余为候选。相似度 >= threshold 才替换。
    """
    try:
        thr = int(threshold)
    except Exception:
        thr = 90
    if thr < 0:
        thr = 0
    if thr > 100:
        thr = 100

    if selection is None:
        selection = cmds.ls(selection=True, long=True) or []
    transforms = []
    seen = set()
    for t in collect_mesh_transforms(selection):
        t = _long(t)
        if t in seen:
            continue
        seen.add(t)
        transforms.append(t)
    if len(transforms) < 2:
        return {"ok": False, "error": "need_two"}

    base = transforms[0]
    try:
        base_info = _read_mesh_geom(base)
    except Exception as exc:
        return {
            "ok": False,
            "error": "read",
            "detail": _exc_text(exc),
            "name": _short(base),
        }
    if len(base_info.get("shapes") or []) != 1:
        return {
            "ok": False,
            "error": "fail",
            "detail": u"base has multiple shapes",
            "base": base_info["name"],
        }

    to_convert = []
    already = 0
    skipped = []
    for other in transforms[1:]:
        if _shape_identity(other) == _shape_identity(base):
            already += 1
            continue
        try:
            info = _read_mesh_geom(other)
        except Exception as exc:
            skipped.append({
                "name": _short(other),
                "path": _long(other),
                "score": 0.0,
                "reasons": [{"code": "read", "detail": _exc_text(exc)}],
            })
            continue
        sim = score_mesh_similarity(base_info, info)
        score = float(sim.get("score") or 0.0)
        if score + 1.0e-6 >= float(thr):
            to_convert.append(other)
            continue
        reasons = list(sim.get("reasons") or [])
        reasons.insert(0, {
            "code": "score",
            "score": score,
            "threshold": thr,
        })
        skipped.append({
            "name": info.get("name") or _short(other),
            "path": info.get("transform") or _long(other),
            "score": score,
            "reasons": reasons,
        })

    converted = 0
    failed = None
    failed_name = ""
    chunk_name = "mtuFuzzyInstances"
    chunk_open = False
    new_paths = [base]
    # 已是实例的路径也保留在结果选择里
    for other in transforms[1:]:
        if _shape_identity(other) == _shape_identity(base):
            new_paths.append(_long(other))
    try:
        if to_convert:
            cmds.undoInfo(openChunk=True, chunkName=chunk_name)
            chunk_open = True
            anchor = cmds.createNode(
                "transform", name="mtuUndoAnchor", skipSelect=True)
            for other in to_convert:
                failed_name = _short(other)
                new_paths.append(_replace_with_instance(base, other))
                converted += 1
            if cmds.objExists(anchor):
                cmds.delete(anchor)
    except Exception as exc:
        failed = exc
    finally:
        if chunk_open:
            try:
                cmds.undoInfo(closeChunk=True)
            except Exception:
                pass
    if failed is not None:
        try:
            undo_name = cmds.undoInfo(query=True, undoName=True) or ""
            if chunk_name in undo_name:
                cmds.undo()
        except Exception:
            pass
        detail = _exc_text(failed)
        if failed_name:
            detail = u"%s: %s" % (failed_name, detail)
        return {
            "ok": False,
            "error": "fail",
            "detail": detail,
            "base": base_info["name"],
        }

    try:
        # 转换后路径可能变了；尽量选源 + 仍存在的节点
        keep = []
        for p in new_paths:
            if p and cmds.objExists(p) and p not in keep:
                keep.append(p)
        if keep:
            cmds.select(keep, replace=True)
    except Exception:
        pass
    return {
        "ok": True,
        "base": base_info["name"],
        "base_path": base_info["transform"],
        "threshold": thr,
        "converted": converted,
        "already": already,
        "skipped": skipped,
    }
