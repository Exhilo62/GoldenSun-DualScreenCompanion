"""Reorder the party in a field state, walk until a battle, report which battle slot is choosing. usage: battleorder.py state seed steps o0 o1 o2 o3"""
import sys, random
sys.path.insert(0, '/d/gs3ds/tools')
from gs import GS
st, seed, steps = sys.argv[1], int(sys.argv[2]), int(sys.argv[3]); order = [int(x) for x in sys.argv[4:8]]
g = GS(); g.load(st); random.seed(seed)
for i, v in enumerate(order):
    g.cmd(f'w8 {0x200001c+i:08X} {v:02X}'); g.cmd(f'w8 {0x2000438+i:08X} {v:02X}')
for i in range(steps):
    if g.r(0x04000000, 16) & 7 == 1:
        seen = []
        for _ in range(400):
            sel = g.r(0x03007dec, 8)
            if sel < 4 and g.r(0x03007c46, 8) == 3 and (not seen or seen[-1] != sel):
                seen.append(sel); g.shot(f'bo_{seed}_{len(seen)}.png', 2)
            if len(seen) >= 4: break
            g.tap('A', 3, 25)
        print('choosing slots in order:', seen); break
    g.run(random.choice([10, 20, 40]), random.choice(['L','R','U','D','UL','UR','DL','DR'])); g.run(2)
else: print('no battle')
g.close()
