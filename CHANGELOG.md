# Changelog

## v0.1.0-beta - first public test build
Goal of this release: find obvious problems. Everything below is new in a first release.

**Core**
- Golden Sun and The Lost Age on the 3DS top screen (mGBA core), touch companion UI on the bottom screen.
- Two-core pipeline on New 3DS (emulation on one core, drawing on the other), single-core mode for Old 3DS.
- Installer/patcher for Windows: `.cia` or SD-card files, New / Old 3DS profile, optional save import.
- Themed game pick screen (greyed-out card for a game that is not installed), version shown on it.

**Bottom screen**
- Tabs: Party, Psynergy, Djinn, Map, Items, Menu and an automatic Battle panel.
- Live exploration map, saved between sessions (player marker only).
- Character screen, item descriptions, enemy details and turn order, message log, bestiary, Djinn details.

**Touch actions** (through the game's own menus, Golden Sun)
- Tap to equip with a stat-change preview, "Equip best" per character, tap a Psynergy to cast, tap a Djinn to Set / Standby.
- Djinn sets (All Set, All Standby, three save slots), Repeat round in battle, Lost Age password keyboard.

**Convenience**
- Autosave and resume (on by default), three save-state slots with thumbnails, rewind 5 s, rotating `.sav` backups.
- Fast forward with frame cap, automatic frame skip when the console cannot hold full speed, non-blocking SD writes.

**Patcher**
- Double-click `GoldenSun3DS-Patcher.exe` to open it (a small launcher; `Install.bat` still works).
- Shows the version and has a Patch notes button (this file).
- Round "Mage's Arcane Gazette" badge in the top right corner.
- Console and output type are chosen independently; the .cia is the default output.

**Polish**
- New logo and icon, octagonal gold coin in the footer, the Menu footer shows just the frame rate (details go to `perflog.txt` if you create that file next to the saves).
- Equip best button redrawn with the larger font so letters are not cut off.

**Stability**
- Fixed a crash when starting a game from the .cia (data abort at `0x1FF57FFE`, as soon as sound started): the .cia now declares the DSP memory range the audio library needs. The SD-card (.3dsx) version was never affected.
- As a safety net, if the audio start-up ever crashes a console, the next launch skips sound instead of crashing again; delete `audio_crash.flag` in `sdmc:/3ds/GoldenSun3DS/` to retry.

- Fewer stutters while walking: the map is saved to the SD card every 3 minutes (and on exit) instead of every 30 seconds, in 4 KB pieces with each slow file operation on its own frame.

- The map file is now overwritten in place (no delete and rename, the slowest SD operations). With an empty `perflog.txt` next to the saves, every loop that takes longer than 45 ms is logged as a `SPIKE` line with where the time went (emulation, SD write, rewind, autosave).

**Known issues**: see the README ("Known limitations in this beta").
