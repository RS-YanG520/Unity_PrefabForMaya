# -*- coding: utf-8 -*-
"""Maya 材质 / 贴图 → Unity 材质映射。

基础映射：
  Maya 主颜色（baseColor / color）→ Unity _Color
  Maya 主贴图（主颜色连接的 file 节点）→ Unity _MainTex
  Maya metalness 上的 file（可含打包金属/光滑）→ Unity _MetallicGlossMap
  Maya normalCamera / bumpMapping（经 bump2d）→ Unity _BumpMap
  Arnold metalness / specularRoughness 数值 → Unity _Metallic / _Glossiness

约定：金属粗糙打包图只需接到 aiStandardSurface / standardSurface 的 Metalness；
Unity Built-in Standard 按 MetallicGlossMap 读：R=Metallic，A=Smoothness。
法线接 Geometry 下的 Bump Mapping / normalCamera。

复杂 shading network（程序纹理、混合节点）不在第一版范围内，需要烘焙。
"""
from __future__ import print_function

import os

import maya.cmds as cmds

from constants import guid_from_string


class Texture(object):
    """一张待导出的贴图。"""

    def __init__(self, name, guid, src_path, srgb=True, is_normal=False):
        self.name = name
        self.guid = guid
        self.src_path = src_path  # Maya 里的源文件绝对路径
        self.srgb = srgb          # False = 线性（金属/法线等）
        self.is_normal = is_normal


class Material(object):
    """一个待导出的 Unity 材质。"""

    def __init__(self, name, guid):
        self.name = name
        self.guid = guid
        self.color = (1.0, 1.0, 1.0, 1.0)
        self.albedo_texture = None       # Texture 或 None
        self.metallic_texture = None     # Texture 或 None → _MetallicGlossMap
        self.normal_texture = None       # Texture 或 None → _BumpMap
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


def _get_texture(mat, attr):
    """读取材质属性上连接的 file 节点，返回 (texture_name, src_path) 或 None。

    支持 outColor / outColorR 等单通道接到 float 属性（如 metalness）。
    """
    if not cmds.attributeQuery(attr, node=mat, exists=True):
        return None
    files = cmds.listConnections(mat + '.' + attr, type='file') or []
    if not files:
        srcs = cmds.listConnections(mat + '.' + attr, source=True, destination=False) or []
        for s in srcs:
            if cmds.nodeType(s) == 'file':
                files = [s]
                break
            nested = cmds.listConnections(s, source=True, destination=False, type='file') or []
            if nested:
                files = [nested[0]]
                break
    if not files:
        return None
    path = cmds.getAttr(files[0] + '.fileTextureName')
    if not path:
        return None
    return os.path.basename(path), path


def _register_texture(texture_by_name, textures, tex_name, tex_path, srgb=True, is_normal=False):
    """去重登记贴图；同名已存在则复用，必要时升级为法线设定。"""
    if tex_name not in texture_by_name:
        texture = Texture(
            tex_name, guid_from_string(tex_name), tex_path,
            srgb=srgb, is_normal=is_normal,
        )
        texture_by_name[tex_name] = texture
        textures.append(texture)
    else:
        texture = texture_by_name[tex_name]
        if is_normal:
            texture.is_normal = True
            texture.srgb = False
    return texture_by_name[tex_name]


def _get_normal_texture(mat):
    """从 normalCamera / bumpMapping 读取法线 file（可经 bump2d）。"""
    for attr in ('normalCamera', 'bumpMapping'):
        tex = _get_texture(mat, attr)
        if tex:
            return tex
    return None


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
                    material.albedo_texture = _register_texture(
                        texture_by_name, textures, tex_name, tex_path, srgb=True
                    )
                break

        # 金属度贴图：只认 metalness 上的 file → Unity Metallic
        tex = _get_texture(mat, 'metalness')
        if tex:
            tex_name, tex_path = tex
            material.metallic_texture = _register_texture(
                texture_by_name, textures, tex_name, tex_path, srgb=False
            )

        # 法线贴图：normalCamera / bumpMapping → Unity _BumpMap
        tex = _get_normal_texture(mat)
        if tex:
            tex_name, tex_path = tex
            material.normal_texture = _register_texture(
                texture_by_name, textures, tex_name, tex_path,
                srgb=False, is_normal=True,
            )

        # PBR 标量（有贴图时 Metallic 作 multiplier=1）
        if cmds.attributeQuery('metalness', node=mat, exists=True):
            try:
                if material.metallic_texture is None:
                    material.metallic = cmds.getAttr(mat + '.metalness')
                else:
                    material.metallic = 1.0
            except Exception:
                pass
        if cmds.attributeQuery('specularRoughness', node=mat, exists=True):
            try:
                material.smoothness = 1.0 - cmds.getAttr(mat + '.specularRoughness')
            except Exception:
                pass

        materials.append(material)

    return materials, textures
