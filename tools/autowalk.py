"""Biased random walk: python tools/autowalk.py start.state steps seed tag DIRS  (DIRS e.g. 'LLLULD'). Wins battles, saves a state on every fade-out."""
import sys, random, time
sys.path.insert(0, 'tools')
from gs import GS, frame_bytes
start, steps, seed, tag, dirs = sys.argv[1], int(sys.argv[2]), int(sys.argv[3]), sys.argv[4], sys.argv[5]
g = GS(); g.load(start); random.seed(seed)
def mode(): return g.r(0x04000000, 16) & 7
def bright():
    f = frame_bytes(g); return sum(f[::97]) / len(f[::97])
opts = [{'L': 'L', 'R': 'R', 'U': 'U', 'D': 'D', 'l': 'UL', 'r': 'UR', 'a': 'DL', 'b': 'DR'}[c] for c in dirs]
fades = 0; battles = 0; prev = bright()
for i in range(steps):
    if mode() == 1:
        battles += 1
        for _ in range(80):
            g.tap('A', 3, 20)
            if mode() != 1: break
        g.tap('B', 3, 20); g.tap('B', 3, 20); continue
    if random.random() < 0.02:
        g.tap('A', 3, 12); g.tap('B', 3, 12); g.tap('B', 3, 12)
    else:
        g.run(random.choice([10, 20, 40]), random.choice(opts)); g.run(2)
    if i % 4 == 0:
        b = bright()
        if b < 12 and prev >= 12:
            g.run(70); g.save(f'{tag}_fade{fades}.state'); g.shot(f'{tag}_fade{fades}.png', 1); fades += 1
        prev = b
    if i % 3000 == 0:
        g.save(f'{tag}_ck{i//3000}.state'); g.shot(f'{tag}_ck{i//3000}.png', 1)
print('done', 'fades', fades, 'battles', battles)
g.save(f'{tag}_end.state'); g.shot(f'{tag}_end.png', 1); g.close()
