"""Read dialogue text from RAM (UTF-16LE strings near 0x02033900). usage: dialogue.py start.state steps [out_state]"""
import sys, re
sys.path.insert(0, '/d/gs3ds/tools')
from gs import GS
def strings(d):
    # EWRAM window where the text engine keeps decoded strings
    e = d[0x8000 + 0x33800: 0x8000 + 0x34800]
    out = []
    for m in re.finditer(rb'(?:[\x20-\x7e]\x00){5,}', e):
        out.append(m.group().decode('utf-16le'))
    return out
if __name__ == '__main__':
    g = GS(); g.load(sys.argv[1]); seen = set(); n = int(sys.argv[2])
    for i in range(n):
        g.tap('A', 3, 25)
        for s in strings(g.dump()):
            if s not in seen:
                seen.add(s); print(i, s, flush=True)
    if len(sys.argv) > 3: g.save(sys.argv[3])
    g.close()
