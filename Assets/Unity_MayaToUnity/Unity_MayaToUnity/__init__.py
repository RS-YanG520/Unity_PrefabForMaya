# -*- coding: utf-8 -*-
"""Unity MayaToUnity —— Maya → Unity Prefab 导出插件。

对标 UnrealToUnity：在 Maya 2018（Python 2.7）侧直接手写 Unity 的文本资产
（FBX/.mat/.prefab/.unity 及对应 .meta），导出一个可直接放入 Unity 项目的目录。

识别 Maya 实例（共享同一 shape 的多个 transform）→ 每组生成一个 Prefab，
组内每个实例按各自的世界变换在场景里摆放。

安装：
  把本包目录放到 Maya 脚本目录，例如
    D:\\My Documents\\maya\\2018\\zh_CN\\scripts\\Unity_MayaToUnity

使用（在 Maya 脚本编辑器里）：
    import Unity_MayaToUnity
    Unity_MayaToUnity.install_menu()

会弹出工具窗口（不再往 Maya 主菜单栏塞标签）。界面中英文可切换，
偏好写入 optionVar mtu_ui_lang。
"""
from __future__ import print_function

import maya.cmds as cmds

from exporter import export
import debug
import material_convert
import texture_drop
from constants import to_maya_ui
import i18n
import ui as toolbox_ui


MENU_NAME = "unity_maya_to_unity_menu"  # 旧菜单名，仅用于清理


def _pick_output_dir():
    """弹出目录选择框，返回目录路径或 None。"""
    result = cmds.fileDialog2(
        dialogStyle=2,
        fileMode=3,
        caption=i18n.tr("dlg_export_caption"),
    )
    return result[0] if result else None


def _export_with_dialog():
    out = _pick_output_dir()
    if not out:
        return
    selection = cmds.ls(selection=True) or None
    # 必须等 UI 事件结束后再改场景，否则 Outliner/Qt 易 ACCESS_VIOLATION
    def _run():
        try:
            generated = export(out, selection=selection, scene_name="MayaToUnity")
        except Exception as exc:
            cmds.warning(to_maya_ui("Unity_MayaToUnity export failed: %s" % exc))
            return
        if not generated:
            return
        msg = i18n.tr("dlg_export_msg", len(generated), to_maya_ui(out))
        ok = i18n.tr("dlg_ok")
        title = i18n.tr("dlg_export_title")

        def _dlg():
            cmds.confirmDialog(
                title=title,
                message=msg,
                button=[ok],
                defaultButton=ok,
                cancelButton=ok,
                dismissString=ok,
            )

        try:
            cmds.evalDeferred(_dlg)
        except Exception:
            _dlg()

    try:
        cmds.evalDeferred(_run)
    except Exception:
        _run()


def _confirm_report(title_key, result):
    title = i18n.tr(title_key)
    text = to_maya_ui(material_convert._format_report(title, result))
    print(text)
    ok = i18n.tr("dlg_ok")
    cmds.confirmDialog(
        title=title,
        message=text[:900],
        button=[ok],
        defaultButton=ok,
        cancelButton=ok,
        dismissString=ok,
    )


def convert_materials_to_pbr():
    """检测并转换材质到 standardSurface / Arnold；选中物体优先，否则全场景。"""
    selection = cmds.ls(selection=True) or None
    result = material_convert.convert_materials_to_pbr(selection)
    _confirm_report("dlg_convert_title", result)
    return result


def restore_original_materials():
    """还原到转换前的源材质。"""
    selection = cmds.ls(selection=True) or None
    result = material_convert.restore_original_materials(selection)
    _confirm_report("dlg_restore_title", result)
    return result


def install_menu():
    """打开工具窗口（兼容旧名 install_menu；不再安装 Maya 主菜单）。"""
    return show_toolbox()


def show_toolbox(force_recreate=False):
    """显示 / 前置工具窗口。"""
    try:
        texture_drop.install()
    except Exception:
        pass
    return toolbox_ui.show_toolbox(force_recreate=force_recreate)


def set_debug(enabled):
    """开启 / 关闭调试日志（控制台实时打印）。"""
    debug.set_debug(enabled)


def set_texture_drop(enabled):
    """开启 / 关闭属性编辑器外部贴图拖放。"""
    texture_drop.set_enabled(enabled)


def reload_modules():
    """强制重载本包子模块（改完代码后在工具窗口点 Reload，或脚本里调用）。

    注意：不能先删掉 sys.modules['Unity_MayaToUnity'] 再 import，
    否则会触发 Parent module not found 警告。
    """
    import sys
    try:
        texture_drop.uninstall()
    except Exception:
        pass
    doomed = []
    for k, mod in list(sys.modules.items()):
        if k == "Unity_MayaToUnity":
            continue
        if k.startswith("Unity_MayaToUnity."):
            doomed.append(k)
            continue
        if k in (
            "exporter", "coords", "constants", "debug",
            "instance_detector", "material_mapper", "yaml_writer", "hierarchy",
            "material_convert", "texture_drop", "i18n", "ui",
        ):
            path = getattr(mod, "__file__", "") or ""
            if "Unity_MayaToUnity" in path.replace("\\", "/"):
                doomed.append(k)
    for k in doomed:
        del sys.modules[k]

    import Unity_MayaToUnity as pkg
    reload(pkg)

    def _rebuild():
        try:
            # 强制重建窗口，绑定到新代码
            pkg.show_toolbox(force_recreate=True)
        except Exception:
            pass
        try:
            print(pkg.i18n.tr("msg_reloaded"))
        except Exception:
            print("Unity_MayaToUnity: modules reloaded.")

    try:
        cmds.evalDeferred(_rebuild)
    except Exception:
        _rebuild()
    return True


def diagnose(show_dialog=False):
    """dry-run 诊断当前场景（不导出）。

    返回 diagnose_scene 的 dict（summary / meshes / report）。
    工具窗口会显示汇总与模型列表；脚本编辑器仍打印完整 report。
    """
    selection = cmds.ls(selection=True) or None
    result = debug.diagnose_scene(selection)
    if show_dialog:
        ok = i18n.tr("dlg_ok")
        summary = (result or {}).get("summary") or {}
        msg = i18n.tr(
            "ui_diag_summary",
            int(summary.get("model_count", 0)),
            int(summary.get("material_count", 0)),
            int(summary.get("texture_count", 0)),
            int(summary.get("total_faces", 0)),
            summary.get("total_export_size") or "0 B",
        )
        cmds.confirmDialog(
            title=i18n.tr("dlg_diagnose_title"),
            message=msg,
            button=[ok],
            defaultButton=ok,
            cancelButton=ok,
            dismissString=ok,
        )
    return result
