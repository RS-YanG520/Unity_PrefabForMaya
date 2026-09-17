# -*- coding: utf-8 -*-
"""Maya 内置灯光 → Unity Light 属性映射。

仅处理：directionalLight / spotLight / pointLight / ambientLight。
area / Arnold 不在本版范围。
"""
from __future__ import print_function

import maya.cmds as cmds

# Maya shape type -> Unity Light.m_Type
# 0=Spot, 1=Directional, 2=Point（ambient 不写 Light 组件）
_UNITY_TYPE = {
    "directionalLight": 1,
    "spotLight": 0,
    "pointLight": 2,
}

_MAYA_LIGHT_TYPES = (
    "directionalLight",
    "spotLight",
    "pointLight",
    "ambientLight",
)

# Unity 点光/聚光默认范围（米）；Maya 无硬范围
_DEFAULT_RANGE = 10.0


def _short(node):
    name = (node or "").split("|")[-1]
    if ":" in name:
        name = name.split(":")[-1]
    return name


def _long(node):
    longs = cmds.ls(node, long=True) or []
    return longs[0] if longs else node


def _attr(node, name, default=None):
    plug = "%s.%s" % (node, name)
    if not cmds.attributeQuery(name, node=node, exists=True):
        return default
    try:
        val = cmds.getAttr(plug)
    except Exception:
        return default
    return val


def _color3(node, attr="color"):
    """读颜色；Maya 常返回 ((r,g,b),) 或 [r,g,b]。"""
    raw = _attr(node, attr, default=(1.0, 1.0, 1.0))
    if raw is None:
        return (1.0, 1.0, 1.0)
    if isinstance(raw, (list, tuple)) and raw and isinstance(raw[0], (list, tuple)):
        raw = raw[0]
    try:
        return (float(raw[0]), float(raw[1]), float(raw[2]))
    except Exception:
        return (1.0, 1.0, 1.0)


def _light_shape(transform):
    """返回 (shape_long, maya_type) 或 (None, None)。"""
    shapes = cmds.listRelatives(
        transform, shapes=True, noIntermediate=True, fullPath=True) or []
    for sh in shapes:
        ntype = cmds.nodeType(sh)
        if ntype in _MAYA_LIGHT_TYPES:
            return _long(sh), ntype
    return None, None


def is_light_transform(node):
    """transform 下是否挂了本版支持的灯光 shape。"""
    if cmds.nodeType(node) != "transform":
        return False
    _sh, ntype = _light_shape(node)
    return ntype is not None


def collect_lights(selection=None):
    """收集要导出的灯光 transform（long path）。

    selection 为 None：全场景；否则只收选中子树内灯光。
    """
    shapes = cmds.ls(type=list(_MAYA_LIGHT_TYPES), long=True) or []
    transforms = []
    seen = set()
    for sh in shapes:
        parents = cmds.listRelatives(sh, parent=True, fullPath=True) or []
        if not parents:
            continue
        t = _long(parents[0])
        if t in seen:
            continue
        seen.add(t)
        transforms.append(t)

    if not selection:
        return transforms

    # 选中范围：选中自身或其子孙
    allowed = set()
    for obj in selection:
        obj = _long(obj)
        if not cmds.objExists(obj):
            continue
        ntype = cmds.nodeType(obj)
        if ntype in _MAYA_LIGHT_TYPES:
            for p in cmds.listRelatives(obj, parent=True, fullPath=True) or []:
                allowed.add(_long(p))
        elif ntype == "transform":
            allowed.add(obj)
            for child in cmds.listRelatives(
                    obj, ad=True, type="transform", fullPath=True) or []:
                allowed.add(_long(child))
        else:
            # 其它类型：若父是 transform 也试一下
            for p in cmds.listRelatives(obj, parent=True, fullPath=True) or []:
                allowed.add(_long(p))

    return [t for t in transforms if t in allowed]


def map_light(transform):
    """映射单个灯光 transform → 属性 dict。

    字段：
      name, path, shape, maya_type, unity_type (None=ambient),
      color (r,g,b), intensity, range, spot_angle, inner_spot,
      shadows (0/1/2), enabled, is_ambient
    """
    path = _long(transform)
    shape, maya_type = _light_shape(path)
    if not shape or not maya_type:
        return None

    color = _color3(shape, "color")
    intensity = float(_attr(shape, "intensity", 1.0) or 1.0)
    enabled = True
    try:
        # transform 或 shape 上的 visibility
        if cmds.getAttr("%s.visibility" % path) == 0:
            enabled = False
    except Exception:
        pass

    is_ambient = maya_type == "ambientLight"
    unity_type = None if is_ambient else _UNITY_TYPE.get(maya_type, 1)

    # 阴影
    shadows = 0
    if not is_ambient:
        use_dm = bool(_attr(shape, "useDepthMapShadows", False))
        use_rt = bool(_attr(shape, "useRayTraceShadows", False))
        if use_dm or use_rt:
            shadows = 2  # Soft

    spot_angle = 30.0
    inner_spot = 21.80208
    if maya_type == "spotLight":
        cone = float(_attr(shape, "coneAngle", 40.0) or 40.0)
        spot_angle = max(1.0, min(179.0, cone))
        penumbra = float(_attr(shape, "penumbra", 0.0) or 0.0)
        # penumbra 可为负；内角 ≈ 外角 - |penumbra|
        inner = spot_angle - abs(penumbra)
        inner_spot = max(0.0, min(spot_angle, inner))

    return {
        "name": _short(path),
        "path": path,
        "shape": shape,
        "maya_type": maya_type,
        "unity_type": unity_type,
        "color": color,
        "intensity": intensity,
        "range": _DEFAULT_RANGE,
        "spot_angle": spot_angle,
        "inner_spot": inner_spot,
        "shadows": shadows,
        "enabled": enabled,
        "is_ambient": is_ambient,
    }


def map_lights(selection=None):
    """收集并映射；返回 (scene_lights, ambients)。

    scene_lights：要写 Light GO 的列表（directional/spot/point）
    ambients：ambientLight 映射列表
    """
    scene_lights = []
    ambients = []
    for t in collect_lights(selection):
        info = map_light(t)
        if not info:
            continue
        if info["is_ambient"]:
            ambients.append(info)
        else:
            scene_lights.append(info)
    return scene_lights, ambients


def ambient_color_from(ambients):
    """合并 ambient → (r,g,b) 或 None。多盏取平均 color*intensity。"""
    if not ambients:
        return None
    rs = gs = bs = 0.0
    n = 0
    for a in ambients:
        c = a.get("color") or (1, 1, 1)
        inten = float(a.get("intensity") or 1.0)
        rs += float(c[0]) * inten
        gs += float(c[1]) * inten
        bs += float(c[2]) * inten
        n += 1
    if n <= 0:
        return None
    return (rs / n, gs / n, bs / n)
