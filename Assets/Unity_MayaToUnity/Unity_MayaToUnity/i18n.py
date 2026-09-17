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
    "menu_texdrop": {
        "en": u"Drag Texture onto AE Map Button",
        "zh": u"允许拖贴图到属性编辑器图标",
    },
    "ui_texdrop": {
        "en": u"Enable Texture Drag-Drop",
        "zh": u"开启贴图拖放",
    },
    "ui_texdrop_tip": {
        "en": u"Drag onto AE map buttons, material swatch, or a mesh in the viewport.",
        "zh": u"可拖到属性槽图标、材质预览球，或视口中的模型上。",
    },
    "ui_texdrop_siblings": {
        "en": u"Auto-Fill Sibling Maps",
        "zh": u"单张拖材质球/模型时自动补齐同套贴图",
    },
    "ui_texdrop_siblings_tip": {
        "en": u"When enabled, dropping one map on the swatch or a mesh also finds other suffix maps with the same base name in the same folder.",
        "zh": u"勾选后：只拖一张到材质球或视口模型时，会在同目录按后缀自动找齐同套其它贴图。不勾选：只贴这一张。",
    },
    "ui_texdrop_rules_btn": {
        "en": u"Texture Suffix Rules...",
        "zh": u"贴图后缀规则...",
    },
    "ui_texdrop_rules_title": {
        "en": u"Texture Suffix Rules",
        "zh": u"贴图后缀规则",
    },
    "ui_texdrop_rules_tip": {
        "en": u"Filename ending (e.g. _B, _N) maps to a material attribute. Longer suffixes match first. Drop multiple files onto the material swatch to auto-assign.",
        "zh": u"文件名后缀（如 _B、_N）对应材质属性。较长后缀优先匹配。多选贴图拖到材质预览球即可自动分配。",
    },
    "ui_texdrop_col_suffix": {
        "en": u"Suffix",
        "zh": u"后缀",
    },
    "ui_texdrop_col_attr": {
        "en": u"Attribute",
        "zh": u"属性",
    },
    "ui_texdrop_col_label": {
        "en": u"Label",
        "zh": u"说明",
    },
    "ui_texdrop_rules_add": {
        "en": u"Add",
        "zh": u"添加",
    },
    "ui_texdrop_rules_del": {
        "en": u"Remove",
        "zh": u"删除",
    },
    "ui_texdrop_rules_reset": {
        "en": u"Reset Defaults",
        "zh": u"恢复默认",
    },
    "ui_texdrop_ok": {
        "en": u"Unity_MayaToUnity: connected %s -> %s.%s (file: %s)",
        "zh": u"Unity_MayaToUnity: 已连接 %s → %s.%s（file: %s）",
    },
    "ui_texdrop_skip": {
        "en": u"Unity_MayaToUnity: skipped %s (%s)",
        "zh": u"Unity_MayaToUnity: 已跳过 %s（%s）",
    },
    "ui_texdrop_batch_ok": {
        "en": u"Unity_MayaToUnity: assigned %d texture(s) to %s by suffix rules.",
        "zh": u"Unity_MayaToUnity: 已按后缀规则分配 %d 张贴图到 %s。",
    },
    "ui_texdrop_no_target": {
        "en": u"Unity_MayaToUnity: drop on a material map button, the material swatch, or a mesh in the viewport.",
        "zh": u"Unity_MayaToUnity: 请拖到属性槽图标、材质预览球，或视口中的模型上。",
    },
    "ui_texdrop_no_mesh": {
        "en": u"Unity_MayaToUnity: no mesh under the cursor. Drop onto a model in the viewport.",
        "zh": u"Unity_MayaToUnity: 光标下没有模型，请拖到视口中的网格物体上。",
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
        "en": u"Tip: set export path above; check Selection Only to export selection only.",
        "zh": u"提示：上方填写或浏览导出路径后点导出；勾选「仅选中」则只导当前选择。",
    },
    "ui_diag_title": {
        "en": u"Diagnosis",
        "zh": u"诊断信息",
    },
    "ui_diag_placeholder": {
        "en": u"Click Diagnose to refresh summary, meshes, lights and cameras.",
        "zh": u"点击「诊断」刷新汇总、模型、灯光与摄影机列表。",
    },
    "ui_diag_summary": {
        "en": u"Models: %d  |  Lights: %d  |  Cams: %d  |  Mats: %d  |  Tex: %d  |  Faces: %d  |  Est: %s",
        "zh": u"模型: %d  |  灯光: %d  |  相机: %d  |  材质: %d  |  贴图: %d  |  面数: %d  |  预估导出: %s",
    },
    "ui_diag_lights_sep": {
        "en": u"──────── Lights ────────",
        "zh": u"──────── 灯光 ────────",
    },
    "ui_diag_cameras_sep": {
        "en": u"──────── Cameras ────────",
        "zh": u"──────── 摄影机 ────────",
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
    "ui_col_uv0": {
        "en": u"UV0",
        "zh": u"UV0",
    },
    "ui_col_uv1": {
        "en": u"UV1",
        "zh": u"UV1",
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
    "ui_ctx_locate": {
        "en": u"Locate Exported File",
        "zh": u"定位到导出文件",
    },
    "ui_ctx_locate_multi": {
        "en": u"Locate Exported Files (%d)",
        "zh": u"定位到导出文件（%d 个）",
    },
    "ui_ctx_rename_uv0": {
        "en": u"Rename UV0...",
        "zh": u"重命名 UV0...",
    },
    "ui_ctx_rename_uv1": {
        "en": u"Rename UV1...",
        "zh": u"重命名 UV1...",
    },
    "ui_ctx_rename_uv0_multi": {
        "en": u"Rename UV0 (%d selected)...",
        "zh": u"重命名 UV0（已选 %d 个）...",
    },
    "ui_ctx_rename_uv1_multi": {
        "en": u"Rename UV1 (%d selected)...",
        "zh": u"重命名 UV1（已选 %d 个）...",
    },
    "ui_uv_rename_title": {
        "en": u"Rename %s",
        "zh": u"重命名 %s",
    },
    "ui_uv_rename_label": {
        "en": u"New %s name (%d mesh(es)):",
        "zh": u"新的 %s 名称（%d 个模型）：",
    },
    "ui_uv_rename_ok": {
        "en": u"Unity_MayaToUnity: renamed %s -> %s on %d mesh(es)",
        "zh": u"Unity_MayaToUnity: 已将 %s 重命名为 %s（%d 个）",
    },
    "ui_warn_uv_missing": {
        "en": u"Unity_MayaToUnity: no %s channel on the selection.",
        "zh": u"Unity_MayaToUnity: 选中项没有 %s 通道。",
    },
    "ui_warn_uv_no_source": {
        "en": u"Unity_MayaToUnity: cannot create UV1 — UV0 is missing.",
        "zh": u"Unity_MayaToUnity: 无法创建 UV1，缺少 UV0 通道。",
    },
    "ui_uv_copied": {
        "en": u"Unity_MayaToUnity: copied UV0 -> UV1 on %d mesh(es).",
        "zh": u"Unity_MayaToUnity: 已从 UV0 复制出 UV1（%d 个）。",
    },
    "ui_warn_uv_empty": {
        "en": u"Unity_MayaToUnity: UV set name cannot be empty.",
        "zh": u"Unity_MayaToUnity: UV 通道名不能为空。",
    },
    "ui_warn_uv_invalid": {
        "en": u"Unity_MayaToUnity: invalid UV set name: %s",
        "zh": u"Unity_MayaToUnity: UV 通道名不合法：%s",
    },
    "ui_warn_uv_rename_partial": {
        "en": u"Unity_MayaToUnity: UV rename finished with %d ok, %d failed.",
        "zh": u"Unity_MayaToUnity: UV 重命名完成：成功 %d，失败 %d。",
    },
    "ui_locate_multi_ok": {
        "en": u"Unity_MayaToUnity: selected %d exported file(s) in Explorer:\n%s",
        "zh": u"Unity_MayaToUnity: 已在资源管理器中选中 %d 个导出文件：\n%s",
    },
    "ui_warn_export_missing": {
        "en": u"Unity_MayaToUnity: exported file not found for '%s' under:\n%s\nExport first, then try again.",
        "zh": u"Unity_MayaToUnity: 未找到 '%s' 的导出文件（目录：\n%s）\n请先导出后再定位。",
    },
    "ui_warn_export_partial": {
        "en": u"Unity_MayaToUnity: located %d file(s); %d missing (export first).",
        "zh": u"Unity_MayaToUnity: 已定位 %d 个文件，另有 %d 个未找到（请先导出）。",
    },
    "ui_menu_options": {
        "en": u"Options",
        "zh": u"选项",
    },
    "ui_menu_help": {
        "en": u"Help",
        "zh": u"帮助",
    },
    "ui_btn_browse": {
        "en": u"Browse",
        "zh": u"浏览",
    },
    "ui_btn_export": {
        "en": u"Export",
        "zh": u"导出",
    },
    "ui_path_placeholder": {
        "en": u"Export folder path...",
        "zh": u"填写或浏览导出目录...",
    },
    "ui_warn_no_path": {
        "en": u"Unity_MayaToUnity: please enter or browse an export folder path.",
        "zh": u"Unity_MayaToUnity: 请先填写或浏览选择导出目录。",
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
