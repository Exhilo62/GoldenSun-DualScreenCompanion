"""Convert a SharkPort .sps save to a raw GBA .sav. usage: sps2sav.py in.sps out.sav"""
import sys, struct
d = open(sys.argv[1], 'rb').read()
o = 0
def u32():
    global o; v, = struct.unpack_from('<I', d, o); o += 4; return v
def blob():
    global o; n = u32(); b = d[o:o+n]; o += n; return b
assert blob() == b'SharkPortSave'
u32()
for label in ('title', 'date', 'notes'): print(label, blob()[:100])
n = u32(); data = d[o:o+n]
print('data length', n, 'file', len(d), 'remaining', len(d) - o - n)
hdr = data.find(b"CAMELOT"); print("save header bytes stripped:", hdr)
open(sys.argv[2], "wb").write(data[hdr:])
