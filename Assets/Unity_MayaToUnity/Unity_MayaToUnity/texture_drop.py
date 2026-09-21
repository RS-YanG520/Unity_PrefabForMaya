# -*- coding: utf-8 -*-
"""贴图拖放：AE 属性槽 / 材质球 / 视口模型 → 建 file 并按规则连接。

用 Maya MExternalDropCallback 拦截从资源管理器拖入的文件。
Arnold 的「Bump Mapping」对应属性是 normalCamera（不是 bumpMapping）。
"""
from __future__ import print_function

import os
import re
import json

import maya.cmds as cmds
import maya.mel as mel
import maya.OpenMaya as om
import maya.OpenMayaUI as omui

try:
    from urllib import unquote  # Py2
except ImportError:
    from urllib.parse import unquote  # Py3

try:
    from PySide2 import QtCore, QtWidgets, QtGui
    from shiboken2 import wrapInstance
except ImportError:
    from PySide import QtCore, QtGui
    from PySide import QtGui as QtWidgets
    from shiboken import wrapInstance

import i18n
import debug
from constants import to_maya_ui

_IMAGE_EXTS = (
    ".png", ".jpg", ".jpeg", ".tga", ".tif", ".tiff",
    ".exr", ".hdr", ".bmp", ".psd", ".tx", ".dds",
)

# UI 标签 / 控件名子串 → 材质属性（长词优先）
# 注意：Arnold Geometry「Bump Mapping」= normalCamera
_LABEL_TO_ATTR = (
    (u"specularroughness", "specularRoughness"),
    (u"diffuseroughness", "diffuseRoughness"),
    (u"specular roughness", "specularRoughness"),
    (u"diffuse roughness", "diffuseRoughness"),
    (u"镜面粗糙度", "specularRoughness"),
    (u"漫反射粗糙度", "diffuseRoughness"),
    (u"粗糙度", "specularRoughness"),
    (u"basecolor", "baseColor"),
    (u"base color", "baseColor"),
    (u"基础色", "baseColor"),
    (u"基础颜色", "baseColor"),
    (u"specularcolor", "specularColor"),
    (u"emissioncolor", "emissionColor"),
    (u"bump mapping", "normalCamera"),
    (u"bumpmapping", "normalCamera"),
    (u"凹凸贴图", "normalCamera"),
    (u"凹凸映射", "normalCamera"),
    (u"法线", "normalCamera"),
    (u"normalcamera", "normalCamera"),
    (u"normal", "normalCamera"),
    (u"bump", "normalCamera"),
    (u"metalness", "metalness"),
    (u"金属度", "metalness"),
    (u"opacity", "opacity"),
    (u"不透明度", "opacity"),
    (u"opacity", "opacity"),
    (u"transmission", "transmission"),
    (u"color", "color"),
    (u"颜色", "color"),
)

_PBR_TYPES = ("standardSurface", "aiStandardSurface")
_SHADER_TYPES = _PBR_TYPES + (
    "lambert", "blinn", "phong", "phongE", "anisotropic", "oceanShader",
)
# file 也有 outColor，绝不能当材质；bump/place2d 同理
_UTILITY_TYPES = (
    "file", "place2dTexture", "bump2d", "bump3d", "aiNormalMap",
    "ramp", "layerTexture", "psdFileTex", "checker", "cloth",
    "bulge", "grid", "noise", "fractal", "mountain", "stucco",
    "water", "cloud", "brownian", "solidFractal", "projection",
    "envBall", "envCube", "envSphere", "envSky", "envChrome",
    "multiplyDivide", "reverse", "blendColors", "luminance",
    "clamp", "setRange", "remapValue", "remapColor", "remapHsv",
    "gammaCorrect", "contrast", "hsvToRgb", "rgbToHsv",
)

_callback = None
_enabled = True
_last_material = None  # 最近一次有效的表面材质（拖放后 AE 跳走时用）
_OPTION_VAR = "mtu_tex_drop"  # 1=开 0=关，默认开
_RULES_VAR = "mtu_tex_suffix_rules"  # JSON 字符串
_SIBLINGS_VAR = "mtu_tex_auto_siblings"  # 单张拖到材质球时是否补齐同套其它贴图，默认开

# 默认后缀规则（与示例贴图 T_*_B / _M / _N 对齐；匹配时按后缀长度从长到短）
# 每项: {"suffix": "_B", "attr": "baseColor", "label": "Base Color"}
_DEFAULT_SUFFIX_RULES = [
    {"suffix": "_BaseColor", "attr": "baseColor", "label": "Base Color"},
    {"suffix": "_Albedo", "attr": "baseColor", "label": "Albedo"},
    {"suffix": "_Diffuse", "attr": "baseColor", "label": "Diffuse"},
    {"suffix": "_Diff", "attr": "baseColor", "label": "Diffuse"},
    {"suffix": "_BC", "attr": "baseColor", "label": "Base Color"},
    {"suffix": "_B", "attr": "baseColor", "label": "Base Color"},
    {"suffix": "_Metalness", "attr": "metalness", "label": "Metalness"},
    {"suffix": "_Metallic", "attr": "metalness", "label": "Metallic"},
    {"suffix": "_Metal", "attr": "metalness", "label": "Metal"},
    {"suffix": "_M", "attr": "metalness", "label": "Metalness"},
    {"suffix": "_Roughness", "attr": "specularRoughness", "label": "Roughness"},
    {"suffix": "_Rough", "attr": "specularRoughness", "label": "Roughness"},
    {"suffix": "_R", "attr": "specularRoughness", "label": "Roughness"},
    {"suffix": "_Normal", "attr": "normalCamera", "label": "Normal"},
    {"suffix": "_Norm", "attr": "normalCamera", "label": "Normal"},
    {"suffix": "_Nor", "attr": "normalCamera", "label": "Normal"},
    {"suffix": "_N", "attr": "normalCamera", "label": "Normal"},
    {"suffix": "_Emissive", "attr": "emissionColor", "label": "Emission"},
    {"suffix": "_Emission", "attr": "emissionColor", "label": "Emission"},
    {"suffix": "_Emit", "attr": "emissionColor", "label": "Emission"},
    {"suffix": "_E", "attr": "emissionColor", "label": "Emission"},
    {"suffix": "_Opacity", "attr": "opacity", "label": "Opacity"},
    {"suffix": "_Alpha", "attr": "opacity", "label": "Opacity"},
    {"suffix": "_O", "attr": "opacity", "label": "Opacity"},
]


def default_suffix_rules():
    """返回默认规则的深拷贝列表。"""
    out = []
    for r in _DEFAULT_SUFFIX_RULES:
        out.append({
            "suffix": r["suffix"],
            "attr": r["attr"],
            "label": r.get("label") or r["attr"],
        })
    return out


def get_suffix_rules():
    """读取用户后缀规则；无效则回退默认。"""
    try:
        if cmds.optionVar(exists=_RULES_VAR):
            raw = cmds.optionVar(q=_RULES_VAR) or ""
            if raw:
                data = json.loads(raw)
                rules = _normalize_rules(data)
                if rules:
                    return rules
    except Exception:
        pass
    return default_suffix_rules()


def set_suffix_rules(rules):
    """保存后缀规则到 optionVar。"""
    rules = _normalize_rules(rules)
    if not rules:
        rules = default_suffix_rules()
    try:
        cmds.optionVar(sv=(_RULES_VAR, json.dumps(rules)))
    except Exception as exc:
        cmds.warning(to_maya_ui(
            "Unity_MayaToUnity: save suffix rules failed: %s" % exc))
    return rules


def reset_suffix_rules():
    """恢复默认后缀规则。"""
    return set_suffix_rules(default_suffix_rules())


def _normalize_rules(data):
    if not isinstance(data, (list, tuple)):
        return []
    out = []
    seen = set()
    for item in data:
        if not isinstance(item, dict):
            continue
        suf = (item.get("suffix") or "").strip()
        attr = (item.get("attr") or "").strip()
        label = (item.get("label") or attr).strip()
        if not suf or not attr:
            continue
        if not suf.startswith("_") and not suf.startswith("."):
            # 统一成 _XXX，便于 endswith；用户写 B 也当成 _B
            if len(suf) <= 8 and suf.replace("-", "").isalnum():
                suf = "_" + suf
        key = suf.lower()
        if key in seen:
            continue
        seen.add(key)
        out.append({"suffix": suf, "attr": attr, "label": label or attr})
    return out


def match_suffix_attr(filepath, rules=None):
    """按文件名后缀匹配材质属性；无匹配返回 None。"""
    if rules is None:
        rules = get_suffix_rules()
    stem = os.path.splitext(os.path.basename(filepath or ""))[0]
    if not stem:
        return None
    stem_l = stem.lower()
    ordered = sorted(rules, key=lambda r: len(r.get("suffix") or ""), reverse=True)
    for rule in ordered:
        suf = rule.get("suffix") or ""
        if not suf:
            continue
        if stem_l.endswith(suf.lower()):
            return rule.get("attr")
    return None


def split_base_and_suffix(filepath, rules=None):
    """拆出基础名与命中的后缀。返回 (base, suffix, attr) 或 (None, None, None)。"""
    if rules is None:
        rules = get_suffix_rules()
    stem = os.path.splitext(os.path.basename(filepath or ""))[0]
    if not stem:
        return None, None, None
    stem_l = stem.lower()
    ordered = sorted(rules, key=lambda r: len(r.get("suffix") or ""), reverse=True)
    for rule in ordered:
        suf = rule.get("suffix") or ""
        if not suf:
            continue
        if stem_l.endswith(suf.lower()):
            base = stem[:-len(suf)]
            if base:
                return base, suf, rule.get("attr")
    return None, None, None


def expand_sibling_textures(filepath, rules=None):
    """根据一张贴图，在同目录查找同基础名的其它后缀贴图。

    返回去重后的路径列表（含原文件）；找不到其它类型时仅含原文件。
    每种属性只取一个文件（优先与拖入文件同扩展名）。
    """
    if not filepath or not os.path.isfile(filepath):
        return [filepath] if filepath else []
    if rules is None:
        rules = get_suffix_rules()

    base, _hit_suf, _hit_attr = split_base_and_suffix(filepath, rules)
    if not base:
        return [filepath]

    folder = os.path.dirname(filepath)
    drop_ext = os.path.splitext(filepath)[1].lower()
    # attr -> (path, prefer_score)  score: 同扩展名优先
    best = {}

    try:
        names = os.listdir(folder)
    except Exception:
        return [filepath]

    ordered = sorted(rules, key=lambda r: len(r.get("suffix") or ""), reverse=True)
    for name in names:
        full = os.path.join(folder, name)
        if not os.path.isfile(full):
            continue
        ext = os.path.splitext(name)[1].lower()
        if ext not in _IMAGE_EXTS:
            continue
        stem = os.path.splitext(name)[0]
        stem_l = stem.lower()
        base_l = base.lower()
        for rule in ordered:
            suf = rule.get("suffix") or ""
            attr = rule.get("attr")
            if not suf or not attr:
                continue
            expect_l = (base + suf).lower()
            if stem_l != expect_l:
                continue
            score = 2 if ext == drop_ext else 1
            # 拖入文件本身再加分
            if os.path.normcase(os.path.abspath(full)) == \
                    os.path.normcase(os.path.abspath(filepath)):
                score += 2
            prev = best.get(attr)
            if prev is None or score > prev[1]:
                best[attr] = (full, score)
            break  # 该文件已命中最长后缀

    if not best:
        return [filepath]

    # 保持稳定顺序：按规则表顺序
    out = []
    seen = set()
    attr_order = []
    for rule in rules:
        a = rule.get("attr")
        if a and a not in attr_order:
            attr_order.append(a)
    for attr in attr_order:
        if attr in best:
            p = best[attr][0]
            key = os.path.normcase(os.path.abspath(p))
            if key not in seen:
                seen.add(key)
                out.append(p)
    # 确保拖入文件在列表中
    drop_key = os.path.normcase(os.path.abspath(filepath))
    if drop_key not in seen:
        out.insert(0, filepath)
    return out


def _read_enabled_pref():
    try:
        if cmds.optionVar(exists=_OPTION_VAR):
            return bool(int(cmds.optionVar(q=_OPTION_VAR)))
    except Exception:
        pass
    return True  # 默认开启


def _write_enabled_pref(flag):
    try:
        cmds.optionVar(iv=(_OPTION_VAR, 1 if flag else 0))
    except Exception:
        pass


def is_enabled():
    return bool(_enabled)


def set_enabled(flag):
    global _enabled
    _enabled = bool(flag)
    _write_enabled_pref(_enabled)
    if _enabled:
        install()
    else:
        uninstall()


def is_auto_siblings():
    """单张拖到材质球时，是否自动补齐同目录同套其它后缀贴图。"""
    try:
        if cmds.optionVar(exists=_SIBLINGS_VAR):
            return bool(int(cmds.optionVar(q=_SIBLINGS_VAR)))
    except Exception:
        pass
    return True  # 默认开启


def set_auto_siblings(flag):
    try:
        cmds.optionVar(iv=(_SIBLINGS_VAR, 1 if flag else 0))
    except Exception:
        pass


def install():
    """注册外部拖放回调（可重复调用，先卸再装）。

    若偏好为关闭，只同步 _enabled，不注册回调。
    """
    global _callback, _enabled
    _enabled = _read_enabled_pref()
    uninstall()
    if not _enabled:
        debug.log("texture_drop: disabled by preference")
        return False
    try:
        cb = _TextureDropCallback()
        omui.MExternalDropCallback.addCallback(cb)
        _callback = cb
        debug.log("texture_drop: MExternalDropCallback installed")
    except Exception as exc:
        cmds.warning(to_maya_ui(
            "Unity_MayaToUnity: texture drop install failed: %s" % exc))
        _callback = None
        return False
    return True


def uninstall():
    global _callback
    if _callback is None:
        return
    try:
        omui.MExternalDropCallback.removeCallback(_callback)
    except Exception:
        pass
    _callback = None


# ------------------------------------------------------------------ helpers

def _url_to_path(url):
    if not url:
        return ""
    text = url
    if text.startswith("file:"):
        text = re.sub(r"^file:///+", "", text, count=1, flags=re.I)
        text = re.sub(r"^file:/+", "", text, count=1, flags=re.I)
        text = unquote(text)
        if re.match(r"^[A-Za-z]:", text):
            pass
        elif re.match(r"^/[A-Za-z]:", text):
            text = text[1:]
        text = text.replace("/", os.sep)
    return text


def _paths_from_drop(data):
    paths = []
    try:
        if data.hasUrls():
            for url in data.urls():
                p = _url_to_path(url)
                if p:
                    paths.append(p)
    except Exception:
        pass
    try:
        if not paths and data.hasText():
            raw = data.text() or ""
            for line in raw.replace("\r", "\n").split("\n"):
                line = line.strip().strip('"')
                if line:
                    paths.append(
                        _url_to_path(line) if line.lower().startswith("file:")
                        else line)
    except Exception:
        pass
    out = []
    for p in paths:
        if os.path.isfile(p) and os.path.splitext(p)[1].lower() in _IMAGE_EXTS:
            out.append(p)
    return out


def _match_label_to_attr(text):
    if not text:
        return None
    # 去空白后小写比对；中文原样包含
    raw = text.strip()
    compact = re.sub(r"\s+", "", raw).lower()
    lower = raw.lower()
    for key, attr in _LABEL_TO_ATTR:
        k = key.lower()
        kc = re.sub(r"\s+", "", k)
        if k in lower or kc in compact or key in raw:
            return attr
    return None


def _mel_exists(proc_name):
    try:
        return bool(mel.eval('exists("%s")' % proc_name))
    except Exception:
        return False


def _is_surface_shader(node):
    if not node or not cmds.objExists(node):
        return False
    ntype = cmds.nodeType(node)
    if ntype in _UTILITY_TYPES or ntype == "shadingEngine":
        return False
    if ntype in _SHADER_TYPES:
        return True
    try:
        for c in (cmds.getClassification(ntype) or []):
            if "shader/surface" in c:
                return True
    except Exception:
        pass
    return False


def _shader_from_node(node):
    """从任意节点追到表面材质（file/bump2d → 下游 shader）。"""
    if not node or not cmds.objExists(node):
        return None
    if _is_surface_shader(node):
        return node

    ntype = cmds.nodeType(node)
    if ntype == "shadingEngine":
        mats = cmds.listConnections(node + ".surfaceShader") or []
        return mats[0] if mats and _is_surface_shader(mats[0]) else None

    # mesh / transform
    shapes = []
    if ntype == "mesh":
        shapes = [node]
    elif ntype == "transform":
        shapes = cmds.listRelatives(
            node, shapes=True, noIntermediate=True) or []
    for sh in shapes:
        for sg in (cmds.listConnections(sh, type="shadingEngine") or []):
            mats = cmds.listConnections(sg + ".surfaceShader") or []
            for m in mats:
                if _is_surface_shader(m):
                    return m

    # 工具节点：沿 destination 广度优先找表面材质
    if ntype in _UTILITY_TYPES or not _is_surface_shader(node):
        visited = set()
        queue = [node]
        while queue:
            cur = queue.pop(0)
            if cur in visited or not cmds.objExists(cur):
                continue
            visited.add(cur)
            if _is_surface_shader(cur):
                return cur
            if cmds.nodeType(cur) == "shadingEngine":
                mats = cmds.listConnections(cur + ".surfaceShader") or []
                for m in mats:
                    if _is_surface_shader(m):
                        return m
            dests = cmds.listConnections(
                cur, source=False, destination=True) or []
            for d in dests:
                if d not in visited:
                    queue.append(d)
    return None


def _ae_node_from_selection_connection():
    """Maya 2018 兼容：从 AE 相关 selectionConnection 取节点。"""
    names = []
    try:
        for panel in (cmds.getPanel(type="attributeEditor") or []):
            names.append(panel)
            names.append(panel + ".selectionConnection")
    except Exception:
        pass
    names.extend((
        "AEPanel.selectionConnection",
        "attributeEditorPanel1.selectionConnection",
        "attributeEditorPanel2.selectionConnection",
    ))
    for name in names:
        try:
            objs = cmds.selectionConnection(name, q=True, object=True) or []
        except Exception:
            objs = []
        for obj in objs:
            if obj and cmds.objExists(obj):
                return obj
    return None


def _ae_focus_node():
    """当前应贴图的表面材质（绝不是 file/bump2d）。"""
    global _last_material
    node = None

    # Maya 2018 没有 getAttributeEditorFocusNode；先 exists 再调，避免刷屏报错
    for proc in (
        "getAttributeEditorFocusNode",
        "getNodesInAttributeEditor",
    ):
        if not _mel_exists(proc):
            continue
        try:
            node = mel.eval("%s()" % proc)
        except Exception:
            node = None
        if not node:
            continue
        if isinstance(node, (list, tuple)):
            node = node[0] if node else None
        if node and cmds.objExists(node):
            break
        node = None

    if not node:
        node = _ae_node_from_selection_connection()

    if not node:
        sel = cmds.ls(selection=True) or []
        node = sel[0] if sel else None

    mat = _shader_from_node(node) if node else None

    # AE 跟到 file/bump 后，selection 可能仍是模型；再试 last_material
    if not mat and _last_material and cmds.objExists(_last_material):
        if _is_surface_shader(_last_material):
            mat = _last_material

    if mat and _is_surface_shader(mat):
        _last_material = mat
        return mat
    return None


def _control_ancestors(control_name):
    names = []
    cur = control_name
    seen = set()
    while cur and cur not in seen:
        seen.add(cur)
        names.append(cur)
        try:
            parent = cmds.control(cur, q=True, parent=True)
        except Exception:
            try:
                parent = cmds.layout(cur, q=True, parent=True)
            except Exception:
                parent = None
        if not parent or parent == cur:
            break
        cur = parent
    return names


def _in_attribute_editor(control_name):
    cn = control_name or ""
    keys = (
        "AttributeEditor", "AttrEd", "attributeEditor",
        "MainAttributeEditor", "AEroot", "AEForm",
        "aiStandardSurface", "standardSurface",
    )
    for k in keys:
        if k in cn:
            return True
    # Qt 光标下是否在 AE 窗里
    try:
        w = QtWidgets.QApplication.widgetAt(QtGui.QCursor.pos())
        depth = 0
        while w is not None and depth < 20:
            name = (w.objectName() or "") + " " + (w.windowTitle() or "")
            if "Attribute Editor" in name or "属性编辑器" in name \
                    or "AttributeEditor" in name:
                return True
            w = w.parentWidget()
            depth += 1
    except Exception:
        pass
    return False


def _qt_texts_near_cursor():
    """收集光标附近控件及其父/兄弟上的文本，用于认标签。"""
    texts = []
    try:
        pos = QtGui.QCursor.pos()
        w = QtWidgets.QApplication.widgetAt(pos)
    except Exception:
        return texts
    depth = 0
    while w is not None and depth < 14:
        depth += 1
        for getter in ("objectName", "toolTip", "statusTip",
                       "accessibleName", "text", "windowTitle"):
            try:
                fn = getattr(w, getter, None)
                if callable(fn):
                    t = fn()
                    if t:
                        texts.append(t)
            except Exception:
                pass
        # 同级 QLabel（属性行左侧标签）
        try:
            parent = w.parentWidget()
            if parent is not None:
                for child in parent.findChildren(QtWidgets.QLabel):
                    try:
                        t = child.text()
                        if t:
                            texts.append(t)
                    except Exception:
                        pass
                # 再上一层（行布局）
                gp = parent.parentWidget()
                if gp is not None:
                    for child in gp.findChildren(QtWidgets.QLabel):
                        try:
                            t = child.text()
                            if t:
                                texts.append(t)
                        except Exception:
                            pass
        except Exception:
            pass
        try:
            w = w.parentWidget()
        except Exception:
            break
    return texts


def _attr_from_qt_under_cursor():
    for text in _qt_texts_near_cursor():
        attr = _match_label_to_attr(text)
        if attr:
            return attr
    return None


def _attr_from_control(control_name):
    """从落点 UI 解析目标属性名。"""
    for name in _control_ancestors(control_name):
        leaf = name.split("|")[-1]
        queries = (
            ("attrNavigationControlGrp", "attribute"),
            ("attrFieldSliderGrp", "attribute"),
            ("attrColorSliderGrp", "attribute"),
            ("attrFieldGrp", "attribute"),
            ("attrControlGrp", "attribute"),
        )
        for cmd_name, flag in queries:
            cmd = getattr(cmds, cmd_name, None)
            if cmd is None:
                continue
            name_use = None
            try:
                if cmd(name, exists=True):
                    name_use = name
                elif cmd(leaf, exists=True):
                    name_use = leaf
            except Exception:
                continue
            if not name_use:
                continue
            try:
                attr = cmd(name_use, q=True, **{flag: True})
            except Exception:
                attr = None
            if attr:
                if "." in attr:
                    attr = attr.split(".")[-1]
                return _normalize_attr_name(attr)

    path_attr = _match_label_to_attr(control_name or "")
    if path_attr:
        return path_attr

    return _attr_from_qt_under_cursor()


def _normalize_attr_name(attr):
    """统一别名：bumpMapping → 优先当 normalCamera 用。"""
    if not attr:
        return attr
    if attr in ("bumpMapping", "bump", "BumpMapping"):
        return "normalCamera"
    return attr


def _resolve_attr_on_material(mat, attr):
    """确保属性在材质上存在；Arnold bump 回退到 normalCamera。"""
    if not mat or not attr:
        return None
    attr = _normalize_attr_name(attr)
    if cmds.attributeQuery(attr, node=mat, exists=True):
        return attr
    if attr == "normalCamera":
        for alt in ("normalCamera", "bumpMapping", "normal"):
            if cmds.attributeQuery(alt, node=mat, exists=True):
                return alt
    if attr == "baseColor" and cmds.attributeQuery(
            "color", node=mat, exists=True):
        return "color"
    if attr == "color" and cmds.attributeQuery(
            "baseColor", node=mat, exists=True):
        return "baseColor"
    return None


def _attr_type(node, attr):
    try:
        return cmds.getAttr(node + "." + attr, type=True)
    except Exception:
        return None


def _disconnect_plug(plug):
    try:
        srcs = cmds.listConnections(
            plug, source=True, destination=False, plugs=True) or []
        for s in srcs:
            try:
                cmds.disconnectAttr(s, plug)
            except Exception:
                pass
    except Exception:
        pass


def _link_place2d(p2d, file_node):
    pairs = (
        ("coverage", "coverage"),
        ("translateFrame", "translateFrame"),
        ("rotateFrame", "rotateFrame"),
        ("mirrorU", "mirrorU"),
        ("mirrorV", "mirrorV"),
        ("stagger", "stagger"),
        ("wrapU", "wrapU"),
        ("wrapV", "wrapV"),
        ("repeatUV", "repeatUV"),
        ("offset", "offset"),
        ("rotateUV", "rotateUV"),
        ("noiseUV", "noiseUV"),
        ("vertexUvOne", "vertexUvOne"),
        ("vertexUvTwo", "vertexUvTwo"),
        ("vertexUvThree", "vertexUvThree"),
        ("vertexCameraOne", "vertexCameraOne"),
        ("outUV", "uvCoord"),
        ("outUvFilterSize", "uvFilterSize"),
    )
    for a, b in pairs:
        try:
            if cmds.attributeQuery(a, node=p2d, exists=True) and \
                    cmds.attributeQuery(b, node=file_node, exists=True):
                cmds.connectAttr(
                    "%s.%s" % (p2d, a), "%s.%s" % (file_node, b), force=True)
        except Exception:
            pass


def _create_file_node(filepath):
    base = os.path.splitext(os.path.basename(filepath))[0]
    base = re.sub(r"[^0-9A-Za-z_]", "_", base) or "file"
    # skipSelect：避免新建节点抢走选择 / 属性编辑器焦点
    file_node = cmds.shadingNode(
        "file", asTexture=True, isColorManaged=True,
        name=base, skipSelect=True)
    p2d = cmds.shadingNode(
        "place2dTexture", asUtility=True,
        name=base + "_place2d", skipSelect=True)
    _link_place2d(p2d, file_node)
    cmds.setAttr(file_node + ".fileTextureName", filepath, type="string")
    return file_node


def _connect_texture(mat, attr, file_node):
    plug = mat + "." + attr
    _disconnect_plug(plug)

    if attr in ("normalCamera", "bumpMapping"):
        bump = cmds.shadingNode(
            "bump2d", asUtility=True,
            name=file_node + "_bump", skipSelect=True)
        try:
            cmds.connectAttr(
                file_node + ".outAlpha", bump + ".bumpValue", force=True)
        except Exception:
            cmds.connectAttr(
                file_node + ".outColorR", bump + ".bumpValue", force=True)
        try:
            cmds.setAttr(bump + ".bumpInterp", 1)  # Tangent Space Normals
        except Exception:
            pass
        cmds.connectAttr(bump + ".outNormal", plug, force=True)
        try:
            cmds.setAttr(file_node + ".colorSpace", "Raw", type="string")
            cmds.setAttr(file_node + ".alphaIsLuminance", 1)
        except Exception:
            pass
        return

    atype = _attr_type(mat, attr)
    if atype in ("float3", "double3"):
        cmds.connectAttr(file_node + ".outColor", plug, force=True)
        return

    try:
        cmds.setAttr(file_node + ".alphaIsLuminance", 1)
    except Exception:
        pass
    try:
        cmds.connectAttr(file_node + ".outAlpha", plug, force=True)
    except Exception:
        cmds.connectAttr(file_node + ".outColorR", plug, force=True)


def _restore_ae_to_material(mat, prev_sel=None):
    """拖放连线后属性编辑器常会跟到 file/bump；强制切回材质页。"""
    if not mat or not cmds.objExists(mat):
        return

    def _go():
        try:
            if prev_sel:
                alive = [s for s in prev_sel if cmds.objExists(s)]
                if alive:
                    cmds.select(alive, replace=True)
        except Exception:
            pass
        if _mel_exists("showEditorExact"):
            try:
                safe = mat.replace("\\", "\\\\").replace('"', '\\"')
                mel.eval('showEditorExact("%s")' % safe)
                return
            except Exception:
                pass
        if _mel_exists("updateAE"):
            try:
                mel.eval('updateAE("%s")' % mat.replace('"', '\\"'))
            except Exception:
                pass

    try:
        cmds.evalDeferred(_go)
    except Exception:
        _go()


def assign_texture_to_attr(mat, attr, filepath, restore_ae=True):
    """给材质属性挂上贴图；成功返回 (file 节点名, 实际属性)。"""
    global _last_material
    mat = _shader_from_node(mat) or mat
    if not mat or not _is_surface_shader(mat):
        raise RuntimeError("not a surface material: %s" % mat)
    attr = _resolve_attr_on_material(mat, attr)
    if not attr:
        raise RuntimeError("attribute missing on %s" % mat)
    if not filepath or not os.path.isfile(filepath):
        raise RuntimeError("file not found: %s" % filepath)

    prev_sel = cmds.ls(selection=True, long=True) or []
    file_node = _create_file_node(filepath)
    _connect_texture(mat, attr, file_node)
    _last_material = mat
    if restore_ae:
        _restore_ae_to_material(mat, prev_sel)
    return file_node, attr


def assign_textures_by_suffix(mat, filepaths, rules=None):
    """按后缀规则批量贴图到材质；返回 (成功列表, 跳过列表)。

    成功项: (path, attr, file_node)
    跳过项: (path, reason)
    """
    global _last_material
    mat = _shader_from_node(mat) or mat
    if not mat or not _is_surface_shader(mat):
        raise RuntimeError("not a surface material: %s" % mat)
    if rules is None:
        rules = get_suffix_rules()

    prev_sel = cmds.ls(selection=True, long=True) or []
    ok_list = []
    skip_list = []
    for path in filepaths or []:
        if not path or not os.path.isfile(path):
            skip_list.append((path, "missing file"))
            continue
        attr = match_suffix_attr(path, rules)
        if not attr:
            skip_list.append((path, "no suffix rule"))
            continue
        resolved = _resolve_attr_on_material(mat, attr)
        if not resolved:
            skip_list.append((path, "attr %s not on material" % attr))
            continue
        try:
            file_node, used = assign_texture_to_attr(
                mat, resolved, path, restore_ae=False)
            ok_list.append((path, used, file_node))
        except Exception as exc:
            skip_list.append((path, str(exc)))

    _last_material = mat
    _restore_ae_to_material(mat, prev_sel)
    return ok_list, skip_list


def _is_swatch_drop(control_name):
    """是否拖到材质预览球 / 无具体属性槽。"""
    cn = (control_name or "").lower()
    keys = (
        "swatch", "materialswatch", "renderedswatch",
        "preview", "icon", "swatchlabel",
    )
    for k in keys:
        if k in cn:
            return True
    try:
        w = QtWidgets.QApplication.widgetAt(QtGui.QCursor.pos())
        depth = 0
        while w is not None and depth < 10:
            name = (w.objectName() or "").lower()
            tip = (w.toolTip() or "").lower()
            if "swatch" in name or "swatch" in tip:
                return True
            # 预览球一般是较大的带图 QLabel
            try:
                if isinstance(w, QtWidgets.QLabel) and w.pixmap() is not None:
                    if w.width() >= 48 and w.height() >= 48:
                        return True
            except Exception:
                pass
            w = w.parentWidget()
            depth += 1
    except Exception:
        pass
    return False


def _in_model_panel(control_name):
    """落点是否在视口 / modelPanel 上。"""
    cn = control_name or ""
    if "modelPanel" in cn or "modelEditor" in cn:
        return True
    if "viewPanes" in cn or "viewPane" in cn:
        return True
    try:
        focus = cmds.getPanel(withFocus=True)
        if focus and cmds.getPanel(typeOf=focus) == "modelPanel":
            # 拖到主窗口视口区域时 control 名可能不含 modelPanel
            if "MainPane" in cn or "MayaWindow" in cn or not cn:
                return True
    except Exception:
        pass
    return False


def _model_panel_from_control(control_name):
    """从控件路径解析 modelPanel 名。"""
    parts = (control_name or "").split("|")
    panels = cmds.getPanel(type="modelPanel") or []
    for p in parts:
        if p in panels:
            return p
        if p.startswith("modelPanel"):
            return p
    try:
        focus = cmds.getPanel(withFocus=True)
        if focus and cmds.getPanel(typeOf=focus) == "modelPanel":
            return focus
    except Exception:
        pass
    try:
        vis = cmds.getPanel(visiblePanels=True) or []
        for p in vis:
            if cmds.getPanel(typeOf=p) == "modelPanel":
                return p
    except Exception:
        pass
    return None


def _viewport_cursor_xy(panel=None):
    """光标在活动视口中的 Maya 像素坐标（原点左下）。失败返回 None。"""
    try:
        view = omui.M3dView.active3dView()
    except Exception:
        return None
    try:
        ptr = view.widget()
        try:
            ptr = long(ptr)
        except NameError:
            ptr = int(ptr)
        widget = wrapInstance(ptr, QtWidgets.QWidget)
    except Exception:
        return None
    try:
        lp = widget.mapFromGlobal(QtGui.QCursor.pos())
        x = int(lp.x())
        y = int(view.portHeight() - lp.y())
        if x < 0 or y < 0 or x >= view.portWidth() or y >= view.portHeight():
            return None
        return x, y
    except Exception:
        return None


def _parse_face_component(hit):
    """hitTest 若已带面组件，返回 (shape, face_id)。"""
    if not hit or ".f[" not in hit:
        return None
    node, rest = hit.split(".f[", 1)
    digits = []
    for ch in rest:
        if ch.isdigit():
            digits.append(ch)
        else:
            break
    if not digits or not cmds.objExists(node):
        return None
    if cmds.nodeType(node) != "mesh":
        return None
    long_name = (cmds.ls(node, long=True) or [node])[0]
    return long_name, int("".join(digits))


def _mesh_shapes_of(hit):
    if not hit or not cmds.objExists(hit.split(".")[0]):
        return []
    node = hit.split(".")[0]
    ntype = cmds.nodeType(node)
    if ntype == "mesh":
        return cmds.ls(node, long=True) or [node]
    if ntype == "transform":
        return cmds.listRelatives(
            node, shapes=True, noIntermediate=True,
            fullPath=True, type="mesh") or []
    return []


def _dag_path(node):
    sel = om.MSelectionList()
    sel.add(node)
    dag = om.MDagPath()
    sel.getDagPath(0, dag)
    if dag.hasFn(om.MFn.kTransform):
        try:
            dag.extendToShape()
        except Exception:
            pass
    return dag


def _ray_hit_face(shape, x, y):
    """从视口像素发射线，返回 (face_id, distance)，打不中则 None。"""
    try:
        view = omui.M3dView.active3dView()
        ray_source = om.MPoint()
        ray_dir = om.MVector()
        view.viewToWorld(int(x), int(y), ray_source, ray_dir)
        dag = _dag_path(shape)
        if not dag.hasFn(om.MFn.kMesh):
            return None
        fn = om.MFnMesh(dag)
        src = om.MFloatPoint(ray_source.x, ray_source.y, ray_source.z)
        direction = om.MFloatVector(ray_dir.x, ray_dir.y, ray_dir.z)
        hit_point = om.MFloatPoint()
        u_param = om.MScriptUtil()
        u_param.createFromDouble(0.0)
        p_param = u_param.asFloatPtr()
        u_face = om.MScriptUtil()
        u_face.createFromInt(0)
        p_face = u_face.asIntPtr()
        u_tri = om.MScriptUtil()
        u_tri.createFromInt(0)
        p_tri = u_tri.asIntPtr()
        u_b1 = om.MScriptUtil()
        u_b1.createFromDouble(0.0)
        p_b1 = u_b1.asFloatPtr()
        u_b2 = om.MScriptUtil()
        u_b2.createFromDouble(0.0)
        p_b2 = u_b2.asFloatPtr()
        hit = fn.closestIntersection(
            src, direction, None, None, False,
            om.MSpace.kWorld, 1.0e6, False, None,
            hit_point, p_param, p_face, p_tri, p_b1, p_b2)
        if not hit:
            return None
        return om.MScriptUtil.getInt(p_face), om.MScriptUtil.getFloat(p_param)
    except Exception as exc:
        debug.log("texture_drop raycast failed (%s): %s" % (shape, exc))
        return None


def _surface_from_shader_obj(mobj):
    if mobj is None or mobj.isNull():
        return None
    fn = om.MFnDependencyNode(mobj)
    nodes = []
    try:
        uid = fn.uuid().asString()
        if uid:
            nodes = cmds.ls(uid, type="shadingEngine") or []
    except Exception:
        nodes = []
    if not nodes:
        nodes = cmds.ls(fn.name(), type="shadingEngine") or []
    for sg in nodes:
        mat = _shader_from_node(sg)
        if mat:
            return mat
    return None


def _material_for_mesh_face(shape, face_id):
    """这个面所属的表面材质。同一模型多个材质时按面区分。"""
    try:
        dag = _dag_path(shape)
        if not dag.hasFn(om.MFn.kMesh):
            return None
        fn = om.MFnMesh(dag)
        if face_id < 0 or face_id >= fn.numPolygons():
            return None
        shaders = om.MObjectArray()
        indices = om.MIntArray()
        fn.getConnectedShaders(dag.instanceNumber(), shaders, indices)
        if face_id >= indices.length():
            return None
        slot = indices[face_id]
        if slot < 0 or slot >= shaders.length():
            return None
        return _surface_from_shader_obj(shaders[slot])
    except Exception as exc:
        debug.log("texture_drop face material failed: %s" % exc)
        return None


def _material_under_cursor(control_name=None):
    """视口光标下那个面的材质。打不中则 None。"""
    panel = _model_panel_from_control(control_name)
    xy = _viewport_cursor_xy(panel)
    if xy is None:
        return None
    x, y = xy
    shapes = []
    if panel:
        try:
            hits = cmds.hitTest(panel, x, y) or []
        except Exception as exc:
            debug.log("hitTest failed: %s" % exc)
            hits = []
        for h in hits:
            parsed = _parse_face_component(h)
            if parsed:
                mat = _material_for_mesh_face(parsed[0], parsed[1])
                if mat:
                    debug.log(
                        "texture_drop face %s.f[%s] -> %s"
                        % (parsed[0], parsed[1], mat))
                    return mat
            for sh in _mesh_shapes_of(h):
                if sh not in shapes:
                    shapes.append(sh)
    best = None
    for sh in shapes:
        got = _ray_hit_face(sh, x, y)
        if not got:
            continue
        face_id, dist = got
        if best is None or dist < best[0]:
            best = (dist, sh, face_id)
    if not best:
        return None
    mat = _material_for_mesh_face(best[1], best[2])
    debug.log(
        "texture_drop ray %s.f[%s] -> %s"
        % (best[1], best[2], mat))
    return mat


def _mesh_under_cursor(control_name=None):
    """视口光标下的 mesh/transform（不永久改选择）。"""
    panel = _model_panel_from_control(control_name)
    xy = _viewport_cursor_xy(panel)
    if xy is None:
        return None
    x, y = xy

    # 优先 hitTest（不改选择）
    if panel:
        try:
            hits = cmds.hitTest(panel, x, y) or []
            for h in hits:
                if not h or not cmds.objExists(h):
                    continue
                # 组件 → 物体
                node = h.split(".")[0]
                if cmds.nodeType(node) == "mesh":
                    pars = cmds.listRelatives(node, parent=True, fullPath=True) or []
                    return pars[0] if pars else node
                if cmds.nodeType(node) == "transform":
                    return cmds.ls(node, long=True)[0]
                sh = _shader_from_node(node)
                if sh:
                    return node
        except Exception as exc:
            debug.log("hitTest failed: %s" % exc)

    # 回退：selectFromScreen（临时选择，随后恢复）
    prev = cmds.ls(selection=True, long=True) or []
    try:
        om.MGlobal.selectFromScreen(
            x, y, x, y, om.MGlobal.kReplaceList)
        sel = cmds.ls(selection=True, long=True) or []
        if not sel:
            try:
                om.MGlobal.selectFromScreen(
                    x, y, x, y,
                    om.MGlobal.kReplaceList,
                    om.MGlobal.kSurfaceSelectMethod)
            except Exception:
                pass
            sel = cmds.ls(selection=True, long=True) or []
        if sel:
            return sel[0]
    except Exception as exc:
        debug.log("selectFromScreen failed: %s" % exc)
    finally:
        try:
            if prev:
                cmds.select(prev, replace=True)
            else:
                cmds.select(clear=True)
        except Exception:
            pass
    return None


def _resolve_target(control_name):
    """解析拖放目标材质与属性槽。

    视口模型：取光标下模型的表面材质，属性槽为空（走后缀批量）。
    属性编辑器：沿用 AE 焦点 + 控件属性。
    """
    if _in_model_panel(control_name):
        mat = _material_under_cursor(control_name)
        if not mat:
            hit = _mesh_under_cursor(control_name)
            mat = _shader_from_node(hit) if hit else None
        debug.log(
            "texture_drop viewport mat=%s" % mat)
        return mat, None

    mat = _ae_focus_node()
    attr = _attr_from_control(control_name)
    attr = _resolve_attr_on_material(mat, attr) if mat else None
    return mat, attr


def _can_handle(control_name, data):
    if not _enabled:
        return False
    paths = _paths_from_drop(data)
    if not paths:
        return False

    in_ae = _in_attribute_editor(control_name)
    in_vp = _in_model_panel(control_name)

    # 视口：拖入过程先接受，松手时再 hit-test（否则禁止图标）
    if in_vp:
        debug.log(
            "texture_drop probe: viewport ctrl=%s files=%d"
            % (control_name, len(paths)))
        return True

    mat, attr = _resolve_target(control_name)

    debug.log(
        "texture_drop probe: doDrop=? ctrl=%s inAE=%s mat=%s attr=%s files=%d"
        % (control_name, in_ae, mat, attr, len(paths)))

    if mat and attr:
        return True
    if in_ae and mat:
        return True
    if mat and (_is_swatch_drop(control_name) or len(paths) > 1):
        return True

    return False


def _do_assign(control_name, data):
    paths = _paths_from_drop(data)
    if not paths:
        return

    in_vp = _in_model_panel(control_name)
    mat, attr = _resolve_target(control_name)

    if mat and not attr and not in_vp:
        attr = _attr_from_qt_under_cursor()
        attr = _resolve_attr_on_material(mat, attr)

    if not mat:
        if in_vp:
            cmds.warning(i18n.tr("ui_texdrop_no_mesh"))
        else:
            cmds.warning(i18n.tr("ui_texdrop_no_target"))
        return

    # 视口模型 / 材质球 / 多文件 → 按后缀；单张可自动补齐同套
    on_swatch = (not in_vp) and (_is_swatch_drop(control_name) or not attr)
    use_batch = in_vp or len(paths) > 1 or on_swatch

    if use_batch:
        auto_fill = is_auto_siblings() and len(paths) == 1 and (in_vp or on_swatch)
        if auto_fill:
            expanded = expand_sibling_textures(paths[0])
            if len(expanded) > 1:
                debug.log(
                    "texture_drop auto-siblings: %d -> %d files"
                    % (1, len(expanded)))
                paths = expanded

        try:
            ok_list, skip_list = assign_textures_by_suffix(mat, paths)
        except Exception as exc:
            cmds.warning(to_maya_ui(
                "Unity_MayaToUnity: batch texture drop failed: %s" % exc))
            return
        for path, used_attr, file_node in ok_list:
            print(to_maya_ui(i18n.tr(
                "ui_texdrop_ok",
                os.path.basename(path), mat, used_attr, file_node)))
        for path, reason in skip_list:
            cmds.warning(to_maya_ui(i18n.tr(
                "ui_texdrop_skip",
                os.path.basename(path or "?"), reason)))
        if ok_list:
            print(to_maya_ui(i18n.tr(
                "ui_texdrop_batch_ok", len(ok_list), mat)))
        elif not skip_list:
            cmds.warning(i18n.tr("ui_texdrop_no_target"))
        return

    # 单文件 + 明确属性槽：直接连到该槽（不自动补齐）
    filepath = paths[0]
    try:
        file_node, used_attr = assign_texture_to_attr(mat, attr, filepath)
    except Exception as exc:
        cmds.warning(to_maya_ui(
            "Unity_MayaToUnity: texture drop failed: %s" % exc))
        return
    msg = i18n.tr(
        "ui_texdrop_ok",
        os.path.basename(filepath), mat, used_attr, file_node)
    print(to_maya_ui(msg))


def show_suffix_rules_dialog(parent=None):
    """弹出后缀规则编辑对话框（可增删改、恢复默认）。"""
    rules = get_suffix_rules()

    dlg = QtWidgets.QDialog(parent)
    dlg.setWindowTitle(i18n.tr("ui_texdrop_rules_title"))
    dlg.setMinimumWidth(420)
    dlg.setMinimumHeight(360)
    lay = QtWidgets.QVBoxLayout(dlg)

    tip = QtWidgets.QLabel(i18n.tr("ui_texdrop_rules_tip"))
    tip.setWordWrap(True)
    tip.setStyleSheet("color: #aaaaaa; font-size: 10px;")
    lay.addWidget(tip)

    table = QtWidgets.QTableWidget(0, 3)
    table.setHorizontalHeaderLabels([
        i18n.tr("ui_texdrop_col_suffix"),
        i18n.tr("ui_texdrop_col_attr"),
        i18n.tr("ui_texdrop_col_label"),
    ])
    table.horizontalHeader().setStretchLastSection(True)
    table.setSelectionBehavior(QtWidgets.QAbstractItemView.SelectRows)
    table.setSelectionMode(QtWidgets.QAbstractItemView.SingleSelection)
    lay.addWidget(table, 1)

    def _fill(rows):
        table.setRowCount(0)
        for r in rows:
            row = table.rowCount()
            table.insertRow(row)
            table.setItem(row, 0, QtWidgets.QTableWidgetItem(r.get("suffix") or ""))
            table.setItem(row, 1, QtWidgets.QTableWidgetItem(r.get("attr") or ""))
            table.setItem(row, 2, QtWidgets.QTableWidgetItem(r.get("label") or ""))

    _fill(rules)

    btn_row = QtWidgets.QHBoxLayout()
    btn_add = QtWidgets.QPushButton(i18n.tr("ui_texdrop_rules_add"))
    btn_del = QtWidgets.QPushButton(i18n.tr("ui_texdrop_rules_del"))
    btn_reset = QtWidgets.QPushButton(i18n.tr("ui_texdrop_rules_reset"))
    btn_row.addWidget(btn_add)
    btn_row.addWidget(btn_del)
    btn_row.addWidget(btn_reset)
    btn_row.addStretch(1)
    lay.addLayout(btn_row)

    def _on_add():
        row = table.rowCount()
        table.insertRow(row)
        table.setItem(row, 0, QtWidgets.QTableWidgetItem("_New"))
        table.setItem(row, 1, QtWidgets.QTableWidgetItem("baseColor"))
        table.setItem(row, 2, QtWidgets.QTableWidgetItem("New"))
        table.setCurrentCell(row, 0)

    def _on_del():
        row = table.currentRow()
        if row >= 0:
            table.removeRow(row)

    def _on_reset():
        _fill(default_suffix_rules())

    btn_add.clicked.connect(_on_add)
    btn_del.clicked.connect(_on_del)
    btn_reset.clicked.connect(_on_reset)

    buttons = QtWidgets.QDialogButtonBox(
        QtWidgets.QDialogButtonBox.Ok | QtWidgets.QDialogButtonBox.Cancel)
    lay.addWidget(buttons)

    def _on_ok():
        rows = []
        for i in range(table.rowCount()):
            def _cell(c):
                it = table.item(i, c)
                return it.text().strip() if it else ""
            rows.append({
                "suffix": _cell(0),
                "attr": _cell(1),
                "label": _cell(2),
            })
        set_suffix_rules(rows)
        dlg.accept()

    buttons.accepted.connect(_on_ok)
    buttons.rejected.connect(dlg.reject)
    dlg.exec_()
    return get_suffix_rules()


class _TextureDropCallback(omui.MExternalDropCallback):
    def __init__(self):
        omui.MExternalDropCallback.__init__(self)

    def externalDropCallback(self, doDrop, controlName, data):
        try:
            ok = _can_handle(controlName, data)
        except Exception as exc:
            debug.log("texture_drop can_handle error: %s" % exc)
            ok = False

        if not ok:
            return omui.MExternalDropCallback.kMayaDefault

        # 拖入过程必须 Accept，松手才会再进 doDrop=True
        if doDrop:
            try:
                _do_assign(controlName, data)
            except Exception as exc:
                cmds.warning(to_maya_ui(
                    "Unity_MayaToUnity: texture drop error: %s" % exc))
        return omui.MExternalDropCallback.kNoMayaDefaultAndAccept
