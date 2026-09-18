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

from constants import INSTANCE_BASE_SUFFIX, MTU_PREFAB_NAME_ATTR


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


def _is_mesh_transform(node):
    if cmds.nodeType(node) != 'transform':
        return False
    shapes = cmds.listRelatives(node, shapes=True, noIntermediate=True) or []
    return any(cmds.nodeType(s) == 'mesh' for s in shapes)


def collect_mesh_transforms(selection=None):
    """收集要导出的 mesh transform。"""
    if not selection:
        return [t for t in cmds.ls(type='transform') if _is_mesh_transform(t)]

    result = []
    for obj in selection:
        ntype = cmds.nodeType(obj)
        if ntype == 'mesh':
            for p in cmds.listRelatives(obj, parent=True, fullPath=True) or []:
                if p not in result:
                    result.append(p)
        elif ntype == 'transform':
            if _is_mesh_transform(obj):
                if obj not in result:
                    result.append(obj)
            else:
                for child in cmds.listRelatives(
                        obj, ad=True, type='transform', fullPath=True) or []:
                    if _is_mesh_transform(child) and child not in result:
                        result.append(child)
    return result


def _mesh_shape(transform):
    """返回 transform 下第一个非 intermediate mesh shape（long name）。"""
    shapes = cmds.listRelatives(
        transform, shapes=True, noIntermediate=True) or []
    for s in shapes:
        if cmds.nodeType(s) == 'mesh':
            return _long(s)
    return None


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


def analyze(selection=None):
    """分析场景，返回 (groups, singletons)。

    groups：真·Maya Instance 组（共享 shape，成员 >= 2），每组只导一份 FBX。
    singletons：独立网格（含普通 Duplicate 的副本），各自导出。
    """
    transforms = [_long(t) for t in collect_mesh_transforms(selection)]

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
