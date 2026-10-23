"""Decode The Lost Age's compressed text (same Huffman scheme as Golden Sun, see textdec.py). The decoder overlay is copied from ROM 0x0803843C into
IWRAM 0x03003EEC (tools/ta_textdec_iwram.bin); the init routine sits at 0x03004030 (Golden Sun: 0x0300402C), next-symbol at 0x03003EEC.
usage: python tools/ta_textdec.py GROUP [GROUP...] [> out.txt]   (needs build/ta.gba and build/taf_2636.state)"""
import sys, re, os
os.environ['GS_ROM'] = '/d/gs3ds/build/ta.gba'
sys.path.insert(0, '/d/gs3ds/tools')
from gs import GS
groups = [int(x) for x in sys.argv[1:]]
g = GS(); g.load('taf_2636.state'); g.run(2)
g.cmd('loadbin /d/gs3ds/tools/ta_textdec_iwram.bin 03003EEC')
S = 0x0203FE00
def callarm(addr, r0=0, r1=0, r2=0, r3=0):
    out = g.cmd('callarm %X %X %X %X %X' % (addr, r0, r1, r2, r3))
    for l in out:
        m = re.match(r'callarm r0=(\w+) r1=(\w+) r2=(\w+) r3=(\w+) steps=(\d+)', l)
        if m: return [int(x, 16) for x in m.groups()[:4]] + [int(m.group(5))]
    raise RuntimeError(out[-3:])
for grp in groups:
    for idx in range(256):
        callarm(0x03004030, S, (grp << 8) | idx)
        s = ''
        for k in range(80):
            r = callarm(0x03003EEC, S)
            if r[0] == 0: break
            s += chr(r[0]) if 32 <= r[0] < 127 else '<%X>' % r[0]
        print(grp, idx, repr(s), flush=True)
g.close()
