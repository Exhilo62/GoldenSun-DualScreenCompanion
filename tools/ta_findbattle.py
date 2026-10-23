"""Random-walk a Lost Age state until a battle starts; save build/<tag>_b0.state at the first command prompt. usage: ta_findbattle.py state tag seed steps"""
import sys, os, random
os.environ.setdefault('GS_ROM', '/d/gs3ds/build/ta.gba')
sys.path.insert(0, '/d/gs3ds/tools')
from gs import GS
st, tag, seed, steps = sys.argv[1], sys.argv[2], int(sys.argv[3]), int(sys.argv[4])
g = GS(); g.load(st); random.seed(seed)
for i in range(steps):
    if g.r(0x04000000, 16) & 7 == 1:
        g.run(300)
        g.shot(f'{tag}_b0.png', 2); g.save(f'{tag}_b0.state'); print('battle at step', i); break
    g.run(random.choice([10, 20, 40]), random.choice(['L', 'R', 'U', 'D', 'UL', 'UR', 'DL', 'DR'])); g.run(2)
else: print('no battle')
g.close()
