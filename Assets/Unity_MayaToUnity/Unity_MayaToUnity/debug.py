# -*- coding: utf-8 -*-
"""调试与诊断工具。

用途：
  - set_debug(True) 开启导出过程日志：实时打印到脚本编辑器；
    导出的 `<输出目录>/export_report.log` 始终会写入完整日志。
  - diagnose_scene() 做 dry-run：不导出，只分析当前场景并打印诊断报告，
    用于排查「为什么没识别出实例 / 材质没对上」等问题。
"""
from __future__ import print_function

import maya.cmds as cmds


DEBUG = False
LOG = []


def set_debug(enabled):
    """开启 / 关闭控制台实时打印（导出报告文件始终写入）。"""
    global DEBUG
    DEBUG = bool(enabled)


def log(msg, warn=False):
    """记录一条日志；DEBUG 开启时同步打印（warn=True 用 warning 高亮）。"""
    LOG.append(msg)
    if DEBUG:
        if warn:
            cmds.warning("Unity_MayaToUnity: " + msg)
        else:
            print("[Unity_MayaToUnity] " + msg)


def clear_log():
    del LOG[:]


def get_report():
    """返回当前累积的日志文本（供写入 export_report.log）。"""
    return "\n".join(LOG)


def _f(v):
    if isinstance(v, float) and v == int(v):
        return str(int(v))
    return repr(v)


def _fmt_vec3(v):
    return "(%s, %s, %s)" % (_f(v[0]), _f(v[1]), _f(v[2]))


def _fmt_quat(q):
    return "(%s, %s, %s, %s)" % (_f(q[0]), _f(q[1]), _f(q[2]), _f(q[3]))


def _fmt_color(c):
    return "(%s, %s, %s, %s)" % (_f(c[0]), _f(c[1]), _f(c[2]), _f(c[3]))


def diagnose_scene(selection=None):
    """dry-run 分析场景：打印实例组 / 唯一网格 / 材质 / 贴图及坐标预览。

    不导出任何文件。返回报告文本。
    """
    from instance_detector import analyze
    from material_mapper import collect_materials
    from coords import transform_to_unity

    groups, singletons = analyze(selection)
    materials, textures = collect_materials()

    lines = []
    lines.append("========== Unity_MayaToUnity 场景诊断 ==========")
    lines.append("实例组: %d 组" % len(groups))
    for g in groups:
        lines.append("  [实例组] prefab=%s  shape=%s  成员=%d" % (g.prefab_name, g.shape, len(g.members)))
        for m in g.members:
            pos, rot, scale = transform_to_unity(m)
            lines.append("      - %s" % m)
            lines.append("          pos=%s  rot=%s  scale=%s" % (
                _fmt_vec3(pos), _fmt_quat(rot), _fmt_vec3(scale)))

    lines.append("唯一网格: %d 个" % len(singletons))
    for s in singletons:
        pos, rot, scale = transform_to_unity(s)
        lines.append("  [唯一] %s  pos=%s  rot=%s  scale=%s" % (
            s, _fmt_vec3(pos), _fmt_quat(rot), _fmt_vec3(scale)))

    lines.append("材质: %d 个" % len(materials))
    for m in materials:
        tex = m.albedo_texture.name if m.albedo_texture else "(无贴图)"
        lines.append("  - %s  color=%s  metallic=%s  smoothness=%s  tex=%s" % (
            m.name, _fmt_color(m.color), m.metallic, m.smoothness, tex))

    lines.append("贴图: %d 个" % len(textures))
    for t in textures:
        lines.append("  - %s -> %s" % (t.name, t.src_path))

    lines.append("===============================================")

    report = "\n".join(lines)
    print(report)
    return report
