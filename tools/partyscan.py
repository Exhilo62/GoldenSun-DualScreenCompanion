"""Print party mask and per-record name/level for each field_<tag>.state. usage: partyscan.py tag..."""
import sys
sys.path.insert(0, '/d/gs3ds/tools')
from gs import GS
for t in sys.argv[1:]:
    g = GS(); g.load(f'field_{t}.state')
    recs = []
    for n in range(8):
        b = 0x2000500 + n*0x14C
        nm = bytes(g.r(b+i, 8) for i in range(6)).split(b'\0')[0].decode('latin1')
        recs.append(f'{nm}:{g.r(b+0xF, 8)}')
    print(t, 'mask', bin(g.r(0x2000040, 8)), 'map', hex(g.r(0x2000400, 16)), ' '.join(recs), flush=True)
    g.close()
