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
    """直接由 Maya 节点名得到 Unity 下的 (position, quaternion, scale)。世界空间。"""
    return matrix_to_unity(world_matrix(node))


def _mat4_mul(a, b):
    """4x4 行主序矩阵乘法：result = a * b。"""
    out = [0.0] * 16
    for i in range(4):
        for j in range(4):
            out[i * 4 + j] = (
                a[i * 4 + 0] * b[0 * 4 + j]
                + a[i * 4 + 1] * b[1 * 4 + j]
                + a[i * 4 + 2] * b[2 * 4 + j]
                + a[i * 4 + 3] * b[3 * 4 + j]
            )
    return out


def _mat4_invert_affine(m):
    """求仿射 4x4（行主序，最后一列多为 0,0,0,1）的逆。失败返回单位阵。"""
    # 上 3x3
    r00, r01, r02 = m[0], m[1], m[2]
    r10, r11, r12 = m[4], m[5], m[6]
    r20, r21, r22 = m[8], m[9], m[10]
    tx, ty, tz = m[12], m[13], m[14]

    det = (
        r00 * (r11 * r22 - r12 * r21)
        - r01 * (r10 * r22 - r12 * r20)
        + r02 * (r10 * r21 - r11 * r20)
    )
    if abs(det) < 1e-12:
        return [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1]
    inv_det = 1.0 / det

    i00 = (r11 * r22 - r12 * r21) * inv_det
    i01 = (r02 * r21 - r01 * r22) * inv_det
    i02 = (r01 * r12 - r02 * r11) * inv_det
    i10 = (r12 * r20 - r10 * r22) * inv_det
    i11 = (r00 * r22 - r02 * r20) * inv_det
    i12 = (r02 * r10 - r00 * r12) * inv_det
    i20 = (r10 * r21 - r11 * r20) * inv_det
    i21 = (r01 * r20 - r00 * r21) * inv_det
    i22 = (r00 * r11 - r01 * r10) * inv_det

    # t' = -t * R^-1  （行向量）
    itx = -(tx * i00 + ty * i10 + tz * i20)
    ity = -(tx * i01 + ty * i11 + tz * i21)
    itz = -(tx * i02 + ty * i12 + tz * i22)

    return [
        i00, i01, i02, 0.0,
        i10, i11, i12, 0.0,
        i20, i21, i22, 0.0,
        itx, ity, itz, 1.0,
    ]


def matrix_relative_to(node, ancestor):
    """node 相对 ancestor 的变换矩阵；ancestor 为 None 时返回世界矩阵。

    行向量约定：M_rel = M_world(node) * inv(M_world(ancestor))
    """
    wc = world_matrix(node)
    if not ancestor:
        return wc
    wa = world_matrix(ancestor)
    return _mat4_mul(wc, _mat4_invert_affine(wa))


def transform_to_unity_local(node, parent_path=None):
    """相对 Unity 父节点的本地 TRS → Unity。

    parent_path：层级中的父节点 fullPath（可能与 Maya 直接父不同，若中间组被跳过）。
    None 表示挂在场景根，使用世界变换。
    """
    return matrix_to_unity(matrix_relative_to(node, parent_path))


def _quat_mul(a, b):
    """四元数乘法 a * b，均为 (x, y, z, w)。"""
    ax, ay, az, aw = a
    bx, by, bz, bw = b
    return (
        aw * bx + ax * bw + ay * bz - az * by,
        aw * by - ax * bz + ay * bw + az * bx,
        aw * bz + ax * by - ay * bx + az * bw,
        aw * bw - ax * bx - ay * by - az * bz,
    )


# Maya 灯光沿局部 -Z 照射；Unity Light 沿局部 +Z（forward）。
# 转换完 TRS 后再本地绕 Y 转 180°，把瞄准轴对齐。
_LIGHT_AIM_FLIP_Y180 = (0.0, 1.0, 0.0, 0.0)


def transform_to_unity_local_light(node, parent_path=None):
    """灯光专用：与 mesh 相同的坐标转换，再补瞄准轴（-Z→+Z）。

    缩放强制为 (1,1,1)：Maya 灯光 Scale 只影响图标大小，不影响光照。
    """
    pos, rot, _scale = transform_to_unity_local(node, parent_path)
    rot = _quat_mul(rot, _LIGHT_AIM_FLIP_Y180)
    return pos, rot, (1.0, 1.0, 1.0)
