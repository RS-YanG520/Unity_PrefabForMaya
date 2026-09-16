# -*- coding: utf-8 -*-
"""场景层级收集：从已导出 mesh 向上追溯祖先空组，生成父先于子的有序节点列表。

导出到 Unity 时：
  - kind=group → 空 GameObject
  - kind=mesh  → PrefabInstance
层级只写在 .unity 场景里，不打进 Prefab。
"""
from __future__ import print_function

import maya.cmds as cmds

from instance_detector import _is_mesh_transform


class HierarchyNode(object):
    """场景层级中的一个节点。"""

    def __init__(self, path, kind, parent_path, name, depth):
        self.path = path                # Maya fullPath
        self.kind = kind                # 'group' | 'mesh'
        self.parent_path = parent_path  # 父 fullPath，根为 None
        self.name = name                # 短名
        self.depth = depth              # 深度（根=0）
        self.children = []              # 子 path 列表（稍后填充）


def _to_long(node):
    """统一成 Maya long name；失败返回原值。"""
    longs = cmds.ls(node, long=True) or []
    return longs[0] if longs else node


def _parent_path(node):
    """返回父 transform 的 fullPath；无父或父不是 transform 时返回 None。"""
    parents = cmds.listRelatives(node, parent=True, fullPath=True) or []
    if not parents:
        return None
    p = parents[0]
    if cmds.nodeType(p) != 'transform':
        return None
    return p


def _depth(path):
    """用路径中 '|' 段数估计深度（跳过空段）。"""
    return len([s for s in path.split('|') if s])


def _is_exportable_group(node):
    """可作为空组导出的 transform：非 mesh，且不是默认相机壳。"""
    if cmds.nodeType(node) != 'transform':
        return False
    if _is_mesh_transform(node):
        return False
    leaf = node.split('|')[-1].lower()
    skip = (
        'persp', 'top', 'front', 'side',
        'left', 'right', 'back', 'bottom',
    )
    if leaf in skip:
        return False
    return True


def build_hierarchy(mesh_transforms):
    """由 mesh transform 列表构建有序层级节点（父先于子）。

    mesh_transforms：groups 成员 + singletons（可用短名或 long 名）。
    返回 HierarchyNode 列表，已按 depth 升序；同深度按 path 排序。
    """
    meshes = []
    seen_mesh = set()
    for t in mesh_transforms:
        path = _to_long(t)
        if path in seen_mesh:
            continue
        if not cmds.objExists(path):
            continue
        if not _is_mesh_transform(path):
            continue
        seen_mesh.add(path)
        meshes.append(path)

    nodes = {}

    def ensure_ancestors(mesh_path):
        """把 mesh 及其祖先空组登记进 nodes。"""
        chain = []
        cur = mesh_path
        while cur:
            chain.append(cur)
            cur = _parent_path(cur)
        chain.reverse()  # 根 → 叶

        filtered = []
        for i, p in enumerate(chain):
            is_last = (i == len(chain) - 1)
            if is_last:
                filtered.append(p)
            elif _is_exportable_group(p):
                filtered.append(p)
            elif p in seen_mesh:
                # 已导出的 mesh 作为父节点时也保留（mesh 下挂 mesh）
                filtered.append(p)

        for i, p in enumerate(filtered):
            parent = filtered[i - 1] if i > 0 else None
            kind = 'mesh' if p in seen_mesh else 'group'
            if p not in nodes:
                nodes[p] = HierarchyNode(
                    path=p,
                    kind=kind,
                    parent_path=parent,
                    name=p.split('|')[-1],
                    depth=i,
                )
            else:
                nodes[p].parent_path = parent
                nodes[p].kind = kind

        for i, p in enumerate(filtered):
            n = nodes[p]
            parent = filtered[i - 1] if i > 0 else None
            n.parent_path = parent
            if parent and parent in nodes:
                n.depth = nodes[parent].depth + 1
            else:
                n.depth = 0

    for m in meshes:
        ensure_ancestors(m)

    for n in nodes.values():
        n.children = []

    ordered = sorted(nodes.values(), key=lambda n: (n.depth, n.path))
    for n in ordered:
        if n.parent_path and n.parent_path in nodes:
            nodes[n.parent_path].children.append(n.path)

    return ordered
