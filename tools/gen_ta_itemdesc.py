"""Adds The Lost Age item descriptions to src/gs_itemdesc.c from the game's own text (text id = 146 + item id, groups 0-2; dumps from
tools/ta_textdec.py in build/td_*.txt). Golden Sun uses 117 + id. Idempotent: the generated block sits between BEGIN/END TLA markers."""
import re
BS = chr(92)
T = {}
for g in (0, 1, 2):
    for l in open('/d/gs3ds/build/td_%d.txt' % g):
        m = re.match(r"(\d+) (\d+) (.*)$", l.strip())
        if m: T[(int(m.group(1)) << 8) | int(m.group(2))] = eval(m.group(3))
esc = lambda s: s.replace(BS, BS + BS).replace('"', BS + '"')
clean = lambda s: re.sub(r'<[0-9A-F]+>', ' ', s).strip()
d = {}
for i in range(1, 461):
    t = T.get(146 + i)
    if t and t != '?' and clean(t): d[i] = clean(t)
block = 'static const char* const descsTla[461] = {   // The Lost Age: text id = 146 + item id\n' + ''.join('\t[%d] = "%s",\n' % (k, esc(v)) for k, v in sorted(d.items())) + '};\n'
p = '/d/gs3ds/src/gs_itemdesc.c'
s = open(p).read()
s = re.sub(r'// BEGIN TLA ITEMDESC.*?// END TLA ITEMDESC\n', '', s, flags=re.S)
anchor = 'const char* gsItemDescription(unsigned id) {'
i = s.index(anchor)
s = s[:i] + '// BEGIN TLA ITEMDESC\n' + block + '// END TLA ITEMDESC\n\n' + 'const char* gsItemDescription(unsigned id) {\n\tif (gsRam() == &gsRamTla) return id < 461 && descsTla[id] ? descsTla[id] : "";\n\treturn id < 269 && descs[id] ? descs[id] : "";\n}\n'
if '#include "gs_ram.h"' not in s: s = s.replace('#include "gs_itemdesc.h"', '#include "gs_itemdesc.h"\n#include "gs_ram.h"', 1)
open(p, 'w').write(s)
print(len(d), 'Lost Age item descriptions; e.g. id 10:', d.get(10), '| id 180:', d.get(180), '| id 357:', d.get(357))
