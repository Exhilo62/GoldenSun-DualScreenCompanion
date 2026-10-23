"""Record map replay frames from BFS cell states: mkreplay.py out.bin count seed [start]"""
import sys, json, random, struct
sys.path.insert(0, '/d/gs3ds/tools')
from gs import GS, frame_bytes
out, count, seed = sys.argv[1], int(sys.argv[2]), int(sys.argv[3])
cells = json.load(open('/d/gs3ds/build/bfs/big2_cells.json')); items = sorted(cells.items(), key=lambda kv: kv[1])
random.seed(seed); sel = random.sample(items, count)
g = GS(); f = open(out, 'wb'); n = 0
def spr(px, py, cx, cy):
    ex, ey = px + 963 - cx - 16, py - 192 - cy - 28; best = None
    for i in range(128):
        a0, a1 = g.r(0x07000000 + 8*i, 16), g.r(0x07000000 + 8*i + 2, 16)
        if (a0 >> 8) & 3 == 2 or (a0 >> 14) & 3 != 0 or (a1 >> 14) & 3 != 2: continue
        y, x = a0 & 0xFF, a1 & 0x1FF
        if x > 255: x -= 512
        d = (x - ex) ** 2 + (y - ey) ** 2
        if best is None or d < best[0]: best = (d, x, y)
    return (best[1], best[2]) if best and best[0] < 1600 else (-1, -1)
try:
    for k, st in sel:
        g.load(st); g.run(2)
        fb = frame_bytes(g); px = bytearray()
        for i in range(0, len(fb), 3): px += struct.pack('<H', ((fb[i] >> 3) << 11) | ((fb[i+1] >> 2) << 5) | (fb[i+2] >> 3))
        cx, cy, pxx, pyy = g.r(0x03001ad4, 16), g.r(0x03001ad6, 16), g.r(0x02030ec6, 16), g.r(0x02030ece, 16)
        sx, sy = spr(pxx, pyy, cx, cy)
        f.write(struct.pack('<7i', cx, cy, pxx, pyy, g.r(0x02000400, 8), sx, sy)); f.write(px); n += 1
except BrokenPipeError:
    pass
f.close(); print('frames', n)
