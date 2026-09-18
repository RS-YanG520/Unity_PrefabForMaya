# -*- coding: utf-8 -*-
"""UE 贴图 → Unity 贴图。

对照 Substance 图「贴图转换.sbs」：
  _D   原样输出为 _B（Base Color）
  _N   Normal Invert：翻转 G 通道（UE DirectX → Unity）
  _ORM R=AO G=Roughness B=Metallic
       → _M：RGB = Metallic(B)，A = 1 - Roughness(G)
"""
from __future__ import print_function
from __future__ import division

import os

_IMAGE_EXTS = (".png", ".jpg", ".jpeg", ".tga", ".tif", ".tiff", ".bmp")

# (源后缀小写, 种类, 输出后缀)
_RULES = (
    ("_orm", "orm", "_M"),
    ("_basecolor", "base", "_B"),
    ("_albedo", "base", "_B"),
    ("_diffuse", "base", "_B"),
    ("_normal", "normal", "_N"),
    ("_bc", "base", "_B"),
    ("_d", "base", "_B"),
    ("_n", "normal", "_N"),
)

_VAR_DIR = "mtu_ue_tex_dir"
_VAR_SUF = "mtu_ue_tex_suffix"
_VAR_SUB = "mtu_ue_tex_sub"
_VAR_SUF_BASE = "mtu_ue_tex_suf_base"
_VAR_SUF_NORMAL = "mtu_ue_tex_suf_normal"
_VAR_SUF_ORM = "mtu_ue_tex_suf_orm"
_DEFAULT_SUFFIX = "_Unity"
_DEFAULT_KIND_SUF = {
    "base": "_B",
    "normal": "_N",
    "orm": "_M",
}


def normalize_kind_suffix(text, kind):
    """输出后缀，空或非法时回退到该类型的默认值。"""
    fallback = _DEFAULT_KIND_SUF.get(kind) or "_X"
    suf = (text or "").strip()
    if not suf:
        suf = fallback
    if not suf.startswith("_"):
        suf = "_" + suf
    bad = '\\/:*?"<>|'
    for ch in bad:
        suf = suf.replace(ch, "")
    return suf or fallback


def kind_suffixes(base=None, normal=None, orm=None):
    return {
        "base": normalize_kind_suffix(base, "base"),
        "normal": normalize_kind_suffix(normal, "normal"),
        "orm": normalize_kind_suffix(orm, "orm"),
    }


def split_kind(stem):
    """返回 (kind, 去掉源后缀的前缀)。无法识别时 (None, None)。"""
    low = (stem or "").lower()
    for suf, kind, _out in _RULES:
        if low.endswith(suf) and len(stem) > len(suf):
            return kind, stem[:-len(suf)]
    return None, None


def classify_stem(stem, suffixes=None):
    """返回 (kind, out_stem)。无法识别时 (None, None)。"""
    kind, prefix = split_kind(stem)
    if not kind:
        return None, None
    sufs = suffixes or kind_suffixes()
    return kind, prefix + sufs[kind]


def normalize_suffix(text):
    suf = (text or "").strip()
    if not suf:
        suf = _DEFAULT_SUFFIX
    if not suf.startswith("_"):
        suf = "_" + suf
    bad = '\\/:*?"<>|'
    for ch in bad:
        suf = suf.replace(ch, "")
    return suf or _DEFAULT_SUFFIX


def output_dir_for(src_dir, suffix):
    """输出目录与源贴图文件夹同级：父目录 / (文件夹名 + 后缀)。"""
    src_dir = os.path.abspath(src_dir)
    parent = os.path.dirname(src_dir.rstrip("\\/"))
    name = os.path.basename(src_dir.rstrip("\\/"))
    suf = normalize_suffix(suffix)
    out_name = name + suf
    if out_name.lower() == name.lower():
        out_name = name + _DEFAULT_SUFFIX
    return os.path.join(parent, out_name)


def unique_out_path(folder, stem, ext, suffix):
    """同名则在主文件名后加自定义后缀，仍冲突再加序号。"""
    ext = ext or ".png"
    path = os.path.join(folder, stem + ext)
    if not os.path.exists(path):
        return path
    suf = normalize_suffix(suffix)
    path = os.path.join(folder, stem + suf + ext)
    if not os.path.exists(path):
        return path
    n = 2
    while n < 1000:
        path = os.path.join(folder, "%s%s%d%s" % (stem, suf, n, ext))
        if not os.path.exists(path):
            return path
        n += 1
    return path


def convert_buffer(data, width, height, bpl, kind):
    """就地改 RGBA 缓冲。kind: base / normal / orm。"""
    if kind == "base" or width <= 0 or height <= 0:
        return data
    for y in range(height):
        row = y * bpl
        x = 0
        while x < width:
            i = row + x * 4
            if kind == "normal":
                data[i + 1] = 255 - data[i + 1]
            elif kind == "orm":
                metal = data[i + 2]
                smooth = 255 - data[i + 1]
                data[i] = metal
                data[i + 1] = metal
                data[i + 2] = metal
                data[i + 3] = smooth
            x += 1
    return data


def fmt_bytes(n):
    n = float(n or 0)
    for unit in ("B", "KB", "MB", "GB"):
        if n < 1024.0 or unit == "GB":
            if unit == "B":
                return "%d B" % int(n)
            return "%.1f %s" % (n, unit)
        n /= 1024.0
    return "0 B"


def _row_from_path(path, root=None):
    name = os.path.basename(path)
    stem, ext = os.path.splitext(name)
    if ext.lower() not in _IMAGE_EXTS:
        return None
    kind, prefix = split_kind(stem)
    try:
        size = os.path.getsize(path)
    except Exception:
        size = 0
    show = name
    if root:
        try:
            rel = os.path.relpath(path, root)
        except Exception:
            rel = name
        if rel and not rel.startswith(".."):
            show = rel.replace("\\", "/")
    return {
        "name": show,
        "file_name": name,
        "path": path,
        "ext": ext,
        "size": size,
        "kind": kind,
        "prefix": prefix,
    }


def list_textures(folder, recursive=False):
    """返回 dict 列表。recursive 时包含子文件夹里的贴图。"""
    out = []
    if not folder or not os.path.isdir(folder):
        return out
    folder = os.path.abspath(folder)
    if not recursive:
        try:
            names = os.listdir(folder)
        except Exception:
            return out
        names.sort(key=lambda s: s.lower())
        for name in names:
            path = os.path.join(folder, name)
            if not os.path.isfile(path):
                continue
            row = _row_from_path(path, folder)
            if row:
                out.append(row)
        return out
    for dirpath, dirnames, filenames in os.walk(folder):
        dirnames.sort(key=lambda s: s.lower())
        filenames = sorted(filenames, key=lambda s: s.lower())
        for name in filenames:
            row = _row_from_path(os.path.join(dirpath, name), folder)
            if row:
                out.append(row)
    return out


def rows_from_files(paths, root=None):
    out = []
    seen = set()
    for path in paths or []:
        if not path or not os.path.isfile(path):
            continue
        key = os.path.normcase(os.path.abspath(path))
        if key in seen:
            continue
        row = _row_from_path(path, root)
        if not row:
            continue
        seen.add(key)
        out.append(row)
    out.sort(key=lambda r: (r.get("name") or "").lower())
    return out


def common_dir(paths):
    """多个文件/文件夹的公共父目录。"""
    dirs = []
    for p in paths or []:
        if not p:
            continue
        p = os.path.abspath(p)
        dirs.append(p if os.path.isdir(p) else os.path.dirname(p))
    if not dirs:
        return ""
    parts = [d.split(os.sep) for d in dirs]
    count = min(len(p) for p in parts)
    common = []
    for i in range(count):
        sample = parts[0][i].lower()
        if any(p[i].lower() != sample for p in parts):
            break
        common.append(parts[0][i])
    if not common:
        return dirs[0]
    path = os.sep.join(common)
    if os.name == "nt" and len(common) == 1 and common[0].endswith(":"):
        path = common[0] + os.sep
    if os.path.isdir(path):
        return path
    return dirs[0]


def output_stem(row, kind_sufs=None):
    if not row or not row.get("kind") or row.get("prefix") is None:
        return None
    sufs = kind_sufs or kind_suffixes()
    return row["prefix"] + sufs[row["kind"]]


def planned_output_name(row, suffix, kind_sufs=None):
    """列表里显示的输出贴图名（含同级输出文件夹）。"""
    out_stem = output_stem(row, kind_sufs)
    if not out_stem:
        return "-"
    ext = ".png" if row["kind"] != "base" else (row.get("ext") or ".png")
    parent = os.path.dirname(row["path"])
    out_dir = output_dir_for(parent, suffix)
    dst = unique_out_path(out_dir, out_stem, ext, suffix)
    base = os.path.dirname(os.path.abspath(parent))
    try:
        rel = os.path.relpath(dst, base)
    except Exception:
        rel = os.path.basename(dst)
    return rel.replace("\\", "/")


def _win_path(path):
    path = os.path.abspath(os.path.normpath(path))
    try:
        if not isinstance(path, type(u"")):
            path = path.decode("mbcs")
    except Exception:
        pass
    return path


def reveal_files(file_paths):
    """在资源管理器中打开并选中一个或多个文件。"""
    import subprocess
    import ctypes
    from ctypes import wintypes

    files = []
    for p in file_paths or []:
        if p and os.path.isfile(p):
            files.append(_win_path(p))
    if not files:
        return False
    if len(files) == 1:
        subprocess.Popen([u"explorer", u"/select,", files[0]])
        return True

    groups = {}
    for f in files:
        groups.setdefault(os.path.dirname(f), []).append(f)

    shell32 = ctypes.windll.shell32
    ole32 = ctypes.windll.ole32
    try:
        ole32.CoInitialize(None)
    except Exception:
        pass

    pidl_t = ctypes.c_void_p
    create = shell32.ILCreateFromPathW
    create.argtypes = [wintypes.LPCWSTR]
    create.restype = pidl_t
    free = shell32.ILFree
    free.argtypes = [pidl_t]
    clone = shell32.ILClone
    clone.argtypes = [pidl_t]
    clone.restype = pidl_t
    find_child = shell32.ILFindChild
    find_child.argtypes = [pidl_t, pidl_t]
    find_child.restype = pidl_t
    open_sel = shell32.SHOpenFolderAndSelectItems
    open_sel.argtypes = [
        pidl_t, ctypes.c_uint, ctypes.POINTER(pidl_t), ctypes.c_ulong]
    open_sel.restype = ctypes.c_long

    any_ok = False
    for folder, flist in groups.items():
        folder_pidl = create(folder)
        if not folder_pidl:
            try:
                subprocess.Popen([u"explorer", folder])
            except Exception:
                pass
            continue
        abs_pidls = []
        rel_pidls = (pidl_t * len(flist))()
        try:
            ready = True
            for i, fpath in enumerate(flist):
                abs_pidl = create(fpath)
                abs_pidls.append(abs_pidl)
                if not abs_pidl:
                    ready = False
                    break
                rel = find_child(folder_pidl, abs_pidl)
                cloned = clone(rel) if rel else None
                if not cloned:
                    ready = False
                    break
                rel_pidls[i] = cloned
            if ready and open_sel(folder_pidl, len(flist), rel_pidls, 0) == 0:
                any_ok = True
            else:
                subprocess.Popen([u"explorer", folder])
        finally:
            for i in range(len(flist)):
                if rel_pidls[i]:
                    try:
                        free(rel_pidls[i])
                    except Exception:
                        pass
            for p in abs_pidls:
                if p:
                    try:
                        free(p)
                    except Exception:
                        pass
            try:
                free(folder_pidl)
            except Exception:
                pass
    return any_ok


def _self_check():
    kind, stem = classify_stem("T_Pedestal_01a_D")
    if kind != "base" or stem != "T_Pedestal_01a_B":
        return "base %s %s" % (kind, stem)
    kind, stem = classify_stem("T_Pedestal_01a_N")
    if kind != "normal" or stem != "T_Pedestal_01a_N":
        return "normal"
    kind, stem = classify_stem("T_Pedestal_01a_ORM")
    if kind != "orm" or stem != "T_Pedestal_01a_M":
        return "orm %s" % stem
    custom = kind_suffixes("_Albedo", "Normal", "Mask")
    kind, stem = classify_stem("T_A_D", custom)
    if stem != "T_A_Albedo":
        return "custom %s" % stem
    data = bytearray([10, 20, 30, 255])
    convert_buffer(data, 1, 1, 4, "normal")
    if data[1] != 235:
        return "flip g"
    data = bytearray([1, 100, 200, 255])
    convert_buffer(data, 1, 1, 4, "orm")
    if list(data) != [200, 200, 200, 155]:
        return "orm bytes %s" % list(data)
    out = output_dir_for(r"E:\tex\Foo", "_Unity")
    if not out.replace("\\", "/").endswith("/tex/Foo_Unity"):
        return "out " + out
    return "ok"


try:
    from PySide2 import QtCore, QtWidgets, QtGui
except ImportError:
    try:
        from PySide import QtCore, QtGui
        from PySide import QtGui as QtWidgets
    except ImportError:
        QtCore = None
        QtWidgets = None
        QtGui = None


def _read_rgba(path):
    img = QtGui.QImage(path)
    if img.isNull():
        return None
    img = img.convertToFormat(QtGui.QImage.Format_RGBA8888)
    ptr = img.bits()
    ptr.setsize(img.byteCount())
    raw = bytearray(ptr.asstring())
    return img.width(), img.height(), img.bytesPerLine(), raw


def _write_rgba(data, width, height, bpl, path):
    import ctypes
    img = QtGui.QImage(width, height, QtGui.QImage.Format_RGBA8888)
    if img.isNull():
        return False
    # 行对齐可能和源图不同，按目标 bytesPerLine 重排
    dst_bpl = img.bytesPerLine()
    if dst_bpl != bpl:
        packed = bytearray(img.byteCount())
        for y in range(height):
            src = y * bpl
            dst = y * dst_bpl
            packed[dst:dst + width * 4] = data[src:src + width * 4]
        data = packed
        bpl = dst_bpl
    ptr = img.bits()
    ptr.setsize(img.byteCount())
    buf = (ctypes.c_char * len(data)).from_buffer(data)
    ctypes.memmove(int(ptr), buf, len(data))
    copied = img.copy()
    folder = os.path.dirname(path)
    if folder and not os.path.isdir(folder):
        os.makedirs(folder)
    return bool(copied.save(path, "PNG"))


def image_size(path):
    reader = QtGui.QImageReader(path)
    try:
        reader.setDecideFormatFromContent(True)
    except Exception:
        pass
    sz = reader.size()
    if sz.isValid() and sz.width() > 0 and sz.height() > 0:
        return sz.width(), sz.height()
    img = QtGui.QImage(path)
    if img.isNull():
        return 0, 0
    return img.width(), img.height()


def convert_file(src_path, kind, dst_path):
    """转换一张贴图。base 直接复制，normal / orm 改通道后写 PNG。"""
    import shutil
    folder = os.path.dirname(dst_path)
    if folder and not os.path.isdir(folder):
        os.makedirs(folder)
    if kind == "base":
        shutil.copy2(src_path, dst_path)
        return True
    loaded = _read_rgba(src_path)
    if not loaded:
        return False
    w, h, bpl, data = loaded
    convert_buffer(data, w, h, bpl, kind)
    # 通道图统一存 PNG，和 SD 导出一致
    root, _ext = os.path.splitext(dst_path)
    dst_path = root + ".png"
    return _write_rgba(data, w, h, bpl, dst_path)


if QtWidgets is not None:

    class UeTexWindow(QtWidgets.QWidget):
        """选择 UE 贴图目录，列出分辨率和大小，转换到同级输出目录。"""

        def __init__(self, parent=None):
            QtWidgets.QWidget.__init__(self, parent)
            self.setObjectName("UnityMayaToUnityUeTex")
            self.setAttribute(QtCore.Qt.WA_DeleteOnClose, True)
            self.setWindowFlags(QtCore.Qt.Window)
            self.resize(720, 520)
            self._rows = []
            self._explicit = None
            self.setAcceptDrops(True)

            root = QtWidgets.QVBoxLayout(self)
            root.setContentsMargins(8, 8, 8, 8)
            root.setSpacing(6)

            path_row = QtWidgets.QHBoxLayout()
            self.edit_path = QtWidgets.QLineEdit()
            self.btn_browse = QtWidgets.QPushButton()
            self.btn_browse.clicked.connect(self._on_browse)
            path_row.addWidget(self.edit_path, 1)
            path_row.addWidget(self.btn_browse)
            root.addLayout(path_row)

            suf_row = QtWidgets.QHBoxLayout()
            self.lbl_suffix = QtWidgets.QLabel()
            self.edit_suffix = QtWidgets.QLineEdit(_DEFAULT_SUFFIX)
            self.edit_suffix.setMaximumWidth(140)
            self.edit_suffix.textChanged.connect(self._on_suffix_changed)
            self.chk_sub = QtWidgets.QCheckBox()
            self.chk_sub.toggled.connect(self._on_sub_toggled)
            self.btn_run = QtWidgets.QPushButton()
            self.btn_run.clicked.connect(self._on_convert)
            suf_row.addWidget(self.lbl_suffix)
            suf_row.addWidget(self.edit_suffix)
            suf_row.addWidget(self.chk_sub)
            suf_row.addStretch(1)
            suf_row.addWidget(self.btn_run)
            root.addLayout(suf_row)

            name_row = QtWidgets.QHBoxLayout()
            self.lbl_suf_b = QtWidgets.QLabel()
            self.edit_suf_b = QtWidgets.QLineEdit(_DEFAULT_KIND_SUF["base"])
            self.edit_suf_n = QtWidgets.QLineEdit(_DEFAULT_KIND_SUF["normal"])
            self.edit_suf_m = QtWidgets.QLineEdit(_DEFAULT_KIND_SUF["orm"])
            self.lbl_suf_n = QtWidgets.QLabel()
            self.lbl_suf_m = QtWidgets.QLabel()
            for edit in (self.edit_suf_b, self.edit_suf_n, self.edit_suf_m):
                edit.setMaximumWidth(110)
                edit.textChanged.connect(self._on_suffix_changed)
            name_row.addWidget(self.lbl_suf_b)
            name_row.addWidget(self.edit_suf_b)
            name_row.addSpacing(10)
            name_row.addWidget(self.lbl_suf_n)
            name_row.addWidget(self.edit_suf_n)
            name_row.addSpacing(10)
            name_row.addWidget(self.lbl_suf_m)
            name_row.addWidget(self.edit_suf_m)
            name_row.addStretch(1)
            root.addLayout(name_row)

            self.lbl_out = QtWidgets.QLabel()
            self.lbl_out.setWordWrap(True)
            self.lbl_out.setStyleSheet("color: #b0b0b0; font-size: 11px;")
            root.addWidget(self.lbl_out)

            self.table = QtWidgets.QTableWidget(0, 4)
            self.table.setEditTriggers(QtWidgets.QAbstractItemView.NoEditTriggers)
            self.table.setSelectionBehavior(QtWidgets.QAbstractItemView.SelectRows)
            self.table.setSelectionMode(QtWidgets.QAbstractItemView.ExtendedSelection)
            self.table.verticalHeader().setVisible(False)
            self.table.setColumnWidth(0, 220)
            self.table.setColumnWidth(1, 220)
            self.table.setColumnWidth(2, 100)
            self.table.setColumnWidth(3, 80)
            self.table.setAcceptDrops(True)
            self.table.viewport().setAcceptDrops(True)
            self.table.dragEnterEvent = self.dragEnterEvent
            self.table.dragMoveEvent = self.dragMoveEvent
            self.table.dropEvent = self.dropEvent
            self.table.viewport().dragEnterEvent = self.dragEnterEvent
            self.table.viewport().dragMoveEvent = self.dragMoveEvent
            self.table.viewport().dropEvent = self.dropEvent
            self.table.setContextMenuPolicy(QtCore.Qt.CustomContextMenu)
            self.table.customContextMenuRequested.connect(self._on_context_menu)
            header = self.table.horizontalHeader()
            try:
                header.setStretchLastSection(True)
            except Exception:
                pass
            root.addWidget(self.table, 1)

            self.lbl_status = QtWidgets.QLabel()
            self.lbl_status.setWordWrap(True)
            self.lbl_status.setStyleSheet("color: #c8c8c8; font-size: 11px;")
            root.addWidget(self.lbl_status)

            self.setStyleSheet(
                "QWidget { background: #2b2b2b; color: #e6e6e6; }"
                "QLineEdit, QTableWidget { background: #1e1e1e; color: #eee; }"
                "QPushButton { background: #3a3a3a; color: #eee; padding: 4px 10px; }"
                "QHeaderView::section { background: #333; color: #ddd; }"
                "QMenu { background: #2b2b2b; color: #eee; }"
                "QMenu::item:selected { background: #3d5a80; }"
            )
            self.edit_path.editingFinished.connect(self._on_path_edited)
            self._load_prefs()
            for widget in (
                self.edit_path, self.lbl_out, self.lbl_status, self.chk_sub,
            ):
                widget.setAcceptDrops(True)
                widget.dragEnterEvent = self.dragEnterEvent
                widget.dragMoveEvent = self.dragMoveEvent
                widget.dropEvent = self.dropEvent
            self.retranslate()
            self._reload_list()

        def retranslate(self):
            try:
                import i18n
                self.setWindowTitle(i18n.tr("ui_ue_tex_title"))
                self.btn_browse.setText(i18n.tr("ui_btn_browse"))
                self.lbl_suffix.setText(i18n.tr("ui_ue_tex_suffix"))
                self.lbl_suf_b.setText(i18n.tr("ui_ue_tex_suf_base"))
                self.lbl_suf_n.setText(i18n.tr("ui_ue_tex_suf_normal"))
                self.lbl_suf_m.setText(i18n.tr("ui_ue_tex_suf_orm"))
                self.chk_sub.setText(i18n.tr("ui_ue_tex_sub"))
                self.btn_run.setText(i18n.tr("ui_ue_tex_run"))
                self.table.setHorizontalHeaderLabels([
                    i18n.tr("ui_ue_tex_col_name"),
                    i18n.tr("ui_ue_tex_col_out"),
                    i18n.tr("ui_ue_tex_col_res"),
                    i18n.tr("ui_ue_tex_col_size"),
                ])
                self.edit_path.setPlaceholderText(i18n.tr("ui_ue_tex_path_ph"))
            except Exception:
                self.setWindowTitle("UE to Unity Textures")
            self._refresh_out_label()

        def _load_prefs(self):
            try:
                import maya.cmds as cmds
                if cmds.optionVar(exists=_VAR_DIR):
                    self.edit_path.setText(cmds.optionVar(q=_VAR_DIR) or "")
                if cmds.optionVar(exists=_VAR_SUF):
                    self.edit_suffix.setText(
                        cmds.optionVar(q=_VAR_SUF) or _DEFAULT_SUFFIX)
                if cmds.optionVar(exists=_VAR_SUF_BASE):
                    self.edit_suf_b.setText(
                        cmds.optionVar(q=_VAR_SUF_BASE) or _DEFAULT_KIND_SUF["base"])
                if cmds.optionVar(exists=_VAR_SUF_NORMAL):
                    self.edit_suf_n.setText(
                        cmds.optionVar(q=_VAR_SUF_NORMAL) or _DEFAULT_KIND_SUF["normal"])
                if cmds.optionVar(exists=_VAR_SUF_ORM):
                    self.edit_suf_m.setText(
                        cmds.optionVar(q=_VAR_SUF_ORM) or _DEFAULT_KIND_SUF["orm"])
                if cmds.optionVar(exists=_VAR_SUB):
                    self.chk_sub.blockSignals(True)
                    self.chk_sub.setChecked(bool(cmds.optionVar(q=_VAR_SUB)))
                    self.chk_sub.blockSignals(False)
            except Exception:
                pass

        def _save_prefs(self):
            try:
                import maya.cmds as cmds
                cmds.optionVar(sv=(_VAR_DIR, self.edit_path.text() or ""))
                cmds.optionVar(sv=(_VAR_SUF, self.edit_suffix.text() or _DEFAULT_SUFFIX))
                cmds.optionVar(sv=(_VAR_SUF_BASE, self.edit_suf_b.text() or _DEFAULT_KIND_SUF["base"]))
                cmds.optionVar(sv=(_VAR_SUF_NORMAL, self.edit_suf_n.text() or _DEFAULT_KIND_SUF["normal"]))
                cmds.optionVar(sv=(_VAR_SUF_ORM, self.edit_suf_m.text() or _DEFAULT_KIND_SUF["orm"]))
                cmds.optionVar(iv=(_VAR_SUB, 1 if self.chk_sub.isChecked() else 0))
            except Exception:
                pass

        def _on_path_edited(self):
            self._explicit = None
            self._reload_list()

        def _on_browse(self):
            start = self.edit_path.text() or ""
            path = QtWidgets.QFileDialog.getExistingDirectory(
                self, self.windowTitle(), start)
            if not path:
                return
            self.edit_path.setText(path)
            self._explicit = None
            self._save_prefs()
            self._reload_list()

        def _kind_sufs(self):
            return kind_suffixes(
                self.edit_suf_b.text(),
                self.edit_suf_n.text(),
                self.edit_suf_m.text())

        def _on_suffix_changed(self, _text):
            self._refresh_out_label()
            if self._rows:
                self._fill_table()

        def _on_sub_toggled(self, _checked):
            self._explicit = None
            self._save_prefs()
            self._reload_list()

        def _src_dir(self):
            return (self.edit_path.text() or "").strip()

        def _refresh_out_label(self):
            src = self._src_dir()
            try:
                import i18n
                hint = i18n.tr("ui_ue_tex_hint")
            except Exception:
                hint = ""
            if not src or not os.path.isdir(src):
                self.lbl_out.setText(hint)
                return
            if self.chk_sub.isChecked():
                try:
                    import i18n
                    self.lbl_out.setText(
                        i18n.tr("ui_ue_tex_out_each") + "\n" + hint)
                except Exception:
                    self.lbl_out.setText(hint)
                return
            out = output_dir_for(src, self.edit_suffix.text())
            try:
                import i18n
                self.lbl_out.setText(i18n.tr("ui_ue_tex_out", out) + "\n" + hint)
            except Exception:
                self.lbl_out.setText(out)

        def _reload_list(self):
            self._save_prefs()
            src = self._src_dir()
            explicit = getattr(self, "_explicit", None)
            if explicit:
                self._rows = rows_from_files(explicit, src)
            else:
                self._rows = list_textures(
                    src, bool(self.chk_sub.isChecked()))
            self._fill_table()
            self._refresh_out_label()
            QtCore.QTimer.singleShot(0, self._fill_resolutions)

        def _fill_table(self):
            self.table.setRowCount(0)
            self.table.setRowCount(len(self._rows))
            for i, row in enumerate(self._rows):
                name = QtWidgets.QTableWidgetItem(row["name"])
                out_name = QtWidgets.QTableWidgetItem(
                    planned_output_name(
                        row, self.edit_suffix.text(), self._kind_sufs()))
                res = QtWidgets.QTableWidgetItem(row.get("res") or "...")
                size = QtWidgets.QTableWidgetItem(fmt_bytes(row.get("size")))
                if not row.get("kind"):
                    brush = QtGui.QBrush(QtGui.QColor("#888888"))
                    name.setForeground(brush)
                    out_name.setForeground(brush)
                    res.setForeground(brush)
                    size.setForeground(brush)
                self.table.setItem(i, 0, name)
                self.table.setItem(i, 1, out_name)
                self.table.setItem(i, 2, res)
                self.table.setItem(i, 3, size)

        def _on_context_menu(self, pos):
            index = self.table.indexAt(pos)
            if not index.isValid():
                return
            row_i = index.row()
            item = self.table.item(row_i, 0)
            if item is not None and not item.isSelected():
                self.table.clearSelection()
                self.table.selectRow(row_i)
            picked = []
            seen = set()
            for idx in self.table.selectionModel().selectedRows():
                r = idx.row()
                if r in seen or not (0 <= r < len(self._rows)):
                    continue
                seen.add(r)
                path = self._rows[r].get("path")
                if path and os.path.isfile(path):
                    picked.append(path)
            if not picked:
                self._warn("ui_ue_tex_locate_missing")
                return
            menu = QtWidgets.QMenu(self.table)
            try:
                import i18n
                if len(picked) > 1:
                    label = i18n.tr("ui_ue_tex_locate_multi", len(picked))
                else:
                    label = i18n.tr("ui_ue_tex_locate")
            except Exception:
                label = "Show in Explorer"
            act = menu.addAction(label)
            chosen = menu.exec_(self.table.viewport().mapToGlobal(pos))
            if chosen == act:
                try:
                    reveal_files(picked)
                except Exception as exc:
                    self.lbl_status.setText("Explorer: %s" % exc)

        def _fill_resolutions(self):
            for i, row in enumerate(self._rows):
                w, h = image_size(row["path"])
                if w > 0 and h > 0:
                    text = "%d x %d" % (w, h)
                else:
                    text = "-"
                row["res"] = text
                item = self.table.item(i, 2)
                if item is not None:
                    item.setText(text)
                try:
                    QtWidgets.QApplication.processEvents()
                except Exception:
                    pass

        def _on_convert(self):
            src = self._src_dir()
            if not src or not os.path.isdir(src):
                self._warn("ui_ue_tex_no_dir")
                return
            jobs = [r for r in self._rows if r.get("kind")]
            if not jobs:
                self._warn("ui_ue_tex_none")
                return
            self._save_prefs()
            suffix = self.edit_suffix.text()
            kind_sufs = self._kind_sufs()
            ok_n = 0
            fail_n = 0
            out_dirs = []
            self.btn_run.setEnabled(False)
            try:
                for i, row in enumerate(jobs):
                    ext = ".png" if row["kind"] != "base" else row["ext"]
                    out_dir = output_dir_for(
                        os.path.dirname(row["path"]), suffix)
                    if out_dir not in out_dirs:
                        out_dirs.append(out_dir)
                    dst = unique_out_path(
                        out_dir, output_stem(row, kind_sufs), ext, suffix)
                    try:
                        import i18n
                        self.lbl_status.setText(i18n.tr(
                            "ui_ue_tex_working", i + 1, len(jobs),
                            row.get("file_name") or row["name"]))
                    except Exception:
                        self.lbl_status.setText(row["name"])
                    try:
                        QtWidgets.QApplication.processEvents()
                    except Exception:
                        pass
                    try:
                        good = convert_file(row["path"], row["kind"], dst)
                    except Exception:
                        good = False
                    if good:
                        ok_n += 1
                    else:
                        fail_n += 1
            finally:
                self.btn_run.setEnabled(True)
            if len(out_dirs) == 1:
                where = out_dirs[0]
            else:
                try:
                    import i18n
                    where = i18n.tr("ui_ue_tex_out_each")
                except Exception:
                    where = "%d folders" % len(out_dirs)
            try:
                import i18n
                self.lbl_status.setText(i18n.tr(
                    "ui_ue_tex_done", ok_n, fail_n, where))
            except Exception:
                self.lbl_status.setText("%s ok, %s fail" % (ok_n, fail_n))

        def dragEnterEvent(self, event):
            if self._drop_paths(event):
                event.acceptProposedAction()
            else:
                event.ignore()

        def dragMoveEvent(self, event):
            if self._drop_paths(event):
                event.acceptProposedAction()
            else:
                event.ignore()

        def dropEvent(self, event):
            paths = self._drop_paths(event)
            if not paths:
                event.ignore()
                return
            event.acceptProposedAction()
            self._apply_drop(paths)

        def _drop_paths(self, event):
            md = event.mimeData()
            paths = []
            if md.hasUrls():
                for url in md.urls():
                    local = url.toLocalFile()
                    if local:
                        paths.append(local)
            if not paths and md.hasText():
                for line in (md.text() or "").replace("\r", "\n").split("\n"):
                    line = line.strip().strip('"')
                    if line and os.path.exists(line):
                        paths.append(line)
            return paths

        def _apply_drop(self, paths):
            dirs = []
            files = []
            for raw in paths:
                path = os.path.normpath(raw)
                if os.path.isdir(path):
                    dirs.append(path)
                elif os.path.isfile(path):
                    ext = os.path.splitext(path)[1].lower()
                    if ext in _IMAGE_EXTS:
                        files.append(path)
            if not dirs and not files:
                self._warn("ui_ue_tex_no_dir")
                return
            if dirs and not files and len(dirs) == 1:
                self._explicit = None
                self.edit_path.setText(dirs[0])
                self._save_prefs()
                self._reload_list()
                return
            rows = []
            seen = set()
            recursive = bool(self.chk_sub.isChecked())
            for folder in dirs:
                for row in list_textures(folder, recursive):
                    key = os.path.normcase(os.path.abspath(row["path"]))
                    if key in seen:
                        continue
                    seen.add(key)
                    rows.append(row)
            root = common_dir(dirs + files)
            for row in rows_from_files(files, root):
                key = os.path.normcase(os.path.abspath(row["path"]))
                if key in seen:
                    continue
                seen.add(key)
                rows.append(row)
            self._rows = rows
            self._explicit = [r["path"] for r in rows]
            if root:
                self.edit_path.setText(root)
            self._save_prefs()
            self._fill_table()
            self._refresh_out_label()
            QtCore.QTimer.singleShot(0, self._fill_resolutions)

        def _warn(self, key):
            try:
                import i18n
                import maya.cmds as cmds
                cmds.warning(i18n.tr(key))
                self.lbl_status.setText(i18n.tr(key))
            except Exception:
                pass
