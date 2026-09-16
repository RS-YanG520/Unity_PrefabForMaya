# -*- coding: utf-8 -*-
"""UI 中英文切换。

语言存 Maya optionVar: mtu_ui_lang = \"en\" | \"zh\"
所有菜单 / 弹窗文案经 tr() 取出，再经 to_maya_ui 交给 cmds。
"""
from __future__ import print_function

import maya.cmds as cmds

from constants import to_maya_ui

OPTION_VAR = "mtu_ui_lang"

# key -> { "en": u"...", "zh": u"..." }
_STRINGS = {
    "menu_root": {
        "en": u"Unity MayaToUnity",
        "zh": u"Unity MayaToUnity",
    },
    "menu_export": {
        "en": u"Export to Unity Project...",
        "zh": u"导出到 Unity 项目...",
    },
    "menu_diagnose": {
        "en": u"Diagnose Scene...",
        "zh": u"诊断当前场景...",
    },
    "menu_convert_pbr": {
        "en": u"Convert Materials to PBR...",
        "zh": u"材质转换为 PBR...",
    },
    "menu_restore_mat": {
        "en": u"Restore Original Materials...",
        "zh": u"还原原始材质...",
    },
    "menu_reload": {
        "en": u"Reload Plugin Modules",
        "zh": u"重新加载插件模块",
    },
    "menu_debug": {
        "en": u"Enable Debug Log",
        "zh": u"开启调试日志",
    },
    "menu_language": {
        "en": u"Language",
        "zh": u"语言",
    },
    "lang_en": {
        "en": u"English",
        "zh": u"English",
    },
    "lang_zh": {
        "en": u"中文",
        "zh": u"中文",
    },
    "dlg_export_caption": {
        "en": u"Select Unity export folder",
        "zh": u"选择 Unity 导出目录",
    },
    "dlg_export_title": {
        "en": u"Export Complete",
        "zh": u"导出完成",
    },
    "dlg_export_msg": {
        "en": u"Exported %d files to:\n%s",
        "zh": u"已导出 %d 个文件到:\n%s",
    },
    "dlg_ok": {
        "en": u"OK",
        "zh": u"确定",
    },
    "dlg_diagnose_title": {
        "en": u"Scene Diagnosis",
        "zh": u"场景诊断",
    },
    "dlg_diagnose_msg": {
        "en": u"Diagnosis report printed to the Script Editor.",
        "zh": u"诊断报告已打印到脚本编辑器。",
    },
    "dlg_convert_title": {
        "en": u"Convert Materials to PBR",
        "zh": u"材质转换为 PBR",
    },
    "dlg_restore_title": {
        "en": u"Restore Original Materials",
        "zh": u"还原原始材质",
    },
    "warn_no_main_window": {
        "en": u"Unity_MayaToUnity: main window 'MayaWindow' not found, cannot install menu.",
        "zh": u"Unity_MayaToUnity: 未找到主窗口 MayaWindow，无法安装菜单。",
    },
    "msg_reloaded": {
        "en": u"Unity_MayaToUnity: modules reloaded.",
        "zh": u"Unity_MayaToUnity: 模块已重新加载。",
    },
    "msg_lang_switched": {
        "en": u"Unity_MayaToUnity: language set to English.",
        "zh": u"Unity_MayaToUnity: 界面语言已切换为中文。",
    },
}


def _detect_default_lang():
    """按 Maya UI 语言猜默认值；失败则英文。"""
    try:
        ui = cmds.about(uiLanguage=True)
        if ui and "zh" in str(ui).lower():
            return "zh"
    except Exception:
        pass
    try:
        # 部分版本用 language
        ui = cmds.about(language=True)
        if ui and "zh" in str(ui).lower():
            return "zh"
    except Exception:
        pass
    return "en"


def get_lang():
    """当前语言代码：en / zh。"""
    try:
        if cmds.optionVar(exists=OPTION_VAR):
            lang = cmds.optionVar(q=OPTION_VAR)
            if lang in ("en", "zh"):
                return lang
    except Exception:
        pass
    return _detect_default_lang()


def set_lang(lang, rebuild_menu=True):
    """设置语言并可选重建菜单。

    重建必须延迟到当前菜单事件结束之后（evalDeferred），
    否则在 Language 菜单项回调里 deleteUI 会触发 Qt ACCESS_VIOLATION 崩溃。
    """
    if lang not in ("en", "zh"):
        lang = "en"
    try:
        cmds.optionVar(sv=(OPTION_VAR, lang))
    except Exception:
        pass
    if rebuild_menu:
        # 不能在 menuItem command 里同步 deleteUI/重建同一菜单
        def _rebuild():
            try:
                import Unity_MayaToUnity as pkg
                pkg.install_menu()
            except Exception:
                pass
        try:
            cmds.evalDeferred(_rebuild)
        except Exception:
            _rebuild()
    print(to_maya_ui(_STRINGS["msg_lang_switched"][lang]))
    return lang


def tr(key, *args):
    """取当前语言文案；可选 % 格式化参数。返回 unicode。"""
    lang = get_lang()
    entry = _STRINGS.get(key) or {}
    text = entry.get(lang) or entry.get("en") or key
    if args:
        try:
            text = text % args
        except Exception:
            pass
    return to_maya_ui(text)
