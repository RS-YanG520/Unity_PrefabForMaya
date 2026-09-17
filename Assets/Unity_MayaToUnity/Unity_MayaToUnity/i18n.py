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
        "en": u"Unity_MayaToUnity: main window 'MayaWindow' not found, cannot open toolbox.",
        "zh": u"Unity_MayaToUnity: 未找到主窗口 MayaWindow，无法打开工具窗口。",
    },
    "msg_reloaded": {
        "en": u"Unity_MayaToUnity: modules reloaded.",
        "zh": u"Unity_MayaToUnity: 模块已重新加载。",
    },
    "msg_lang_switched": {
        "en": u"Unity_MayaToUnity: language set to English.",
        "zh": u"Unity_MayaToUnity: 界面语言已切换为中文。",
    },
    "ui_sec_export": {
        "en": u"Export",
        "zh": u"导出",
    },
    "ui_sec_materials": {
        "en": u"Materials",
        "zh": u"材质",
    },
    "ui_sec_options": {
        "en": u"Options",
        "zh": u"选项",
    },
    "ui_sec_plugin": {
        "en": u"Plugin",
        "zh": u"插件",
    },
    "ui_status_hint": {
        "en": u"Tip: check Selection Only, then select meshes before Export.",
        "zh": u"提示：勾选「仅选中」后先选物体再导出；不勾则导出全场景。",
    },
    "ui_diag_title": {
        "en": u"Diagnosis",
        "zh": u"诊断信息",
    },
    "ui_diag_placeholder": {
        "en": u"Click Diagnose to refresh summary and mesh list.",
        "zh": u"点击「诊断」刷新汇总与模型列表。",
    },
    "ui_diag_summary": {
        "en": u"Models: %d  |  Mats: %d  |  Tex: %d  |  Faces: %d  |  Est: %s",
        "zh": u"模型: %d  |  材质: %d  |  贴图: %d  |  面数: %d  |  预估导出: %s",
    },
    "ui_col_name": {
        "en": u"Name",
        "zh": u"名称",
    },
    "ui_col_faces": {
        "en": u"Faces",
        "zh": u"面数",
    },
    "ui_col_instance": {
        "en": u"Instance",
        "zh": u"实例化",
    },
    "ui_col_size": {
        "en": u"Est. Size",
        "zh": u"预估大小",
    },
    "ui_col_base": {
        "en": u"Inst Base",
        "zh": u"基础模型",
    },
    "ui_base_none": {
        "en": u"-",
        "zh": u"-",
    },
    "ui_size_shared": {
        "en": u"(shared)",
        "zh": u"(共享)",
    },
    "ui_inst_yes": {
        "en": u"Yes",
        "zh": u"是",
    },
    "ui_inst_no": {
        "en": u"No",
        "zh": u"否",
    },
    "ui_warn_mesh_missing": {
        "en": u"Unity_MayaToUnity: mesh no longer exists in the scene.",
        "zh": u"Unity_MayaToUnity: 场景中已找不到该模型。",
    },
    "ui_menu_options": {
        "en": u"Options",
        "zh": u"选项",
    },
    "ui_menu_help": {
        "en": u"Help",
        "zh": u"帮助",
    },
    "ui_btn_export": {
        "en": u"Export",
        "zh": u"导出",
    },
    "ui_btn_diagnose": {
        "en": u"Diagnose",
        "zh": u"诊断",
    },
    "ui_btn_convert": {
        "en": u"To PBR",
        "zh": u"转 PBR",
    },
    "ui_btn_restore": {
        "en": u"Restore",
        "zh": u"还原材质",
    },
    "ui_btn_reload": {
        "en": u"Reload Modules",
        "zh": u"重载模块",
    },
    "ui_sel_only": {
        "en": u"Selection Only",
        "zh": u"仅导出选中",
    },
    "ui_warn_no_sel": {
        "en": u"Unity_MayaToUnity: Selection Only is on, but nothing is selected.",
        "zh": u"Unity_MayaToUnity: 已勾选仅导出选中，但当前没有选中物体。",
    },
    "ui_about": {
        "en": u"About",
        "zh": u"关于",
    },
    "ui_about_msg": {
        "en": u"Unity MayaToUnity\nMaya 2018 → Unity Prefab exporter.",
        "zh": u"Unity MayaToUnity\nMaya 2018 → Unity Prefab 导出工具。",
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
    """设置语言并可选刷新工具窗口文案。

    rebuild_menu 参数保留兼容；现为刷新 toolbox，不再 deleteUI 主菜单。
    """
    if lang not in ("en", "zh"):
        lang = "en"
    try:
        cmds.optionVar(sv=(OPTION_VAR, lang))
    except Exception:
        pass
    if rebuild_menu:
        def _refresh():
            try:
                import ui as toolbox_ui
                toolbox_ui.refresh_toolbox()
            except Exception:
                try:
                    import Unity_MayaToUnity as pkg
                    pkg.show_toolbox()
                except Exception:
                    pass
        try:
            cmds.evalDeferred(_refresh)
        except Exception:
            _refresh()
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
