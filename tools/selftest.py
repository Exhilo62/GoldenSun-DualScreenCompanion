"""Regression checks for the host-side pieces, run against the real saved states in build/.
usage: python tools/selftest.py     (needs build/gs.gba, build/mgba-host/libmgba.a, the saves in roms/saves/ and the *.state files in build/)
Builds stateprobe, navtest, maptest and ui_preview with gcc, then asserts on what they report."""
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
B = os.path.join(ROOT, 'build')
FLAGS = ('-O1 -w -D_GNU_SOURCE -DBUILD_STATIC -DENABLE_DEBUGGERS -DENABLE_GDB_STUB -DENABLE_DIRECTORIES -DENABLE_VFS -DENABLE_VFS_FD '
         '-DM_CORE_GBA -DUSE_PTHREADS').split() + ['-I' + os.path.join(ROOT, 'third_party', 'mgba', 'include'),
                                                  '-I' + os.path.join(ROOT, 'build', 'mgba-host', 'include')]
LIB = [os.path.join(B, 'mgba-host', 'libmgba.a'), '-lz', '-lm', '-pthread']
SAV_A = os.path.join(ROOT, 'roms', 'saves', 'a', 'golden_sun.sav')
SAV_B = os.path.join(ROOT, 'roms', 'saves', 'b', 'golden_sun.sav')
fails = []
passes = 0


def check(name, cond, detail=''):
    global passes
    if cond:
        passes += 1
        print('  ok  ', name)
    else:
        fails.append(name)
        print('  FAIL', name, detail)


def build(out, srcs, host_mgba=True):
    cmd = ['gcc'] + FLAGS + ['-o', os.path.join(B, out)] + [os.path.join(ROOT, s) for s in srcs] + (LIB if host_mgba else ['-lm'])
    r = subprocess.run(cmd, capture_output=True, text=True)
    check('build ' + out, r.returncode == 0, r.stderr[-300:])
    return r.returncode == 0


def run(exe, *args):
    r = subprocess.run([os.path.join(B, exe)] + [str(a) for a in args], capture_output=True, text=True, cwd=B)
    return '\n'.join(l for l in r.stdout.splitlines() if not l.startswith(('GBA', 'Savestate')))


def run_env(env, exe, *args):
    r = subprocess.run([os.path.join(B, exe)] + [str(a) for a in args], capture_output=True, text=True, cwd=B, env=dict(os.environ, **env))
    return chr(10).join(l for l in r.stdout.splitlines() if not l.startswith(('GBA', 'Savestate')))


def probe(state):
    return run('stateprobe.exe', os.path.join(B, 'gs.gba'), '-', os.path.join(B, state))


def nav(state, *taps):
    out = run('navtest.exe', os.path.join(B, 'gs.gba'), SAV_B, os.path.join(B, state), *taps, os.path.join(B, 'nav_selftest.ppm'))
    return [tuple(map(int, m)) for m in re.findall(r'done after \d+ frames: hand at \((-?\d+),(-?\d+)\)', out)]


def main():
    print('== build')
    ok = build('stateprobe.exe', ['tools/stateprobe.c', 'src/gs_state.c'])
    ok &= build('navtest.exe', ['tools/navtest.c', 'src/menunav.c', 'src/gs_state.c'])
    ok &= build('maptest.exe', ['tools/maptest.c', 'src/mapview.c', 'src/mapdecode.c', 'src/gfx.c', 'src/font_data.c', 'src/art.c', 'src/art_data.c', 'src/logo_data.c', 'src/icon_data.c', 'src/mapicon_data.c', 'src/gs_areas.c', 'src/gs_state.c'])
    ok &= build('ui_preview.exe', ['tools/ui_preview.c', 'src/ui.c', 'src/gfx.c', 'src/font_data.c', 'src/art.c', 'src/art_data.c', 'src/logo_data.c', 'src/icon_data.c', 'src/mapicon_data.c', 'src/mapview.c', 'src/mapdecode.c', 'src/menunav.c', 'src/gs_state.c', 'src/gs_areas.c', 'src/gs_items.c', 'src/gs_abilities.c', 'src/bestiary.c', 'src/gs_djinn.c', 'src/dialoglog.c', 'src/gs_field.c', 'src/gs_classes.c', 'src/gs_itemdesc.c'],
                host_mgba=False)
    ok &= build('dlog_test.exe', ['tools/dlog_test.c', 'src/dialoglog.c'], host_mgba=False)
    ok &= build('best_test.exe', ['tools/best_test.c', 'src/bestiary.c'], host_mgba=False)
    ok &= build('mapdecode_test.exe', ['tools/mapdecode_test.c', 'src/mapdecode.c'], host_mgba=False)
    if not ok:
        print('build failed')
        return 1

    print('== state reader: Lv 27 save (Venus Lighthouse grounds)')
    o = probe('lh_field.state')
    check('field mode, map 0xB5', 'mode=field map=181' in o, o[:120])
    check('four members', 'party=4' in o)
    check('Isaac Lv 27', 'member 0 Isaac lv27' in o, o)
    check('Venus 6 / Mars 5 Djinn owned', 'djinn owned 0003f 0005d' in o, o)

    print('== state reader: Lv 54 save (all 28 Djinn)')
    o = probe('far_field.state')
    check('map 0xBA', 'map=186' in o, o[:120])
    check('Isaac Lv 54', 'member 0 Isaac lv54' in o)
    print('== state reader: Jenna joins (order array [0,1,5] plus a stale 4th byte)')
    o = probe('jenna_test.state')
    check('three members', 'party=3' in o, o[:120])
    check('third member is Jenna, not Ivan', 'member 2 Jenna lv1' in o, o)
    check('all Djinn owned', 'djinn owned 0007f 0007f 0007f 0007f' in o, o)
    check('all Djinn set', 'set 0007f 0007f 0007f 0007f' in o)
    check('coins 999999', 'coins=999999' in o)
    check('menu flag clear in the field', 'menu=0' in o)
    check('Garet is cursed, Isaac is not', re.search(r'member 1 Garet.*cursed=1', o) is not None and re.search(r'member 0 Isaac.*cursed=0', o) is not None, o)
    print('== state reader: Garet just joined (order array still all zeros, mask 0x03)')
    o = probe('garet_test.state')
    check('two members', 'party=2' in o, o[:120])
    check('second member is Garet, not a second Isaac', 'member 1 Garet lv1' in o, o)

    print('== state reader: menus')
    o = probe('menu_djinn.state')
    check('menu flag set on the Djinn page', 'menu=1' in o, o[:100])
    check('hand cursor found on the Djinn page', 'hand=1,' in o)

    print('== state reader: battles')
    o = probe('esw_b1.state')
    check('battle mode', 'mode=battle' in o, o[:100])
    check('Kobold + Warrior Bee', 'enemy 0 Kobold lv20 hp 220/220' in o and 'enemy 1 Warrior Bee' in o, o)
    check('menu flag ignored in battle', 'menu=0' in o)
    o = probe('esw_b2.state')
    check('single enemy battle (Brutal Troll)', 'enemies=1' in o and 'Brutal Troll' in o, o[:200])

    print('== tap-to-select')
    for tap, want, name in [((92, 99), (57, 99), 'Ember'), ((180, 123), (169, 115), 'Dew'), ((40, 83), (2, 83), 'Quartz')]:
        got = nav('menu_djinn.state', *tap)
        check('Djinn page: tap %s -> hand near %s' % (name, want), bool(got) and abs(got[-1][0] - want[0]) <= 4 and abs(got[-1][1] - want[1]) <= 3, got)
    got = nav('menu_items_a.state', 150, 92)
    check('Items list: tap lands on the row at y~92', bool(got) and abs(got[-1][1] - 92) <= 4, got)

    print('== map stitching')
    out = run('maptest.exe', os.path.join(B, 'gs.gba'), SAV_A, os.path.join(B, 'lh_field.state'), 5, 120, os.path.join(B, 'map_selftest.ppm'))
    m = re.search(r'map key (\d+) bounds (\d+) x (\d+)', out)
    check('mosaic built', m is not None and int(m.group(2)) > 100 and int(m.group(3)) > 100, out[-160:])
    out = run('maptest.exe', os.path.join(B, 'gs.gba'), SAV_A, os.path.join(B, 'ms_a1_002.state'), 3, 5, os.path.join(B, 'map_selftest2.ppm'))
    check('world map is not stitched', 'empty map' in out or 'map key 2 ' not in out, out[-160:])

    print('== The Lost Age (needs build/ta.gba and the taf_*/bp_* states from tools/ta_savefield.py and tools/ta_battlephases.py)')
    ta = os.path.join(B, 'ta.gba')
    if not os.path.exists(ta):
        print('  skipped: build/ta.gba not found')
    else:
        tp = lambda state: run_env({'GS_TLA': '1'}, 'stateprobe.exe', ta, '-', os.path.join(B, state))
        o = tp('taf_2636.state')
        check('TLA: party Felix, Jenna, Garet, Piers in order', all(('member %d %s' % (i, n)) in o for i, n in enumerate(['Felix', 'Jenna', 'Garet', 'Piers'])), o[:300])
        check('TLA: map 287, coins 994200', 'map=287' in o and 'coins=994200' in o, o[:120])
        check('TLA: 18 Djinn per element', 'djinn owned 3ffff 3ffff 3ffff 3ffff' in o, o[-200:])
        o = tp('taf_35372.state')
        check('TLA: Isaac leads a party of the original four', 'member 0 Isaac' in o and 'member 3 Mia' in o, o[:300])
        # the opening of a new game (no password): the save-data party list is still empty, the game's second copy holds the party (regression: four Isaacs)
        for st, want, label in (('taf_prologue.state', ('party=3', 'member 0 Felix', 'member 1 Jenna', 'member 2 Mia'), 'Felix, Jenna, Mia at the Venus Lighthouse'),
                                ('taf_prologue_jenna.state', ('party=1', 'member 0 Jenna'), 'Jenna alone once she leads')):
            if os.path.exists(os.path.join(B, st)):
                o = tp(st)
                check('TLA opening (%s): %s' % (st, label), all(w in o for w in want) and 'Isaac' not in o, o[:300])
            else:
                print('  skipped: build/%s not found (made by running a new game on the desktop build, see docs/LOST_AGE.md)' % st)
        o = tp('taf_4716.state')
        check('TLA: renamed lead and Sheba read', 'member 0 Feliz' in o and 'member 2 Sheba' in o, o[:300])
        want = {'fight': 'phase=1 top=1 cmd=0', 'cmd': 'phase=1 top=0 cmd=0', 'cmdpsy': 'phase=1 top=0 cmd=1', 'list': 'phase=2', 'target': 'phase=3 top=0 cmd=-1 target=0'}
        for ph, w in want.items():
            o = tp('bp_%s.state' % ph)
            line = next((l for l in o.splitlines() if l.startswith('battle')), '')
            check('TLA battle %s: %s' % (ph, w), w in line, line)
        o = tp('bp_list.state')
        check('TLA battle list shows the ability description', "text=Pierce a foe with a colossal sword." in o, o[:200])
        o = tp('bp_target.state')
        check('TLA battle target names the enemy', 'text=Fire Bird' in o, o[:200])
        if os.path.exists(os.path.join(B, 'al_target.state')):
            o = tp('al_target.state')
            check('TLA battle: party member chosen as a target (Herb) is read in party order', 'target=3 ally=1' in o and 'text=Piers' in o, o[:240])
        # camera alignment: stitch frames at their camera position and count pixels that disagree with earlier paint (3.5% when the camera is read one frame early, 20-30% otherwise)
        out = run_env({'GS_TLA': '1', 'GS_MISMATCH': '1'}, 'maptest.exe', ta, '-', os.path.join(B, 'taf_35372.state'), 2, 250, os.path.join(B, 'map_ta_selftest.ppm'))
        m = re.search(r'alignment: \d+ of \d+ overlapping pixels disagree \(([\d.]+)%\)', out)
        check('TLA map: camera stays aligned with the picture (< 6% disagreement)', m is not None and float(m.group(1)) < 6.0, out[-200:])
        out = run_env({'GS_TLA': '1', 'GS_MISMATCH': '1', 'GS_BATCH': '4'}, 'maptest.exe', ta, '-', os.path.join(B, 'taf_35372.state'), 2, 250, os.path.join(B, 'map_ta_selftest2.ppm'))
        m = re.search(r'alignment: \d+ of \d+ overlapping pixels disagree \(([\d.]+)%\)', out)
        check('TLA map: still aligned when only every 4th frame is stitched (fast-forward)', m is not None and float(m.group(1)) < 8.0, out[-200:])

    print('== message log')
    dump = os.path.join(B, 'ta_dialog.bin')
    if not os.path.exists(dump):
        print('  skipped: build/ta_dialog.bin not found')
    else:
        out = run('dlog_test.exe', dump, '02031E40')
        lines = [re.sub(r'^\[\d+\] ', '', l) for l in out.splitlines() if l.startswith('[')]
        check('log: found the dialogue lines', len(lines) >= 4, out[:300])
        check('log: no entry is a leftover piece of another line', not any(a != b and a in b for a in lines for b in lines), out[:400])
        check('log: the full line is kept', any(l.startswith("Don't change the subject, Alex! What did you mean") for l in lines), out[:400])

    print('== bestiary')
    out = run('best_test.exe')
    n = 0
    for line in out.splitlines():
        if line.startswith(('ok ', 'FAIL ')):
            n += 1
            check('bestiary: ' + line.split(' ', 1)[1], line.startswith('ok '))
    check('bestiary test ran', n >= 10, out[-200:])

    print('== map drawing from the ROM')
    out = run('mapdecode_test.exe', os.path.join(B, 'gs.gba'), -1, 'x')
    check('every map id decodes', '205 of 205 ok' in out, out[-200:])
    check('Vale is 240x752 half-res pixels', ' 3:240x752 ' in out or out.startswith('3:240x752 '), out[:200])
    out = run('mapdecode_test.exe', os.path.join(B, 'gs.gba'), 3, os.path.join(B, 'c_map3.u16'))
    check('Vale canvas written', 'canvas 240x752' in out, out)
    import struct as _st
    d = open(os.path.join(B, 'c_map3.u16'), 'rb').read()
    px = _st.unpack_from('<%dH' % (240 * 752), d, 8)
    check('Vale canvas is fully painted', sum(1 for v in px if v) > 240 * 752 * 0.99)
    ta = os.path.join(B, 'ta.gba')
    if os.path.exists(ta):   # The Lost Age: every map id but one (an unused entry) decodes
        out = run_env({'MD_GAME': '2'}, 'mapdecode_test.exe', ta, -1, 'x')
        check('Lost Age maps decode', re.search(r'32[34] of 325 ok', out) is not None, out[-120:])

    print('== battle command bar with fewer icons')
    if os.path.exists(os.path.join(B, 'bar4.state')):   # a 4-icon bar (Summon and one more hidden), cursor on the third icon: that is Item, not Djinn
        out = run('stateprobe.exe', os.path.join(B, 'gs.gba'), '-', os.path.join(B, 'bar4.state'))
        check('4-icon bar: third icon is Item', re.search(r'phase=1 .*cmd=4 ', out) is not None, out[-200:])
    print('== UI self-tests')
    out = run('ui_preview.exe', os.path.join(B, 'far.bin'), os.path.join(B, 'uit'))
    n = 0
    for line in out.splitlines():
        mm = re.search(r'^(.*) -> (.*) \(want (.*)\)', line)
        if mm:
            n += 1
            got, want = mm.group(2), mm.group(3)
            norm = lambda v: v.split('=')[-1].strip('()')
            check(mm.group(1), norm(got) == norm(want), line)
    check('ui_preview reported its self-tests', n >= 3, out[-200:])

    print('\n%d passed, %d failed' % (passes, len(fails)))
    return 1 if fails else 0


if __name__ == '__main__':
    sys.exit(main())
