#!/usr/bin/env python3
"""Convert the Ur asset-pack GLB into romfs/scene.bin for the 3DS renderer.

Layout (little endian):
  'URS1' u32 mesh_count u32 node_count
  per mesh:  u32 material, u32 vertex_count, u32 index_count,
             vertex_count * (f32 px py pz  u v  nx ny nz),
             index_count * u16, padded to 4 bytes
  per node:  u32 mesh, u32 kind, 16 * f32 matrix (column major)

Materials: 0 = board texture, 1 = pieces atlas, 2+ = extra textures listed in
the order they first appear (printed so the loader table can match).
Kinds: 0 static (board, backdrop), 1 light counter, 2 dark counter, 3 die.

UVs: glTF has v=0 at the top of the image; tex3ds textures put t=0 at the
bottom, so v is flipped here and nowhere else.
"""
import json
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


def node_kind(name):
    n = name.lower()
    if 'shell' in n or 'light' in n:
        return 1
    if 'dark' in n:
        return 2
    if 'die' in n or 'dice' in n:
        return 3
    return 0


def main():
    if len(sys.argv) < 3:
        sys.exit('usage: glb2scene.py out.bin in.glb [in2.glb ...]')
    out_path, inputs = sys.argv[1], sys.argv[2:]
    images = {'ur_board_basecolor': 0, 'ur_pieces_basecolor': 1}
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
                nodes.append((mesh_base + node['mesh'], node_kind(node.get('name', '')), m))
            for c in node.get('children', []):
                walk(c, m)

        ident = [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1]
        for root in doc['scenes'][doc.get('scene', 0)]['nodes']:
            walk(root, ident)

    blob = bytearray(b'URS1' + struct.pack('<II', len(meshes), len(nodes)))
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
        print('  mesh %d %-28s material %d  %d verts  %d tris' % (i, name, mat, len(pos), len(idx) // 3))
    print('  textures:', ', '.join('%d=%s' % (v, k) for k, v in sorted(images.items(), key=lambda kv: kv[1])))


if __name__ == '__main__':
    main()
