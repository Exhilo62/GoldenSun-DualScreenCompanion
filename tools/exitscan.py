"""For each BFS cell, walk each direction ~25 frames and flag moves where the screen fades (door / map exit)."""
import sys, json
sys.path.insert(0, '/d/gs3ds/tools')
from gs import GS, frame_bytes
cellsfile, out = sys.argv[1], sys.argv[2]
cells = json.load(open(cellsfile)); items = sorted(cells.items(), key=lambda kv: kv[1])
g = GS()
def bright():
    f = frame_bytes(g); return sum(f[::53]) / len(f[::53])
exits = {}
for k, st in items:
    for d in ('U', 'D', 'L', 'R'):
        g.load(st); g.run(2); b0 = bright()
        lo = b0
        for _ in range(4):
            g.run(6, d); lo = min(lo, bright())
        if lo < b0 * 0.55:
            exits[f'{k}{d}'] = st; print(k, d, round(b0), round(lo), st, flush=True)
            g.run(60); g.shot('exit_' + st.split('/')[-1].replace('.state', '') + d + '.png', 1)
json.dump(exits, open(out, 'w'))
