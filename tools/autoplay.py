"""Random-walk autoplay: wins battles with A, logs event-flag changes (0x02000000-0x02000500), saves a state per change."""
import sys, random, time
sys.path.insert(0, 'tools')
from gs import GS
start = sys.argv[1]; steps = int(sys.argv[2]); seed = int(sys.argv[3]); tag = sys.argv[4]
g = GS(); g.load(start)
random.seed(seed)
def flags(): 
    d = g.dump(); return d[0x8000:0x8000 + 0x500]
def mode(): return g.r(0x04000000, 16) & 7
cur = flags(); log = []; battles = 0; t = time.time()
for i in range(steps):
    if mode() == 1:
        battles += 1
        for _ in range(60):
            g.tap('A', 3, 20)
            if mode() != 1: break
        g.tap('B', 3, 20); g.tap('B', 3, 20)
        continue
    r = random.random()
    if r < 0.03:
        g.tap('A', 3, 12); g.tap('B', 3, 12); g.tap('B', 3, 12)
    else:
        k = random.choice(['U', 'R', 'L', 'D', 'UR', 'UL', 'DR', 'DL'])
        g.run(random.choice([10, 20, 40]), k); g.run(2)
    if i % 2000 == 0:
        g.save(f'{tag}_ck{i//2000}.state'); g.shot(f'{tag}_ck{i//2000}.png', 1)
    if i % 25 == 0:
        f = flags()
        if f != cur:
            ch = [(0x02000000 + o, cur[o], f[o]) for o in range(len(f)) if f[o] != cur[o] and not (0x500 > o >= 0x3C0)]
            if ch:
                n = len(log); g.save(f'{tag}_flag{n}.state'); g.shot(f'{tag}_flag{n}.png', 1)
                log.append((i, ch[:12]))
                pass
            cur = f
print('done', 'battles', battles, 'flagevents', len(log), round(time.time() - t), 's')
g.save(f'{tag}_end.state'); g.shot(f'{tag}_end.png', 1); g.close()
