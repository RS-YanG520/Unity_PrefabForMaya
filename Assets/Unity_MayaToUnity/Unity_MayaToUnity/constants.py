# -*- coding: utf-8 -*-
"""Maya → Unity 导出器的核心常量与约定。

对标 UnrealToUnity 的做法：Unity 资源引用靠「GUID + fileID」两要素。
本插件在 Maya 侧手写这些值，fileID 采用与 UnrealToUnity 一致的固定区间，
GUID 用字符串的确定性 md5 生成（32 位 hex，Unity 完全兼容）。

注意：本文件运行在 Maya 2018（Python 2.7）。
"""
from __future__ import print_function

import hashlib


# =====================================================================
# Unity 内置资源 GUID（Built-in 渲染管线）
# =====================================================================
# Unity 内置资源（Standard shader、默认材质等）的 GUID 都以 f000... 结尾。
BUILTIN_GUID_PREFIX = "0000000000000000f000000000000000"
# 内置 Standard shader 的完整 GUID（m_Shader 引用用它，fileID 为 46）。
STANDARD_SHADER_GUID = "0000000000000000f000000000000000"


# =====================================================================
# fileID 固定约定（与 UnrealToUnity 保持一致）
# =====================================================================
# FBX / Prefab 内部对象的 fileID。写进 .meta 的 fileIDToRecycleName 后，
# Unity 导入时会按这些值分配，于是场景/材质里就能用确定值引用。
FILEID_ROOT_GAMEOBJECT = 100000      # FBX / Prefab 内的根 GameObject
FILEID_ROOT_TRANSFORM = 400000       # 根 Transform
FILEID_MESH_RENDERER = 2300000       # MeshRenderer
FILEID_MESH_FILTER = 3300000         # MeshFilter
FILEID_MESH = 4300000                # FBX 内第一个 Mesh 子资产
FILEID_MESH_SECOND = 4300002         # FBX 内第二个 Mesh（LOD / 碰撞）
FILEID_MATERIAL = 2100000            # 材质 mainObject 的 fileID
FILEID_TEXTURE = 2800000             # 贴图 mainObject 的 fileID
FILEID_PREFAB_SOURCE = 100100000     # PrefabInstance 的 m_SourcePrefab 固定值
FILEID_SHADER_STANDARD = 46          # 内置 Standard shader 的 fileID


def _to_bytes(value):
    """把 str / unicode 统一成 bytes（Python 2 下 hashlib 需要 str）。"""
    if isinstance(value, unicode):
        return value.encode("utf-8")
    return value


def guid_from_string(value):
    """由字符串生成确定性 GUID（32 位十六进制）。

    同一字符串永远得到同一 GUID，不同名字得到不同 GUID，
    用于生成 FBX / 贴图 / 材质 / Prefab 的 .meta guid。
    """
    return hashlib.md5(_to_bytes(value)).hexdigest()


def guid_from_mesh_name(mesh_name):
    """由 mesh 名生成 GUID，与文件名保持一一对应。"""
    return guid_from_string(mesh_name)
