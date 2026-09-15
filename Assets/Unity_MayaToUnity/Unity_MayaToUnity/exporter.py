# -*- coding: utf-8 -*-
"""导出主流程。

整体流程：
  1. 收集材质 / 贴图（material_mapper）
  2. 识别实例组（instance_detector）
  3. 每个实例组的 base（及每个唯一网格）导出一个 FBX
  4. 解析 FBX 内 mesh 名 → 写 FBX .meta（fileIDToRecycleName 固定 fileID）
  5. 生成 Prefab（.prefab + .meta）、材质（.mat + .meta）、贴图（复制 + .meta）
  6. 生成场景（.unity，Prefab 实例按 Maya 世界变换摆放）
"""
from __future__ import print_function

import os
import re
import shutil

import maya.cmds as cmds

from constants import guid_from_string
from coords import transform_to_unity
from instance_detector import analyze, strip_trailing_digits
from material_mapper import collect_materials
import yaml_writer
import debug


def _write_text(path, text):
    """写 UTF-8 文本（Python 2 下 str/unicode 统一按字节写）。"""
    if isinstance(text, unicode):
        text = text.encode("utf-8")
    with open(path, "wb") as fh:
        fh.write(text)


def _load_fbx_plugin():
    if not cmds.pluginInfo('fbxmaya', q=True, loaded=True):
        cmds.loadPlugin('fbxmaya')


def _export_fbx(node, out_path):
    """把单个 transform 导出为 FBX（只导出选中）。"""
    orig = cmds.ls(selection=True)
    cmds.select(node, r=True)
    try:
        cmds.FBXExport(f=out_path, s=True)
    finally:
        if orig:
            cmds.select(orig, r=True)
        else:
            cmds.select(clear=True)


def _read_fbx_mesh_name(fbx_path):
    """从 ASCII FBX 里提取主 Mesh 节点名；失败返回 None。"""
    try:
        with open(fbx_path, "rb") as fh:
            data = fh.read(300000)
    except Exception:
        return None
    try:
        text = data.decode("utf-8", "ignore")
    except Exception:
        text = data
    names = re.findall(r'"Geometry::([^"]+)"\s*,\s*"Mesh"', text)
    if not names:
        return None
    for n in names:
        if "_LOD" not in n:
            return n
    return names[0]


def _shape_to_material(materials):
    """建立 shape 短名 -> Material 的映射（通过 shadingEngine 成员反查）。"""
    result = {}
    for se in cmds.ls(type='shadingEngine'):
        mats = cmds.listConnections(se + '.surfaceShader') or []
        if not mats:
            continue
        leaf = mats[0].split('|')[-1]
        mat = next((m for m in materials if m.name == leaf), None)
        if mat is None:
            continue
        for member in cmds.sets(se, q=True) or []:
            if cmds.nodeType(member) == 'mesh':
                result[member.split('|')[-1]] = mat
    return result


def _make_item(transform, shape, prefab_name, used):
    """构造导出项，并对重名 Prefab 追加序号。"""
    name = prefab_name
    if name in used:
        used[name] += 1
        name = "%s_%d" % (prefab_name, used[name])
    else:
        used[name] = 1
    return {"transform": transform, "shape": shape, "prefab_name": name}


def export(output_dir, selection=None, scene_name="MayaToUnity"):
    """导出整个场景（或选中对象）为 Unity 项目资源。

    返回生成的文件路径列表；没有可导出的网格时返回 None。
    """
    if not os.path.isdir(output_dir):
        os.makedirs(output_dir)

    materials_dir = os.path.join(output_dir, "Materials")
    textures_dir = os.path.join(output_dir, "Textures")
    for d in (materials_dir, textures_dir):
        if not os.path.isdir(d):
            os.makedirs(d)

    _load_fbx_plugin()

    debug.clear_log()
    debug.log("Export started -> %s" % output_dir)
    debug.log("Scope: %s" % ("selection" if selection else "whole scene"))
    debug.log("Debug mode: %s" % ("on" if debug.DEBUG else "off"))

    # 1. 材质 / 贴图
    materials, textures = collect_materials()
    for m in materials:
        m.guid = guid_from_string("mat:" + m.name)
    for t in textures:
        t.guid = guid_from_string("tex:" + t.name)

    debug.log("Materials: %d, Textures: %d" % (len(materials), len(textures)))

    shape_mat = _shape_to_material(materials)

    # 2. 实例分析
    groups, singletons = analyze(selection)
    debug.log("Instance groups: %d, singletons: %d" % (len(groups), len(singletons)))

    # 3. 组装导出项（去重 Prefab 名）
    used = {}
    export_items = []
    for g in groups:
        export_items.append(_make_item(g.base, g.shape, g.prefab_name, used))
    for s in singletons:
        leaf = s.split('|')[-1]
        shape = (cmds.listRelatives(s, shapes=True, noIntermediate=True) or [None])[0]
        export_items.append(_make_item(s, shape, strip_trailing_digits(leaf), used))

    if not export_items:
        cmds.warning("Unity_MayaToUnity: no mesh to export.")
        return None

    # 4. 每个导出项：FBX + meta + prefab + prefab meta
    prefab_guid_by_transform = {}
    generated = []

    for item in export_items:
        t = item["transform"]
        shape = item["shape"]
        prefab_name = item["prefab_name"]

        fbx_guid = guid_from_string("fbx:" + prefab_name)
        prefab_guid = guid_from_string("prefab:" + prefab_name)

        debug.log("Item: transform=%s shape=%s prefab=%s" % (t, shape, prefab_name))

        fbx_path = os.path.join(output_dir, prefab_name + ".fbx")
        _export_fbx(t, fbx_path)

        parsed_mesh = _read_fbx_mesh_name(fbx_path)
        if parsed_mesh is None:
            debug.log("Cannot parse FBX mesh name, fallback to shape: %s" % (shape or prefab_name), warn=True)
        mesh_name = parsed_mesh or shape or prefab_name
        debug.log("  FBX mesh name: %s" % mesh_name)
        _write_text(fbx_path + ".meta", yaml_writer.fbx_meta(fbx_guid, mesh_name))

        shape_key = shape.split('|')[-1] if shape else None
        material = shape_mat.get(shape_key)
        material_guid = material.guid if material else None
        debug.log("  fbx_guid=%s prefab_guid=%s material=%s" % (
            fbx_guid, prefab_guid, material.name if material else "(none)"))

        prefab_path = os.path.join(output_dir, prefab_name + ".prefab")
        _write_text(prefab_path, yaml_writer.prefab_file(prefab_name, fbx_guid, material_guid))
        _write_text(prefab_path + ".meta", yaml_writer.prefab_meta(prefab_guid))

        prefab_guid_by_transform[t] = prefab_guid
        generated.append(prefab_path)

    # 5. 材质文件（去重）
    written_mats = set()
    for item in export_items:
        shape = item["shape"]
        shape_key = shape.split('|')[-1] if shape else None
        material = shape_mat.get(shape_key)
        if material is None or material.name in written_mats:
            continue
        written_mats.add(material.name)
        mat_path = os.path.join(materials_dir, material.name + ".mat")
        _write_text(mat_path, yaml_writer.material_file(material))
        _write_text(mat_path + ".meta", yaml_writer.material_meta(material.guid))
        generated.append(mat_path)

    # 6. 贴图文件（复制 + meta）
    for tex in textures:
        dst = os.path.join(textures_dir, tex.name)
        if os.path.isfile(tex.src_path) and not os.path.isfile(dst):
            try:
                shutil.copyfile(tex.src_path, dst)
            except Exception as e:
                cmds.warning("Unity_MayaToUnity: failed to copy texture %s: %s" % (tex.src_path, e))
        _write_text(dst + ".meta", yaml_writer.texture_meta(tex.guid))
        generated.append(dst)

    # 7. 场景：Prefab 实例按 Maya 世界变换摆放
    blocks = []
    instance_id = 1000
    transform_id = 1000000

    ordered = []
    for g in groups:
        ordered.extend(g.members)
    ordered.extend(singletons)

    for t in ordered:
        pos, rot, scale = transform_to_unity(t)
        blocks.append(yaml_writer.prefab_instance_block(
            instance_id, transform_id, prefab_guid_by_transform[t],
            t.split('|')[-1], pos, rot, scale))
        instance_id += 1
        transform_id += 1

    debug.log("Scene instances: %d" % len(ordered))

    scene_path = os.path.join(output_dir, scene_name + ".unity")
    _write_text(scene_path, yaml_writer.scene_file("".join(blocks)))
    generated.append(scene_path)

    # 写诊断报告（始终写入，排查问题有据可查）
    report_path = os.path.join(output_dir, "export_report.log")
    _write_text(report_path, debug.get_report())
    generated.append(report_path)

    debug.log("Export done, %d files" % len(generated))

    return generated
