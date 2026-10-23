"""Rewrites the Lost Age name tables in src/gs_items.c and src/gs_abilities.c from build/ta_names.json (made from tools/ta_textdec.py dumps)."""
import json
BS = chr(92)
d = json.load(open('build/ta_names.json'))
items = {int(k): v for k, v in d['items'].items() if int(k) <= 460}
ab = {int(k): v for k, v in d['abilities'].items() if int(k) <= 413}
esc = lambda s: s.replace(BS, BS + BS).replace('"', BS + '"')
def table(name, m):
    return 'static const char* const %s[] = {\n' % name + ''.join('\t[%d] = "%s",\n' % (k, esc(v)) for k, v in sorted(m.items())) + '};\n'
FUNC = '''
const char* %s(unsigned id) {
	bool tla = gsRam() == &gsRamTla;
	const char* const* t = tla ? namesTla : names;
	unsigned n = tla ? sizeof(namesTla) / sizeof(namesTla[0]) : sizeof(names) / sizeof(names[0]);
	if (id < n && t[id]) return t[id];
	return "?";
}
'''
for path, fn, tbl, note in (('src/gs_items.c', 'gsItemName', items, '// The Lost Age: decoded from its own ROM text (tools/ta_textdec.py): text id = 0x25F + item id.\n'),
                            ('src/gs_abilities.c', 'gsAbilityName', ab, '// The Lost Age: decoded from its own ROM text (tools/ta_textdec.py): text id = 0x5A7 + ability id (same numbering as Golden Sun).\n')):
    s = open(path).read()
    a = s.find('// The Lost Age: decoded from its own ROM text')   # cut the previously generated block (table + function) and rewrite it
    if a < 0: a = len(s)
    s = s[:a] + note + table('namesTla', tbl) + FUNC % fn
    open(path, 'w').write(s)
print(len(items), 'items;', len(ab), 'abilities')
