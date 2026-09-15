# -*- coding: utf-8 -*-
"""Maya → Unity 坐标 / 单位 / 轴系转换。

Maya：Y-up、右手系、单位 cm，旋转用 XYZ 欧拉顺序。
Unity：Y-up、左手系、单位 m，旋转用四元数。

转换规则：
  position : (x/100, y/100, -z/100)     # cm→m，且 Z 轴取反（右手→左手）
  rotation : 四元数 (qx, qy, -qz, qw)   # 绕 Z 镜像
  scale    : 不变

通过 Maya 世界矩阵（行主序）手动分解得到，避免依赖易错的高层 API。
"""
from __future__ import print_function

import math

import maya.cmds as cmds


def world_matrix(node):
    """返回节点的世界变换矩阵（16 元素，行主序）。"""
    return cmds.xform(node, q=True, ws=True, matrix=True)


def rotation_matrix_to_quaternion(R):
    """3x3 旋转矩阵（列主序，即第 j 列为第 j 个基向量）→ 四元数 (x, y, z, w)。"""
    m00, m01, m02 = R[0][0], R[0][1], R[0][2]
    m10, m11, m12 = R[1][0], R[1][1], R[1][2]
    m20, m21, m22 = R[2][0], R[2][1], R[2][2]

    trace = m00 + m11 + m22
    if trace > 0.0:
        s = math.sqrt(trace + 1.0) * 2.0
        w = 0.25 * s
        x = (m21 - m12) / s
        y = (m02 - m20) / s
        z = (m10 - m01) / s
    elif m00 > m11 and m00 > m22:
        s = math.sqrt(1.0 + m00 - m11 - m22) * 2.0
        w = (m21 - m12) / s
        x = 0.25 * s
        y = (m01 + m10) / s
        z = (m02 + m20) / s
    elif m11 > m22:
        s = math.sqrt(1.0 + m11 - m00 - m22) * 2.0
        w = (m02 - m20) / s
        x = (m01 + m10) / s
        y = 0.25 * s
        z = (m12 + m21) / s
    else:
        s = math.sqrt(1.0 + m22 - m00 - m11) * 2.0
        w = (m10 - m01) / s
        x = (m02 + m20) / s
        y = (m12 + m21) / s
        z = 0.25 * s
    return (x, y, z, w)


def matrix_to_unity(m):
    """Maya 世界矩阵（16 元素，行主序）→ (position, quaternion, scale)，Unity 约定。"""
    # 平移：cm → m，Z 取反
    position = (m[12] / 100.0, m[13] / 100.0, -m[14] / 100.0)

    # 旋转+缩放的 3x3（行主序存储，列为基向量）
    col0 = (m[0], m[4], m[8])
    col1 = (m[1], m[5], m[9])
    col2 = (m[2], m[6], m[10])

    sx = math.sqrt(col0[0] * col0[0] + col0[1] * col0[1] + col0[2] * col0[2])
    sy = math.sqrt(col1[0] * col1[0] + col1[1] * col1[1] + col1[2] * col1[2])
    sz = math.sqrt(col2[0] * col2[0] + col2[1] * col2[1] + col2[2] * col2[2])

    if sx < 1e-12 or sy < 1e-12 or sz < 1e-12:
        # 退化缩放，直接给单位旋转
        return position, (0.0, 0.0, 0.0, 1.0), (sx, sy, sz)

    # 归一化旋转矩阵（每列除以对应 scale）
    R = [
        [m[0] / sx, m[1] / sy, m[2] / sz],
        [m[4] / sx, m[5] / sy, m[6] / sz],
        [m[8] / sx, m[9] / sy, m[10] / sz],
    ]

    # 镜像 Z 轴：R_unity = S · R · S，S = diag(1, 1, -1)
    Ru = [
        [R[0][0], R[0][1], -R[0][2]],
        [R[1][0], R[1][1], -R[1][2]],
        [-R[2][0], -R[2][1], R[2][2]],
    ]

    quaternion = rotation_matrix_to_quaternion(Ru)
    return position, quaternion, (sx, sy, sz)


def transform_to_unity(node):
    """直接由 Maya 节点名得到 Unity 约定下的 (position, quaternion, scale)。"""
    return matrix_to_unity(world_matrix(node))
