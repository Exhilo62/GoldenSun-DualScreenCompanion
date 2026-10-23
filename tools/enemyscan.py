"""Random-walk until battles start; log enemy records (name/level/HP) at GS_ENEMY_BASE + n*0x14C. usage: enemyscan.py start.state steps seed tag"""
import sys, random, struct
sys.path.insert(0, 'tools')
from gs import GS
start, steps, seed, tag = sys.argv[1], int(sys.argv[2]), int(sys.argv[3]), sys.argv[4]
g = GS(); g.load(start); random.seed(seed)
def mode(): return g.r(0x04000000, 16) & 7
def enemies():
    d = g.dump(); out = []
    for i in range(6):
        o = 0x8000 + 0x30878 + i * 0x14C
        nm = d[o:o+15].split(b'\0')[0]
        lv, mhp, hp = d[o+15], *struct.unpack_from('<HH', d, o+0x34)[:1], struct.unpack_from('<H', d, o+0x38)[0]
        out.append((nm, lv, hp, mhp))
    return out
seen = set(); nb = 0
for i in range(steps):
    if mode() == 1:
        g.run(120); nb += 1
        e = enemies(); print(i, e, flush=True)
        key = tuple(x[0] for x in e if x[0]); 
        if key not in seen: seen.add(key); g.save(f'{tag}_b{len(seen)}.state')
        for _ in range(60):
            g.tap('A', 3, 20)
            if mode() != 1: break
        g.tap('B', 3, 20); g.tap('B', 3, 20); continue
    k = random.choice(['U', 'R', 'L', 'D', 'UR', 'UL', 'DR', 'DL'])
    g.run(random.choice([10, 20, 40]), k); g.run(2)
print('battles', nb, 'distinct', seen)
