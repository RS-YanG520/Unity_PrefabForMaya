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
"""
from __future__ import print_function

import maya.cmds as cmds
import maya.mel as mel

from exporter import export
import debug


MENU_NAME = "unity_maya_to_unity_menu"
MENU_LABEL = "Unity MayaToUnity"


def _pick_output_dir():
    """弹出目录选择框，返回目录路径或 None。"""
    result = cmds.fileDialog2(dialogStyle=2, fileMode=3, caption="选择 Unity 导出目录")
    return result[0] if result else None


def _export_with_dialog():
    out = _pick_output_dir()
    if not out:
        return
    selection = cmds.ls(selection=True) or None
    generated = export(out, selection=selection, scene_name="MayaToUnity")
    if generated:
        cmds.confirmDialog(
            title="导出完成",
            message="已导出 %d 个文件到：\n%s" % (len(generated), out),
            button=["确定"],
            defaultButton="确定",
            cancelButton="确定",
            dismissString="确定",
        )


def install_menu():
    """在主菜单栏安装「Unity MayaToUnity」菜单。"""
    main = mel.eval('$tmp = $gMainWindow')
    if cmds.menu(MENU_NAME, exists=True):
        cmds.deleteUI(MENU_NAME)
    cmds.menu(MENU_NAME, label=MENU_LABEL, parent=main, tearOff=True)
    cmds.menuItem(label="导出到 Unity 项目...", command=lambda *_: _export_with_dialog())
    cmds.menuItem(label="诊断当前场景...", command=lambda *_: diagnose())
    cmds.menuItem(divider=True)
    cmds.menuItem(label="开启调试日志", checkBox=False, command=_toggle_debug)


def set_debug(enabled):
    """开启 / 关闭调试日志（控制台实时打印）。"""
    debug.set_debug(enabled)


def diagnose():
    """dry-run 诊断当前场景（不导出），结果打印到脚本编辑器并弹窗提示。"""
    selection = cmds.ls(selection=True) or None
    debug.diagnose_scene(selection)
    cmds.confirmDialog(
        title="场景诊断",
        message="诊断报告已打印到脚本编辑器。",
        button=["确定"],
        defaultButton="确定",
        cancelButton="确定",
        dismissString="确定",
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
