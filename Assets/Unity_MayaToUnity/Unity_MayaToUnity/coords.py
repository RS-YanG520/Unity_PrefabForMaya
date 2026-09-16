# -*- coding: utf-8 -*-
"""Maya → Unity 坐标 / 单位转换。

Maya：Y-up、右手系、单位 cm。矩阵按行向量：p' = p * S * R * T。
Unity：Y-up、左手系、单位 m。

实测：
  - 位置：(-x/100, y/100, z/100)  （X 因视口轴向相反需取反；Z 只做 cm→m）
  - 缩放：取世界矩阵上 3x3 的**行**长度（不是列）。列长度在「非均匀缩放+旋转」
    时会把轴向搅在一起，例如 Maya sy=0.714 会被错成 ≈1.170。
  - 旋转：行归一化得到 R → S·R·S（S=diag(-1,1,1)）→ 四元数 → 共轭

通过 Maya 世界矩阵（行主序）手动分解得到。
"""
from __future__ import print_function

import math

import maya.cmds as cmds


def world_matrix(node):
    """返回节点的世界变换矩阵（16 元素，行主序）。"""
    return cmds.xform(node, q=True, ws=True, matrix=True)


def rotation_matrix_to_quaternion(R):
    """3x3 旋转矩阵 → 四元数 (x, y, z, w)。"""
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
    """Maya 世界矩阵（16 元素，行主序）→ (position, quaternion, scale)。"""
    position = (-m[12] / 100.0, m[13] / 100.0, m[14] / 100.0)

    # Maya: p' = p * S * R * T → 局部缩放在**行**上
    row0 = (m[0], m[1], m[2])
    row1 = (m[4], m[5], m[6])
    row2 = (m[8], m[9], m[10])

    sx = math.sqrt(row0[0] * row0[0] + row0[1] * row0[1] + row0[2] * row0[2])
    sy = math.sqrt(row1[0] * row1[0] + row1[1] * row1[1] + row1[2] * row1[2])
    sz = math.sqrt(row2[0] * row2[0] + row2[1] * row2[1] + row2[2] * row2[2])

    if sx < 1e-12 or sy < 1e-12 or sz < 1e-12:
        return position, (0.0, 0.0, 0.0, 1.0), (sx, sy, sz)

    # 每行除以该行缩放，得到纯旋转 R
    R = [
        [row0[0] / sx, row0[1] / sx, row0[2] / sx],
        [row1[0] / sy, row1[1] / sy, row1[2] / sy],
        [row2[0] / sz, row2[1] / sz, row2[2] / sz],
    ]

    # 与位置 X 取反一致：R' = S · R · S，S = diag(-1, 1, 1)
    Ru = [
        [R[0][0], -R[0][1], -R[0][2]],
        [-R[1][0], R[1][1], R[1][2]],
        [-R[2][0], R[2][1], R[2][2]],
    ]

    qx, qy, qz, qw = rotation_matrix_to_quaternion(Ru)
    quaternion = (-qx, -qy, -qz, qw)
    return position, quaternion, (sx, sy, sz)


def transform_to_unity(node):
    """直接由 Maya 节点名得到 Unity 下的 (position, quaternion, scale)。"""
    return matrix_to_unity(world_matrix(node))
