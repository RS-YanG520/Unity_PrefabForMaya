# -*- coding: utf-8 -*-
"""场景模型大小图（类似 WizTree 的矩形树图）。

面积 = 面数（场景统计）或预估导出体积。
实例组用暖色、独立模型用冷色；大块在左上。
点击方块选中对应模型，点组标题选中整组。
"""
from __future__ import print_function
from __future__ import division

# 按类型：与顶部图例色块一致，不再按组换色相
_TYPE_INST_HUE = 28
_TYPE_UNIQ_HUE = 205
_TYPE_VAL = 190
_TYPE_SAT = 168


def _weight_of(row, mode):
    faces = int(row.get("faces") or 0)
    if mode == "export":
        if row.get("shared"):
            return 0
        w = int(row.get("export_bytes") or 0)
        return w if w > 0 else faces
    return faces


def build_nodes(mesh_rows, mode="faces"):
    """诊断模型行 → 树图节点。实例按组嵌套，独立模型为叶子。"""
    groups = {}
    order = []
    uniques = []
    for row in mesh_rows or []:
        if not row:
            continue
        kind = row.get("kind")
        if kind not in ("instance", "unique"):
            continue
        weight = _weight_of(row, mode)
        if weight <= 0:
            continue
        if kind == "instance" and mode != "export":
            key = row.get("shape") or row.get("base_name") or row.get("prefab") or ""
            if not key:
                key = row.get("path") or row.get("name") or ""
            if key not in groups:
                groups[key] = {
                    "name": row.get("base_name") or row.get("prefab") or row.get("name") or "",
                    "kind": "instance",
                    "weight": 0,
                    "faces": 0,
                    "paths": [],
                    "children": [],
                    "size": row.get("size") or "",
                }
                order.append(key)
            g = groups[key]
            path = row.get("path") or ""
            g["children"].append({
                "name": row.get("name") or "",
                "kind": "instance",
                "weight": weight,
                "faces": int(row.get("faces") or 0),
                "paths": [path] if path else [],
                "children": [],
                "size": row.get("size") or "",
            })
            g["weight"] += weight
            g["faces"] += int(row.get("faces") or 0)
            if path:
                g["paths"].append(path)
        else:
            path = row.get("path") or ""
            name = row.get("name") or ""
            if kind == "instance":
                name = row.get("base_name") or row.get("prefab") or name
            uniques.append({
                "name": name,
                "kind": kind,
                "weight": weight,
                "faces": int(row.get("faces") or 0),
                "paths": [path] if path else list(row.get("member_paths") or []),
                "children": [],
                "size": row.get("size") or "",
            })
    return [groups[k] for k in order] + uniques


def _worst(row, length):
    if not row or length <= 0:
        return 1e18
    total = 0.0
    rmax = 0.0
    rmin = 1e18
    for it in row:
        w = it["_area"]
        total += w
        if w > rmax:
            rmax = w
        if w < rmin:
            rmin = w
    if total <= 0 or rmin <= 0:
        return 1e18
    s2 = total * total
    w2 = length * length
    a = (w2 * rmax) / s2
    b = s2 / (w2 * rmin)
    return a if a > b else b


def _place(row, x, y, w, h, vertical):
    total = 0.0
    for it in row:
        total += it["_area"]
    if total <= 0:
        total = 1.0
    if vertical:
        yy = y
        for it in row:
            ih = h * (it["_area"] / total)
            it["rect"] = (x, yy, w, ih)
            yy += ih
    else:
        xx = x
        for it in row:
            iw = w * (it["_area"] / total)
            it["rect"] = (xx, y, iw, h)
            xx += iw


def _squarify(children, row, x, y, w, h):
    if w < 1 or h < 1:
        return
    if not children:
        if row:
            _place(row, x, y, w, h, w >= h)
        return
    c = children[0]
    if not row:
        _squarify(children[1:], [c], x, y, w, h)
        return
    short = w if w < h else h
    if _worst(row + [c], short) <= _worst(row, short):
        _squarify(children[1:], row + [c], x, y, w, h)
        return
    area = 0.0
    for it in row:
        area += it["_area"]
    if w >= h:
        row_w = area / h if h else w
        if row_w > w:
            row_w = w
        _place(row, x, y, row_w, h, True)
        _squarify(children, [], x + row_w, y, w - row_w, h)
    else:
        row_h = area / w if w else h
        if row_h > h:
            row_h = h
        _place(row, x, y, w, row_h, False)
        _squarify(children, [], x, y + row_h, w, h - row_h)


def _layout_items(items, x, y, w, h):
    alive = [it for it in items if it.get("weight", 0) > 0 and w > 1 and h > 1]
    if not alive:
        return
    total = 0.0
    for it in alive:
        total += float(it["weight"])
    if total <= 0:
        return
    area = float(w) * float(h)
    for it in alive:
        it["_area"] = float(it["weight"]) / total * area
    alive.sort(key=lambda it: -it["_area"])
    _squarify(alive, [], x, y, w, h)


def apply_colors(paint, mode="type", seed=1):
    """按模式改方块颜色。type 保持布局时的类型色。"""
    mode = mode or "type"
    if mode == "type":
        return paint
    tiles = [it for it in (paint or []) if not it.get("nest")]
    if mode == "random":
        import random
        rng = random.Random(int(seed or 1))
        for it in tiles:
            if it.get("header"):
                it["hue"] = 0
                it["sat"] = 0
                it["val"] = 70
                continue
            it["hue"] = rng.randint(0, 359)
            it["sat"] = 150 + rng.randint(0, 70)
            it["val"] = 160 + rng.randint(0, 50)
        return paint
    # heat：面数高=红(0)，中=黄(60)，低=绿(120)
    values = []
    for it in tiles:
        if it.get("header"):
            continue
        values.append(float(it.get("faces") or 0))
    if not values:
        return paint
    lo = min(values)
    hi = max(values)
    span = hi - lo
    for it in tiles:
        if it.get("header"):
            it["hue"] = 0
            it["sat"] = 0
            it["val"] = 70
            continue
        faces = float(it.get("faces") or 0)
        if span <= 0:
            t = 0.5
        else:
            t = (faces - lo) / span
        it["hue"] = int(round(120 * (1.0 - t)))
        it["sat"] = 190
        it["val"] = 210
    return paint


def _assign_hues(nodes):
    """按类型只使用图例上的两种颜色。"""
    for n in nodes:
        if n.get("kind") == "instance":
            n["hue"] = _TYPE_INST_HUE
        else:
            n["hue"] = _TYPE_UNIQ_HUE
        n["val"] = _TYPE_VAL
        n["sat"] = _TYPE_SAT
        for c in n.get("children") or []:
            c["hue"] = n["hue"]
            c["val"] = _TYPE_VAL
            c["sat"] = _TYPE_SAT


def layout_tree(nodes, x, y, w, h):
    """布局并返回可绘制条目（先父后子）。"""
    paint = []
    nodes = [n for n in (nodes or []) if n.get("weight", 0) > 0]
    if not nodes or w < 2 or h < 2:
        return paint
    total = 0.0
    for n in nodes:
        total += float(n["weight"])
    _assign_hues(nodes)
    _layout_items(nodes, x, y, w, h)

    def _add(node, depth):
        rect = node.get("rect")
        if not rect:
            return
        rx, ry, rw, rh = rect
        kids = [c for c in (node.get("children") or []) if c.get("weight", 0) > 0]
        nest = (
            depth == 0 and len(kids) > 1 and rw >= 72 and rh >= 46
        )
        header = 16 if nest else 0
        pct = (100.0 * float(node["weight"]) / total) if total else 0.0
        paint.append({
            "rect": (rx, ry, rw, rh),
            "paths": list(node.get("paths") or []),
            "name": node.get("name") or "",
            "kind": node.get("kind") or "",
            "faces": int(node.get("faces") or 0),
            "size": node.get("size") or "",
            "weight": node.get("weight") or 0,
            "pct": pct,
            "hue": node.get("hue", 200),
            "val": node.get("val", 188),
            "sat": node.get("sat", _TYPE_SAT),
            "header": False,
            "nest": nest,
            "header_h": header,
            "depth": depth,
        })
        if not nest:
            return
        # 标题条单独可点（选中整组）
        paint.append({
            "rect": (rx, ry, rw, header),
            "paths": list(node.get("paths") or []),
            "name": node.get("name") or "",
            "kind": node.get("kind") or "",
            "faces": int(node.get("faces") or 0),
            "size": node.get("size") or "",
            "weight": node.get("weight") or 0,
            "pct": pct,
            "hue": node.get("hue", 200),
            "val": node.get("val", _TYPE_VAL),
            "sat": node.get("sat", _TYPE_SAT),
            "header": True,
            "nest": False,
            "header_h": 0,
            "depth": depth,
        })
        pad = 2
        _layout_items(
            kids, rx + pad, ry + header, max(1, rw - pad * 2), max(1, rh - header - pad))
        for c in kids:
            _add(c, depth + 1)

    for n in nodes:
        _add(n, 0)
    return paint


def _self_check():
    rows = []
    for i in range(6):
        rows.append({
            "kind": "instance",
            "name": "B_%d" % i,
            "path": "|B_%d" % i,
            "shape": "shA",
            "base_name": "Bucket_Inst",
            "faces": 1000 * (i + 1),
            "shared": i != 0,
            "export_bytes": 8000 if i == 0 else 0,
            "size": "8 KB",
            "member_paths": ["|B_%d" % j for j in range(6)],
        })
    rows.append({
        "kind": "unique",
        "name": "Plane",
        "path": "|Plane",
        "faces": 400,
        "shared": False,
        "export_bytes": 200,
        "size": "200 B",
    })
    nodes = build_nodes(rows, "faces")
    paint = layout_tree(nodes, 0, 0, 400, 300)
    if len(nodes) != 2:
        return "expected 2 top nodes, got %d" % len(nodes)
    if not paint:
        return "empty paint"
    # 顶层面积应接近画布
    top = [p for p in paint if p["depth"] == 0 and not p["header"]]
    area = 0.0
    for p in top:
        _x, _y, w, h = p["rect"]
        if w < 0 or h < 0:
            return "negative rect"
        area += w * h
    if area < 400 * 300 * 0.9:
        return "area too small: %s" % area
    exp = build_nodes(rows, "export")
    if len(exp) != 2:
        return "export nodes %d" % len(exp)
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


if QtWidgets is not None:

    def _qcolor(hue, val, sat=168):
        c = QtGui.QColor.fromHsv(
            int(hue) % 360,
            int(max(0, min(255, sat))),
            int(max(0, min(255, val))),
        )
        return c

    class SizeMapCanvas(QtWidgets.QWidget):
        """矩形树图画布。picked(paths)，hovered(text)。"""

        picked = QtCore.Signal(object)
        hovered = QtCore.Signal(object)

        def __init__(self, parent=None):
            QtWidgets.QWidget.__init__(self, parent)
            self.setMouseTracking(True)
            self.setMinimumSize(320, 200)
            self._rows = []
            self._mode = "faces"
            self._color_mode = "type"
            self._color_seed = 1
            self._paint = []
            self._hover = -1
            self._selected = set()
            self._empty = u""
            self._drag_origin = None
            self._rubber = None
            self.setCursor(QtCore.Qt.CrossCursor)

        def set_rows(self, rows, mode="faces", color_mode="type", color_seed=1):
            self._rows = list(rows or [])
            self._mode = mode or "faces"
            self._color_mode = color_mode or "type"
            self._color_seed = color_seed or 1
            self._relayout()
            self.update()

        def set_color(self, color_mode, color_seed=1):
            self._color_mode = color_mode or "type"
            self._color_seed = color_seed or 1
            self._relayout()
            self.update()

        def set_selected(self, paths):
            self._selected = set(paths or [])
            self.update()

        def _relayout(self):
            m = 3
            w = max(1, self.width() - m * 2)
            h = max(1, self.height() - m * 2)
            nodes = build_nodes(self._rows, self._mode)
            self._paint = layout_tree(nodes, m, m, w, h)
            apply_colors(self._paint, self._color_mode, self._color_seed)
            self._hover = -1

        def resizeEvent(self, event):
            self._relayout()
            QtWidgets.QWidget.resizeEvent(self, event)

        def paintEvent(self, event):
            p = QtGui.QPainter(self)
            p.setRenderHint(QtGui.QPainter.Antialiasing, False)
            p.fillRect(self.rect(), QtGui.QColor("#141414"))
            if not self._paint:
                p.setPen(QtGui.QColor("#888888"))
                msg = self._empty or u""
                p.drawText(self.rect(), QtCore.Qt.AlignCenter, msg)
                p.end()
                return
            font = p.font()
            font.setPixelSize(11)
            p.setFont(font)
            fm = QtGui.QFontMetrics(font)
            for i, item in enumerate(self._paint):
                # 有子块的父矩形只画边，避免盖住子块
                if item.get("nest"):
                    rx, ry, rw, rh = item["rect"]
                    pen = QtGui.QPen(QtGui.QColor("#0c0c0c"))
                    pen.setWidth(2)
                    p.setPen(pen)
                    p.setBrush(QtCore.Qt.NoBrush)
                    p.drawRect(int(rx), int(ry), int(rw) - 1, int(rh) - 1)
                    continue
                self._draw_item(p, fm, item, i == self._hover)
            self._draw_rubber(p)
            p.end()

        def _draw_item(self, p, fm, item, hover):
            rx, ry, rw, rh = item["rect"]
            if rw < 1 or rh < 1:
                return
            inset = 1
            x = int(rx) + inset
            y = int(ry) + inset
            w = int(rw) - inset * 2
            h = int(rh) - inset * 2
            if w < 1 or h < 1:
                return
            col = _qcolor(
                item["hue"], item["val"], item.get("sat", 168))
            if hover:
                col = col.lighter(115)
            p.setPen(QtCore.Qt.NoPen)
            p.setBrush(col)
            p.drawRect(x, y, w, h)

            # Py2 的列表推导会把循环变量泄漏到函数作用域，不能用 p（那是 QPainter）
            paths = [path for path in (item.get("paths") or []) if path]
            if item.get("header"):
                selected = bool(paths) and all(
                    path in self._selected for path in paths)
            else:
                selected = any(path in self._selected for path in paths)
            if selected or hover:
                pen = QtGui.QPen(QtGui.QColor("#ffffff" if selected else "#f0f0f0"))
                pen.setWidth(2 if selected else 1)
                p.setPen(pen)
                p.setBrush(QtCore.Qt.NoBrush)
                p.drawRect(x, y, w - 1, h - 1)

            if w < 28 or h < 12:
                return
            p.setPen(QtGui.QColor("#1a1a1a"))
            name = item.get("name") or ""
            label = fm.elidedText(name, QtCore.Qt.ElideRight, max(1, w - 6))
            if item.get("header"):
                p.drawText(
                    x + 3, y, w - 6, h,
                    QtCore.Qt.AlignLeft | QtCore.Qt.AlignVCenter,
                    label,
                )
                return
            if h >= 26 and w >= 48:
                pct = "%.0f%%" % (item.get("pct") or 0)
                block = label + u"\n" + pct
                p.drawText(
                    x + 3, y + 1, w - 6, h - 2,
                    QtCore.Qt.AlignLeft | QtCore.Qt.AlignVCenter,
                    block,
                )
            else:
                p.drawText(
                    x + 3, y, w - 6, h,
                    QtCore.Qt.AlignLeft | QtCore.Qt.AlignVCenter,
                    label,
                )

        def _draw_rubber(self, painter):
            rubber = self._rubber
            if not rubber:
                return
            rx, ry, rw, rh = rubber
            painter.setPen(QtGui.QPen(QtGui.QColor("#ffffff")))
            painter.setBrush(QtGui.QColor(255, 255, 255, 40))
            painter.drawRect(int(rx), int(ry), int(rw), int(rh))

        def _hit(self, pos):
            x = pos.x()
            y = pos.y()
            for i in range(len(self._paint) - 1, -1, -1):
                item = self._paint[i]
                if item.get("nest"):
                    continue
                rx, ry, rw, rh = item["rect"]
                if rx <= x < rx + rw and ry <= y < ry + rh:
                    return i
            return -1

        def mouseMoveEvent(self, event):
            if self._drag_origin is not None and (
                    event.buttons() & QtCore.Qt.LeftButton):
                x0, y0 = self._drag_origin
                x1 = event.pos().x()
                y1 = event.pos().y()
                if abs(x1 - x0) > 3 or abs(y1 - y0) > 3:
                    self._rubber = (
                        min(x0, x1), min(y0, y1), abs(x1 - x0), abs(y1 - y0))
                    self.update()
                return
            i = self._hit(event.pos())
            if i != self._hover:
                self._hover = i
                self.update()
                self.hovered.emit(self._tip(i))
            QtWidgets.QWidget.mouseMoveEvent(self, event)

        def leaveEvent(self, event):
            if self._drag_origin is not None:
                return
            if self._hover != -1:
                self._hover = -1
                self.update()
                self.hovered.emit(u"")
            QtWidgets.QWidget.leaveEvent(self, event)

        def mousePressEvent(self, event):
            if event.button() != QtCore.Qt.LeftButton:
                return
            self._drag_origin = (event.pos().x(), event.pos().y())
            self._rubber = None
            try:
                self.grabMouse()
            except Exception:
                pass

        def mouseReleaseEvent(self, event):
            if event.button() != QtCore.Qt.LeftButton:
                return
            origin = self._drag_origin
            rubber = self._rubber
            self._drag_origin = None
            self._rubber = None
            try:
                self.releaseMouse()
            except Exception:
                pass
            self.update()
            if origin is None:
                return
            if rubber and rubber[2] >= 4 and rubber[3] >= 4:
                paths = self._paths_in_rect(rubber)
                mods = event.modifiers()
                add = bool(mods & (
                    QtCore.Qt.ControlModifier | QtCore.Qt.ShiftModifier))
                if add:
                    paths = self._merge_paths(list(self._selected), paths)
                if paths:
                    self.picked.emit(paths)
                return
            i = self._hit(event.pos())
            if i < 0:
                return
            paths = self._paint[i].get("paths") or []
            if paths:
                self.picked.emit(list(paths))

        def _paths_in_rect(self, rubber):
            """框内的模型。优先单块；只框到组标题时才选整组。"""
            rx, ry, rw, rh = rubber
            x2 = rx + rw
            y2 = ry + rh
            seen = set()
            out = []
            headers = []
            for item in self._paint:
                if item.get("nest"):
                    continue
                ix, iy, iw, ih = item["rect"]
                if ix >= x2 or iy >= y2 or ix + iw <= rx or iy + ih <= ry:
                    continue
                if item.get("header"):
                    headers.append(item)
                    continue
                for path in item.get("paths") or []:
                    if path and path not in seen:
                        seen.add(path)
                        out.append(path)
            if out:
                return out
            for item in headers:
                for path in item.get("paths") or []:
                    if path and path not in seen:
                        seen.add(path)
                        out.append(path)
            return out

        def _merge_paths(self, first, second):
            seen = set()
            out = []
            for path in list(first or []) + list(second or []):
                if path and path not in seen:
                    seen.add(path)
                    out.append(path)
            return out

        def _tip(self, index):
            if index < 0 or index >= len(self._paint):
                return u""
            item = self._paint[index]
            try:
                import i18n
                kind = i18n.tr(
                    "ui_size_kind_inst" if item.get("kind") == "instance"
                    else "ui_size_kind_unique")
                return i18n.tr(
                    "ui_size_tip",
                    item.get("name") or "",
                    kind,
                    int(item.get("faces") or 0),
                    item.get("size") or "-",
                    float(item.get("pct") or 0),
                )
            except Exception:
                return item.get("name") or ""

    class SizeMapWindow(QtWidgets.QWidget):
        """独立窗口：场景模型大小图。"""

        def __init__(self, parent=None):
            QtWidgets.QWidget.__init__(self, parent)
            self.setObjectName("UnityMayaToUnitySizeMap")
            self.setAttribute(QtCore.Qt.WA_DeleteOnClose, True)
            self.setWindowFlags(QtCore.Qt.Window)
            self.resize(760, 520)
            self._rows = []

            root = QtWidgets.QVBoxLayout(self)
            root.setContentsMargins(8, 8, 8, 8)
            root.setSpacing(6)

            bar = QtWidgets.QHBoxLayout()
            bar.setSpacing(8)
            self.combo = QtWidgets.QComboBox()
            self.combo.setMinimumWidth(180)
            self.combo.currentIndexChanged.connect(self._on_mode)
            bar.addWidget(self.combo)

            self.combo_color = QtWidgets.QComboBox()
            self.combo_color.setMinimumWidth(130)
            self.combo_color.activated.connect(self._on_color)
            bar.addWidget(self.combo_color)

            self.swatch_inst = QtWidgets.QLabel()
            self.swatch_inst.setFixedSize(12, 12)
            self.lbl_inst = QtWidgets.QLabel()
            self.swatch_uniq = QtWidgets.QLabel()
            self.swatch_uniq.setFixedSize(12, 12)
            self.lbl_uniq = QtWidgets.QLabel()
            bar.addWidget(self.swatch_inst)
            bar.addWidget(self.lbl_inst)
            bar.addSpacing(8)
            bar.addWidget(self.swatch_uniq)
            bar.addWidget(self.lbl_uniq)

            self.lbl_heat_high = QtWidgets.QLabel()
            self.heat_bar = QtWidgets.QLabel()
            self.heat_bar.setFixedSize(96, 12)
            self.lbl_heat_low = QtWidgets.QLabel()
            bar.addWidget(self.lbl_heat_high)
            bar.addWidget(self.heat_bar)
            bar.addWidget(self.lbl_heat_low)
            bar.addStretch(1)
            root.addLayout(bar)

            self.canvas = SizeMapCanvas()
            self.canvas.picked.connect(self._on_picked)
            self.canvas.hovered.connect(self._on_hover)
            root.addWidget(self.canvas, 1)

            self.lbl_status = QtWidgets.QLabel()
            self.lbl_status.setWordWrap(True)
            self.lbl_status.setAlignment(
                QtCore.Qt.AlignLeft | QtCore.Qt.AlignTop)
            # 悬停说明是两行，高度固定，避免把上面的面积图顶上去
            self.lbl_status.setFixedHeight(40)
            self.lbl_status.setSizePolicy(
                QtWidgets.QSizePolicy.Expanding, QtWidgets.QSizePolicy.Fixed)
            self.lbl_status.setStyleSheet(
                "color: #c8c8c8; font-size: 11px; padding: 0px;")
            root.addWidget(self.lbl_status)

            self.setStyleSheet(
                "QWidget { background: #2b2b2b; color: #e6e6e6; }"
                "QComboBox { background: #3a3a3a; color: #eee; padding: 2px 6px; }"
                "QComboBox QAbstractItemView { background: #3a3a3a; color: #eee; }"
            )
            self._on_pick = None
            self._mode_name = "faces"
            self._color_mode = "type"
            self._color_seed = 1
            self.retranslate()

        def retranslate(self):
            try:
                import i18n
                self.setWindowTitle(i18n.tr("ui_size_map_title"))
                mode = self._mode_name if getattr(self, "_mode_name", None) else "faces"
                self.combo.blockSignals(True)
                self.combo.clear()
                self.combo.addItem(i18n.tr("ui_size_metric_faces"), "faces")
                self.combo.addItem(i18n.tr("ui_size_metric_export"), "export")
                self.combo.setCurrentIndex(1 if mode == "export" else 0)
                self.combo.blockSignals(False)
                self._mode_name = mode
                self.lbl_inst.setText(i18n.tr("ui_size_legend_inst"))
                self.lbl_uniq.setText(i18n.tr("ui_size_legend_unique"))
                color = getattr(self, "_color_mode", None) or "type"
                self.combo_color.blockSignals(True)
                self.combo_color.clear()
                self.combo_color.addItem(i18n.tr("ui_size_color_type"), "type")
                self.combo_color.addItem(i18n.tr("ui_size_color_random"), "random")
                self.combo_color.addItem(i18n.tr("ui_size_color_heat"), "heat")
                color_idx = {"random": 1, "heat": 2}.get(color, 0)
                self.combo_color.setCurrentIndex(color_idx)
                self.combo_color.blockSignals(False)
                self._color_mode = color
                self.lbl_heat_high.setText(i18n.tr("ui_size_heat_high"))
                self.lbl_heat_low.setText(i18n.tr("ui_size_heat_low"))
                self.canvas._empty = i18n.tr("ui_size_empty")
                if not self.lbl_status.text():
                    self.lbl_status.setText(i18n.tr("ui_size_hint"))
            except Exception:
                self.setWindowTitle("Scene Size Map")
            self._paint_swatches()
            self._update_legend()
            self.canvas.update()

        def _paint_swatches(self):
            for lab, hue in (
                (self.swatch_inst, _TYPE_INST_HUE),
                (self.swatch_uniq, _TYPE_UNIQ_HUE),
            ):
                pm = QtGui.QPixmap(12, 12)
                pm.fill(_qcolor(hue, _TYPE_VAL, _TYPE_SAT))
                lab.setPixmap(pm)
            bar = QtGui.QPixmap(96, 12)
            painter = QtGui.QPainter(bar)
            for x in range(96):
                hue = int(120 * (x / 95.0))
                painter.setPen(_qcolor(hue, 210, 190))
                painter.drawLine(x, 0, x, 11)
            painter.end()
            self.heat_bar.setPixmap(bar)

        def _update_legend(self):
            mode = getattr(self, "_color_mode", None) or "type"
            show_type = mode == "type"
            show_heat = mode == "heat"
            for w in (
                self.swatch_inst, self.lbl_inst,
                self.swatch_uniq, self.lbl_uniq,
            ):
                w.setVisible(show_type)
            for w in (self.lbl_heat_high, self.heat_bar, self.lbl_heat_low):
                w.setVisible(show_heat)

        def set_pick_handler(self, fn):
            self._on_pick = fn

        def set_mesh_rows(self, rows):
            self._rows = list(rows or [])
            self.canvas.set_rows(
                self._rows, self._mode(), self._color_mode, self._color_seed)
            self._on_hover(u"")

        def set_highlight(self, paths):
            self.canvas.set_selected(paths)

        def _mode(self):
            return getattr(self, "_mode_name", None) or "faces"

        def _on_mode(self, idx):
            data = self.combo.itemData(idx) if idx >= 0 else None
            mode = "faces"
            try:
                if data is not None and "export" in str(data):
                    mode = "export"
            except Exception:
                mode = "faces"
            self._mode_name = mode
            self.canvas.set_rows(
                self._rows, mode, self._color_mode, self._color_seed)

        def _on_color(self, idx):
            mode = "type"
            if idx == 1:
                mode = "random"
            elif idx == 2:
                mode = "heat"
            self._color_mode = mode
            if mode == "random":
                import time
                self._color_seed = int(time.time() * 1000) % 1000000 or 1
            self.canvas.set_color(mode, self._color_seed)
            self._update_legend()

        def _on_picked(self, paths):
            if self._on_pick:
                try:
                    self._on_pick(paths)
                except Exception:
                    pass

        def _on_hover(self, text):
            if text:
                self.lbl_status.setText(text)
                return
            try:
                import i18n
                self.lbl_status.setText(i18n.tr("ui_size_hint"))
            except Exception:
                self.lbl_status.setText("")
