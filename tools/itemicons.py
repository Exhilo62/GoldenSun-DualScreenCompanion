"""Capture Golden Sun item icons: poke ids into Isaac's inventory, open the Items menu, crop the 5x3 icon grid.
usage: itemicons.py first last  -> build/icons/<prefix>item_<id>.rgb (16x16 RGB, raw) and build/icons/<prefix>grid_<first>.png
Environment for The Lost Age: GS_ROM=build/ta.gba ICON_STATE=taf_2636.state ICON_REC=0x2000A50 (Felix's record) ICON_PREFIX=ta_"""
import sys, os
sys.path.insert(0, '/d/gs3ds/tools')
from gs import GS
first, last = int(sys.argv[1]), int(sys.argv[2])
STATE = os.environ.get('ICON_STATE', 'field_34493.state'); REC = int(os.environ.get('ICON_REC', '0x2000500'), 16); PFX = os.environ.get('ICON_PREFIX', '')
COLS = [124, 148, 172, 196, 220]; ROWS = [48, 64, 80]   # icon centres (x, y) at 1x
def read_ppm(p):
    d = open(p, 'rb').read(); parts = d.split(b'\n', 3); w, h = map(int, parts[1].split()); return w, h, parts[3]
ids = list(range(first, last + 1))
for b in range(0, len(ids), 15):
    batch = ids[b:b + 15]
    g = GS(); g.load(STATE); g.run(20)
    for k in range(15):
        v = batch[k] if k < len(batch) else 0
        g.cmd(f'w16 {REC + 0xD8 + 2 * k:08X} {v:04X}')
    g.tap('A', 4, 40)
    for _ in range(2): g.tap('R', 4, 15)
    g.tap('A', 4, 50); g.run(30)
    ppm = f'/d/gs3ds/build/icons/{PFX}tmp_{first}_{b}.ppm'
    g.cmd('shot ' + ppm)
    if b == 0: g.shot(f'icons/{PFX}grid_{first}.png', 3)
    g.close()
    w, h, d = read_ppm(ppm)
    for k, id_ in enumerate(batch):
        cx, cy = COLS[k % 5], ROWS[k // 5]
        x0, y0 = cx - 8, cy - 8
        rows = b''.join(d[((y0 + y) * w + x0) * 3:((y0 + y) * w + x0 + 16) * 3] for y in range(16))
        open(f'/d/gs3ds/build/icons/{PFX}item_{id_}.rgb', 'wb').write(rows)
    os.remove(ppm)
print('done', first, last)
