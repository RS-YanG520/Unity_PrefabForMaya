# -*- coding: utf-8 -*-
"""调试与诊断工具。

用途：
  - set_debug(True) 开启导出过程日志：实时打印到脚本编辑器；
    导出的 `<输出目录>/export_report.log` 始终会写入完整日志。
  - diagnose_scene() 做 dry-run：不导出，返回汇总 + 模型列表（面数/尺寸）。
"""
from __future__ import print_function

import maya.cmds as cmds

from constants import to_maya_ui


DEBUG = False
LOG = []


def set_debug(enabled):
    """开启 / 关闭控制台实时打印（导出报告文件始终写入）。"""
    global DEBUG
    DEBUG = bool(enabled)


def log(msg, warn=False):
    """记录一条日志；DEBUG 开启时同步打印（warn=True 用 warning 高亮）。"""
    msg = to_maya_ui(msg)
    LOG.append(msg)
    if DEBUG:
        if warn:
            cmds.warning(u"Unity_MayaToUnity: " + msg)
        else:
            print(u"[Unity_MayaToUnity] " + msg)


def clear_log():
    del LOG[:]


def get_report():
    """返回当前累积的日志文本（供写入 export_report.log）。"""
    return "\n".join(LOG)


def _f(v):
    if isinstance(v, float) and v == int(v):
        return str(int(v))
    return "%.3g" % v if isinstance(v, float) else repr(v)


def _fmt_vec3(v):
    return "(%s, %s, %s)" % (_f(v[0]), _f(v[1]), _f(v[2]))


def _fmt_quat(q):
    return "(%s, %s, %s, %s)" % (_f(q[0]), _f(q[1]), _f(q[2]), _f(q[3]))


def _fmt_color(c):
    return "(%s, %s, %s, %s)" % (_f(c[0]), _f(c[1]), _f(c[2]), _f(c[3]))


def _short_name(node):
    name = (node or "").split("|")[-1]
    if ":" in name:
        name = name.split(":")[-1]
    return name or node


def _fmt_bytes(num_bytes):
    """可读文件大小。"""
    try:
        n = float(num_bytes)
    except Exception:
        n = 0.0
    if n < 0:
        n = 0.0
    if n < 1024:
        return "%d B" % int(round(n))
    if n < 1024 * 1024:
        return "%.1f KB" % (n / 1024.0)
    return "%.2f MB" % (n / (1024.0 * 1024.0))


def _mesh_geo_stats(transform):
    """返回 (faces, verts, shape_paths)。"""
    shapes = cmds.listRelatives(
        transform, shapes=True, noIntermediate=True, fullPath=True) or []
    faces = 0
    verts = 0
    shape_paths = []
    for sh in shapes:
        if cmds.nodeType(sh) != "mesh":
            continue
        shape_paths.append(sh)
        try:
            faces += int(cmds.polyEvaluate(sh, face=True) or 0)
        except Exception:
            pass
        try:
            verts += int(cmds.polyEvaluate(sh, vertex=True) or 0)
        except Exception:
            pass
    return faces, verts, shape_paths


def _estimate_export_bytes(faces, verts):
    """预计导出 FBX（Binary）体积。

    按顶点/面数据量估算，接近 Maya 默认 Binary 导出量级（不含贴图）。
    """
    # Binary：pos+normal+uv 约 32B/vert，索引约 12B/face，另加节点开销
    return int(verts * 32 + faces * 16 + 8192)


def _mesh_uv_set_names(transform):
    """返回 mesh 的 UV Set 名称列表（顺序即 UV0、UV1…）。"""
    shapes = cmds.listRelatives(
        transform, shapes=True, noIntermediate=True, fullPath=True) or []
    for sh in shapes:
        if cmds.nodeType(sh) != "mesh":
            continue
        try:
            names = cmds.polyUVSet(sh, query=True, allUVSets=True) or []
        except Exception:
            names = []
        # 部分版本返回扁平列表；去空、保序
        out = []
        for n in names:
            if not n:
                continue
            # 偶发成对出现 (name, name)
            if isinstance(n, (list, tuple)):
                n = n[0] if n else ""
            n = str(n).strip()
            if n and n not in out:
                out.append(n)
        return out
    return []


def _collect_mesh_row(transform, kind, prefab_name=None):
    faces, verts, shapes = _mesh_geo_stats(transform)
    export_bytes = _estimate_export_bytes(faces, verts)
    shape_key = shapes[0] if shapes else ""
    uv_sets = _mesh_uv_set_names(transform)
    uv0 = uv_sets[0] if len(uv_sets) > 0 else ""
    uv1 = uv_sets[1] if len(uv_sets) > 1 else ""
    return {
        "name": _short_name(transform),
        "path": transform,
        "kind": kind,
        "prefab": prefab_name or "",
        "faces": faces,
        "verts": verts,
        "export_bytes": export_bytes,
        "size": _fmt_bytes(export_bytes),
        "shape": shape_key,
        "uv_sets": uv_sets,
        "uv0": uv0,
        "uv1": uv1,
    }


def diagnose_scene(selection=None):
    """dry-run 分析场景。

    返回 dict：
      summary  - 模型总数 / 材质 / 贴图 / 总面数（按唯一 shape 计）
      meshes   - 每个模型一行（名称、类型、面数、尺寸）
      report   - 纯文本报告（脚本编辑器 / 兼容旧调用）
    """
    from instance_detector import analyze
    from material_mapper import collect_materials
    from coords import transform_to_unity
    from light_mapper import map_lights, ambient_color_from
    from camera_mapper import map_cameras

    groups, singletons = analyze(selection)
    materials, textures = collect_materials()
    scene_lights, ambients = map_lights(selection)
    ambient_col = ambient_color_from(ambients)
    scene_cameras = map_cameras(selection)

    meshes = []
    for g in groups:
        base_long = cmds.ls(g.base, long=True) or [g.base]
        base_long = base_long[0]
        member_paths = []
        for m in g.members:
            member_paths.append((cmds.ls(m, long=True) or [m])[0])
        for m in g.members:
            row = _collect_mesh_row(m, "instance", g.prefab_name)
            m_long = (cmds.ls(m, long=True) or [m])[0]
            row["member_paths"] = list(member_paths)
            row["base_name"] = g.prefab_name
            if m_long != base_long:
                row["export_bytes"] = 0
                row["size"] = u"(shared)"
                row["shared"] = True
            else:
                row["shared"] = False
            meshes.append(row)
    for s in singletons:
        row = _collect_mesh_row(s, "unique", _short_name(s))
        row["shared"] = False
        row["base_name"] = u""
        row["member_paths"] = [row.get("path") or s]
        meshes.append(row)

    # 总面数 / 预估导出：每组只计 base + 各 singleton
    unique_faces = {}
    unique_export = {}
    for g in groups:
        row = _collect_mesh_row(g.base, "instance", g.prefab_name)
        key = "group:" + g.prefab_name
        unique_faces[key] = row["faces"]
        unique_export[key] = row["export_bytes"]
    for s in singletons:
        row = _collect_mesh_row(s, "unique", _short_name(s))
        key = row["shape"] or row["path"]
        unique_faces[key] = row["faces"]
        unique_export[key] = row["export_bytes"]
    total_faces = sum(unique_faces.values())
    total_export_bytes = sum(unique_export.values())

    summary = {
        "model_count": len(meshes),
        "material_count": len(materials),
        "texture_count": len(textures),
        "total_faces": total_faces,
        "total_export_bytes": total_export_bytes,
        "total_export_size": _fmt_bytes(total_export_bytes),
        "instance_groups": len(groups),
        "singletons": len(singletons),
        "light_count": len(scene_lights),
        "ambient_count": len(ambients),
        "camera_count": len(scene_cameras),
    }

    lines = []
    lines.append("========== Unity_MayaToUnity Scene Diagnosis ==========")
    lines.append(
        "Models: %d  |  Materials: %d  |  Textures: %d  |  Total Faces: %d  |  Est. Export: %s"
        % (
            summary["model_count"],
            summary["material_count"],
            summary["texture_count"],
            summary["total_faces"],
            summary["total_export_size"],
        )
    )
    lines.append(
        "Instance groups: %d  |  Singletons: %d  |  Lights: %d  |  Ambient: %d  |  Cameras: %d"
        % (
            summary["instance_groups"],
            summary["singletons"],
            summary["light_count"],
            summary["ambient_count"],
            summary["camera_count"],
        )
    )
    lines.append("")
    lines.append("---- Mesh List ----")
    lines.append(
        "%-18s %6s %4s %10s %-16s %-12s %s"
        % ("Name", "Faces", "Inst", "Est.Size", "Base", "UV0", "UV1")
    )
    for row in meshes:
        inst = "yes" if row.get("kind") == "instance" else "no"
        lines.append(
            "%-18s %6d %4s %10s %-16s %-12s %s"
            % (
                (row["name"] or "")[:18],
                row["faces"],
                inst,
                row["size"],
                (row.get("base_name") or "-")[:16],
                (row.get("uv0") or "-")[:12],
                row.get("uv1") or "-",
            )
        )

    lines.append("")
    lines.append("Instance groups detail: %d" % len(groups))
    for g in groups:
        lines.append(
            "  [Group] prefab=%s  shape=%s  members=%d"
            % (g.prefab_name, g.shape, len(g.members))
        )
        for m in g.members:
            pos, rot, scale = transform_to_unity(m)
            lines.append("      - %s" % m)
            lines.append(
                "          pos=%s  rot=%s  scale=%s"
                % (_fmt_vec3(pos), _fmt_quat(rot), _fmt_vec3(scale))
            )

    lines.append("Singleton meshes: %d" % len(singletons))
    for s in singletons:
        pos, rot, scale = transform_to_unity(s)
        lines.append(
            "  [Singleton] %s  pos=%s  rot=%s  scale=%s"
            % (s, _fmt_vec3(pos), _fmt_quat(rot), _fmt_vec3(scale))
        )

    lines.append("Materials: %d" % len(materials))
    for m in materials:
        tex = m.albedo_texture.name if m.albedo_texture else "(none)"
        metal_tex = (
            m.metallic_texture.name
            if getattr(m, "metallic_texture", None)
            else "(none)"
        )
        normal_tex = (
            m.normal_texture.name
            if getattr(m, "normal_texture", None)
            else "(none)"
        )
        lines.append(
            "  - %s  color=%s  metallic=%s  smoothness=%s  albedo=%s"
            "  metallicMap=%s  normal=%s"
            % (
                m.name,
                _fmt_color(m.color),
                m.metallic,
                m.smoothness,
                tex,
                metal_tex,
                normal_tex,
            )
        )

    lines.append("Textures: %d" % len(textures))
    for t in textures:
        lines.append("  - %s -> %s" % (t.name, t.src_path))

    lines.append("Lights: %d" % len(scene_lights))
    _unity_names = {0: "Spot", 1: "Directional", 2: "Point"}
    for L in scene_lights:
        pos, rot, scale = transform_to_unity(L["path"])
        ut = _unity_names.get(L.get("unity_type"), "?")
        c = L.get("color") or (1, 1, 1)
        lines.append(
            "  - %s  maya=%s -> Unity %s  intensity=%s  color=(%.3f,%.3f,%.3f)"
            "  shadows=%s"
            % (
                L.get("name"),
                L.get("maya_type"),
                ut,
                L.get("intensity"),
                c[0], c[1], c[2],
                L.get("shadows"),
            )
        )
        lines.append(
            "      pos=%s  rot=%s  scale=%s"
            % (_fmt_vec3(pos), _fmt_quat(rot), _fmt_vec3(scale))
        )
    if ambients:
        lines.append("Ambient lights: %d" % len(ambients))
        for a in ambients:
            c = a.get("color") or (1, 1, 1)
            lines.append(
                "  - %s  intensity=%s  color=(%.3f,%.3f,%.3f)"
                % (a.get("name"), a.get("intensity"), c[0], c[1], c[2])
            )
        if ambient_col:
            lines.append(
                "  -> RenderSettings ambient ~= (%.3f, %.3f, %.3f)"
                % (ambient_col[0], ambient_col[1], ambient_col[2])
            )

    lines.append("Cameras: %d" % len(scene_cameras))
    for C in scene_cameras:
        pos, rot, scale = transform_to_unity(C["path"])
        lines.append(
            "  - %s  fov=%.1f  near=%.3f  far=%.3f  ortho=%s"
            % (
                C.get("name"),
                float(C.get("fov") or 0),
                float(C.get("near") or 0),
                float(C.get("far") or 0),
                "yes" if C.get("orthographic") else "no",
            )
        )
        lines.append(
            "      pos=%s  rot=%s  scale=%s"
            % (_fmt_vec3(pos), _fmt_quat(rot), _fmt_vec3(scale))
        )

    lines.append("====================================================")

    report = "\n".join(lines)
    print(report)
    return {
        "summary": summary,
        "meshes": meshes,
        "lights": scene_lights,
        "ambients": ambients,
        "cameras": scene_cameras,
        "report": report,
    }
