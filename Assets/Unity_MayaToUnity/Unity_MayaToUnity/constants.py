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

# 实例组「基础模型」导出文件名后缀，便于和独立网格区分
# 例：SM_Bucket_Small → SM_Bucket_Small_Inst.fbx / .prefab
INSTANCE_BASE_SUFFIX = "_Inst"

# 写在共享 mesh shape 上：覆盖实例组导出 Prefab/FBX 名（诊断列表可改）
MTU_PREFAB_NAME_ATTR = "mtu_prefabName"


def _to_bytes(value):
    """把 str / unicode 统一成 bytes（Python 2 下 hashlib 需要 str）。"""
    if isinstance(value, unicode):
        return value.encode("utf-8")
    return value


def to_unicode(value):
    """把 Maya / Windows 返回的 str 尽量解成 unicode。

    中文版 Maya 常把路径或异常信息以 GBK/mbcs 字节返回；若直接当 UTF-8
    传给 confirmDialog / warning，会报「收到的字符串不是有效的 utf8 字符串」。
    """
    if value is None:
        return u""
    if isinstance(value, unicode):
        return value
    if not isinstance(value, basestring):
        try:
            value = str(value)
        except Exception:
            value = repr(value)
    for enc in ("utf-8", "gbk", "mbcs"):
        try:
            return value.decode(enc)
        except Exception:
            continue
    return value.decode("utf-8", "replace")


def to_maya_ui(value):
    """供 cmds.confirmDialog / cmds.warning 使用的安全 unicode 文案。"""
    return to_unicode(value)


def guid_from_string(value):
    """由字符串生成确定性 GUID（32 位十六进制）。

    同一字符串永远得到同一 GUID，不同名字得到不同 GUID，
    用于生成 FBX / 贴图 / 材质 / Prefab 的 .meta guid。
    """
    return hashlib.md5(_to_bytes(value)).hexdigest()


def guid_from_mesh_name(mesh_name):
    """由 mesh 名生成 GUID，与文件名保持一一对应。"""
    return guid_from_string(mesh_name)
