# -*- coding: utf-8 -*-
"""临时脚本：解析二进制 FBX（7500 用 64 位节点头），看 Model 的 Lcl 变换与顶点包围盒。"""
import struct
import sys

def parse_props(data, pos, n, wide):
    out = []
    for _ in range(n):
        t = chr(data[pos]); pos += 1
        if t == 'Y':
            v, = struct.unpack_from('<h', data, pos); pos += 2
        elif t == 'C':
            v = data[pos] != 0; pos += 1
        elif t == 'I':
            v, = struct.unpack_from('<i', data, pos); pos += 4
        elif t == 'F':
            v, = struct.unpack_from('<f', data, pos); pos += 4
        elif t == 'D':
            v, = struct.unpack_from('<d', data, pos); pos += 8
        elif t == 'L':
            v, = struct.unpack_from('<q', data, pos); pos += 8
        elif t in 'fdil':
            if wide:
                cnt, enc, ln = struct.unpack_from('<QQQ', data, pos); pos += 24
            else:
                cnt, enc, ln = struct.unpack_from('<III', data, pos); pos += 12
            fmt = {'f': 'f', 'd': 'd', 'i': 'i', 'l': 'q'}[t]
            if enc == 1:
                import zlib
                raw = zlib.decompress(data[pos:pos + ln])
                v = list(struct.unpack_from('<' + fmt * cnt, raw))
            else:
                v = list(struct.unpack_from('<' + fmt * cnt, data, pos))
            pos += ln
        elif t == 'S':
            ln, = struct.unpack_from('<I', data, pos); pos += 4
            v = data[pos:pos + ln].decode('latin-1'); pos += ln
        elif t == 'R':
            ln, = struct.unpack_from('<I', data, pos); pos += 4
            v = data[pos:pos + ln]; pos += ln
        else:
            raise ValueError('unknown prop type %r at %d' % (t, pos - 1))
        out.append((t, v))
    return pos, out


def walk(data, pos, end, depth, wide, acc):
    while pos < end - 1:
        if wide:
            endoff, np_, pl = struct.unpack_from('<QQQ', data, pos)
            nl = data[pos + 24]
            name = data[pos + 25:pos + 25 + nl].decode('latin-1')
            p2 = pos + 25 + nl
        else:
            endoff, np_, pl = struct.unpack_from('<III', data, pos)
            nl = data[pos + 12]
            name = data[pos + 13:pos + 13 + nl].decode('latin-1')
            p2 = pos + 13 + nl
        if endoff == 0:
            return  # 后续全是 0，结束
        if np_:
            p2, props = parse_props(data, p2, np_, wide)
        else:
            props = []
        acc.append((depth, name, props))
        if endoff > p2:
            walk(data, p2, endoff, depth + 1, wide, acc)
        pos = endoff


def load(fname):
    data = open(fname, 'rb').read()
    ver, = struct.unpack_from('<I', data, 23)
    wide = ver >= 7500
    acc = []
    walk(data, 27, len(data), 0, wide, acc)
    return ver, acc


for fname in ['SM_Bucket_Small.fbx', 'SM_Bucket_Small_2.fbx']:
    ver, nodes = load(fname)
    print('==== %s  (FBX version %d) ====' % (fname, ver))
    model_names = []
    for depth, name, props in nodes:
        if name == 'Model':
            model_names.append(props[1][1] if len(props) > 1 else '?')
        if name == 'P' and props and props[0][1] in ('Lcl Translation', 'Lcl Rotation', 'Lcl Scaling'):
            vals = [p[1] for p in props[2:] if p[0] in 'DF']
            print('   %-16s = %s' % (props[0][1], ['%.6g' % v for v in vals]))
        if name == 'Properties70':
            pass
    print('   Models: %s' % model_names)
    # 顶点包围盒
    for depth, name, props in nodes:
        if name == 'Vertices' and props and isinstance(props[0][1], list):
            v = props[0][1]
            xs = v[0::3]; ys = v[1::3]; zs = v[2::3]
            print('   Vertices=%d  bbox x[%.3f,%.3f] y[%.3f,%.3f] z[%.3f,%.3f]' % (
                len(v) // 3, min(xs), max(xs), min(ys), max(ys), min(zs), max(zs)))
