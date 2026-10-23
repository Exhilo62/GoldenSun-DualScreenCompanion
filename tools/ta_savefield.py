"""Boot a converted Lost Age .sav (build/tasaves/ta.<tag>.sav), tap through the title/continue menus, save build/taf_<tag>.state. usage: ta_savefield.py tag"""
import sys, os
os.environ['GS_ROM'] = '/d/gs3ds/build/ta.gba'
tag = sys.argv[1]
os.environ['GS_SAV'] = '/d/gs3ds/build/tasaves/ta.%s.sav' % tag
sys.path.insert(0, '/d/gs3ds/tools')
from gs import GS
g = GS()
g.run(500)
shots = []
for i in range(40):
    g.tap('A', 4, 60)
    if i % 4 == 3: g.shot(f'taf_{tag}_{i}.png', 1)
for i in range(8): g.tap('B', 4, 30)
g.run(60)
g.shot(f'taf_{tag}.png', 2); g.save(f'taf_{tag}.state')
B = 0x2000A50
print(tag, [(bytes(g.r(B+n*0x14C+i, 8) for i in range(8)).split(b'\0')[0].decode('latin1'), g.r(B+n*0x14C+0xF, 8)) for n in range(8)], 'coins', g.r(0x2000250, 32), 'mask', hex(g.r(0x2000040, 8)))
g.close()
