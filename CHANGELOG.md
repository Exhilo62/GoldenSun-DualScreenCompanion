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
- "Also make a .3ds file for emulators" (with the .cia output): writes a cartridge image (`.3ds`) next to the .cia that Azahar and similar emulators open directly. It does not run on a real console. Command line: `-Make3ds`.
- Console and output type are chosen independently; the .cia is the default output.

**Polish**
- New logo and icon, octagonal gold coin in the footer, the Menu footer shows just the frame rate (details go to `perflog.txt` if you create that file next to the saves).
- Equip best button redrawn with the larger font so letters are not cut off.

**Stability**
- Fixed a crash when starting a game from the .cia (data abort at `0x1FF57FFE`, as soon as sound started): the .cia now declares the DSP memory range the audio library needs. The SD-card (.3dsx) version was never affected.
- As a safety net, if the audio start-up ever crashes a console, the next launch skips sound instead of crashing again; delete `audio_crash.flag` in `sdmc:/3ds/GoldenSun3DS/` to retry.

- Fewer stutters while walking: the map is saved to the SD card every 3 minutes (and on exit) instead of every 30 seconds, in 4 KB pieces with each slow file operation on its own frame.

- The map file is now overwritten in place (no delete and rename, the slowest SD operations). With an empty `perflog.txt` next to the saves, every loop that takes longer than 45 ms is logged as a `SPIKE` line with where the time went (emulation, SD write, rewind, autosave).

- All SD-card writes (map, autosave, settings, Djinn sets, bestiary) now run on a separate low-priority thread, so a slow card can no longer freeze the game (a 207 ms hitch after a battle was traced to a bestiary save on the main thread). Automatic frame skip now stays on for 30 s of full speed before it tests full drawing again, which removed periodic dips.

- Fixed the party list showing Ivan instead of Jenna after Jenna joins (stale byte in the game's party-order list made the reader fall back to the default order).

- Smoother top screen: the drawing thread now paints into private buffers and only waits for the display at the very end, so far fewer frames are dropped (the log showed a third of the top-screen frames being skipped).

- The pick screen now shows the build time under the version number (and the Patch notes window shows when the patcher was built), so two installs of the same version can be told apart.

- The `perflog.txt` lines are now also written by the background thread, and each SPIKE line splits the time into input/state (`pre`), emulation, the rest of the frame (`mid`), submit and the pacing sleep.

- Fewer dropped top-screen frames: the drawing thread used to wait for the screen refresh whenever the last picture was swapped in less than 17 ms earlier, which at 60 fps is almost always, so it often lost a whole frame to waiting. It now predicts the refresh timing and only waits when it has to (the `w` figure in `perflog.txt` counts real waits).

- The Menu footer now says why there is no sound: `(no audio)` when the audio system could not start, `(muted: slow)` when "Mute if slow" is silencing a device that cannot hold full speed.

- Sound in emulators without a 3DS audio dump: when the app runs in Azahar / Citra and finds no `dspfirm.cdc`, it writes a placeholder one (zeros) into the emulator's virtual SD and starts audio, because the emulator's built-in audio never reads it. Real consoles are never touched (the emulator is detected first).
- The "Fast forward" and "FF speed" buttons are hidden from the Menu tab (a 3DS cannot emulate fast enough for them to help); holding Y / ZR still speeds up as far as the console allows. The Menu buttons are now two columns of five.

**Known issues**: see the README ("Known limitations in this beta").

- **Old 3DS note:** on an Old 3DS the game runs slower (roughly 25-35 fps) and the automatic helpers (frame skip, "Mute if slow") may not be able to fix that; expect slow motion and choppy sound. The same happens in an emulator left at its default CPU speed.
- **Emulator guide** (`docs/EMULATORS.md`, also in the README): build a `.3ds` with the patcher, then in Azahar turn on New 3DS mode and set the CPU clock to about 300%. No audio dump from a real 3DS is needed (see the placeholder firmware above).
- **Roadmap** added (`docs/ROADMAP.md`): other devices are planned (desktop build, then a native Android app); until then the `.3ds` / `.cia` can be tried elsewhere, with no guarantees outside a New 3DS XL.
- Fixed the party list showing Isaac twice instead of Garet right after Garet joins (the game had not filled in its party-order list yet; the reader now checks it against the party mask and falls back to join order).
- "Menus below" and "Mute if slow" both start Off on a fresh install (they are options; your saved choice is kept). The guides explain both.
- Renamed to **Golden Sun - Dual Screen Companion** (working title, as more platforms are planned). The HOME Menu title, patcher windows, guides and README use the new name; file, folder and save locations (`GoldenSun3DS`) are unchanged so existing saves keep working.
- The patcher now names its output `Golden Sun - Dual Screen.cia` (and `.3ds`) automatically: that is the Save-as default, and picking just a folder works too.
- The pick screen now says "Dual Screen Companion" under the logo (the label box widens to fit it).
- The character screen's "Game" button is now labelled "Status" (it opens the game's own Status page).
- Less noise: the top screen's Fill mode now blends only the seam pixels when stretching (text keeps even stroke widths instead of ragged 1-2 pixel jitter), and the map is built from 2x2 averages of the game picture instead of single pixels, so dithered grass and text look smoother (areas already in `maps.bin` keep their old look until you walk them again, or use Clear map).
- Map tab: the Center and Zoom buttons now use the new gold crosshair and magnifier icons, and the footer no longer repeats the map number next to the area name (it shows "Party Lv N").
- **Maps drawn from the game's own data (Golden Sun and The Lost Age):** the Map tab now shows the complete area, with your position marked, from the first step: the app reads the map graphics out of your cartridge image (nothing is stored or shipped; it takes a fraction of a second when you enter an area). Where a map file holds several rooms side by side only the rooms you are in are shown. The old explore-as-you-go map is still used for the world map and for cartridge versions the app does not recognise (only the USA / Europe releases are supported).
- The Lost Age: the character screen now shows each character's class (Slayer, Paragon, Ronin ...), taken from the game's own text.
