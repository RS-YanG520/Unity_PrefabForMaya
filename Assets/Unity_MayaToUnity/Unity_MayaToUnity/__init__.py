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

菜单支持中英文切换（Language / 语言），偏好写入 optionVar mtu_ui_lang。
"""
from __future__ import print_function

import maya.cmds as cmds

from exporter import export
import debug
import material_convert
from constants import to_maya_ui
import i18n


MENU_NAME = "unity_maya_to_unity_menu"


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
    generated = export(out, selection=selection, scene_name="MayaToUnity")
    if generated:
        msg = i18n.tr("dlg_export_msg", len(generated), to_maya_ui(out))
        ok = i18n.tr("dlg_ok")
        cmds.confirmDialog(
            title=i18n.tr("dlg_export_title"),
            message=msg,
            button=[ok],
            defaultButton=ok,
            cancelButton=ok,
            dismissString=ok,
        )


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


def _set_language(lang, *_):
    i18n.set_lang(lang, rebuild_menu=True)


def install_menu():
    """在主菜单栏安装 Unity MayaToUnity 菜单。"""
    main = "MayaWindow"
    if not cmds.window(main, q=True, exists=True):
        cmds.warning(i18n.tr("warn_no_main_window"))
        return
    if cmds.menu(MENU_NAME, exists=True):
        cmds.deleteUI(MENU_NAME)

    lang = i18n.get_lang()
    cmds.menu(MENU_NAME, label=i18n.tr("menu_root"), parent=main, tearOff=True)
    cmds.menuItem(label=i18n.tr("menu_export"), command=lambda *_: _export_with_dialog())
    cmds.menuItem(label=i18n.tr("menu_diagnose"), command=lambda *_: diagnose())
    cmds.menuItem(divider=True)
    cmds.menuItem(
        label=i18n.tr("menu_convert_pbr"),
        command=lambda *_: convert_materials_to_pbr(),
    )
    cmds.menuItem(
        label=i18n.tr("menu_restore_mat"),
        command=lambda *_: restore_original_materials(),
    )
    cmds.menuItem(divider=True)
    cmds.menuItem(label=i18n.tr("menu_reload"), command=lambda *_: reload_modules())
    cmds.menuItem(
        label=i18n.tr("menu_debug"),
        checkBox=bool(debug.DEBUG),
        command=_toggle_debug,
    )
    cmds.menuItem(divider=True)
    cmds.menuItem(label=i18n.tr("menu_language"), subMenu=True)
    cmds.radioMenuItemCollection()
    cmds.menuItem(
        label=i18n.tr("lang_en"),
        radioButton=(lang == "en"),
        command=lambda *_: _set_language("en"),
    )
    cmds.menuItem(
        label=i18n.tr("lang_zh"),
        radioButton=(lang == "zh"),
        command=lambda *_: _set_language("zh"),
    )
    cmds.setParent("..", menu=True)


def set_debug(enabled):
    """开启 / 关闭调试日志（控制台实时打印）。"""
    debug.set_debug(enabled)


def reload_modules():
    """强制重载本包子模块（改完代码后在 Maya 里先跑这个再导出）。

    注意：不能先删掉 sys.modules['Unity_MayaToUnity'] 再 import，
    否则会触发 Parent module not found 警告。
    """
    import sys
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
            "material_convert", "i18n",
        ):
            path = getattr(mod, "__file__", "") or ""
            if "Unity_MayaToUnity" in path.replace("\\", "/"):
                doomed.append(k)
    for k in doomed:
        del sys.modules[k]
    import Unity_MayaToUnity as pkg
    reload(pkg)
    pkg.install_menu()
    print(i18n.tr("msg_reloaded"))
    return True


def diagnose():
    """dry-run 诊断当前场景（不导出），结果打印到脚本编辑器并弹窗提示。"""
    selection = cmds.ls(selection=True) or None
    debug.diagnose_scene(selection)
    ok = i18n.tr("dlg_ok")
    cmds.confirmDialog(
        title=i18n.tr("dlg_diagnose_title"),
        message=i18n.tr("dlg_diagnose_msg"),
        button=[ok],
        defaultButton=ok,
        cancelButton=ok,
        dismissString=ok,
    )


def _toggle_debug(state, *_):
    debug.set_debug(bool(state))


def _auto_install():
    """import 时延迟安装菜单（等主窗口就绪）。"""
    try:
        cmds.evalDeferred(install_menu)
    except Exception:
        pass


_auto_install()
