"""Convert a P6 PPM of any size to PNG with an integer scale. usage: ppm2png_any.py in.ppm out.png [scale]"""
import sys, struct, zlib
d = open(sys.argv[1], 'rb').read().split(b'\n', 3); w, h = map(int, d[1].split()); S = int(sys.argv[3]) if len(sys.argv) > 3 else 1
raw = b''.join(b'\0' + b''.join(d[3][(y*w+x)*3:(y*w+x)*3+3]*S for x in range(w)) for y in range(h) for _ in range(S))
def ch(t, c): return struct.pack('>I', len(c)) + t + c + struct.pack('>I', zlib.crc32(t + c))
open(sys.argv[2], 'wb').write(b'\x89PNG\r\n\x1a\n' + ch(b'IHDR', struct.pack('>IIBBBBB', w*S, h*S, 8, 2, 0, 0, 0)) + ch(b'IDAT', zlib.compress(raw)) + ch(b'IEND', b''))
