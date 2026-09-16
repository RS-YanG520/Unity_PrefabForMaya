# -*- coding: utf-8 -*-
"""材质类型检测 / 一键切到 PBR / 还原源材质。

目标类型优先 standardSurface；不可用时再试 aiStandardSurface（Arnold）。
转换时保留原材质节点（改名为 *_mtu_src），新材质沿用原名；
还原时把 shadingEngine 接回源材质并恢复原名。

注意：运行时文案用英文，避免 Maya 中文界面 GBK 乱码。
"""
from __future__ import print_function

import maya.cmds as cmds

# 已符合导出预期的 PBR 类型
_PBR_TYPES = ("standardSurface", "aiStandardSurface")

# 备份 / 标记属性（挂在 shadingEngine 上）
_ATTR_ORIGINAL = "mtuOriginalShader"
_ATTR_CONVERTED = "mtuConvertedShader"

_BACKUP_SUFFIX = "_mtu_src"
_DEFAULT_MATERIALS = (
    "lambert1", "standardSurface1", "particleCloud1", "initialShadingGroup",
)


def _short(node):
    return node.split("|")[-1] if node else node


def _resolve_target_type():
    """优先 standardSurface，否则 Arnold aiStandardSurface。"""
    types = set(cmds.allNodeTypes() or [])
    if "standardSurface" in types:
        return "standardSurface"
    if "aiStandardSurface" in types:
        return "aiStandardSurface"
    return None


def _is_pbr(node):
    if not node or not cmds.objExists(node):
        return False
    return cmds.nodeType(node) in _PBR_TYPES


def _ensure_string_attr(node, attr):
    if not cmds.attributeQuery(attr, node=node, exists=True):
        cmds.addAttr(node, ln=attr, dt="string")


def _set_sg_marks(sg, original, converted):
    _ensure_string_attr(sg, _ATTR_ORIGINAL)
    _ensure_string_attr(sg, _ATTR_CONVERTED)
    cmds.setAttr(sg + "." + _ATTR_ORIGINAL, original, type="string")
    cmds.setAttr(sg + "." + _ATTR_CONVERTED, converted, type="string")


def _clear_sg_marks(sg):
    for attr in (_ATTR_ORIGINAL, _ATTR_CONVERTED):
        plug = sg + "." + attr
        if cmds.attributeQuery(attr, node=sg, exists=True):
            try:
                cmds.setAttr(plug, "", type="string")
            except Exception:
                pass


def _get_sg_mark(sg, attr):
    if not cmds.attributeQuery(attr, node=sg, exists=True):
        return None
    try:
        val = cmds.getAttr(sg + "." + attr)
    except Exception:
        return None
    if not val:
        return None
    return val


def _unique_name(base):
    """生成场景内唯一短名。"""
    if not cmds.objExists(base):
        return base
    i = 1
    while cmds.objExists("%s_%d" % (base, i)):
        i += 1
    return "%s_%d" % (base, i)


def _collect_sg_materials(selection=None):
    """收集 (shadingEngine, surfaceShader) 列表。

    有选中：只收集选中物体用到的材质；无选中：全场景。
    """
    pairs = []
    seen_sg = set()

    if selection:
        shapes = cmds.ls(selection, dag=True, shapes=True, long=True) or []
        sgs = set()
        for sh in shapes:
            for sg in (cmds.listConnections(sh, type="shadingEngine") or []):
                sgs.add(sg)
        se_list = sorted(sgs)
    else:
        se_list = cmds.ls(type="shadingEngine") or []

    for sg in se_list:
        if sg in seen_sg:
            continue
        seen_sg.add(sg)
        mats = cmds.listConnections(sg + ".surfaceShader") or []
        if not mats:
            continue
        mat = mats[0]
        leaf = _short(mat)
        if leaf in _DEFAULT_MATERIALS:
            continue
        if sg in ("initialShadingGroup",):
            continue
        pairs.append((sg, mat))
    return pairs


def _disconnect_dest(plug):
    """断开 plug 上已有的输入连接。"""
    existing = cmds.listConnections(plug, source=True, destination=False, plugs=True) or []
    for src in existing:
        try:
            cmds.disconnectAttr(src, plug)
        except Exception:
            pass


def _transfer_attr_connection_or_value(src, src_attrs, dst, dst_attr):
    """把 src 上第一个存在的 attr 的连接或数值转到 dst.dst_attr。"""
    if not cmds.attributeQuery(dst_attr, node=dst, exists=True):
        return False
    for sa in src_attrs:
        if not cmds.attributeQuery(sa, node=src, exists=True):
            continue
        src_plug = src + "." + sa
        dst_plug = dst + "." + dst_attr
        conns = cmds.listConnections(src_plug, source=True, destination=False, plugs=True) or []
        if conns:
            _disconnect_dest(dst_plug)
            try:
                cmds.connectAttr(conns[0], dst_plug, force=True)
                return True
            except Exception:
                pass
        try:
            val = cmds.getAttr(src_plug)
            if isinstance(val, (list, tuple)) and val and isinstance(val[0], (list, tuple)):
                val = val[0]
            if isinstance(val, (list, tuple)) and len(val) >= 3:
                cmds.setAttr(dst_plug, val[0], val[1], val[2], type="double3")
            elif isinstance(val, (int, float)):
                cmds.setAttr(dst_plug, float(val))
            return True
        except Exception:
            continue
    return False


def _transfer_inputs(src, dst):
    """尽量把颜色 / 粗糙度 / 金属度 / 法线从旧材质迁到新 PBR。"""
    _transfer_attr_connection_or_value(src, ("baseColor", "color"), dst, "baseColor")
    _transfer_attr_connection_or_value(src, ("metalness",), dst, "metalness")
    _transfer_attr_connection_or_value(
        src, ("specularRoughness", "roughness"), dst, "specularRoughness"
    )
    # 法线 / bump：Phong 等走 bumpMapping；PBR 走 normalCamera
    if cmds.attributeQuery("normalCamera", node=dst, exists=True):
        moved = _transfer_attr_connection_or_value(
            src, ("normalCamera", "bumpMapping"), dst, "normalCamera"
        )
        if not moved and cmds.attributeQuery("bumpMapping", node=src, exists=True):
            _transfer_attr_connection_or_value(src, ("bumpMapping",), dst, "normalCamera")


def convert_materials_to_pbr(selection=None):
    """检测并转换不符合 PBR 的材质。

    返回 dict: converted / skipped / failed / target / messages
    """
    target = _resolve_target_type()
    result = {
        "converted": [],
        "skipped": [],
        "failed": [],
        "target": target,
        "messages": [],
    }
    if not target:
        msg = "No standardSurface or aiStandardSurface node type available."
        result["messages"].append(msg)
        cmds.warning("Unity_MayaToUnity: " + msg)
        return result

    pairs = _collect_sg_materials(selection)
    # 同一材质可能挂多个 SG：按材质合并
    by_mat = {}
    for sg, mat in pairs:
        by_mat.setdefault(mat, []).append(sg)

    for mat, sgs in by_mat.items():
        short = _short(mat)
        if _is_pbr(mat):
            result["skipped"].append(short)
            continue
        # 已转换过：SG 上已有标记且当前已是 PBR
        already = False
        for sg in sgs:
            orig = _get_sg_mark(sg, _ATTR_ORIGINAL)
            if orig and cmds.objExists(orig) and _is_pbr(mat):
                already = True
                break
        if already:
            result["skipped"].append(short)
            continue

        try:
            backup_name = _unique_name(short + _BACKUP_SUFFIX)
            new_tmp = _unique_name(short + "_mtu_pbr")
            new_mat = cmds.shadingNode(target, asShader=True, name=new_tmp)
            _transfer_inputs(mat, new_mat)

            # 先把原材质改名腾出短名，再把新材质改成原名
            backup = cmds.rename(mat, backup_name)
            final_name = short
            if cmds.objExists(final_name):
                final_name = _unique_name(short)
            new_mat = cmds.rename(new_mat, final_name)

            out_plug = new_mat + ".outColor"
            for sg in sgs:
                cmds.connectAttr(out_plug, sg + ".surfaceShader", force=True)
                _set_sg_marks(sg, backup, new_mat)

            result["converted"].append("%s -> %s (%s)" % (backup, new_mat, target))
        except Exception as e:
            result["failed"].append("%s: %s" % (short, e))
            cmds.warning("Unity_MayaToUnity: convert failed for %s: %s" % (short, e))

    result["messages"].append(
        "Target=%s  converted=%d  skipped=%d  failed=%d"
        % (target, len(result["converted"]), len(result["skipped"]), len(result["failed"]))
    )
    return result


def restore_original_materials(selection=None):
    """把已转换的材质还原为源材质，并恢复原名。

    返回 dict: restored / skipped / failed / messages
    """
    result = {
        "restored": [],
        "skipped": [],
        "failed": [],
        "messages": [],
    }
    pairs = _collect_sg_materials(selection)

    # 按 backup 合并，避免同一源材质被多次 rename
    by_backup = {}
    for sg, current in pairs:
        backup = _get_sg_mark(sg, _ATTR_ORIGINAL)
        if not backup:
            result["skipped"].append(_short(current))
            continue
        if not cmds.objExists(backup):
            result["failed"].append("%s: backup missing (%s)" % (_short(current), backup))
            continue
        by_backup.setdefault(backup, {"sgs": [], "current": current})
        by_backup[backup]["sgs"].append(sg)
        by_backup[backup]["current"] = current

    for backup, info in by_backup.items():
        sgs = info["sgs"]
        current = info["current"]
        try:
            desired = _short(backup)
            if desired.endswith(_BACKUP_SUFFIX):
                desired = desired[: -len(_BACKUP_SUFFIX)]

            # 先接回源材质
            for sg in sgs:
                cmds.connectAttr(backup + ".outColor", sg + ".surfaceShader", force=True)

            # 删掉转换出来的 PBR，腾出原名
            converted = _get_sg_mark(sgs[0], _ATTR_CONVERTED) or current
            if converted and cmds.objExists(converted) and converted != backup:
                try:
                    cmds.delete(converted)
                except Exception:
                    try:
                        cmds.rename(converted, _unique_name(_short(converted) + "_mtu_old"))
                    except Exception:
                        pass

            # 恢复源材质原名
            if _short(backup) != desired:
                if cmds.objExists(desired):
                    desired = _unique_name(desired)
                backup = cmds.rename(backup, desired)

            for sg in sgs:
                _clear_sg_marks(sg)

            result["restored"].append(backup)
        except Exception as e:
            result["failed"].append("%s: %s" % (_short(backup), e))
            cmds.warning("Unity_MayaToUnity: restore failed for %s: %s" % (backup, e))

    result["messages"].append(
        "restored=%d  skipped=%d  failed=%d"
        % (len(result["restored"]), len(result["skipped"]), len(result["failed"]))
    )
    return result


def _format_report(title, result):
    lines = [title, ""]
    for key in ("target", "converted", "restored", "skipped", "failed", "messages"):
        if key not in result:
            continue
        val = result[key]
        if key == "target":
            lines.append("Target type: %s" % val)
            continue
        if key == "messages":
            for m in val:
                lines.append(m)
            continue
        lines.append("%s (%d):" % (key, len(val)))
        if not val:
            lines.append("  (none)")
        else:
            for item in val:
                lines.append("  - %s" % item)
        lines.append("")
    return "\n".join(lines)
