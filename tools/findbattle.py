import sys, random
sys.path.insert(0, '/d/gs3ds/tools')
from gs import GS
start, tag, seed, steps = sys.argv[1], sys.argv[2], int(sys.argv[3]), int(sys.argv[4])
g = GS(); g.load(start); random.seed(seed)
def mode(): return g.r(0x04000000, 16) & 7
opts = ['L', 'R', 'U', 'D', 'UL', 'UR', 'DL', 'DR']
found = 0
for i in range(steps):
    if mode() == 1:
        g.run(200)
        for k in range(6):
            if g.r(0x03007dec, 8) != 0: g.run(30)
        g.save('%s_b%d.state' % (tag, found)); g.shot('%s_b%d.png' % (tag, found), 2)
        found += 1
        if found >= 2: break
        for _ in range(100):
            g.tap('A', 3, 20)
            if mode() != 1: break
        continue
    g.run(random.choice([10, 20, 40]), random.choice(opts)); g.run(2)
print('found', found)
g.close()
