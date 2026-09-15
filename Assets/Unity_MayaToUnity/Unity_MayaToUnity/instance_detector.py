# -*- coding: utf-8 -*-
"""Maya 实例识别。

Maya 里用 Duplicate Special（instance）创建的实例会共享同一个 mesh shape，
因此「多个 transform 的 shape 相同」即是一个实例组。导出时每组生成一个 Prefab，
组内每个 transform 作为该 Prefab 的一个实例，按各自世界变换摆回场景。
"""
from __future__ import print_function

import re

import maya.cmds as cmds


class InstanceGroup(object):
    """一个实例组：共享同一 shape 的一组 transform。"""

    def __init__(self, shape, base, members, prefab_name):
        self.shape = shape              # 共享的 mesh shape 名
        self.base = base                # 导出 FBX 所用的 transform（组内第一个）
        self.members = members          # 组内所有 transform（实例）
        self.prefab_name = prefab_name  # Prefab 名（base 名去掉末尾数字）


def strip_trailing_digits(name):
    """去掉名字末尾数字：pCube2 -> pCube，pCube -> pCube。"""
    m = re.match(r'^(.*?)\d+$', name)
    if m and m.group(1):
        return m.group(1).rstrip('_ .-')
    return name


def _is_mesh_transform(node):
    if cmds.nodeType(node) != 'transform':
        return False
    shapes = cmds.listRelatives(node, shapes=True, noIntermediate=True) or []
    return any(cmds.nodeType(s) == 'mesh' for s in shapes)


def collect_mesh_transforms(selection=None):
    """收集要导出的 mesh transform。

    selection 为 None 时收集整个场景；否则从选中对象展开（支持选中 mesh /
    transform / group）。
    """
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
                for child in cmds.listRelatives(obj, ad=True, type='transform', fullPath=True) or []:
                    if _is_mesh_transform(child) and child not in result:
                        result.append(child)
    return result


def analyze(selection=None):
    """分析场景，返回 (groups, singletons)。

    groups：InstanceGroup 列表（成员 >= 2 的实例组）。
    singletons：唯一 mesh 的 transform 名列表（成员 = 1，不参与 Prefab 实例化）。
    """
    transforms = collect_mesh_transforms(selection)

    # 按共享 shape 分组（实例化会让多个 transform 指向同一 shape）
    shape_map = {}
    order = []
    for t in transforms:
        shapes = cmds.listRelatives(t, shapes=True, noIntermediate=True) or []
        if not shapes:
            continue
        shape = shapes[0]
        if shape not in shape_map:
            shape_map[shape] = []
            order.append(shape)
        if t not in shape_map[shape]:
            shape_map[shape].append(t)

    groups = []
    singletons = []
    for shape in order:
        members = shape_map[shape]
        if len(members) >= 2:
            base = members[0]
            leaf = base.split('|')[-1]
            groups.append(InstanceGroup(shape, base, members, strip_trailing_digits(leaf)))
        else:
            singletons.append(members[0])

    return groups, singletons
