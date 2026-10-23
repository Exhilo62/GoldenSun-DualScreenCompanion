"""Driver for build/probe.exe: run frames/inputs, screenshots as PNG, RAM dumps and diffs."""
import subprocess, struct, zlib, os, sys
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
B = os.path.join(ROOT, 'build')

class GS:
    def __init__(self):
        self.uid = str(os.getpid())
        self.p = subprocess.Popen([os.path.join(B, 'probe.exe'), os.environ.get('GS_ROM') or os.path.join(B, 'gs.gba')] + ([os.environ['GS_SAV']] if os.environ.get('GS_SAV') else []),
                                  stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=(open(os.environ['GS_LOG'], 'w') if os.environ.get('GS_LOG') else subprocess.DEVNULL), text=True, cwd=B)
    def cmd(self, s):
        self.p.stdin.write(s + '\n'); self.p.stdin.flush()
        out = []
        while True:
            l = self.p.stdout.readline().strip()
            if l == 'ok' or l == '': return out
            out.append(l)
    def run(self, n, keys=''): self.cmd(f'run {n} {keys}')
    def tap(self, keys, hold=4, wait=12):
        self.cmd(f'run {hold} {keys}'); self.cmd(f'run {wait}')
    def r(self, addr, bits=8): return int(self.cmd(f'r{bits} {addr:08X}')[0], 16)
    def save(self, name): self.cmd('save ' + os.path.join(B, name))
    def load(self, name): self.cmd('load ' + os.path.join(B, name))
    def dump(self, name=None):
        name = name or f'dump_{self.uid}.bin'
        self.cmd('dump ' + os.path.join(B, name)); return open(os.path.join(B, name), 'rb').read()
    def shot(self, name='shot.png', scale=3):
        ppm = os.path.join(B, f'shot_{self.uid}.ppm'); self.cmd('shot ' + ppm)
        d = open(ppm, 'rb').read().split(b'\n', 3)[3]
        w, h = 240, 160; rows = []
        for y in range(h):
            row = b''.join(d[(y*w+x)*3:(y*w+x)*3+3]*scale for x in range(w))
            rows += [b'\0' + row] * scale
        raw = b''.join(rows)
        def ch(t, c): return struct.pack('>I', len(c)) + t + c + struct.pack('>I', zlib.crc32(t + c))
        png = b'\x89PNG\r\n\x1a\n' + ch(b'IHDR', struct.pack('>IIBBBBB', w*scale, h*scale, 8, 2, 0, 0, 0)) + ch(b'IDAT', zlib.compress(raw)) + ch(b'IEND', b'')
        path = os.path.join(B, name); open(path, 'wb').write(png); return path
    def close(self): self.cmd('quit')

def walk(g, keys, frames, shotname=None):
    g.run(frames, keys)
    g.run(6)
    return g.shot(shotname, 2) if shotname else None

def montage(g, shots, out='mont.png', cols=3):
    """shots: list of PNG names saved in build/ at scale 1; stitches them."""
    imgs = []
    for n in shots:
        d = open(os.path.join(B, n), 'rb').read(); i = 8; idat = b''
        while i < len(d):
            l, = struct.unpack('>I', d[i:i+4]); t = d[i+4:i+8]
            if t == b'IDAT': idat += d[i+8:i+8+l]
            if t == b'IHDR': w, h = struct.unpack('>II', d[i+8:i+16])
            i += 12 + l
        raw = zlib.decompress(idat); imgs.append((w, h, [raw[y*(w*3+1)+1:(y+1)*(w*3+1)] for y in range(h)]))
    w, h = imgs[0][0], imgs[0][1]; nrows = (len(imgs) + cols - 1) // cols; rows = []
    blank = [b'\0' * (w * 3)] * h
    for r in range(nrows):
        for y in range(h):
            rows.append(b'\0' + b''.join((imgs[r*cols+c][2][y] if r*cols+c < len(imgs) else blank[y]) for c in range(cols)))
    def ch(t, c): return struct.pack('>I', len(c)) + t + c + struct.pack('>I', zlib.crc32(t + c))
    p = os.path.join(B, out)
    open(p, 'wb').write(b'\x89PNG\r\n\x1a\n' + ch(b'IHDR', struct.pack('>IIBBBBB', w*cols, h*nrows, 8, 2, 0, 0, 0)) + ch(b'IDAT', zlib.compress(b''.join(rows))) + ch(b'IEND', b''))
    return p

def frame_bytes(g):
    ppm = os.path.join(B, f'cmp_{g.uid}.ppm'); g.cmd('shot ' + ppm)
    return open(ppm, 'rb').read().split(b'\n', 3)[3]

def diff_frac(a, b):
    n = len(a) // 3; d = 0
    for i in range(0, len(a), 3):
        if abs(a[i]-b[i]) + abs(a[i+1]-b[i+1]) + abs(a[i+2]-b[i+2]) > 60: d += 1
    return d / n

def try_move(g, keys, frames=40):
    a = frame_bytes(g); g.run(frames, keys); g.run(4); b = frame_bytes(g)
    return diff_frac(a, b)

def shot_crop(g, x, y, w, h, name, scale=2):
    ppm = os.path.join(B, f'crop_{g.uid}.ppm'); g.cmd('shot ' + ppm)
    d = open(ppm, 'rb').read().split(b'\n', 3)[3]
    rows = []
    for yy in range(y, y + h):
        row = b''.join(d[(yy*240+xx)*3:(yy*240+xx)*3+3]*scale for xx in range(x, x + w))
        rows += [b'\0' + row] * scale
    def ch(t, c): return struct.pack('>I', len(c)) + t + c + struct.pack('>I', zlib.crc32(t + c))
    p = os.path.join(B, name)
    open(p, 'wb').write(b'\x89PNG\r\n\x1a\n' + ch(b'IHDR', struct.pack('>IIBBBBB', w*scale, h*scale, 8, 2, 0, 0, 0)) + ch(b'IDAT', zlib.compress(b''.join(rows))) + ch(b'IEND', b''))
    return p
