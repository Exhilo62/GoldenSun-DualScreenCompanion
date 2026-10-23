"""Adds The Lost Age description tables from build/ta_descs_all.json (built from tools/ta_textdec.py dumps: ability descriptions are text id 0x885 + id,
Djinn descriptions start at text id 0x9B1 with 20 slots per element in the order Venus, Mercury, Mars, Jupiter) to src/gs_abilities.c and src/gs_djinn.c.
Idempotent: the generated blocks sit between BEGIN/END TLA markers."""
import json, re
BS = chr(92)
esc = lambda s: s.replace(BS, BS + BS).replace('"', BS + '"')
d = json.load(open('build/ta_descs_all.json'))
names = json.load(open('build/ta_names.json'))
def put(path, begin_anchor, block, end_marker):
    s = open(path).read()
    s = re.sub(r'// BEGIN TLA %s.*?// END TLA %s\n' % (end_marker, end_marker), '', s, flags=re.S)
    i = s.index(begin_anchor)
    s = s[:i] + '// BEGIN TLA %s\n%s// END TLA %s\n' % (end_marker, block, end_marker) + s[i:]
    open(path, 'w').write(s)
# abilities
ab = {int(k): v for k, v in d['abil'].items()}
block = 'static const char* const descsTla[] = {   // The Lost Age ability descriptions (text id 0x885 + id)\n' + ''.join('\t[%d] = "%s",\n' % (k, esc(v)) for k, v in sorted(ab.items())) + '};\n'
put('src/gs_abilities.c', 'const char* gsAbilityByDescription(', block, 'DESCS')
s = open('src/gs_abilities.c').read()
if 'gsRam() == &gsRamTla ? descsTla' not in s:
    s = s.replace('\tint found = -1;\n\tfor (unsigned i = 0; i < sizeof(descs) / sizeof(descs[0]); ++i) {\n\t\tif (!descs[i] || strcmp(descs[i], text)) continue;',
                  '\tint found = -1;\n\tbool tla = gsRam() == &gsRamTla;\n\tconst char* const* dt = tla ? descsTla : descs;\n\tunsigned dn = tla ? sizeof(descsTla) / sizeof(descsTla[0]) : sizeof(descs) / sizeof(descs[0]);\n\tfor (unsigned i = 0; i < dn; ++i) {\n\t\tif (!dt[i] || strcmp(dt[i], text)) continue;', 1)
    assert 'dt[i]' in s
    open('src/gs_abilities.c', 'w').write(s)
# Djinn: app element order is Venus, Mars, Jupiter, Mercury
order = ('venus', 'mars', 'jupiter', 'mercury')
nm = json.load(open('build/ta_djinn_names.json'))
block = 'static const char* const djinnNamesTla[4][18] = {   // The Lost Age (18 per element)\n' + ''.join('\t{' + ', '.join('"%s"' % esc(x) for x in nm[e]) + '},\n' for e in order) + '};\n'
block += 'static const char* const djinnDescsTla[4][18] = {\n' + ''.join('\t{' + ', '.join('"%s"' % esc(x) for x in d['djinn'][e]) + '},\n' for e in order) + '};\n'
put('src/gs_djinn.c', 'const char* gsDjinnName(', block, 'DJINN')
s = open('src/gs_djinn.c').read()
if 'djinnNamesTla[element][index]' not in s:
    s = s.replace('const char* gsDjinnName(int element, int index) {\n\tif (element < 0 || element > 3 || index < 0 || index > 6) return "?";',
                  'const char* gsDjinnName(int element, int index) {\n\tif (gsRam() == &gsRamTla) return element < 0 || element > 3 || index < 0 || index > 17 ? "?" : djinnNamesTla[element][index];\n\tif (element < 0 || element > 3 || index < 0 || index > 6) return "?";', 1)
    s = s.replace('\tconst char* found = NULL;\n\tfor (int e = 0; e < 4; ++e)\n\t\tfor (int i = 0; i < 7; ++i)\n\t\t\tif (!strcmp(djinnDescs[e][i], text)) {\n\t\t\t\tif (found) return NULL;   // shared text: do not guess\n\t\t\t\tfound = djinnNames[e][i];\n\t\t\t}\n\treturn found;',
                  '\tconst char* found = NULL;\n\tbool tla = gsRam() == &gsRamTla;\n\tfor (int e = 0; e < 4; ++e)\n\t\tfor (int i = 0; i < (tla ? 18 : 7); ++i) {\n\t\t\tconst char* dsc = tla ? djinnDescsTla[e][i] : djinnDescs[e][i];\n\t\t\tif (!strcmp(dsc, text)) {\n\t\t\t\tif (found) return NULL;   // shared text: do not guess\n\t\t\t\tfound = tla ? djinnNamesTla[e][i] : djinnNames[e][i];\n\t\t\t}\n\t\t}\n\treturn found;', 1)
    assert 'djinnDescsTla[e][i]' in s
    if '"gs_ram.h"' not in s: s = s.replace('#include "gs_djinn.h"', '#include "gs_djinn.h"\n#include "gs_ram.h"', 1)
    open('src/gs_djinn.c', 'w').write(s)
print('ok')
