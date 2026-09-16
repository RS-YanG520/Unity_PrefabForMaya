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
    Unity_MayaToUnity.install_menu()          # 在主菜单栏加「Unity MayaToUnity」菜单

  或直接调用：
    import Unity_MayaToUnity
    Unity_MayaToUnity.export(r"D:\\UnityProject\\Assets\\GeneratedPrefabs")

  选中对象时只导出选中；不选时导出整个场景。

  调试：
    Unity_MayaToUnity.set_debug(True)   # 开启导出日志（控制台实时打印）
    Unity_MayaToUnity.diagnose()        # dry-run 诊断场景（不导出）
    导出的 <输出目录>/export_report.log 始终写入完整日志。

  注意：菜单/弹窗/日志等运行时文案用英文，避免 Maya 中文界面（GBK）显示乱码。
"""
from __future__ import print_function

import maya.cmds as cmds

from exporter import export
import debug


MENU_NAME = "unity_maya_to_unity_menu"
MENU_LABEL = "Unity MayaToUnity"


def _pick_output_dir():
    """弹出目录选择框，返回目录路径或 None。"""
    result = cmds.fileDialog2(dialogStyle=2, fileMode=3, caption="Select Unity export folder")
    return result[0] if result else None


def _export_with_dialog():
    out = _pick_output_dir()
    if not out:
        return
    selection = cmds.ls(selection=True) or None
    generated = export(out, selection=selection, scene_name="MayaToUnity")
    if generated:
        cmds.confirmDialog(
            title="Export Complete",
            message="Exported %d files to:\n%s" % (len(generated), out),
            button=["OK"],
            defaultButton="OK",
            cancelButton="OK",
            dismissString="OK",
        )


def install_menu():
    """在主菜单栏安装「Unity MayaToUnity」菜单。"""
    main = 'MayaWindow'
    if not cmds.window(main, q=True, exists=True):
        cmds.warning("Unity_MayaToUnity: main window 'MayaWindow' not found, cannot install menu.")
        return
    if cmds.menu(MENU_NAME, exists=True):
        cmds.deleteUI(MENU_NAME)
    cmds.menu(MENU_NAME, label=MENU_LABEL, parent=main, tearOff=True)
    cmds.menuItem(label="Export to Unity Project...", command=lambda *_: _export_with_dialog())
    cmds.menuItem(label="Diagnose Scene...", command=lambda *_: diagnose())
    cmds.menuItem(divider=True)
    cmds.menuItem(label="Reload Plugin Modules", command=lambda *_: reload_modules())
    cmds.menuItem(label="Enable Debug Log", checkBox=False, command=_toggle_debug)


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
        ):
            path = getattr(mod, "__file__", "") or ""
            if "Unity_MayaToUnity" in path.replace("\\", "/"):
                doomed.append(k)
    for k in doomed:
        del sys.modules[k]
    # 保留包壳，reload 会重新执行 __init__.py 并拉取新子模块
    import Unity_MayaToUnity as pkg
    reload(pkg)
    pkg.install_menu()
    cmds.warning("Unity_MayaToUnity: modules reloaded.")
    return True


def diagnose():
    """dry-run 诊断当前场景（不导出），结果打印到脚本编辑器并弹窗提示。"""
    selection = cmds.ls(selection=True) or None
    debug.diagnose_scene(selection)
    cmds.confirmDialog(
        title="Scene Diagnosis",
        message="Diagnosis report printed to the Script Editor.",
        button=["OK"],
        defaultButton="OK",
        cancelButton="OK",
        dismissString="OK",
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
