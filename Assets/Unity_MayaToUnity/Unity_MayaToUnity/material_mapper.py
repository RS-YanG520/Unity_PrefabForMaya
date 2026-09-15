# -*- coding: utf-8 -*-
"""Maya 材质 / 贴图 → Unity 材质映射。

第一版做基础映射：
  Maya 主颜色（baseColor / color）→ Unity _Color
  Maya 主贴图（主颜色连接的 file 节点）→ Unity _MainTex
  Arnold metalness / specularRoughness → Unity _Metallic / _Glossiness

复杂 shading network（程序纹理、混合节点）不在第一版范围内，需要烘焙。
"""
from __future__ import print_function

import os

import maya.cmds as cmds

from constants import guid_from_string


class Texture(object):
    """一张待导出的贴图。"""

    def __init__(self, name, guid, src_path):
        self.name = name
        self.guid = guid
        self.src_path = src_path  # Maya 里的源文件绝对路径


class Material(object):
    """一个待导出的 Unity 材质。"""

    def __init__(self, name, guid):
        self.name = name
        self.guid = guid
        self.color = (1.0, 1.0, 1.0, 1.0)
        self.albedo_texture = None  # Texture 或 None
        self.metallic = 0.0
        self.smoothness = 0.5


# Maya 默认材质，跳过不导出
_DEFAULT_MATERIALS = ('lambert1', 'standardSurface1', 'particleCloud1', 'initialShadingGroup')


def _get_color(mat):
    """读取材质主颜色，返回 (r, g, b, a)。"""
    for attr in ('baseColor', 'color'):
        if cmds.attributeQuery(attr, node=mat, exists=True):
            try:
                val = cmds.getAttr(mat + '.' + attr)[0]
                return (val[0], val[1], val[2], 1.0)
            except Exception:
                continue
    return (1.0, 1.0, 1.0, 1.0)


def _get_texture(mat, color_attr):
    """读取主颜色连接的 file 节点，返回 (texture_name, src_path) 或 None。"""
    files = cmds.listConnections(mat + '.' + color_attr, type='file') or []
    if not files:
        return None
    path = cmds.getAttr(files[0] + '.fileTextureName')
    if not path:
        return None
    return os.path.basename(path), path


def collect_materials():
    """遍历 shadingEngine，返回 (materials, textures) 两个列表。"""
    materials = []
    textures = []
    texture_by_name = {}

    for se in cmds.ls(type='shadingEngine'):
        mats = cmds.listConnections(se + '.surfaceShader') or []
        if not mats:
            continue
        mat = mats[0]
        leaf = mat.split('|')[-1]
        if leaf in _DEFAULT_MATERIALS or leaf in [m.name for m in materials]:
            continue

        material = Material(leaf, guid_from_string(leaf))

        # 主颜色
        material.color = _get_color(mat)

        # 主贴图（优先 Arnold baseColor，其次标准 color）
        for color_attr in ('baseColor', 'color'):
            if cmds.attributeQuery(color_attr, node=mat, exists=True):
                tex = _get_texture(mat, color_attr)
                if tex:
                    tex_name, tex_path = tex
                    if tex_name not in texture_by_name:
                        texture = Texture(tex_name, guid_from_string(tex_name), tex_path)
                        texture_by_name[tex_name] = texture
                        textures.append(texture)
                    material.albedo_texture = texture_by_name[tex_name]
                break

        # Arnold PBR 参数
        if cmds.attributeQuery('metalness', node=mat, exists=True):
            try:
                material.metallic = cmds.getAttr(mat + '.metalness')
            except Exception:
                pass
        if cmds.attributeQuery('specularRoughness', node=mat, exists=True):
            try:
                material.smoothness = 1.0 - cmds.getAttr(mat + '.specularRoughness')
            except Exception:
                pass

        materials.append(material)

    return materials, textures
