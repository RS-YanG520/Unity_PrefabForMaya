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

        self.tbl_meshes = QtWidgets.QTableWidget(0, 5)
        self.tbl_meshes.setHorizontalHeaderLabels(
            ["Name", "Faces", "Instance", "Size", "Base"])
        self.tbl_meshes.setEditTriggers(
            QtWidgets.QAbstractItemView.NoEditTriggers)
        self.tbl_meshes.setSelectionBehavior(
            QtWidgets.QAbstractItemView.SelectRows)
        self.tbl_meshes.setSelectionMode(
            QtWidgets.QAbstractItemView.SingleSelection)
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
            for c in range(5):
                header.setSectionResizeMode(c, interactive)
        except Exception:
            try:
                interactive = QtWidgets.QHeaderView.Interactive
                for c in range(5):
                    header.setResizeMode(c, interactive)
            except Exception:
                pass
        self.tbl_meshes.setColumnWidth(0, 90)
        self.tbl_meshes.setColumnWidth(1, 44)
        self.tbl_meshes.setColumnWidth(2, 48)
        self.tbl_meshes.setColumnWidth(3, 64)
        self.tbl_meshes.setColumnWidth(4, 90)
        self._mesh_rows = []
        self._sort_col = 0
        self._sort_asc = True
        header.sectionClicked.connect(self._on_mesh_header_clicked)
        self.tbl_meshes.cellClicked.connect(self._on_mesh_cell_clicked)
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
        if not self.lbl_diag_summary.text():
            self.lbl_diag_summary.setText(i18n.tr("ui_diag_placeholder"))
        self.tbl_meshes.setHorizontalHeaderLabels([
            i18n.tr("ui_col_name"),
            i18n.tr("ui_col_faces"),
            i18n.tr("ui_col_instance"),
            i18n.tr("ui_col_size"),
            i18n.tr("ui_col_base"),
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
                try:
                    generated = pkg.export(
                        out, selection=sel, scene_name="MayaToUnity")
                except Exception as exc:
                    cmds.warning(
                        "Unity_MayaToUnity export failed: %s" % exc)
                    return
                if not generated:
                    return
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

    def set_diagnose_result(self, result):
        """刷新底部汇总与模型列表。"""
        if not isinstance(result, dict):
            self.lbl_diag_summary.setText(i18n.tr("ui_diag_placeholder"))
            self._mesh_rows = []
            self.tbl_meshes.setRowCount(0)
            return

        summary = result.get("summary") or {}
        self._mesh_rows = list(result.get("meshes") or [])
        self.lbl_diag_summary.setText(
            i18n.tr(
                "ui_diag_summary",
                int(summary.get("model_count", 0)),
                int(summary.get("material_count", 0)),
                int(summary.get("texture_count", 0)),
                int(summary.get("total_faces", 0)),
                summary.get("total_export_size") or "0 B",
            )
        )
        self._sort_col = 0
        self._sort_asc = True
        self._apply_mesh_sort()

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
            return 0

        try:
            self._mesh_rows.sort(key=_key, reverse=not asc)
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
        meshes = self._mesh_rows or []
        self.tbl_meshes.setRowCount(0)
        self.tbl_meshes.setRowCount(len(meshes))
        yes = i18n.tr("ui_inst_yes")
        no = i18n.tr("ui_inst_no")
        none_base = i18n.tr("ui_base_none")
        link_brush = None
        try:
            link_brush = QtGui.QBrush(QtGui.QColor("#7ec8ff"))
        except Exception:
            link_brush = None
        for i, row in enumerate(meshes):
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
            items = [
                QtWidgets.QTableWidgetItem(name),
                QtWidgets.QTableWidgetItem(str(faces)),
                QtWidgets.QTableWidgetItem(inst_txt),
                QtWidgets.QTableWidgetItem(size),
                QtWidgets.QTableWidgetItem(base_name),
            ]
            items[1].setTextAlignment(
                QtCore.Qt.AlignRight | QtCore.Qt.AlignVCenter)
            items[2].setTextAlignment(
                QtCore.Qt.AlignHCenter | QtCore.Qt.AlignVCenter)
            items[3].setTextAlignment(
                QtCore.Qt.AlignRight | QtCore.Qt.AlignVCenter)
            items[0].setData(QtCore.Qt.UserRole, row.get("path") or "")
            # 基础模型列：存整组成员路径，点击多选
            items[4].setData(
                QtCore.Qt.UserRole, list(row.get("member_paths") or []))
            if is_inst and link_brush is not None:
                items[4].setForeground(link_brush)
            tip = row.get("path") or name
            if is_inst and row.get("prefab"):
                tip = u"%s\n%s: %s" % (
                    tip, i18n.tr("ui_col_base"), row.get("prefab"))
            tip = u"%s\n%s: %s\n%s: %s (verts=%s)" % (
                tip,
                i18n.tr("ui_col_instance"),
                inst_txt,
                i18n.tr("ui_col_size"),
                size,
                row.get("verts", 0),
            )
            for col, item in enumerate(items):
                item.setToolTip(tip)
                self.tbl_meshes.setItem(i, col, item)
        self.tbl_meshes.resizeRowsToContents()

    def _on_mesh_cell_clicked(self, row, col):
        # 点名称：选中单个模型
        if col == 0:
            item = self.tbl_meshes.item(row, 0)
            if item is None:
                return
            path = item.data(QtCore.Qt.UserRole) or ""
            self._select_mesh_in_maya(path)
            return
        # 点基础模型名：选中该实例组全部成员
        if col == 4:
            item = self.tbl_meshes.item(row, 4)
            if item is None:
                return
            paths = item.data(QtCore.Qt.UserRole) or []
            if not paths:
                return
            # 仅实例组有效（独立模型为 "-"）
            data_row = None
            if 0 <= row < len(self._mesh_rows or []):
                data_row = self._mesh_rows[row]
            if data_row and data_row.get("kind") != "instance":
                return
            self._select_meshes_in_maya(paths)
            return

    def _select_meshes_in_maya(self, dag_paths):
        """选中一组实例成员。"""
        alive = []
        for p in dag_paths or []:
            if p and cmds.objExists(p):
                alive.append(p)
        if not alive:
            cmds.warning(i18n.tr("ui_warn_mesh_missing"))
            return
        try:
            cmds.select(alive, replace=True)
        except Exception as exc:
            cmds.warning(
                "Unity_MayaToUnity: select failed: %s" % exc)
            return
        for panel in (cmds.getPanel(type="outlinerPanel") or []):
            try:
                cmds.outlinerEditor(panel, edit=True, showSelected=True)
            except Exception:
                pass

    def _select_mesh_in_maya(self, dag_path):
        """在场景中选中模型，并尽量让 Outliner 滚到选中项。"""
        if not dag_path:
            return
        if not cmds.objExists(dag_path):
            cmds.warning(i18n.tr("ui_warn_mesh_missing"))
            return
        try:
            cmds.select(dag_path, replace=True)
        except Exception as exc:
            cmds.warning(
                "Unity_MayaToUnity: select failed: %s" % exc)
            return
        # 让大纲显示选中（各 outliner 面板）
        for panel in (cmds.getPanel(type="outlinerPanel") or []):
            try:
                cmds.outlinerEditor(panel, edit=True, showSelected=True)
            except Exception:
                pass
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
