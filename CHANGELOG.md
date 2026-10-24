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
- Shows the version and has a Patch notes button (this file).
- Console and output type are chosen independently; the .cia is the default output.

**Polish**
- New logo and icon, octagonal gold coin in the footer, the Menu footer shows just the frame rate (details go to `perflog.txt` if you create that file next to the saves).
- Equip best button redrawn with the larger font so letters are not cut off.

**Known issues**: see the README ("Known limitations in this beta").
