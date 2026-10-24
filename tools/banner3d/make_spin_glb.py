#!/usr/bin/env python3
"""Adds a turntable spin to the 3D relief logo: one root bone, every mesh skinned to it, one looping 360 degree rotation about the vertical axis.
usage: make_spin_glb.py in.glb out.glb [seconds]
Plain standard library only. The result is converted to a banner CGFX with pycgfx (see docs/BANNER3D.md): that converter plays glTF animations and
skins, but cannot animate a bare mesh node, which is why a one-bone skin is used."""
import json, math, struct, sys

def read_glb(path):
    d = open(path, 'rb').read()
    magic, version, length = struct.unpack('<III', d[:12])
    assert magic == 0x46546C67 and version == 2 and length == len(d), 'not a GLB 2.0 file'
    off, js, binc = 12, None, b''
    while off < len(d):
        clen, ctype = struct.unpack('<II', d[off:off + 8])
        body = d[off + 8:off + 8 + clen]
        if ctype == 0x4E4F534A: js = json.loads(body)
        elif ctype == 0x004E4942: binc = bytes(body)
        off += 8 + clen
    return js, bytearray(binc)

def write_glb(path, js, binc):
    jb = json.dumps(js, separators=(',', ':')).encode()
    jb += b' ' * (-len(jb) % 4)
    binc = bytes(binc) + b'\0' * (-len(binc) % 4)
    total = 12 + 8 + len(jb) + 8 + len(binc)
    with open(path, 'wb') as f:
        f.write(struct.pack('<III', 0x46546C67, 2, total))
        f.write(struct.pack('<II', len(jb), 0x4E4F534A)); f.write(jb)
        f.write(struct.pack('<II', len(binc), 0x004E4942)); f.write(binc)

def main():
    src, dst = sys.argv[1], sys.argv[2]
    seconds = float(sys.argv[3]) if len(sys.argv) > 3 else 6.0
    j, b = read_glb(src)

    def add_accessor(data, comp, typ, count, extra=None):
        while len(b) % 4: b.append(0)
        view = {'buffer': 0, 'byteOffset': len(b), 'byteLength': len(data)}
        b.extend(data)
        j['bufferViews'].append(view)
        acc = {'bufferView': len(j['bufferViews']) - 1, 'componentType': comp, 'count': count, 'type': typ}
        if extra: acc.update(extra)
        j['accessors'].append(acc)
        return len(j['accessors']) - 1

    # one joint node, in the scene
    j['nodes'].append({'name': 'Spin'})
    joint = len(j['nodes']) - 1
    j['scenes'][j.get('scene', 0)]['nodes'].append(joint)
    ibm = add_accessor(struct.pack('<16f', 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1), 5126, 'MAT4', 1)
    j['skins'] = [{'name': 'SpinSkin', 'joints': [joint], 'inverseBindMatrices': ibm}]

    # every vertex belongs 100% to that joint
    for mesh in j['meshes']:
        for prim in mesh['primitives']:
            n = j['accessors'][prim['attributes']['POSITION']]['count']
            prim['attributes']['JOINTS_0'] = add_accessor(b'\0' * (4 * n), 5121, 'VEC4', n)
            prim['attributes']['WEIGHTS_0'] = add_accessor(struct.pack('<4f', 1, 0, 0, 0) * n, 5126, 'VEC4', n)
    for node in j['nodes']:
        if 'mesh' in node: node['skin'] = 0

    # 360 degrees about +Y in five keys (a quarter turn each) so linear interpolation stays on the short arc
    keys = 4
    times = [seconds * k / keys for k in range(keys + 1)]
    quats = []
    for k in range(keys + 1):
        a = 2 * math.pi * k / keys
        quats.append((0.0, math.sin(a / 2), 0.0, math.cos(a / 2)))
    t_acc = add_accessor(struct.pack('<%df' % len(times), *times), 5126, 'SCALAR', len(times), {'min': [times[0]], 'max': [times[-1]]})
    q_acc = add_accessor(b''.join(struct.pack('<4f', *q) for q in quats), 5126, 'VEC4', len(quats))
    j['animations'] = [{'name': 'Spin', 'samplers': [{'input': t_acc, 'output': q_acc, 'interpolation': 'LINEAR'}],
                        'channels': [{'sampler': 0, 'target': {'node': joint, 'path': 'rotation'}}]}]
    j['buffers'][0]['byteLength'] = len(b)
    write_glb(dst, j, b)
    print('wrote %s: %d meshes skinned to one joint, %.1f s loop, %d bytes' % (dst, len(j['meshes']), seconds, len(b)))

main()
