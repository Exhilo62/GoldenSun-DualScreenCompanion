"""Capture Psynergy list icons from the game's field Psynergy menu (Golden Sun or, with GS_ROM/PSY_* set, The Lost Age).
For every member of a state, step the cursor down the list; the highlighted row's description (UTF-16 at PSY_DESC) identifies the ability
(unique descriptions only, via the app's tables), and the 16x16 icon beside the row is cropped with its flat backdrop replaced by magenta.
usage: psyicons.py state [outdir]   env: PSY_DESC (hex, default 0x203396C)"""
import sys, os, re
sys.path.insert(0, '/d/gs3ds/tools')
from gs import GS
state = sys.argv[1]; out = sys.argv[2] if len(sys.argv) > 2 else '/d/gs3ds/build/psy'
DESC = int(os.environ.get('PSY_DESC', '0x203396C'), 16)
KEY = bytes((255, 0, 255))
os.makedirs(out, exist_ok=True)
src = open('/d/gs3ds/src/gs_abilities.c').read()
a = src.index('static const char* const descs[]'); b = src.index('};', a)
BS = chr(92)
PAT = BS + '[(' + BS + 'd+)' + BS + '] = "((?:[^"' + BS + BS + ']|' + BS + BS + '.)*)"'
descs = {int(i): t for i, t in re.findall(PAT, src[a:b])}
if os.environ.get('PSY_DESCS_JSON'):   # The Lost Age: descriptions decoded from its ROM (text id 0x885 + ability id)
    import json
    descs = {int(k): v for k, v in json.load(open(os.environ['PSY_DESCS_JSON'])).items()}
byd = {}
for i, t in descs.items(): byd.setdefault(t, []).append(i)
ROWS = [43, 59, 75, 91, 107]; X0 = 110
def rd(g):
    s = ''
    for k in range(60):
        c = g.r(DESC + 2 * k, 16)
        if c == 0: break
        s += chr(c) if 32 <= c < 127 else '?'
    return s
def ppm(g):
    p = '/d/gs3ds/build/psy_tmp_%d.ppm' % os.getpid(); g.cmd('shot ' + p)
    d = open(p, 'rb').read(); parts = d.split(b'\n', 3); w, h = map(int, parts[1].split()); os.remove(p); return w, h, parts[3]
found = {}
g = GS(); g.load(state); g.run(30)
g.tap('A', 4, 40); g.tap('A', 4, 50)
for member in range(4):
    if member == 0: g.tap('A', 4, 50)
    else: g.tap('r', 4, 40)   # L/R change character inside the list
    g.run(25)
    frames = []   # per step: (ability id or None, highlighted row, screenshot)
    for step in range(14):
        text = rd(g); w, h, d = ppm(g)
        px = lambda x, y: d[(y * w + x) * 3:(y * w + x) * 3 + 3]
        cols = [px(200, cy) for cy in ROWS]
        row = None
        for r in range(5):
            others = [cols[k] for k in range(5) if k != r]
            if all(c == others[0] for c in others) and cols[r] != others[0]: row = r
        ids = byd.get(text, [])
        frames.append((ids[0] if len(ids) == 1 else None, row, d))
        g.tap('D', 4, 18)
    # list length: the step where the first ability comes round again (the cursor wraps)
    L = next((k for k in range(1, 14) if frames[k][0] is not None and frames[k][0] == frames[0][0]), 14)
    idx = {}   # list index -> ability id (known when the cursor was on it)
    off = {}
    for k in range(min(L, 14)):
        if frames[k][1] is not None: off[k] = k - frames[k][1]
        if frames[k][0] is not None: idx[k] = frames[k][0]
    for k, aid in idx.items():
        if aid in found: continue
        for s in range(min(L, 14)):   # a frame where list index k is visible on a row that is not the highlighted one
            if s not in off: continue
            r = k - off[s]
            if 0 <= r < 5 and frames[s][1] != r:
                d = frames[s][2]; cy = ROWS[r]
                crop = b''.join(d[((cy - 8 + y) * w + X0) * 3:((cy - 8 + y) * w + X0 + 16) * 3] for y in range(16))
                key = crop[0:3]
                crop = b''.join(KEY if crop[i:i + 3] == key else crop[i:i + 3] for i in range(0, len(crop), 3))
                found[aid] = crop; open('%s/psy_%d.rgb' % (out, aid), 'wb').write(crop); break
print(state, 'abilities captured:', sorted(found))
g.close()
