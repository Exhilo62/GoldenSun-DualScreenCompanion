"""Random-walk a state and log every map id reached (with a screenshot). usage: mapscan.py start.state steps seed tag"""
import sys, random
sys.path.insert(0, '/d/gs3ds/tools')
from gs import GS
start, steps, seed, tag = sys.argv[1], int(sys.argv[2]), int(sys.argv[3]), sys.argv[4]
g = GS(); g.load(start); random.seed(seed)
seen = {}
def mid(): return g.r(0x2000400, 16)
for i in range(steps):
    if g.r(0x04000000, 16) & 7 == 1:      # battle: mash A until it ends
        for _ in range(60):
            g.tap('A', 3, 20)
            if g.r(0x04000000, 16) & 7 != 1: break
        continue
    if g.r(0x2000060, 8) & 0x40:          # a menu opened: back out
        for _ in range(4): g.tap('B', 3, 10)
    k = random.choice(['U', 'R', 'L', 'D', 'UR', 'UL', 'DR', 'DL'])
    g.run(random.choice([10, 20, 40]), k); g.run(2)
    m = mid()
    if m not in seen and m != 0x1FE and g.r(0x02030EC6, 16):
        g.run(25)
        if mid() == m:
            seen[m] = (g.r(0x02030EC6, 16), g.r(0x02030ECE, 16))
            g.shot(f'ms_{tag}_{m:03X}.png', 1); g.save(f'ms_{tag}_{m:03X}.state')
            print(f'step {i}: map {m:#05x} player {seen[m]}', flush=True)
print('done', sorted(hex(m) for m in seen))
g.close()
