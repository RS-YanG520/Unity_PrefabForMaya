# -*- coding: utf-8 -*-
"""Unity MayaToUnity 工具窗口。

布局参考 gs_toolbox：窄竖条、顶栏菜单、滚动区、2 列按钮行、横线分区。
Maya 2018 / PySide2 / Python 2.7。
"""
from __future__ import print_function

import os

import maya.cmds as cmds
import maya.OpenMayaUI as omui

try:
    from PySide2 import QtCore, QtWidgets, QtGui
    from shiboken2 import wrapInstance
except ImportError:
    from PySide import QtCore, QtGui
    from PySide import QtGui as QtWidgets
    from shiboken import wrapInstance

try:
    from maya.app.general.mayaMixin import MayaQWidgetDockableMixin
except Exception:
    MayaQWidgetDockableMixin = object

import debug
import i18n
from constants import to_maya_ui

WINDOW_OBJECT = "UnityMayaToUnityToolbox"
WINDOW_TITLE = "Unity MayaToUnity"
WORKSPACE_CONTROL = WINDOW_OBJECT + "WorkspaceControl"
_EXPORT_DIR_VAR = "mtu_export_dir"
# 接近 GS Toolbox 的窄条宽度
_DEFAULT_WIDTH = 210
_DEFAULT_HEIGHT = 520
_BTN_H = 26
_SIDE_BTN_W = 72

_toolbox = None


def _maya_main_window():
    ptr = omui.MQtUtil.mainWindow()
    if ptr is None:
        return None
    try:
        ptr = long(ptr)
    except NameError:
        ptr = int(ptr)
    return wrapInstance(ptr, QtWidgets.QWidget)


def _delete_workspace_control():
    """删掉残留的 dock WorkspaceControl，避免「对象名称不唯一」。"""
    for name in (WORKSPACE_CONTROL, WINDOW_OBJECT):
        try:
            if cmds.workspaceControl(name, q=True, exists=True):
                cmds.deleteUI(name)
        except Exception:
            pass
        try:
            if cmds.window(name, exists=True):
                cmds.deleteUI(name)
        except Exception:
            pass


def _hline():
    line = QtWidgets.QFrame()
    line.setFrameShape(QtWidgets.QFrame.HLine)
    line.setFrameShadow(QtWidgets.QFrame.Sunken)
    line.setFixedHeight(2)
    line.setStyleSheet("color: #2a2a2a; background: #2a2a2a; border: none;")
    return line


class _InstMarkDelegate(QtWidgets.QStyledItemDelegate):
    """实例化列：画一块固定大小的色块，避免字符方块显得太小。"""

    _W = 22
    _H = 14

    def paint(self, painter, option, index):
        flag = index.data(QtCore.Qt.UserRole + 1)
        if flag is None:
            QtWidgets.QStyledItemDelegate.paint(self, painter, option, index)
            return
        opt = QtWidgets.QStyleOptionViewItem(option)
        self.initStyleOption(opt, index)
        opt.text = ""
        widget = opt.widget
        style = widget.style() if widget is not None else QtWidgets.QApplication.style()
        style.drawControl(
            QtWidgets.QStyle.CE_ItemViewItem, opt, painter, widget)
        color = QtGui.QColor("#3dcf5a" if flag else "#e05555")
        rect = option.rect
        box = QtCore.QRect(0, 0, self._W, self._H)
        box.moveCenter(rect.center())
        painter.save()
        painter.setPen(QtCore.Qt.NoPen)
        painter.setBrush(color)
        painter.drawRect(box)
        painter.restore()


class ToolboxWindow(MayaQWidgetDockableMixin, QtWidgets.QWidget):
    """GS Toolbox 风格窄工具条。"""

    def __init__(self, parent=None):
        if parent is None:
            parent = _maya_main_window()
        super(ToolboxWindow, self).__init__(parent=parent)
        self.setObjectName(WINDOW_OBJECT)
        self.setWindowTitle(WINDOW_TITLE)
        self.setMinimumWidth(240)
        self.setMaximumWidth(560)
        self.resize(340, _DEFAULT_HEIGHT)

        self._build()
        self._apply_style()
        self.retranslate()
        self._sel_job = None
        self._syncing_to_maya = False
        self._syncing_from_maya = False
        self._install_selection_job()

    # ------------------------------------------------------------------ build

    def _build(self):
        root = QtWidgets.QVBoxLayout(self)
        root.setContentsMargins(0, 0, 0, 0)
        root.setSpacing(0)

        # 顶栏菜单（对齐 GS：Options / Help）
        self.menubar = QtWidgets.QMenuBar(self)
        self.menubar.setNativeMenuBar(False)
        self.menu_options = self.menubar.addMenu("Options")
        self.menu_help = self.menubar.addMenu("Help")

        self.act_ue_tex = QtWidgets.QAction(self)
        self.act_ue_tex.triggered.connect(self._on_ue_tex)
        self.menubar.insertAction(
            self.menu_help.menuAction(), self.act_ue_tex)

        self.act_debug = QtWidgets.QAction(self)
        self.act_debug.setCheckable(True)
        self.act_debug.setChecked(bool(debug.DEBUG))
        self.act_debug.toggled.connect(self._on_debug)
        self.menu_options.addAction(self.act_debug)

        self.menu_lang = self.menu_options.addMenu("Language")
        self.act_lang_en = QtWidgets.QAction("English", self)
        self.act_lang_en.setCheckable(True)
        self.act_lang_zh = QtWidgets.QAction(u"中文", self)
        self.act_lang_zh.setCheckable(True)
        self.lang_group = QtWidgets.QActionGroup(self)
        self.lang_group.setExclusive(True)
        self.lang_group.addAction(self.act_lang_en)
        self.lang_group.addAction(self.act_lang_zh)
        self.menu_lang.addAction(self.act_lang_en)
        self.menu_lang.addAction(self.act_lang_zh)
        self.act_lang_en.triggered.connect(lambda: self._set_lang("en"))
        self.act_lang_zh.triggered.connect(lambda: self._set_lang("zh"))

        self.act_reload = QtWidgets.QAction(self)
        self.act_reload.triggered.connect(self._on_reload)
        self.menu_options.addSeparator()
        self.menu_options.addAction(self.act_reload)

        self.act_about = QtWidgets.QAction(self)
        self.act_about.triggered.connect(self._on_about)
        self.menu_help.addAction(self.act_about)

        root.addWidget(self.menubar)

        # 上部：操作区（不滚动，保持紧凑）
        controls = QtWidgets.QWidget()
        self.body_lay = QtWidgets.QVBoxLayout(controls)
        self.body_lay.setContentsMargins(4, 4, 4, 4)
        self.body_lay.setSpacing(3)

        # 路径框 + 右侧浏览；浏览下方为导出
        self.edit_export_path = QtWidgets.QLineEdit()
        self.edit_export_path.setMinimumHeight(_BTN_H)
        self.edit_export_path.setSizePolicy(
            QtWidgets.QSizePolicy.Expanding, QtWidgets.QSizePolicy.Fixed)
        self.btn_browse = self._side_btn(self._on_browse)
        self.btn_export = self._side_btn(self._on_export)

        side_col = QtWidgets.QVBoxLayout()
        side_col.setContentsMargins(0, 0, 0, 0)
        side_col.setSpacing(3)
        side_col.addWidget(self.btn_browse)
        side_col.addWidget(self.btn_export)

        path_row = QtWidgets.QHBoxLayout()
        path_row.setContentsMargins(0, 0, 0, 0)
        path_row.setSpacing(3)
        path_row.addWidget(self.edit_export_path, 1)
        path_row.addLayout(side_col)
        self.body_lay.addLayout(path_row)

        self.export_progress = QtWidgets.QProgressBar()
        self.export_progress.setTextVisible(False)
        self.export_progress.setFixedHeight(10)
        self.export_progress.setRange(0, 1)
        self.export_progress.setValue(0)
        self.export_progress.setStyleSheet(
            "QProgressBar { background: #2a2a2a; border: none; }"
            "QProgressBar::chunk { background: #3b9eff; }"
        )
        self.body_lay.addWidget(self.export_progress)

        self.btn_diagnose = self._btn(self._on_diagnose)
        self.body_lay.addWidget(self.btn_diagnose)

        self.btn_convert = self._btn(self._on_convert)
        self.btn_restore = self._btn(self._on_restore)
        self.body_lay.addLayout(self._row(self.btn_convert, self.btn_restore))

        self._load_export_path()

        self.body_lay.addWidget(_hline())

        self.chk_sel_only = QtWidgets.QCheckBox()
        self.chk_sel_only.setChecked(False)
        self.body_lay.addWidget(self.chk_sel_only)

        self.chk_texdrop = QtWidgets.QCheckBox()
        try:
            import texture_drop
            self.chk_texdrop.setChecked(texture_drop.is_enabled())
        except Exception:
            self.chk_texdrop.setChecked(True)
        self.chk_texdrop.toggled.connect(self._on_texdrop)
        self.body_lay.addWidget(self.chk_texdrop)

        self.chk_tex_siblings = QtWidgets.QCheckBox()
        try:
            import texture_drop
            self.chk_tex_siblings.setChecked(texture_drop.is_auto_siblings())
        except Exception:
            self.chk_tex_siblings.setChecked(True)
        self.chk_tex_siblings.toggled.connect(self._on_tex_siblings)
        self.body_lay.addWidget(self.chk_tex_siblings)

        self.btn_tex_rules = self._btn(self._on_tex_rules)
        self.btn_tex_rules.setMinimumHeight(_BTN_H)
        self.body_lay.addWidget(self.btn_tex_rules)

        self.lbl_status = QtWidgets.QLabel()
        self.lbl_status.setWordWrap(True)
        self.lbl_status.setStyleSheet(
            "color: #9a9a9a; font-size: 10px; padding: 2px;")
        self.body_lay.addWidget(self.lbl_status)

        root.addWidget(controls)

        root.addWidget(_hline())

        # 下部：诊断汇总 + 模型列表
        self.lbl_diag_title = QtWidgets.QLabel()
        self.lbl_diag_title.setStyleSheet(
            "color: #c8c8c8; font-size: 11px; padding: 2px 4px;")
        root.addWidget(self.lbl_diag_title)

        self.lbl_diag_summary = QtWidgets.QLabel()
        self.lbl_diag_summary.setWordWrap(True)
        self.lbl_diag_summary.setStyleSheet(
            "color: #d0d0d0; font-size: 11px; padding: 2px 4px;")
        root.addWidget(self.lbl_diag_summary)

        self.btn_size_map = self._btn(self._on_size_map)
        root.addWidget(self.btn_size_map)

        self.tbl_meshes = QtWidgets.QTableWidget(0, 7)
        self.tbl_meshes.setHorizontalHeaderLabels(
            ["Name", "Faces", "Instance", "Size", "Base", "UV0", "UV1"])
        self.tbl_meshes.setEditTriggers(
            QtWidgets.QAbstractItemView.NoEditTriggers)
        self.tbl_meshes.setSelectionBehavior(
            QtWidgets.QAbstractItemView.SelectRows)
        # ExtendedSelection：Ctrl 多选、Shift 范围选
        self.tbl_meshes.setSelectionMode(
            QtWidgets.QAbstractItemView.ExtendedSelection)
        self.tbl_meshes.setAlternatingRowColors(True)
        self.tbl_meshes.verticalHeader().setVisible(False)
        self.tbl_meshes.setShowGrid(True)
        self.tbl_meshes.setSortingEnabled(False)
        self.tbl_meshes.setMinimumHeight(140)
        header = self.tbl_meshes.horizontalHeader()
        header.setStretchLastSection(False)
        header.setSectionsClickable(True)
        header.setSortIndicatorShown(True)
        try:
            interactive = QtWidgets.QHeaderView.Interactive
            for c in range(7):
                header.setSectionResizeMode(c, interactive)
        except Exception:
            try:
                interactive = QtWidgets.QHeaderView.Interactive
                for c in range(7):
                    header.setResizeMode(c, interactive)
            except Exception:
                pass
        self.tbl_meshes.setColumnWidth(0, 90)
        self.tbl_meshes.setColumnWidth(1, 44)
        self.tbl_meshes.setColumnWidth(2, 56)
        self.tbl_meshes.setColumnWidth(3, 64)
        self.tbl_meshes.setColumnWidth(4, 90)
        self.tbl_meshes.setColumnWidth(5, 64)
        self.tbl_meshes.setColumnWidth(6, 64)
        self._mesh_rows = []
        self._light_rows = []
        self._camera_rows = []
        self._bulb_icon = None
        self._cam_icon = None
        self._sort_col = 0
        self._sort_asc = True
        self.tbl_meshes.setItemDelegateForColumn(
            2, _InstMarkDelegate(self.tbl_meshes))
        vheader = self.tbl_meshes.verticalHeader()
        vheader.setDefaultSectionSize(26)
        try:
            vheader.setSectionResizeMode(QtWidgets.QHeaderView.Fixed)
        except Exception:
            try:
                vheader.setResizeMode(QtWidgets.QHeaderView.Fixed)
            except Exception:
                pass
        self.tbl_meshes.setContextMenuPolicy(QtCore.Qt.CustomContextMenu)
        self.tbl_meshes.customContextMenuRequested.connect(
            self._on_mesh_context_menu)
        header.sectionClicked.connect(self._on_mesh_header_clicked)
        self.tbl_meshes.cellClicked.connect(self._on_mesh_cell_clicked)
        self.tbl_meshes.cellDoubleClicked.connect(
            self._on_mesh_cell_double_clicked)
        root.addWidget(self.tbl_meshes, 1)

    def _btn(self, slot):
        btn = QtWidgets.QPushButton()
        btn.setMinimumHeight(_BTN_H)
        btn.setSizePolicy(
            QtWidgets.QSizePolicy.Expanding, QtWidgets.QSizePolicy.Fixed)
        btn.setCursor(QtCore.Qt.PointingHandCursor)
        btn.clicked.connect(slot)
        return btn

    def _side_btn(self, slot):
        """路径右侧窄按钮（浏览 / 导出）。"""
        btn = QtWidgets.QPushButton()
        btn.setMinimumHeight(_BTN_H)
        btn.setFixedWidth(_SIDE_BTN_W)
        btn.setSizePolicy(
            QtWidgets.QSizePolicy.Fixed, QtWidgets.QSizePolicy.Fixed)
        btn.setCursor(QtCore.Qt.PointingHandCursor)
        btn.clicked.connect(slot)
        return btn

    def _row(self, *widgets):
        row = QtWidgets.QHBoxLayout()
        row.setContentsMargins(0, 0, 0, 0)
        row.setSpacing(3)
        for w in widgets:
            row.addWidget(w, 1)
        return row

    def _load_export_path(self):
        try:
            if cmds.optionVar(exists=_EXPORT_DIR_VAR):
                path = cmds.optionVar(q=_EXPORT_DIR_VAR) or ""
                if path:
                    self.edit_export_path.setText(path)
        except Exception:
            pass

    def _save_export_path(self, path):
        if not path:
            return
        try:
            cmds.optionVar(sv=(_EXPORT_DIR_VAR, path))
        except Exception:
            pass

    def _current_export_path(self):
        return (self.edit_export_path.text() or "").strip()

    def _apply_style(self):
        # 贴近 Maya / GS Toolbox 深灰扁平风
        self.setStyleSheet(
            """
            QWidget#%s {
                background-color: #444444;
                color: #e6e6e6;
            }
            QMenuBar {
                background-color: #3e3e3e;
                color: #dddddd;
                border-bottom: 1px solid #2b2b2b;
                padding: 1px;
            }
            QMenuBar::item {
                padding: 3px 8px;
                background: transparent;
            }
            QMenuBar::item:selected {
                background: #5a5a5a;
            }
            QMenu {
                background-color: #3e3e3e;
                color: #e6e6e6;
                border: 1px solid #2b2b2b;
            }
            QMenu::item:selected {
                background: #5285a6;
            }
            QPushButton {
                background-color: #5d5d5d;
                border: 1px solid #2f2f2f;
                border-radius: 2px;
                padding: 2px 4px;
                color: #f2f2f2;
                font-size: 11px;
            }
            QPushButton:hover {
                background-color: #6a6a6a;
            }
            QPushButton:pressed {
                background-color: #5285a6;
            }
            QCheckBox {
                color: #e6e6e6;
                spacing: 6px;
                font-size: 11px;
            }
            QLabel {
                color: #e0e0e0;
                font-size: 11px;
            }
            QLineEdit {
                background-color: #4a4a4a;
                border: 1px solid #2f2f2f;
                border-radius: 2px;
                color: #f0f0f0;
                padding: 2px 4px;
                font-size: 11px;
                selection-background-color: #5285a6;
            }
            QComboBox, QDoubleSpinBox {
                background-color: #4a4a4a;
                border: 1px solid #2f2f2f;
                border-radius: 2px;
                color: #f0f0f0;
                padding: 1px 4px;
                font-size: 11px;
            }
            QComboBox::drop-down {
                border: none;
                width: 16px;
            }
            QSlider::groove:horizontal {
                height: 4px;
                background: #2f2f2f;
                border-radius: 2px;
            }
            QSlider::handle:horizontal {
                width: 10px;
                margin: -4px 0;
                background: #b0b0b0;
                border-radius: 2px;
            }
            QScrollArea {
                background: transparent;
                border: none;
            }
            QTextEdit {
                background-color: #2f2f2f;
                color: #d8d8d8;
                border: 1px solid #2a2a2a;
                border-radius: 2px;
                padding: 4px;
                font-size: 10px;
            }
            QTableWidget {
                background-color: #2f2f2f;
                alternate-background-color: #353535;
                color: #d8d8d8;
                border: 1px solid #2a2a2a;
                border-radius: 2px;
                gridline-color: #3a3a3a;
                font-size: 10px;
            }
            QHeaderView::section {
                background-color: #3e3e3e;
                color: #d0d0d0;
                border: none;
                border-right: 1px solid #2a2a2a;
                border-bottom: 1px solid #2a2a2a;
                padding: 3px;
                font-size: 10px;
            }
            QTableWidget::item:selected {
                background-color: #5285a6;
            }
            QScrollBar:vertical {
                background: #3a3a3a;
                width: 10px;
                margin: 0;
            }
            QScrollBar::handle:vertical {
                background: #5a5a5a;
                min-height: 24px;
                border-radius: 2px;
            }
            QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
                height: 0;
            }
            QScrollBar:horizontal {
                background: #3a3a3a;
                height: 10px;
                margin: 0;
            }
            QScrollBar::handle:horizontal {
                background: #5a5a5a;
                min-width: 24px;
                border-radius: 2px;
            }
            QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {
                width: 0;
            }
            """ % WINDOW_OBJECT
        )

    # -------------------------------------------------------------- i18n

    def retranslate(self):
        self.setWindowTitle(i18n.tr("menu_root"))
        self.menu_options.setTitle(i18n.tr("ui_menu_options"))
        self.menu_help.setTitle(i18n.tr("ui_menu_help"))
        self.act_ue_tex.setText(i18n.tr("ui_ue_tex_menu"))
        self.act_ue_tex.setToolTip(i18n.tr("ui_ue_tex_title"))
        self.menu_lang.setTitle(i18n.tr("menu_language"))

        self.btn_browse.setText(i18n.tr("ui_btn_browse"))
        self.btn_browse.setToolTip(i18n.tr("dlg_export_caption"))
        self.btn_export.setText(i18n.tr("ui_btn_export"))
        self.btn_export.setToolTip(i18n.tr("menu_export"))
        self.edit_export_path.setPlaceholderText(i18n.tr("ui_path_placeholder"))
        self.btn_diagnose.setText(i18n.tr("ui_btn_diagnose"))
        self.btn_diagnose.setToolTip(i18n.tr("menu_diagnose"))
        self.btn_convert.setText(i18n.tr("ui_btn_convert"))
        self.btn_convert.setToolTip(i18n.tr("menu_convert_pbr"))
        self.btn_restore.setText(i18n.tr("ui_btn_restore"))
        self.btn_restore.setToolTip(i18n.tr("menu_restore_mat"))

        self.chk_sel_only.setText(i18n.tr("ui_sel_only"))
        self.chk_texdrop.setText(i18n.tr("ui_texdrop"))
        self.chk_texdrop.setToolTip(i18n.tr("ui_texdrop_tip"))
        self.chk_tex_siblings.setText(i18n.tr("ui_texdrop_siblings"))
        self.chk_tex_siblings.setToolTip(i18n.tr("ui_texdrop_siblings_tip"))
        self.btn_tex_rules.setText(i18n.tr("ui_texdrop_rules_btn"))
        self.btn_tex_rules.setToolTip(i18n.tr("ui_texdrop_rules_tip"))
        self.lbl_status.setText(i18n.tr("ui_status_hint"))
        self.lbl_diag_title.setText(i18n.tr("ui_diag_title"))
        self.btn_size_map.setText(i18n.tr("ui_btn_size_map"))
        self.btn_size_map.setToolTip(i18n.tr("ui_size_map_tip"))
        size_win = self._size_map_alive()
        if size_win is not None:
            try:
                size_win.retranslate()
            except Exception:
                pass
        if not self.lbl_diag_summary.text():
            self.lbl_diag_summary.setText(i18n.tr("ui_diag_placeholder"))
        self.tbl_meshes.setHorizontalHeaderLabels([
            i18n.tr("ui_col_name"),
            i18n.tr("ui_col_faces"),
            i18n.tr("ui_col_instance"),
            i18n.tr("ui_col_size"),
            i18n.tr("ui_col_base"),
            i18n.tr("ui_col_uv0"),
            i18n.tr("ui_col_uv1"),
        ])
        try:
            self.tbl_meshes.horizontalHeader().setSortIndicator(
                self._sort_col,
                QtCore.Qt.AscendingOrder if self._sort_asc
                else QtCore.Qt.DescendingOrder,
            )
        except Exception:
            pass

        self.act_debug.setText(i18n.tr("menu_debug"))
        self.act_reload.setText(i18n.tr("menu_reload"))
        self.act_about.setText(i18n.tr("ui_about"))
        self.act_lang_en.setText(i18n.tr("lang_en"))
        self.act_lang_zh.setText(i18n.tr("lang_zh"))

        lang = i18n.get_lang()
        self.act_lang_en.setChecked(lang == "en")
        self.act_lang_zh.setChecked(lang == "zh")

        self.act_debug.blockSignals(True)
        self.act_debug.setChecked(bool(debug.DEBUG))
        self.act_debug.blockSignals(False)

        self.chk_texdrop.blockSignals(True)
        try:
            import texture_drop
            self.chk_texdrop.setChecked(texture_drop.is_enabled())
        except Exception:
            pass
        self.chk_texdrop.blockSignals(False)

        self.chk_tex_siblings.blockSignals(True)
        try:
            import texture_drop
            self.chk_tex_siblings.setChecked(texture_drop.is_auto_siblings())
        except Exception:
            pass
        self.chk_tex_siblings.blockSignals(False)

    # -------------------------------------------------------------- actions

    def _defer(self, fn):
        try:
            cmds.evalDeferred(fn)
        except Exception:
            fn()

    def selection_for_export(self):
        """勾选「仅选中」时返回当前选择，否则 None（全场景）。"""
        if self.chk_sel_only.isChecked():
            return cmds.ls(selection=True) or []
        return None

    def _on_browse(self):
        start = self._current_export_path()
        kwargs = {
            "dialogStyle": 2,
            "fileMode": 3,
            "caption": i18n.tr("dlg_export_caption"),
        }
        if start and os.path.isdir(start):
            kwargs["dir"] = start
        result = cmds.fileDialog2(**kwargs)
        if not result:
            return
        out = result[0]
        self.edit_export_path.setText(out)
        self._save_export_path(out)

    def _on_export(self):
        out = self._current_export_path()
        if not out:
            cmds.warning(i18n.tr("ui_warn_no_path"))
            return
        self._save_export_path(out)
        sel = self.selection_for_export()

        def _go():
            import Unity_MayaToUnity as pkg
            # 空列表表示勾了仅选中但没选东西
            if sel is not None and len(sel) == 0:
                cmds.warning(i18n.tr("ui_warn_no_sel"))
                return

            def _run():
                self.btn_export.setEnabled(False)
                self._set_export_progress(0, 1)
                try:
                    generated = pkg.export(
                        out, selection=sel, scene_name="MayaToUnity",
                        progress_cb=self._set_export_progress)
                except Exception as exc:
                    self._set_export_progress(0, 1)
                    cmds.warning(
                        "Unity_MayaToUnity export failed: %s" % exc)
                    return
                finally:
                    self.btn_export.setEnabled(True)
                if not generated:
                    self._set_export_progress(0, 1)
                    return
                self._set_export_progress(1, 1)
                msg = i18n.tr(
                    "dlg_export_msg", len(generated), out)
                ok = i18n.tr("dlg_ok")

                def _dlg():
                    cmds.confirmDialog(
                        title=i18n.tr("dlg_export_title"),
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

        self._defer(_go)

    def _set_export_progress(self, done, total):
        total = int(total or 1)
        if total < 1:
            total = 1
        done = int(done or 0)
        if done < 0:
            done = 0
        if done > total:
            done = total
        try:
            self.export_progress.setRange(0, total)
            self.export_progress.setValue(done)
            QtWidgets.QApplication.processEvents()
        except Exception:
            pass

    def set_diagnose_result(self, result):
        """刷新底部汇总与模型/灯光/摄影机列表。"""
        if not isinstance(result, dict):
            self.lbl_diag_summary.setText(i18n.tr("ui_diag_placeholder"))
            self._mesh_rows = []
            self._light_rows = []
            self._camera_rows = []
            self.tbl_meshes.setRowCount(0)
            self._refresh_size_map()
            return

        summary = result.get("summary") or {}
        self._mesh_rows = list(result.get("meshes") or [])
        self._light_rows = self._normalize_light_rows(
            result.get("lights") or [])
        self._camera_rows = self._normalize_camera_rows(
            result.get("cameras") or [])
        self.lbl_diag_summary.setText(
            i18n.tr(
                "ui_diag_summary",
                int(summary.get("model_count", 0)),
                int(summary.get("light_count", len(self._light_rows))),
                int(summary.get("camera_count", len(self._camera_rows))),
                int(summary.get("material_count", 0)),
                int(summary.get("texture_count", 0)),
                int(summary.get("total_faces", 0)),
                summary.get("total_export_size") or "0 B",
            )
        )
        self._sort_col = 0
        self._sort_asc = True
        self._apply_mesh_sort()
        # 诊断刷新后按当前 Maya 选择同步一次表高亮
        self._sync_table_from_maya_selection()
        self._refresh_size_map()

    def _normalize_light_rows(self, lights):
        """诊断灯光 → 表格行数据。"""
        _unity = {0: "Spot", 1: "Directional", 2: "Point"}
        out = []
        for L in lights or []:
            if not L:
                continue
            maya_t = L.get("maya_type") or "light"
            unity_t = _unity.get(L.get("unity_type"), "-")
            inten = L.get("intensity")
            out.append({
                "kind": "light",
                "name": L.get("name") or "",
                "path": L.get("path") or "",
                "maya_type": maya_t,
                "unity_type_name": unity_t,
                "intensity": inten,
                "color": L.get("color"),
                "shadows": L.get("shadows"),
                "faces": 0,
                "size": "-",
                "base_name": unity_t,
                "uv0": "-",
                "uv1": "-",
                "uv_sets": [],
                "shared": False,
                "member_paths": [L.get("path") or ""],
            })
        return out

    def _normalize_camera_rows(self, cameras):
        """诊断摄影机 → 表格行数据。"""
        out = []
        for C in cameras or []:
            if not C:
                continue
            proj = "Ortho" if C.get("orthographic") else "Persp"
            out.append({
                "kind": "camera",
                "name": C.get("name") or "",
                "path": C.get("path") or "",
                "maya_type": "camera",
                "fov": C.get("fov"),
                "near": C.get("near"),
                "far": C.get("far"),
                "orthographic": C.get("orthographic"),
                "faces": 0,
                "size": "-",
                "base_name": proj,
                "uv0": "-",
                "uv1": "-",
                "uv_sets": [],
                "shared": False,
                "member_paths": [C.get("path") or ""],
            })
        return out

    def _light_bulb_icon(self):
        """画一个小灯泡图标（不依赖系统 emoji 字体）。"""
        if self._bulb_icon is not None:
            return self._bulb_icon
        try:
            pm = QtGui.QPixmap(14, 14)
            pm.fill(QtCore.Qt.transparent)
            p = QtGui.QPainter(pm)
            p.setRenderHint(QtGui.QPainter.Antialiasing, True)
            # 灯头
            p.setBrush(QtGui.QColor("#f5c842"))
            p.setPen(QtGui.QPen(QtGui.QColor("#d4a017"), 1))
            p.drawEllipse(3, 1, 8, 8)
            # 灯座
            p.setBrush(QtGui.QColor("#b0b0b0"))
            p.setPen(QtGui.QPen(QtGui.QColor("#888888"), 1))
            p.drawRect(5, 9, 4, 2)
            p.drawRect(6, 11, 2, 2)
            p.end()
            self._bulb_icon = QtGui.QIcon(pm)
        except Exception:
            self._bulb_icon = QtGui.QIcon()
        return self._bulb_icon

    def _camera_icon(self):
        """画一个小摄影机图标。"""
        if self._cam_icon is not None:
            return self._cam_icon
        try:
            pm = QtGui.QPixmap(14, 14)
            pm.fill(QtCore.Qt.transparent)
            p = QtGui.QPainter(pm)
            p.setRenderHint(QtGui.QPainter.Antialiasing, True)
            p.setBrush(QtGui.QColor("#6ec1e0"))
            p.setPen(QtGui.QPen(QtGui.QColor("#3a8fb0"), 1))
            p.drawRoundedRect(1, 4, 8, 6, 1, 1)
            # 镜头
            p.setBrush(QtGui.QColor("#d0eef8"))
            p.drawEllipse(7, 3, 6, 6)
            p.end()
            self._cam_icon = QtGui.QIcon(pm)
        except Exception:
            self._cam_icon = QtGui.QIcon()
        return self._cam_icon

    def _table_entries(self):
        """表格显示顺序：模型 →（分隔）→ 灯光 →（分隔）→ 摄影机。"""
        entries = []
        meshes = self._mesh_rows or []
        lights = self._light_rows or []
        cameras = self._camera_rows or []
        for r in meshes:
            entries.append(r)
        if lights:
            if meshes:
                entries.append({
                    "kind": "separator",
                    "label_key": "ui_diag_lights_sep",
                })
            for r in lights:
                entries.append(r)
        if cameras:
            if meshes or lights:
                entries.append({
                    "kind": "separator",
                    "label_key": "ui_diag_cameras_sep",
                })
            for r in cameras:
                entries.append(r)
        return entries

    def _entry_at_table_row(self, row):
        entries = self._table_entries()
        if 0 <= row < len(entries):
            return entries[row]
        return None

    def _on_mesh_header_clicked(self, col):
        if col == self._sort_col:
            self._sort_asc = not self._sort_asc
        else:
            self._sort_col = col
            self._sort_asc = True
        self._apply_mesh_sort()

    def _apply_mesh_sort(self):
        col = self._sort_col
        asc = self._sort_asc

        def _key(row):
            if col == 0:
                return (row.get("name") or "").lower()
            if col == 1:
                return int(row.get("faces") or 0)
            if col == 2:
                return 1 if row.get("kind") == "instance" else 0
            if col == 3:
                return int(row.get("export_bytes") or 0)
            if col == 4:
                return (row.get("base_name") or "").lower()
            if col == 5:
                return (row.get("uv0") or "").lower()
            if col == 6:
                return (row.get("uv1") or "").lower()
            return 0

        try:
            self._mesh_rows.sort(key=_key, reverse=not asc)
        except Exception:
            pass
        # 灯光 / 摄影机按名称稳定排序
        try:
            self._light_rows.sort(
                key=lambda r: (r.get("name") or "").lower())
        except Exception:
            pass
        try:
            self._camera_rows.sort(
                key=lambda r: (r.get("name") or "").lower())
        except Exception:
            pass
        try:
            self.tbl_meshes.horizontalHeader().setSortIndicator(
                col,
                QtCore.Qt.AscendingOrder if asc else QtCore.Qt.DescendingOrder,
            )
        except Exception:
            pass
        self._fill_mesh_table()

    def _fill_mesh_table(self):
        entries = self._table_entries()
        self.tbl_meshes.setRowCount(0)
        self.tbl_meshes.setRowCount(len(entries))
        yes = i18n.tr("ui_inst_yes")
        no = i18n.tr("ui_inst_no")
        none_base = i18n.tr("ui_base_none")
        link_brush = None
        green_brush = None
        red_brush = None
        sep_brush = None
        try:
            link_brush = QtGui.QBrush(QtGui.QColor("#7ec8ff"))
            green_brush = QtGui.QBrush(QtGui.QColor("#3dcf5a"))
            red_brush = QtGui.QBrush(QtGui.QColor("#e05555"))
            sep_brush = QtGui.QBrush(QtGui.QColor("#3a3a3a"))
        except Exception:
            pass

        for i, row in enumerate(entries):
            kind = row.get("kind")

            # —— 分隔条 ——
            if kind == "separator":
                label = i18n.tr(row.get("label_key") or "ui_diag_lights_sep")
                for col in range(7):
                    text = label if col == 0 else u""
                    item = QtWidgets.QTableWidgetItem(text)
                    item.setFlags(QtCore.Qt.ItemIsEnabled)
                    item.setTextAlignment(
                        QtCore.Qt.AlignHCenter | QtCore.Qt.AlignVCenter)
                    if sep_brush is not None:
                        item.setBackground(sep_brush)
                    item.setData(QtCore.Qt.UserRole + 2, "separator")
                    self.tbl_meshes.setItem(i, col, item)
                try:
                    self.tbl_meshes.setSpan(i, 0, 1, 7)
                except Exception:
                    pass
                continue

            # —— 灯光行 ——
            if kind == "light":
                name = row.get("name") or ""
                name_item = QtWidgets.QTableWidgetItem(name)
                try:
                    name_item.setIcon(self._light_bulb_icon())
                except Exception:
                    name_item.setText(u"[L] " + name)
                name_item.setData(QtCore.Qt.UserRole, row.get("path") or "")
                dash = none_base
                items = [
                    name_item,
                    QtWidgets.QTableWidgetItem(dash),
                    QtWidgets.QTableWidgetItem(dash),
                    QtWidgets.QTableWidgetItem(dash),
                    QtWidgets.QTableWidgetItem(
                        row.get("unity_type_name") or dash),
                    QtWidgets.QTableWidgetItem(dash),
                    QtWidgets.QTableWidgetItem(dash),
                ]
                tip = u"%s\n%s -> %s\nintensity=%s" % (
                    row.get("path") or name,
                    row.get("maya_type") or dash,
                    row.get("unity_type_name") or dash,
                    row.get("intensity")
                    if row.get("intensity") is not None else dash,
                )
                for col, item in enumerate(items):
                    item.setToolTip(tip)
                    item.setData(QtCore.Qt.UserRole + 2, "light")
                    self.tbl_meshes.setItem(i, col, item)
                continue

            # —— 摄影机行 ——
            if kind == "camera":
                name = row.get("name") or ""
                name_item = QtWidgets.QTableWidgetItem(name)
                try:
                    name_item.setIcon(self._camera_icon())
                except Exception:
                    name_item.setText(u"[C] " + name)
                name_item.setData(QtCore.Qt.UserRole, row.get("path") or "")
                dash = none_base
                proj = row.get("base_name") or dash
                items = [
                    name_item,
                    QtWidgets.QTableWidgetItem(dash),
                    QtWidgets.QTableWidgetItem(dash),
                    QtWidgets.QTableWidgetItem(dash),
                    QtWidgets.QTableWidgetItem(proj),
                    QtWidgets.QTableWidgetItem(dash),
                    QtWidgets.QTableWidgetItem(dash),
                ]
                tip = u"%s\n%s  fov=%.1f  near=%.3f  far=%.3f" % (
                    row.get("path") or name,
                    proj,
                    float(row.get("fov") or 0),
                    float(row.get("near") or 0),
                    float(row.get("far") or 0),
                )
                for col, item in enumerate(items):
                    item.setToolTip(tip)
                    item.setData(QtCore.Qt.UserRole + 2, "camera")
                    self.tbl_meshes.setItem(i, col, item)
                continue

            # —— 模型行 ——
            name = row.get("name") or ""
            faces = int(row.get("faces") or 0)
            is_inst = row.get("kind") == "instance"
            inst_txt = yes if is_inst else no
            size = row.get("size") or ""
            if row.get("shared"):
                size = i18n.tr("ui_size_shared")
            base_name = row.get("base_name") or ""
            if not is_inst or not base_name:
                base_name = none_base
            inst_item = QtWidgets.QTableWidgetItem(u"")
            inst_item.setTextAlignment(
                QtCore.Qt.AlignHCenter | QtCore.Qt.AlignVCenter)
            if is_inst and green_brush is not None:
                inst_item.setForeground(green_brush)
            elif (not is_inst) and red_brush is not None:
                inst_item.setForeground(red_brush)
            inst_item.setData(QtCore.Qt.UserRole + 1, 1 if is_inst else 0)
            uv0 = row.get("uv0") or none_base
            uv1 = row.get("uv1") or none_base
            items = [
                QtWidgets.QTableWidgetItem(name),
                QtWidgets.QTableWidgetItem(str(faces)),
                inst_item,
                QtWidgets.QTableWidgetItem(size),
                QtWidgets.QTableWidgetItem(base_name),
                QtWidgets.QTableWidgetItem(uv0),
                QtWidgets.QTableWidgetItem(uv1),
            ]
            items[1].setTextAlignment(
                QtCore.Qt.AlignRight | QtCore.Qt.AlignVCenter)
            items[3].setTextAlignment(
                QtCore.Qt.AlignRight | QtCore.Qt.AlignVCenter)
            items[0].setData(QtCore.Qt.UserRole, row.get("path") or "")
            items[4].setData(
                QtCore.Qt.UserRole, list(row.get("member_paths") or []))
            if is_inst and link_brush is not None:
                items[4].setForeground(link_brush)
            tip = row.get("path") or name
            if is_inst and row.get("prefab"):
                tip = u"%s\n%s: %s" % (
                    tip, i18n.tr("ui_col_base"), row.get("prefab"))
            uv_all = row.get("uv_sets") or []
            tip = u"%s\n%s: %s\n%s: %s\n%s: %s | %s: %s\nUV sets: %s (verts=%s)" % (
                tip,
                i18n.tr("ui_col_instance"),
                inst_txt,
                i18n.tr("ui_col_size"),
                size,
                i18n.tr("ui_col_uv0"),
                uv0,
                i18n.tr("ui_col_uv1"),
                uv1,
                u", ".join(uv_all) if uv_all else u"-",
                row.get("verts", 0),
            )
            for col, item in enumerate(items):
                item.setToolTip(tip)
                item.setData(QtCore.Qt.UserRole + 2, "mesh")
                self.tbl_meshes.setItem(i, col, item)
        try:
            self.tbl_meshes.verticalHeader().setDefaultSectionSize(26)
        except Exception:
            pass

    def _selected_table_rows(self):
        """当前表格选中的行号（去重排序，跳过分隔条）。"""
        rows = set()
        try:
            for idx in self.tbl_meshes.selectedIndexes():
                r = idx.row()
                entry = self._entry_at_table_row(r)
                if entry and entry.get("kind") != "separator":
                    rows.add(r)
        except Exception:
            pass
        return sorted(rows)

    def _selected_data_rows(self):
        """当前表格选中的诊断数据行（模型或灯光）。"""
        out = []
        for r in self._selected_table_rows():
            entry = self._entry_at_table_row(r)
            if entry and entry.get("kind") != "separator":
                out.append(entry)
        return out

    def _sync_maya_from_table_selection(self):
        """按表格多选同步 Maya 选择。"""
        paths = []
        seen = set()
        for row in self._selected_data_rows():
            p = row.get("path") or ""
            if p and p not in seen and cmds.objExists(p):
                seen.add(p)
                paths.append(p)
        if not paths:
            return
        if len(paths) == 1:
            self._select_mesh_in_maya(paths[0])
        else:
            self._select_meshes_in_maya(paths)

    def _install_selection_job(self):
        """监听 Maya 选择变化，同步诊断表高亮。"""
        self._kill_selection_job()
        try:
            self._sel_job = cmds.scriptJob(
                event=["SelectionChanged", self._on_maya_selection_changed],
                compressUndo=True,
            )
        except Exception:
            self._sel_job = None

    def _kill_selection_job(self):
        jid = getattr(self, "_sel_job", None)
        if not jid:
            self._sel_job = None
            return
        try:
            if cmds.scriptJob(exists=jid):
                cmds.scriptJob(kill=jid, force=True)
        except Exception:
            pass
        self._sel_job = None

    def _on_maya_selection_changed(self, *_args):
        if getattr(self, "_syncing_to_maya", False):
            return
        if not (self._mesh_rows or self._light_rows or self._camera_rows):
            return
        # 延后一帧，避免与其它 UI 刷新打架
        def _go():
            if getattr(self, "_syncing_to_maya", False):
                return
            self._sync_table_from_maya_selection()
        try:
            cmds.evalDeferred(_go)
        except Exception:
            _go()

    def _maya_selected_transforms(self):
        """当前 Maya 选中对应的 transform long path 集合。"""
        paths = set()
        try:
            sel = cmds.ls(selection=True, long=True) or []
        except Exception:
            return paths
        for s in sel:
            try:
                ntype = cmds.nodeType(s)
            except Exception:
                continue
            if ntype == "transform":
                paths.add(s)
                continue
            # shape / 组件：取父 transform
            try:
                parents = cmds.listRelatives(
                    s, parent=True, fullPath=True) or []
            except Exception:
                parents = []
            for p in parents:
                if cmds.nodeType(p) == "transform":
                    paths.add(p)
        return paths

    def _entry_matches_paths(self, entry, path_set):
        """行是否对应当前 Maya 选中（只匹配本行 path，不扩展到整组实例）。"""
        if not entry or not path_set:
            return False
        path = entry.get("path") or ""
        if path and path in path_set:
            return True
        # 短名回退（跨 namespace / 路径不一致时）
        if path:
            leaf = path.split("|")[-1]
            leaves = set(p.split("|")[-1] for p in path_set)
            if leaf in leaves:
                return True
        return False

    def _sync_table_from_maya_selection(self):
        """大纲 / 视口选中 → 诊断表同步高亮。"""
        if getattr(self, "_syncing_from_maya", False):
            return
        path_set = self._maya_selected_transforms()
        entries = self._table_entries()
        if not entries:
            self._sync_size_map_highlight(path_set)
            return

        self._syncing_from_maya = True
        try:
            self.tbl_meshes.clearSelection()
            first_item = None
            for i, entry in enumerate(entries):
                if entry.get("kind") == "separator":
                    continue
                if not self._entry_matches_paths(entry, path_set):
                    continue
                for c in range(self.tbl_meshes.columnCount()):
                    it = self.tbl_meshes.item(i, c)
                    if it is not None:
                        it.setSelected(True)
                        if first_item is None and c == 0:
                            first_item = it
            if first_item is not None:
                try:
                    self.tbl_meshes.scrollToItem(
                        first_item,
                        QtWidgets.QAbstractItemView.EnsureVisible,
                    )
                except Exception:
                    pass
        finally:
            self._syncing_from_maya = False
        self._sync_size_map_highlight(path_set)

    def closeEvent(self, event):
        try:
            self._kill_selection_job()
        except Exception:
            pass
        try:
            win = self._size_map_alive()
            if win is not None:
                win.close()
        except Exception:
            pass
        try:
            ue = getattr(self, "_ue_tex_win", None)
            if ue is not None:
                ue.close()
        except Exception:
            pass
        try:
            super(ToolboxWindow, self).closeEvent(event)
        except Exception:
            if event is not None:
                event.accept()

    def _size_map_alive(self):
        win = getattr(self, "_size_map", None)
        if win is None:
            return None
        try:
            win.objectName()
        except Exception:
            self._size_map = None
            return None
        return win

    def _refresh_size_map(self):
        win = self._size_map_alive()
        if win is None:
            return
        try:
            win.set_mesh_rows(self._mesh_rows)
        except Exception:
            pass

    def _sync_size_map_highlight(self, paths):
        win = self._size_map_alive()
        if win is None:
            return
        try:
            win.set_highlight(paths)
        except Exception:
            pass

    def _on_ue_tex(self):
        """打开 UE → Unity 贴图转换窗口。"""
        import ue_tex_convert
        win = getattr(self, "_ue_tex_win", None)
        if win is not None:
            try:
                win.objectName()
            except Exception:
                win = None
                self._ue_tex_win = None
        if win is None:
            win = ue_tex_convert.UeTexWindow(_maya_main_window())
            self._ue_tex_win = win
        try:
            win.retranslate()
        except Exception:
            pass
        win.show()
        try:
            win.raise_()
            win.activateWindow()
        except Exception:
            pass

    def _on_size_map(self):
        """打开场景大小图。还没诊断时先诊断。"""
        def _go():
            if not self._mesh_rows:
                import Unity_MayaToUnity as pkg
                result = pkg.diagnose(show_dialog=False)
                self.set_diagnose_result(result)
            import size_map
            win = self._size_map_alive()
            if win is None:
                win = size_map.SizeMapWindow(_maya_main_window())
                win.set_pick_handler(self._on_size_map_pick)
                self._size_map = win
            try:
                win.retranslate()
            except Exception:
                pass
            win.set_mesh_rows(self._mesh_rows)
            win.show()
            try:
                win.raise_()
                win.activateWindow()
            except Exception:
                pass
            self._sync_size_map_highlight(self._maya_selected_transforms())
        self._defer(_go)

    def _on_size_map_pick(self, paths):
        paths = [p for p in (paths or []) if p and cmds.objExists(p)]
        if not paths:
            cmds.warning(i18n.tr("ui_warn_mesh_missing"))
            return
        if len(paths) == 1:
            self._select_mesh_in_maya(paths[0])
        else:
            self._select_meshes_in_maya(paths)
        self._sync_table_from_maya_selection()

    def _on_mesh_cell_clicked(self, row, col):
        entry = self._entry_at_table_row(row)
        if not entry or entry.get("kind") == "separator":
            return
        # 点名称：只选当前行对应模型（Ctrl/Shift 多选时按表选同步）
        if col == 0:
            mods = QtWidgets.QApplication.keyboardModifiers()
            multi = bool(
                mods & (QtCore.Qt.ControlModifier | QtCore.Qt.ShiftModifier))
            if multi:
                self._sync_maya_from_table_selection()
            else:
                path = entry.get("path") or ""
                if path:
                    self._select_mesh_in_maya(path)
            return
        # 点基础模型名：选中该实例组全部成员（仅模型）
        if col == 4 and entry.get("kind") == "instance":
            item = self.tbl_meshes.item(row, 4)
            if item is None:
                return
            paths = item.data(QtCore.Qt.UserRole) or []
            if not paths:
                paths = list(entry.get("member_paths") or [])
            if not paths:
                return
            self._select_meshes_in_maya(paths)
            return

    def _on_mesh_cell_double_clicked(self, row, col):
        """双击 UV0 / UV1：重命名 UV；双击基础模型：重命名导出名。"""
        entry = self._entry_at_table_row(row)
        if not entry or entry.get("kind") in ("light", "camera", "separator"):
            return
        if col == 4 and entry.get("kind") == "instance":
            self._prompt_rename_inst_base([entry])
            return
        if col not in (5, 6):
            return
        uv_index = 0 if col == 5 else 1
        self._prompt_rename_uv([entry], uv_index)

    def _on_mesh_context_menu(self, pos):
        """右键：定位导出文件 / 重命名基础模型 / 批量重命名 UV0·UV1。"""
        index = self.tbl_meshes.indexAt(pos)
        if not index.isValid():
            return
        row = index.row()
        col = index.column()
        entry = self._entry_at_table_row(row)
        if not entry or entry.get("kind") == "separator":
            return
        # 灯光 / 摄影机行：暂无定位/UV 菜单
        if entry.get("kind") in ("light", "camera"):
            return

        # 右键落在未选中行上：改为只选该行
        item = self.tbl_meshes.item(row, 0)
        if item is not None and not item.isSelected():
            self.tbl_meshes.clearSelection()
            self.tbl_meshes.selectRow(row)

        data_rows = [
            r for r in self._selected_data_rows()
            if r.get("kind") not in ("light", "camera", "separator")
        ]
        if not data_rows:
            data_rows = [entry]

        n = len(data_rows)
        menu = QtWidgets.QMenu(self.tbl_meshes)

        act_locate = None
        if col == 0:
            label = i18n.tr("ui_ctx_locate")
            if n > 1:
                label = i18n.tr("ui_ctx_locate_multi", n)
            act_locate = menu.addAction(label)

        # 基础模型列：重命名导出 Prefab/FBX 名
        act_base = None
        inst_rows = [r for r in data_rows if r.get("kind") == "instance"]
        if col == 4 and inst_rows:
            groups = self._unique_inst_groups(inst_rows)
            if len(groups) > 1:
                act_base = menu.addAction(
                    i18n.tr("ui_ctx_rename_base_multi", len(groups)))
            else:
                act_base = menu.addAction(i18n.tr("ui_ctx_rename_base"))

        show_uv = col in (0, 5, 6)
        act_uv0 = None
        act_uv1 = None
        if show_uv:
            if col == 0 and act_locate is not None:
                menu.addSeparator()
            order = (1, 0) if col == 6 else (0, 1)
            for uv_i in order:
                if n > 1:
                    key = ("ui_ctx_rename_uv0_multi" if uv_i == 0
                           else "ui_ctx_rename_uv1_multi")
                    act = menu.addAction(i18n.tr(key, n))
                else:
                    key = ("ui_ctx_rename_uv0" if uv_i == 0
                           else "ui_ctx_rename_uv1")
                    act = menu.addAction(i18n.tr(key))
                if uv_i == 0:
                    act_uv0 = act
                else:
                    act_uv1 = act

        if menu.isEmpty():
            return

        chosen = menu.exec_(self.tbl_meshes.viewport().mapToGlobal(pos))
        if chosen is None:
            return
        if act_locate is not None and chosen == act_locate:
            self._locate_exported_assets(data_rows)
        elif act_base is not None and chosen == act_base:
            self._prompt_rename_inst_base(inst_rows)
        elif act_uv0 is not None and chosen == act_uv0:
            self._prompt_rename_uv(data_rows, 0)
        elif act_uv1 is not None and chosen == act_uv1:
            self._prompt_rename_uv(data_rows, 1)

    def _unique_inst_groups(self, data_rows):
        """按共享 shape / base_path 去重实例组，每组保留一行代表。"""
        out = []
        seen = set()
        for r in data_rows or []:
            if not r or r.get("kind") != "instance":
                continue
            key = r.get("shape") or r.get("base_path") or r.get("prefab") or ""
            if not key:
                key = tuple(sorted(r.get("member_paths") or []))
            if key in seen:
                continue
            seen.add(key)
            out.append(r)
        return out

    def _prompt_rename_inst_base(self, data_rows):
        """弹出输入框，重命名实例组将要导出的 Prefab/FBX 名。"""
        groups = self._unique_inst_groups(data_rows)
        if not groups:
            return

        current = ""
        for r in groups:
            current = r.get("prefab") or r.get("base_name") or ""
            if current:
                break

        title = i18n.tr("ui_base_rename_title")
        label = i18n.tr("ui_base_rename_label", len(groups))
        new_name, ok = QtWidgets.QInputDialog.getText(
            self, title, label, QtWidgets.QLineEdit.Normal, current or "")
        if not ok:
            return
        new_name = (new_name or "").strip()
        if not new_name:
            cmds.warning(i18n.tr("ui_warn_base_empty"))
            return
        for c in ('\\', '/', ':', '*', '?', '"', '<', '>', '|', ' '):
            if c in new_name:
                cmds.warning(i18n.tr("ui_warn_base_invalid", new_name))
                return
        if new_name in (".", ".."):
            cmds.warning(i18n.tr("ui_warn_base_invalid", new_name))
            return

        def _go():
            ok_n, fail_n = self._rename_inst_base_on_rows(groups, new_name)
            self._refresh_base_columns()
            if fail_n:
                cmds.warning(i18n.tr(
                    "ui_warn_base_rename_fail", ok_n, fail_n))
            elif ok_n:
                print(to_maya_ui(i18n.tr(
                    "ui_base_rename_ok", new_name, ok_n)))

        self._defer(_go)

    def _rename_inst_base_on_rows(self, groups, new_name):
        """把导出名写到各组共享 mesh shape，返回 (ok, fail)。"""
        from instance_detector import set_prefab_name_override

        ok_n = 0
        fail_n = 0
        for row in groups or []:
            shape = row.get("shape") or ""
            if not shape or not cmds.objExists(shape):
                # 回退：用当前行 transform 下的 mesh shape
                path = row.get("path") or row.get("base_path") or ""
                shapes = self._mesh_shapes(path) if path else []
                shape = shapes[0] if shapes else ""
            if not shape or not cmds.objExists(shape):
                fail_n += 1
                continue
            if set_prefab_name_override(shape, new_name):
                ok_n += 1
                # 立刻刷新同组所有诊断行（同 shape）
                for mesh_row in (self._mesh_rows or []):
                    if mesh_row.get("kind") != "instance":
                        continue
                    same = (
                        mesh_row.get("shape") == shape
                        or mesh_row.get("shape") == row.get("shape")
                    )
                    if not same and row.get("member_paths"):
                        same = mesh_row.get("path") in (
                            row.get("member_paths") or [])
                    if same:
                        mesh_row["prefab"] = new_name
                        mesh_row["base_name"] = new_name
            else:
                fail_n += 1
        return ok_n, fail_n

    def _refresh_base_columns(self):
        """刷新表格中的基础模型列，保持选中。"""
        selected = set(self._selected_table_rows())
        self._fill_mesh_table()
        self.tbl_meshes.clearSelection()
        for r in selected:
            if not (0 <= r < self.tbl_meshes.rowCount()):
                continue
            for c in range(self.tbl_meshes.columnCount()):
                it = self.tbl_meshes.item(r, c)
                if it is not None:
                    it.setSelected(True)

    def _uv_name_from_row(self, data_row, uv_index):
        if not data_row:
            return ""
        key = "uv0" if uv_index == 0 else "uv1"
        name = data_row.get(key) or ""
        if name in ("", "-", u"-"):
            return ""
        return name

    def _mesh_shapes(self, transform):
        shapes = cmds.listRelatives(
            transform, shapes=True, noIntermediate=True, fullPath=True) or []
        return [s for s in shapes if cmds.nodeType(s) == "mesh"]

    def _prompt_rename_uv(self, data_rows, uv_index):
        """弹出输入框，重命名选中模型的 UV0 或 UV1。

        若目标为 UV1 且尚不存在：先从 UV0 复制出新通道，再进入改名。
        """
        data_rows = [r for r in (data_rows or []) if r]
        if not data_rows:
            return

        slot = "UV0" if uv_index == 0 else "UV1"

        def _show_rename_dialog():
            current = ""
            for r in data_rows:
                current = self._uv_name_from_row(r, uv_index)
                if current:
                    break
            if not current:
                cmds.warning(i18n.tr("ui_warn_uv_missing", slot))
                return

            title = i18n.tr("ui_uv_rename_title", slot)
            label = i18n.tr("ui_uv_rename_label", slot, len(data_rows))
            new_name, ok = QtWidgets.QInputDialog.getText(
                self, title, label, QtWidgets.QLineEdit.Normal, current or "")
            if not ok:
                return
            new_name = (new_name or "").strip()
            if not new_name:
                cmds.warning(i18n.tr("ui_warn_uv_empty"))
                return
            if any(c in new_name for c in (" ", "|", ":", '"', "'")):
                cmds.warning(i18n.tr("ui_warn_uv_invalid", new_name))
                return

            def _go():
                ok_n, fail_n = self._rename_uv_on_rows(
                    data_rows, uv_index, new_name)
                self._refresh_uv_columns()
                if fail_n:
                    cmds.warning(i18n.tr(
                        "ui_warn_uv_rename_partial", ok_n, fail_n))
                elif ok_n:
                    print(to_maya_ui(i18n.tr(
                        "ui_uv_rename_ok", slot, new_name, ok_n)))
                else:
                    cmds.warning(i18n.tr("ui_warn_uv_missing", slot))

            self._defer(_go)

        # UV1 缺失：先从 UV0 复制
        if uv_index == 1:
            need = [
                r for r in data_rows
                if not self._uv_name_from_row(r, 1)
            ]
            if need:
                # 没有 UV0 则无法复制
                no_uv0 = [
                    r for r in need
                    if not self._uv_name_from_row(r, 0)
                ]
                if len(no_uv0) == len(need):
                    cmds.warning(i18n.tr("ui_warn_uv_no_source"))
                    return

                def _ensure_then_rename():
                    created = self._ensure_uv1_from_uv0(need)
                    self._refresh_uv_columns()
                    if created:
                        print(to_maya_ui(i18n.tr("ui_uv_copied", created)))
                    _show_rename_dialog()

                self._defer(_ensure_then_rename)
                return

        _show_rename_dialog()

    def _ensure_uv1_from_uv0(self, data_rows):
        """对缺少 UV1 的模型，从 UV0 复制出新 UV Set。返回成功创建数。"""
        created = 0
        done_shapes = set()
        for row in data_rows or []:
            if self._uv_name_from_row(row, 1):
                continue
            uv0 = self._uv_name_from_row(row, 0)
            if not uv0:
                continue
            path = row.get("path") or ""
            if not path or not cmds.objExists(path):
                continue
            shapes = self._mesh_shapes(path)
            if not shapes:
                continue

            row_created = None
            for sh in shapes:
                sh_long = (cmds.ls(sh, long=True) or [sh])[0]
                if sh_long in done_shapes:
                    # 共享 shape：沿用已创建的第二套 UV 名
                    try:
                        sets = cmds.polyUVSet(
                            sh_long, query=True, allUVSets=True) or []
                        if len(sets) > 1:
                            row_created = sets[1]
                    except Exception:
                        pass
                    continue
                try:
                    sets = cmds.polyUVSet(
                        sh_long, query=True, allUVSets=True) or []
                    sets = [str(s) for s in sets if s]
                    if len(sets) >= 2:
                        # 场景里已有 UV1，只同步到行数据
                        row_created = sets[1]
                        done_shapes.add(sh_long)
                        continue
                    if uv0 not in sets:
                        continue
                    # 取一个不冲突的新名
                    new_name = "map2"
                    n = 2
                    while new_name in sets:
                        n += 1
                        new_name = "map%d" % n
                    cmds.polyUVSet(
                        sh_long,
                        copy=True,
                        uvSet=uv0,
                        newUVSet=new_name,
                    )
                    done_shapes.add(sh_long)
                    row_created = new_name
                except Exception as exc:
                    cmds.warning(
                        "Unity_MayaToUnity: copy UV0->UV1 failed (%s): %s"
                        % (row.get("name"), exc))

            if row_created:
                created += 1
                row["uv1"] = row_created
                uvs = list(row.get("uv_sets") or [])
                if not uvs:
                    uvs = [uv0]
                while len(uvs) < 2:
                    uvs.append("")
                uvs[1] = row_created
                row["uv_sets"] = uvs
        return created

    def _rename_uv_on_rows(self, data_rows, uv_index, new_name):
        """对多行执行 polyUVSet 重命名。共享 shape 只改一次。"""
        ok_n = 0
        fail_n = 0
        done_shapes = set()
        for row in data_rows:
            old_name = self._uv_name_from_row(row, uv_index)
            if not old_name:
                fail_n += 1
                continue
            if old_name == new_name:
                ok_n += 1
                continue
            path = row.get("path") or ""
            if not path or not cmds.objExists(path):
                fail_n += 1
                continue
            shapes = self._mesh_shapes(path)
            if not shapes:
                fail_n += 1
                continue

            row_ok = False
            row_err = False
            for sh in shapes:
                sh_long = (cmds.ls(sh, long=True) or [sh])[0]
                if sh_long in done_shapes:
                    row_ok = True
                    continue
                try:
                    sets = cmds.polyUVSet(
                        sh_long, query=True, allUVSets=True) or []
                    if old_name not in sets:
                        if new_name in sets:
                            # 已被共享 shape 的其它行改过
                            row_ok = True
                            done_shapes.add(sh_long)
                        else:
                            row_err = True
                        continue
                    cmds.polyUVSet(
                        sh_long,
                        rename=True,
                        uvSet=old_name,
                        newUVSet=new_name,
                    )
                    done_shapes.add(sh_long)
                    row_ok = True
                except Exception as exc:
                    row_err = True
                    cmds.warning(
                        "Unity_MayaToUnity: UV rename failed (%s): %s"
                        % (row.get("name"), exc))

            if row_ok:
                ok_n += 1
                key = "uv0" if uv_index == 0 else "uv1"
                row[key] = new_name
                uvs = list(row.get("uv_sets") or [])
                while len(uvs) <= uv_index:
                    uvs.append("")
                uvs[uv_index] = new_name
                row["uv_sets"] = uvs
            elif row_err:
                fail_n += 1
            else:
                fail_n += 1
        return ok_n, fail_n

    def _refresh_uv_columns(self):
        """从场景重新读取全部行的 UV 名并刷新表格（保持选中）。"""
        selected = set(self._selected_table_rows())
        for row in (self._mesh_rows or []):
            path = row.get("path") or ""
            if not path or not cmds.objExists(path):
                continue
            try:
                uvs = debug._mesh_uv_set_names(path)
            except Exception:
                uvs = list(row.get("uv_sets") or [])
            row["uv_sets"] = uvs
            row["uv0"] = uvs[0] if len(uvs) > 0 else ""
            row["uv1"] = uvs[1] if len(uvs) > 1 else ""
        self._fill_mesh_table()
        # 恢复多选（selectRow 会冲掉前面的选中）
        self.tbl_meshes.clearSelection()
        for r in selected:
            if not (0 <= r < self.tbl_meshes.rowCount()):
                continue
            for c in range(self.tbl_meshes.columnCount()):
                it = self.tbl_meshes.item(r, c)
                if it is not None:
                    it.setSelected(True)

    def _exported_asset_name(self, data_row):
        """推导导出 FBX/Prefab 文件名（不含扩展名）。"""
        if not data_row:
            return ""
        if data_row.get("kind") == "instance":
            name = data_row.get("prefab") or data_row.get("base_name") or ""
        else:
            name = data_row.get("name") or ""
        return name

    def _resolve_exported_file(self, data_row, out_dir):
        """返回已存在的导出文件路径，或 None。"""
        import os
        asset = self._exported_asset_name(data_row)
        if not asset:
            return None
        for rel in (
            ("Models", asset + ".fbx"),
            ("Prefabs", asset + ".prefab"),
        ):
            p = os.path.normpath(os.path.join(out_dir, rel[0], rel[1]))
            if os.path.isfile(p):
                return p
        return None

    def _to_win_path(self, path):
        """Windows API 用的 unicode 路径。"""
        import os
        path = os.path.abspath(os.path.normpath(path))
        try:
            # Py2
            if not isinstance(path, type(u"")):
                path = path.decode("mbcs")
        except Exception:
            pass
        return path

    def _explorer_select_files(self, file_paths):
        """在资源管理器中打开并选中一个或多个文件。"""
        import os
        import subprocess
        import ctypes
        from ctypes import wintypes

        files = []
        for p in file_paths or []:
            if p and os.path.isfile(p):
                files.append(self._to_win_path(p))
        if not files:
            return False

        if len(files) == 1:
            subprocess.Popen([u"explorer", u"/select,", files[0]])
            return True

        # 多文件：SHOpenFolderAndSelectItems（按目录分组）
        groups = {}
        for f in files:
            d = os.path.dirname(f)
            groups.setdefault(d, []).append(f)

        shell32 = ctypes.windll.shell32
        ole32 = ctypes.windll.ole32
        try:
            ole32.CoInitialize(None)
        except Exception:
            pass

        PIDLIST = ctypes.c_void_p
        ILCreateFromPathW = shell32.ILCreateFromPathW
        ILCreateFromPathW.argtypes = [wintypes.LPCWSTR]
        ILCreateFromPathW.restype = PIDLIST
        ILFree = shell32.ILFree
        ILFree.argtypes = [PIDLIST]
        ILClone = shell32.ILClone
        ILClone.argtypes = [PIDLIST]
        ILClone.restype = PIDLIST
        ILFindChild = shell32.ILFindChild
        ILFindChild.argtypes = [PIDLIST, PIDLIST]
        ILFindChild.restype = PIDLIST
        SHOpen = shell32.SHOpenFolderAndSelectItems
        SHOpen.argtypes = [
            PIDLIST, ctypes.c_uint, ctypes.POINTER(PIDLIST), ctypes.c_ulong]
        SHOpen.restype = ctypes.c_long

        any_ok = False
        for folder, flist in groups.items():
            folder_pidl = ILCreateFromPathW(folder)
            if not folder_pidl:
                # 回退：只打开目录
                try:
                    subprocess.Popen([u"explorer", folder])
                except Exception:
                    pass
                continue

            abs_pidls = []
            rel_pidls = (PIDLIST * len(flist))()
            for i in range(len(flist)):
                rel_pidls[i] = None
            try:
                ready = True
                for i, fpath in enumerate(flist):
                    abs_pidl = ILCreateFromPathW(fpath)
                    abs_pidls.append(abs_pidl)
                    if not abs_pidl:
                        ready = False
                        break
                    rel = ILFindChild(folder_pidl, abs_pidl)
                    if not rel:
                        ready = False
                        break
                    # 克隆相对段（rel 指向 abs_pidl 内部）
                    cloned = ILClone(rel)
                    if not cloned:
                        ready = False
                        break
                    rel_pidls[i] = cloned
                if ready:
                    hr = SHOpen(folder_pidl, len(flist), rel_pidls, 0)
                    if hr == 0:
                        any_ok = True
                    else:
                        subprocess.Popen([u"explorer", folder])
                else:
                    subprocess.Popen([u"explorer", folder])
            finally:
                for i in range(len(flist)):
                    if rel_pidls[i]:
                        try:
                            ILFree(rel_pidls[i])
                        except Exception:
                            pass
                for p in abs_pidls:
                    if p:
                        try:
                            ILFree(p)
                        except Exception:
                            pass
                try:
                    ILFree(folder_pidl)
                except Exception:
                    pass
        return any_ok

    def _locate_exported_assets(self, data_rows):
        """在资源管理器中定位并选中一个或多个导出文件。"""
        import os

        out_dir = self._current_export_path()
        if not out_dir:
            cmds.warning(i18n.tr("ui_warn_no_path"))
            return

        found = []
        missing = []
        seen = set()
        for row in data_rows or []:
            asset = self._exported_asset_name(row)
            path = self._resolve_exported_file(row, out_dir)
            if path:
                key = os.path.normcase(path)
                if key not in seen:
                    seen.add(key)
                    found.append(path)
            else:
                missing.append(asset or "?")

        if not found:
            models_dir = os.path.join(out_dir, "Models")
            if os.path.isdir(models_dir):
                try:
                    os.startfile(models_dir)
                except Exception:
                    pass
            name = missing[0] if missing else "?"
            cmds.warning(i18n.tr("ui_warn_export_missing", name, out_dir))
            return

        if missing:
            cmds.warning(i18n.tr(
                "ui_warn_export_partial",
                len(found), len(missing)))

        def _go():
            try:
                ok = self._explorer_select_files(found)
                if len(found) > 1:
                    print(to_maya_ui(i18n.tr(
                        "ui_locate_multi_ok",
                        len(found),
                        os.path.dirname(found[0]))))
            except Exception as exc:
                cmds.warning(
                    "Unity_MayaToUnity: open explorer failed: %s" % exc)

        self._defer(_go)

    def _locate_exported_asset(self, data_row):
        """兼容单行定位。"""
        self._locate_exported_assets([data_row] if data_row else [])

    def _reveal_in_outliner(self):
        """让各大纲面板展开并滚到当前选中。"""
        for panel in (cmds.getPanel(type="outlinerPanel") or []):
            try:
                cmds.outlinerEditor(panel, edit=True, showSelected=True)
            except Exception:
                pass
        try:
            import maya.mel as mel
            if mel.eval('exists("OutlinerRevealSelected")'):
                mel.eval("OutlinerRevealSelected")
        except Exception:
            pass

    def _select_meshes_in_maya(self, dag_paths):
        """选中一组实例成员。"""
        alive = []
        for p in dag_paths or []:
            if p and cmds.objExists(p):
                alive.append(p)
        if not alive:
            cmds.warning(i18n.tr("ui_warn_mesh_missing"))
            return
        self._syncing_to_maya = True
        try:
            cmds.select(alive, replace=True)
        except Exception as exc:
            cmds.warning(
                "Unity_MayaToUnity: select failed: %s" % exc)
            return
        finally:
            self._syncing_to_maya = False
        self._reveal_in_outliner()

    def _select_mesh_in_maya(self, dag_path):
        """在场景中选中模型，并尽量让 Outliner 滚到选中项。"""
        if not dag_path:
            return
        if not cmds.objExists(dag_path):
            cmds.warning(i18n.tr("ui_warn_mesh_missing"))
            return
        self._syncing_to_maya = True
        try:
            cmds.select(dag_path, replace=True)
        except Exception as exc:
            cmds.warning(
                "Unity_MayaToUnity: select failed: %s" % exc)
            return
        finally:
            self._syncing_to_maya = False
        self._reveal_in_outliner()
        # cmds.select 已会高亮并刷新属性编辑器，勿再调无参的 updateAE

    def _on_diagnose(self):
        def _go():
            import Unity_MayaToUnity as pkg
            result = pkg.diagnose(show_dialog=False)
            try:
                self.set_diagnose_result(result)
            except Exception:
                pass
        self._defer(_go)

    def _on_convert(self):
        def _go():
            import Unity_MayaToUnity as pkg
            pkg.convert_materials_to_pbr()
        self._defer(_go)

    def _on_restore(self):
        def _go():
            import Unity_MayaToUnity as pkg
            pkg.restore_original_materials()
        self._defer(_go)

    def _on_reload(self):
        def _go():
            import Unity_MayaToUnity as pkg
            pkg.reload_modules()
        self._defer(_go)

    def _on_debug(self, checked):
        debug.set_debug(bool(checked))

    def _on_texdrop(self, checked):
        try:
            import texture_drop
            texture_drop.set_enabled(bool(checked))
        except Exception as exc:
            cmds.warning(
                "Unity_MayaToUnity: texture drop toggle failed: %s" % exc)

    def _on_tex_siblings(self, checked):
        try:
            import texture_drop
            texture_drop.set_auto_siblings(bool(checked))
        except Exception as exc:
            cmds.warning(
                "Unity_MayaToUnity: auto-siblings toggle failed: %s" % exc)

    def _on_tex_rules(self):
        try:
            import texture_drop
            texture_drop.show_suffix_rules_dialog(parent=self)
        except Exception as exc:
            cmds.warning(
                "Unity_MayaToUnity: suffix rules dialog failed: %s" % exc)

    def _set_lang(self, code):
        i18n.set_lang(code, rebuild_menu=False)
        self.retranslate()

    def _on_about(self):
        cmds.confirmDialog(
            title=i18n.tr("menu_root"),
            message=i18n.tr("ui_about_msg"),
            button=[i18n.tr("dlg_ok")],
        )


def _remove_legacy_menu():
    name = "unity_maya_to_unity_menu"
    try:
        if cmds.menu(name, exists=True):
            cmds.deleteUI(name)
    except Exception:
        pass


def show_toolbox(force_recreate=False):
    """显示工具窗口（尽量 dockable，对齐 GS Toolbox）。"""
    global _toolbox
    _remove_legacy_menu()

    # 已有实例且未要求重建：只前置/恢复，不重复创建 WorkspaceControl
    if _toolbox is not None and not force_recreate:
        try:
            if cmds.workspaceControl(WORKSPACE_CONTROL, q=True, exists=True):
                cmds.workspaceControl(
                    WORKSPACE_CONTROL, e=True, restore=True, visible=True)
            try:
                _toolbox.show(dockable=True)
            except TypeError:
                _toolbox.show()
            _toolbox.retranslate()
            _toolbox.raise_()
            _toolbox.activateWindow()
            return _toolbox
        except Exception:
            # 句柄失效则走重建
            _toolbox = None

    close_toolbox()
    _delete_workspace_control()

    _toolbox = ToolboxWindow()
    try:
        _toolbox.show(dockable=True)
    except TypeError:
        _toolbox.show()
    try:
        _toolbox.raise_()
        _toolbox.activateWindow()
    except Exception:
        pass
    return _toolbox


def close_toolbox():
    global _toolbox
    if _toolbox is not None:
        try:
            _toolbox._kill_selection_job()
        except Exception:
            pass
        try:
            _toolbox.close()
        except Exception:
            pass
        try:
            _toolbox.deleteLater()
        except Exception:
            pass
        _toolbox = None
    _delete_workspace_control()


def refresh_toolbox():
    global _toolbox
    if _toolbox is None:
        return
    try:
        _toolbox.retranslate()
    except Exception:
        pass
