import sys, struct, zlib
def ppm2png(src, dst, scale=1):
    d = open(src, 'rb').read().split(b'\n', 3)[3]; w, h = 320, 240; rows = []
    for y in range(h):
        row = b''.join(d[(y*w+x)*3:(y*w+x)*3+3]*scale for x in range(w)); rows += [b'\0' + row] * scale
    def ch(t, c): return struct.pack('>I', len(c)) + t + c + struct.pack('>I', zlib.crc32(t + c))
    open(dst, 'wb').write(b'\x89PNG\r\n\x1a\n' + ch(b'IHDR', struct.pack('>IIBBBBB', w*scale, h*scale, 8, 2, 0, 0, 0)) + ch(b'IDAT', zlib.compress(b''.join(rows))) + ch(b'IEND', b''))
if __name__ == '__main__':
    for i in range(7): ppm2png(f'build/ui_{i}.ppm', f'build/ui_{i}.png', int(sys.argv[1]) if len(sys.argv) > 1 else 1)
