# -*- coding: utf-8 -*-
"""Maya 摄影机 → Unity Camera 属性映射。

跳过默认视图相机（persp / top / front 等），只导出用户相机。
"""
from __future__ import print_function

import maya.cmds as cmds

# 与 hierarchy 空组跳过列表一致：默认视图壳不导出
_DEFAULT_CAMERA_LEAVES = frozenset((
    "persp", "top", "front", "side",
    "left", "right", "back", "bottom",
))


def _short(node):
    name = (node or "").split("|")[-1]
    if ":" in name:
        name = name.split(":")[-1]
    return name


def _long(node):
    longs = cmds.ls(node, long=True) or []
    return longs[0] if longs else node


def _attr(node, name, default=None):
    if not cmds.attributeQuery(name, node=node, exists=True):
        return default
    try:
        return cmds.getAttr("%s.%s" % (node, name))
    except Exception:
        return default


def _camera_shape(transform):
    """返回 camera shape long path，或 None。"""
    shapes = cmds.listRelatives(
        transform, shapes=True, noIntermediate=True, fullPath=True) or []
    for sh in shapes:
        if cmds.nodeType(sh) == "camera":
            return _long(sh)
    return None


def is_camera_transform(node):
    if cmds.nodeType(node) != "transform":
        return False
    return _camera_shape(node) is not None


def is_default_view_camera(transform):
    """是否为 Maya 默认视图相机（不导出）。"""
    leaf = _short(transform).lower()
    return leaf in _DEFAULT_CAMERA_LEAVES


def collect_cameras(selection=None):
    """收集要导出的摄影机 transform（long path）。"""
    shapes = cmds.ls(type="camera", long=True) or []
    transforms = []
    seen = set()
    for sh in shapes:
        parents = cmds.listRelatives(sh, parent=True, fullPath=True) or []
        if not parents:
            continue
        t = _long(parents[0])
        if t in seen:
            continue
        if is_default_view_camera(t):
            continue
        seen.add(t)
        transforms.append(t)

    if not selection:
        return transforms

    allowed = set()
    for obj in selection:
        obj = _long(obj)
        if not cmds.objExists(obj):
            continue
        ntype = cmds.nodeType(obj)
        if ntype == "camera":
            for p in cmds.listRelatives(obj, parent=True, fullPath=True) or []:
                allowed.add(_long(p))
        elif ntype == "transform":
            allowed.add(obj)
            for child in cmds.listRelatives(
                    obj, ad=True, type="transform", fullPath=True) or []:
                allowed.add(_long(child))
        else:
            for p in cmds.listRelatives(obj, parent=True, fullPath=True) or []:
                allowed.add(_long(p))

    return [t for t in transforms if t in allowed]


def map_camera(transform):
    """映射单个摄影机 → 属性 dict。"""
    path = _long(transform)
    if is_default_view_camera(path):
        return None
    shape = _camera_shape(path)
    if not shape:
        return None

    enabled = True
    try:
        if cmds.getAttr("%s.visibility" % path) == 0:
            enabled = False
    except Exception:
        pass

    # 垂直 FOV（度）→ Unity field of view
    fov = 60.0
    try:
        fov = float(cmds.camera(shape, q=True, verticalFieldOfView=True))
    except Exception:
        # 回退：用焦距粗估（35mm 等效近似）
        fl = float(_attr(shape, "focalLength", 35.0) or 35.0)
        # FOV ≈ 2 * atan(18/fl) * 180/pi （半高 18mm）
        import math
        fov = 2.0 * math.degrees(math.atan(18.0 / max(fl, 1e-3)))
    fov = max(1.0, min(179.0, fov))

    # 裁剪面：Maya cm → Unity m
    near_cm = float(_attr(shape, "nearClipPlane", 0.1) or 0.1)
    far_cm = float(_attr(shape, "farClipPlane", 10000.0) or 10000.0)
    near_m = max(0.01, near_cm / 100.0)
    far_m = max(near_m + 0.01, far_cm / 100.0)

    orthographic = bool(_attr(shape, "orthographic", False))
    # Maya orthographicWidth 为宽度（cm）；Unity orthographic size 为半高（m）
    # 近似：宽/2/100（按方形视野）
    ortho_w = float(_attr(shape, "orthographicWidth", 30.0) or 30.0)
    ortho_size = max(0.01, (ortho_w * 0.5) / 100.0)

    return {
        "name": _short(path),
        "path": path,
        "shape": shape,
        "maya_type": "camera",
        "fov": fov,
        "near": near_m,
        "far": far_m,
        "orthographic": orthographic,
        "ortho_size": ortho_size,
        "enabled": enabled,
        "is_main": False,  # 导出时由 exporter 指定第一台
    }


def map_cameras(selection=None):
    """收集并映射用户摄影机列表。"""
    out = []
    for t in collect_cameras(selection):
        info = map_camera(t)
        if info:
            out.append(info)
    return out
