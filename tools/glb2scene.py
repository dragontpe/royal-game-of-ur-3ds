#!/usr/bin/env python3
"""Convert the Ur asset-pack GLB into romfs/scene.bin for the 3DS renderer.

Layout (little endian):
  'URS2' u32 texture_count u32 mesh_count u32 node_count
  per texture: 32-byte NUL-padded name (romfs:/tex/<name>_<format>.t3x)
  per mesh:  u32 material, u32 vertex_count, u32 index_count,
             vertex_count * (f32 px py pz  u v  nx ny nz),
             index_count * u16, padded to 4 bytes
  per node:  u32 mesh, u32 kind, 16 * f32 matrix (column major)

Material = index into the texture table.
Kinds: 0 static, 1 light counter, 2 dark counter, 3 die, 4 lamp flame,
5 Ur board, 6 Twenty Squares board (any mesh node in a GLB whose file name has 'twenty').
Static nodes (board + room) are pre-transformed and merged into one mesh per
texture, so the room costs a handful of draw calls; everything else keeps its
own node so the game can move or tint it.

UVs: glTF has v=0 at the top of the image; tex3ds textures put t=0 at the
bottom, so v is flipped here and nowhere else.
"""
import json
import os
import struct
import sys

COMPONENT = {5126: ('f', 4), 5123: ('H', 2), 5125: ('I', 4), 5121: ('B', 1)}
WIDTH = {'SCALAR': 1, 'VEC2': 2, 'VEC3': 3, 'VEC4': 4}


def read_glb(path):
    data = open(path, 'rb').read()
    magic, _, _ = struct.unpack_from('<4sII', data, 0)
    assert magic == b'glTF', path
    jlen, jtype = struct.unpack_from('<I4s', data, 12)
    assert jtype == b'JSON'
    doc = json.loads(data[20:20 + jlen])
    off = 20 + jlen
    blen, btype = struct.unpack_from('<I4s', data, off)
    assert btype == b'BIN\0'
    return doc, data[off + 8:off + 8 + blen]


def accessor(doc, binc, idx):
    a = doc['accessors'][idx]
    bv = doc['bufferViews'][a['bufferView']]
    fmt, size = COMPONENT[a['componentType']]
    n = WIDTH[a['type']]
    stride = bv.get('byteStride', size * n)
    base = bv.get('byteOffset', 0) + a.get('byteOffset', 0)
    out = []
    for i in range(a['count']):
        vals = struct.unpack_from('<' + fmt * n, binc, base + i * stride)
        out.append(vals if n > 1 else vals[0])
    return out


def trs_matrix(node):
    if 'matrix' in node:
        return list(node['matrix'])
    tx, ty, tz = node.get('translation', [0, 0, 0])
    qx, qy, qz, qw = node.get('rotation', [0, 0, 0, 1])
    sx, sy, sz = node.get('scale', [1, 1, 1])
    r = [
        [1 - 2 * (qy * qy + qz * qz), 2 * (qx * qy - qz * qw), 2 * (qx * qz + qy * qw)],
        [2 * (qx * qy + qz * qw), 1 - 2 * (qx * qx + qz * qz), 2 * (qy * qz - qx * qw)],
        [2 * (qx * qz - qy * qw), 2 * (qy * qz + qx * qw), 1 - 2 * (qx * qx + qy * qy)],
    ]
    s = (sx, sy, sz)
    # column major
    return [r[0][0] * s[0], r[1][0] * s[0], r[2][0] * s[0], 0,
            r[0][1] * s[1], r[1][1] * s[1], r[2][1] * s[1], 0,
            r[0][2] * s[2], r[1][2] * s[2], r[2][2] * s[2], 0,
            tx, ty, tz, 1]


def mat_mul(a, b):
    out = [0.0] * 16
    for c in range(4):
        for r in range(4):
            out[c * 4 + r] = sum(a[k * 4 + r] * b[c * 4 + k] for k in range(4))
    return out


def node_kind(name, path=''):
    if 'twenty' in os.path.basename(path).lower():
        return 6
    if name == 'UR_Board' or name.startswith('UR_Board.') or name.startswith('UR_Board_mesh'):
        return 5
    if name.startswith('UR_Counter_Shell'):
        return 1
    if name.startswith('UR_Counter_Dark'):
        return 2
    if name.startswith('UR_Die'):
        return 3
    if name.startswith('UR_Backdrop_Flame'):
        return 4
    return 0


def transform(m, p, w):
    return tuple(m[0 * 4 + r] * p[0] + m[1 * 4 + r] * p[1] + m[2 * 4 + r] * p[2] + m[3 * 4 + r] * w
                 for r in range(3))


def main():
    if len(sys.argv) < 3:
        sys.exit('usage: glb2scene.py out.bin in.glb [in2.glb ...]')
    out_path, inputs = sys.argv[1], sys.argv[2:]
    images = {}
    meshes, nodes = [], []
    for path in inputs:
        doc, binc = read_glb(path)
        mesh_base = len(meshes)
        for mesh in doc['meshes']:
            assert len(mesh['primitives']) == 1, mesh['name']
            prim = mesh['primitives'][0]
            assert prim.get('mode', 4) == 4, 'triangles only'
            mat = doc['materials'][prim['material']]
            tex = doc['textures'][mat['pbrMetallicRoughness']['baseColorTexture']['index']]
            img = doc['images'][tex['source']]['name']
            if img not in images:
                images[img] = len(images)
            pos = accessor(doc, binc, prim['attributes']['POSITION'])
            uv = accessor(doc, binc, prim['attributes']['TEXCOORD_0'])
            nrm = accessor(doc, binc, prim['attributes']['NORMAL'])
            idx = accessor(doc, binc, prim['indices'])
            assert len(pos) < 65536
            meshes.append((images[img], pos, uv, nrm, idx, mesh['name']))

        def walk(i, parent):
            node = doc['nodes'][i]
            m = mat_mul(parent, trs_matrix(node))
            if 'mesh' in node:
                nodes.append((mesh_base + node['mesh'], node_kind(node.get('name', ''), path), m))
            for c in node.get('children', []):
                walk(c, m)

        ident = [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1]
        for root in doc['scenes'][doc.get('scene', 0)]['nodes']:
            walk(root, ident)

    # Merge static nodes per texture.
    merged = {}
    dynamic = []
    for mesh, kind, m in nodes:
        if kind != 0:
            dynamic.append((mesh, kind, m))
            continue
        mat, pos, uv, nrm, idx, name = meshes[mesh]
        dst = merged.setdefault(mat, ([], [], [], []))
        base = len(dst[0])
        for p, n in zip(pos, nrm):
            dst[0].append(transform(m, p, 1.0))
            t = transform(m, n, 0.0)
            l = (t[0] ** 2 + t[1] ** 2 + t[2] ** 2) ** 0.5 or 1.0
            dst[2].append((t[0] / l, t[1] / l, t[2] / l))
        dst[1].extend(uv)
        dst[3].extend(i + base for i in idx)
    used = sorted({mesh for mesh, _, _ in dynamic})
    remap = {old: i for i, old in enumerate(used)}
    out_meshes = [meshes[i] for i in used]
    ident = [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1]
    out_nodes = []
    for mat in sorted(merged):
        pos, uv, nrm, idx = merged[mat]
        assert len(pos) < 65536
        out_nodes.append((len(out_meshes), 0, ident))
        out_meshes.append((mat, pos, uv, nrm, idx, 'static_' + [k for k, v in images.items() if v == mat][0]))
    out_nodes += [(remap[mesh], kind, m) for mesh, kind, m in dynamic]
    meshes, nodes = out_meshes, out_nodes

    names = sorted(images, key=images.get)
    blob = bytearray(b'URS2' + struct.pack('<III', len(names), len(meshes), len(nodes)))
    for n in names:
        tname = n[:-len('_basecolor')] if n.endswith('_basecolor') else n
        assert len(tname) < 32
        blob += tname.encode().ljust(32, b'\0')
    for mat, pos, uv, nrm, idx, _ in meshes:
        blob += struct.pack('<III', mat, len(pos), len(idx))
        for p, t, n in zip(pos, uv, nrm):
            blob += struct.pack('<8f', p[0], p[1], p[2], t[0], 1.0 - t[1], n[0], n[1], n[2])
        blob += struct.pack('<%dH' % len(idx), *idx)
        while len(blob) % 4:
            blob += b'\0'
    for mesh, kind, m in nodes:
        blob += struct.pack('<II16f', mesh, kind, *m)
    open(out_path, 'wb').write(blob)

    tris = sum(len(meshes[n[0]][4]) // 3 for n in nodes)
    print('%s: %d meshes, %d nodes, %d triangles per eye, %d bytes' %
          (out_path, len(meshes), len(nodes), tris, len(blob)))
    for i, (mat, pos, _, _, idx, name) in enumerate(meshes):
        print('  mesh %d %-30s material %d  %5d verts  %4d tris' % (i, name, mat, len(pos), len(idx) // 3))
    print('  nodes by kind:', {k: sum(1 for n in nodes if n[1] == k) for k in range(7)})
    print('  textures:', ', '.join('%d=%s' % (v, k) for k, v in sorted(images.items(), key=lambda kv: kv[1])))


if __name__ == '__main__':
    main()
