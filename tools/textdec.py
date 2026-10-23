"""Decode the game's own compressed text through the emulator: python tools/textdec.py GROUP [GROUP...]  (run from the repo root, needs
build/probe.exe, build/gs.gba and a state such as build/field_34493.state). Text id = group << 8 | index. The ROM keeps text as an order-1
Huffman stream; the game's IWRAM routines (ROM 0x08009BB8 overlay, dumped to tools/gs_textdec_iwram.bin with probe 'bpdump') do the work and
are called from the probe with 'callarm'. Group 3 holds monster and ability names (ability id = index - 51), group 5 ability descriptions
(index = id + 58), group 6 Djinn descriptions. Output lines: GROUP INDEX 'text' ('<hex>' marks control codes)."""
import sys, re
sys.path.insert(0,'/d/gs3ds/tools')
from gs import GS
groups=[int(x) for x in sys.argv[1:]]
g=GS(); g.load('field_34493.state'); g.run(2)
g.cmd('loadbin /d/gs3ds/tools/gs_textdec_iwram.bin 03003EEC')
S=0x0203FE00
def callarm(addr,r0=0,r1=0,r2=0,r3=0):
    out=g.cmd('callarm %X %X %X %X %X'%(addr,r0,r1,r2,r3))
    for l in out:
        m=re.match(r'callarm r0=(\w+) r1=(\w+) r2=(\w+) r3=(\w+) steps=(\d+)',l)
        if m: return [int(x,16) for x in m.groups()[:4]]+[int(m.group(5))]
    raise RuntimeError(out[-3:])
res={}
for grp in groups:
    for idx in range(256):
        callarm(0x0300402C,S,(grp<<8)|idx)
        s=''
        for k in range(80):
            r=callarm(0x03003EEC,S)
            if r[0]==0: break
            s+=chr(r[0]) if 32<=r[0]<127 else '<%X>'%r[0]
        res[(grp,idx)]=s
        print(grp,idx,repr(s))
g.close()
