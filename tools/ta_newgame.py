"""Boot The Lost Age (build/ta.gba) from no save to first field control (Jenna, Venus Lighthouse) and save build/ta_field.state.
usage: python tools/ta_newgame.py   (copy roms/lost_age.gba to build/ta.gba first)"""
import os, sys
os.environ['GS_ROM'] = '/d/gs3ds/build/ta.gba'
sys.path.insert(0, '/d/gs3ds/tools')
from gs import GS
g = GS()
g.run(900)
g.tap('A', 4, 90)        # accept the default name -> "Transfer data from Golden Sun?"
g.tap('RIGHT', 4, 30)    # No
for i in range(1500): g.tap('A', 3, 22)   # intro, scenes, dialogue until Jenna is controllable (the quick menu pops up)
for i in range(6): g.tap('B', 3, 25)
g.run(60)
B = 0x2000A50
print('Felix/Jenna lv', g.r(B + 0xF, 8), g.r(B + 0x14C + 0xF, 8), 'coins', g.r(0x2000250, 32))
g.save('ta_field.state'); g.shot('ta_field.png', 2); g.close()
