"""At each saved BFS cell state, face 4 directions + A and collect new dialogue strings from RAM."""
import sys, json, glob
sys.path.insert(0, '/d/gs3ds/tools')
from gs import GS
from dialogue import strings
cellsfile, stride, out = sys.argv[1], int(sys.argv[2]), sys.argv[3]
cells = json.load(open(cellsfile)); items = sorted(cells.items(), key=lambda kv: kv[1])[::stride]
g = GS(); found = {}
for k, st in items:
    g.load(st); g.run(3)
    base = set(strings(g.dump()))
    for d in ('U', 'D', 'L', 'R'):
        g.load(st); g.run(3)
        g.run(3, d)            # turn (may step 1px)
        g.tap('A', 3, 30)
        new = [s for s in strings(g.dump()) if s not in base and len(s) > 6 and not any(c in s for c in '|\$@')]
        if new:
            key = ' / '.join(new)
            if key not in found:
                found[key] = (k, d); print(k, d, key, flush=True)
        g.tap('B', 3, 10); g.tap('B', 3, 10)
json.dump(found, open(out, 'w'), indent=1)
