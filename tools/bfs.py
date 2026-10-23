"""Flood-fill walkable space from a state using camera position (0x03001AD4/6) as the cell key.
usage: bfs.py start.state maxnodes outprefix. Saves a state per cell in build/bfs/; reports bounds and jumps (map transitions)."""
import sys, collections, os
sys.path.insert(0, '/d/gs3ds/tools')
from gs import GS
start, maxn, tag = sys.argv[1], int(sys.argv[2]), sys.argv[3]
g = GS(); g.load(start)
def pos(): return g.r(0x02030ec6, 16), g.r(0x02030ece, 16)
def mode(): return g.r(0x04000000, 16) & 7
DIRS = ['U', 'D', 'L', 'R', 'UL', 'UR', 'DL', 'DR']
def cell(p): return (p[0] // 6, p[1] // 6)
p0 = pos(); seen = {cell(p0): f'build/bfs/{tag}_0.state'}; g.save(f'bfs/{tag}_0.state')
q = collections.deque([(cell(p0), f'bfs/{tag}_0.state', p0)]); jumps = []; n = 1
while q and n < maxn:
    c, st, p = q.popleft()
    for d in DIRS:
        g.load(st); g.run(12, d); g.run(2)
        if mode() == 1: continue
        np_ = pos(); nc = cell(np_)
        if abs(np_[0] - p[0]) > 80 or abs(np_[1] - p[1]) > 80:
            jumps.append((p, np_)); g.save(f'bfs/{tag}_jump{len(jumps)}.state'); continue
        if nc not in seen:
            name = f'bfs/{tag}_{n}.state'; g.save(name); seen[nc] = name; q.append((nc, name, np_)); n += 1
xs = [c[0] * 6 for c in seen]; ys = [c[1] * 6 for c in seen]
print('cells', len(seen), 'x', min(xs), max(xs), 'y', min(ys), max(ys), 'jumps', len(jumps), jumps[:6])
import json; json.dump({str(k): v for k, v in seen.items()}, open(f'build/bfs/{tag}_cells.json', 'w'))
g.close()
