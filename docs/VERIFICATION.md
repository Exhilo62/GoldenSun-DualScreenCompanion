# Verification of the other session's work (Oct 23)

Method: rebuilt from scratch, ran `tools/selftest.py`, ran their own `dlog_test`, exercised each feature in Azahar through `tools/azahar_smoke.ps1`
(real app, real ROMs, saved states), ran the installer in its command-line mode into scratch folders, and inspected the CIA. The Azahar SD
folder was backed up / restored around every run.

| Claim (README) | Result | Evidence |
|---|---|---|
| Builds clean | OK | full rebuild; 2 harmless warnings left (unused `only`, unused `iconFns`); 12 before the header-guard fix |
| Game picker, greyed-out missing ROM, "last played" | OK | both ROMs present, and with the Lost Age ROM hidden |
| Two-core mode / `profile.txt` old | OK | footer shows `2c` normally, no `2c` with `profile.txt = old` |
| Character screen (class, EXP to next, Djinn Pow/Res) | OK, both games | Isaac Lv54 (GS), Felix Lv56 (TLA) |
| Item description on long press | OK for Golden Sun; **gap for The Lost Age** ("No description") | fixed: 372 Lost Age descriptions (text id 146 + item id) |
| Tap to cast Psynergy + target picker | OK | Potent Cure cost exactly 10 PP (102 -> 92) |
| Tap Djinn to flip Set/Standby | OK | Flint flipped to Standby through the game's menu |
| Djinn sets (All Standby) | OK | all 28 Djinn went red |
| Rewind 5s | Works mechanically | toast "Rewound 5 s", top screen changes after the press (small because little had moved) |
| Equip best | Runs, no change | gear in that save was already the best the bag allows; not a proof either way |
| Save slots (thumbnail, Save/Load) | OK | |
| Lost Age password keyboard | **Bug found and fixed** | keys typed ("AB") but the keyboard was never drawn when no game is loaded, because the "waiting for a game" panel was drawn first. Now drawn first; regression test added |
| Message log | **Bug found and fixed** | leftover fragments of overwritten lines were logged as messages ("I'm not sure he has, K", "ge the subject, ..."); now dropped; regression test added |
| Patcher, folder output | OK | ROMs copied, both `.sps` saves converted (byte-identical to our own converter), profile + settings written |
| Patcher, wrong ROM | OK | refused with the game code in the message, nothing written |
| CIA output | Structure OK, **not run** | valid header, content ends at end of file, both ROMs inside byte-for-byte. Azahar must install a CIA before running it, which would change the user's emulator, so it was not installed |
| Not verified | | fast-forward by Y key, tap-to-equip of a single item, real 3DS / Android, the CIA at run time |

## CIA run-time (Azahar, verified)
Built with `tools/patcher/GoldenSun3DS-Patcher.ps1 -NoGui -Output cia -Console new -Rom1 ... -Rom2 ...`, installed through Azahar's "CIA must be installed before usage" prompt, and booted from the installed title: the picker lists both games from the RomFS ROMs, and choosing The Lost Age reaches its name-entry screen with the touch UI. The install (title `0004000000E65001`... directory `0e650100`, its ticket) was removed afterwards and the SD folder restored; the Azahar tree matches its pre-test listing.
