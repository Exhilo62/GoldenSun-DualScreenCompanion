# GoldenSun3DS - feature reference and developer notes

mGBA core (`third_party/mgba`) running Golden Sun on the 3DS top screen, with a custom touch UI on the bottom screen.
Android dual-screen devices run the `.3dsx` through a 3DS emulator (Azahar).

## Games
Golden Sun and Golden Sun: The Lost Age are both supported as ROM slots (`golden_sun.gba`, `lost_age.gba`); each has its own saved states, autosave and maps. The Lost Age has a partial info UI (party, stats, coins, area id, enemy list); live map and item/Djinn/Psynergy names are still missing. See docs/LOST_AGE.md.

## Bottom screen
Game input uses the console's real controls; the bottom screen is a touch-only companion UI (no on-screen action buttons).
- Tab bar with icons: Party, Psynergy, Djinn, Map, Items, Menu. Footer shows the area name and coins.
- **Party**: portrait cards with Lv, HP/PP gauges, ATK/DEF/AGI and a CURSE or DOWN badge.  **Psynergy**: PP gauges.  **Items**: per-character inventory with names, equipped bars and quantities.  **Djinn**: four element panels (Venus/Mars/Jupiter/Mercury) listing each Djinn by name; set Djinn bright, standby Djinn red, uncollected shown as `???`.
- **Map**: a live exploration map (works outside Vale: the player marker is calibrated per map from the OAM sprites, followers/NPCs are kept out of the mosaic, map id keys the slot). The app stitches the game's own frames into a per-area mosaic using the camera position, so it fills in as you walk. Drag to pan, `Center` re-centres on the player marker, `Zoom` toggles detail/overview, `Clear` forgets the current area.
  Maps persist: they are saved to `maps.bin` on the SD card (every ~30 s when changed, and on exit) and reloaded at start. The map shows only the player marker: door pins and their "To <area>" labels were removed because they overlapped and showed unnamed areas as raw ids (the door data is still stored in the file). Area names come from a built-in table (`src/gs_areas.c`); unknown ids show as "Area 0xNNN". Extra names can be added without rebuilding in `sdmc:/3ds/GoldenSun3DS/areas.txt`, one per line, e.g. `0x1A3 Kolima Forest` (a `#` starts a comment).
- **Battle** panel appears automatically when the GBA enters BG mode 1; it lists the enemies (name, level, HP gauge) above the party cards; the previous tab returns after the fight.
- **Menu**: fast forward (4x), save state / load state with three slots (`state.ss`, `state2.ss`, `state3.ss` next to the ROM), "Autosave on exit" (writes `auto.ss` on exit and every ~5 minutes in the field, and loads it at start), top screen `1x` or `Fill (stretch)`, "Menus on bottom screen" On/Off (saved in `settings.bin`; see below), clear map.
- Art: tab icons come from the project mockup; the eight character portraits are the games' own 32x32 status-screen portraits (`assets/reference/<name>_game.png`, baked by `tools/genart.ps1` into `src/art_data.c`). Item icons (both games) and the four Djinn creatures are the games' own menu art too, baked by `tools/genicons.py` into `src/icon_data.c`. The ROMs themselves are not bundled.
- Text uses proportional bitmap fonts rasterized from the Ubuntu Font Family (Ubuntu Font Licence 1.0, (c) Canonical Ltd) by `tools/genfont.ps1` into `src/font_data.c`: tiny 10px (inside gauges and tags), body 11px, heading 16px.

## Menus on the bottom screen
With **Menu > Menus on bottom screen = On**, whenever a game menu is open (quick bar, Psynergy, Djinn, Items, Status, their sub-lists) the top screen holds the last menu-free field frame and the bottom screen shows the live game menu with touch buttons: D-pad, A, B, L, R and CLOSE (backs out of every menu). While you are on the info tabs, tapping the tab you are already on opens the matching game screen (Party -> Status, Psynergy, Djinn, Items); tapping a party card opens that member's Status page, tapping a Psynergy row opens that member's Psynergy list, tapping the active name on the Items tab opens that member's items; tapping the footer opens the quick bar. Tapping the mirrored game screen moves the game's pointing hand to the tapped entry (D-pad presses, steered by reading the hand sprite from OAM); tapping the same spot again presses A. The physical buttons keep working.

## Dual-screen devices (Android) and desktop
- Verified here: Azahar on Windows runs the `.3dsx`; top screen = game, bottom screen = this UI, touch via mouse.
- Android (not tested from here; the steps below follow what worked on Azahar for Windows): install Azahar, copy `GoldenSun3DS.3dsx` anywhere it can open it, and put the ROM at `sdmc:/3ds/GoldenSun3DS/golden_sun.gba` inside Azahar's virtual SD card. Not tested on an Android device from here.
- On dual-screen hardware use Azahar's layout options to show the 3DS top screen on one display and the bottom screen on the other; the app itself renders a standard 400x240 top / 320x240 bottom pair, so nothing app-side is device specific.
- Mirror test on desktop: `powershell -ExecutionPolicy Bypass -File tools/azahar_mirror.ps1` (needs a `state.ss` on the virtual SD, e.g. a copy of a host `.state`; enables the option, opens the Djinn game screen from a tap, presses CLOSE, and saves screenshots).
- Smoke test on desktop: `powershell -ExecutionPolicy Bypass -File tools/azahar_smoke.ps1 -LoadState -Tab 4` (loads `state.ss` from the SD card and clicks a tab).

## Build
Build from a path without spaces: `D:/gs3ds` is a junction to this folder.
1. Core (once): configure `build/mgba-3ds` with mGBA's `src/platform/3ds/CMakeToolchain.txt`
   (BUILD_STATIC=ON, GB core off, optional libs off, -DCMAKE_POLICY_VERSION_MINIMUM=3.5), then `make mgba zlibstatic`.
2. App: `cd /d/gs3ds && make` -> `build/app/GoldenSun3DS.3dsx` (+ `.smdh` icon).

## Run
Put your ROM at `sdmc:/3ds/GoldenSun3DS/golden_sun.gba` (not bundled). In Azahar the SD root is `%APPDATA%/Azahar/sdmc`.
Android: same path on Azahar's virtual SD.

## Research tools (host PC)
- `tools/probe.c` -> `build/probe.exe`: headless mGBA (stdin commands: run/shot/dump/save/load/r8/w16...).
- `tools/gs.py`: Python driver (screenshots to PNG, montage, RAM dump/diff).
- `tools/ui_bench.c`: host benchmark of the bottom-UI draw cost per tab (`gcc -DGFX_STATS`); counts pixel writes, blends and rect calls.
- `tools/genart.ps1`: bakes the tab icons, four portraits, ornate frame corner, compass rose and cloud background from `assets/reference/*.webp` into `src/art_data.c` (generated, do not edit).
- `tools/textdec.py`: decodes the game's compressed text (ability names/descriptions, dialogue) through the emulator; see docs/RAM_MAP.md. `tools/probe.exe` gained `trace`, `bp`, `steps`, `bpdump`, `loadbin` and `callarm` for this.
- `tools/savefield.py`, `tools/findbattle.py`: boot a converted `.sav` from `goldensunsaves/` (convert with sps2sav, put in `build/saves/`; `savefield.py <tag>` taps through the title menu and saves `field_<tag>.state`), and random-walk a state until a battle starts (`findbattle.py state tag seed steps` saves `<tag>_bN.state`).
- `tools/ui_preview.c`: renders every bottom-UI tab from a RAM dump and self-tests touch handling.
- `tools/sps2sav.py`: converts a SharkPort `.sps` save (e.g. from GameFAQs) to a raw `.sav`; run research scripts with `GS_SAV=path/to.sav` (or `probe.exe rom sav`) to boot from it.
- `tools/azahar_smoke.ps1`: the only GUI test driver; it aborts unless Azahar is the foreground window. Options: `-LoadState -Tab N` (load `state.ss`, open a tab), `-WaitSeconds N`, and `-Keys "U:900;D:700"` (arrow-key holds in milliseconds, a screenshot after each `;` group). Door points of interest were verified with it: from an exterior state, `-Keys "U:900;D:700"` shows "To Vale - House 1" on the Vale map and "To Vale" inside the house.
  - Game and state selection (no more moving ROMs around): `-Game 1|2` picks Golden Sun / The Lost Age at the game picker; `-State f.state` copies an emulator state into that game's save slot 1 and loads it, and the previous slot file, thumbnail and `lastgame.txt` are restored afterwards; `-Tab N` opens a tab (`-1` keeps the app's own tab, e.g. the battle panel); `-Clicks "x,y,wait;..."` taps in bottom-screen coordinates (Back on the Save slots page is `281,64`); `-Close` stops Azahar. Example: `powershell -File tools/azahar_smoke.ps1 -Game 2 -State build/bp_list.state -Tab -1 -Clicks "281,64,2" -Close`.
- `tools/map_persist_test.c`: host test for map save/load, truncated-file rejection and door points of interest (`gcc ... tools/map_persist_test.c src/gfx.c src/font_data.c src/mapview.c src/gs_areas.c`, then `map_persist_test replay.bin out.ppm`).
- `tools/maptest.c` -> `build/maptest.exe`: runs the real state reader + map stitcher headless while random-walking a state (`maptest rom sav|- start.state seed steps out.ppm`); `tools/ppm2png_any.py` converts the result.
- `tools/selftest.py`: builds the host tools and runs 30 checks against the saved states (state reader, Djinn, battles, menu flag, tap-to-select, map stitching, UI touch handling). Run it after any change.
- `tools/mapscan.py`: random-walks a state and screenshots/saves every new map id it reaches. `tools/navtest.c` -> `build/navtest.exe`: tests tap-to-select (hand-cursor steering) on a saved menu state.
- `tools/enemyscan.py`: random-walks until battles and logs the enemy records.
- `tools/autoplay.py`, `tools/autowalk.py`: random-walk the game headlessly, win fights, save states at transitions.
- `docs/RAM_MAP.md`: verified and unverified RAM findings.

## Android notes (untested)
- The app is a plain `.3dsx`; Azahar on Android should open it like any homebrew. The ROM and the optional `.sav`/`state.ss` live on the emulator's virtual SD card under `3ds/GoldenSun3DS/`, so copy them there through Azahar's user-data folder.
- On a dual-screen phone/handheld assign the 3DS top screen to one display and the bottom screen to the other in Azahar's layout settings. The app renders the standard 400x240 top and 320x240 bottom pair and needs nothing device specific; touch is the bottom screen's touch input.
- If the app shows "Could not open ROM", the path it printed is where it looked.

## Next
more map ids (towns and dungeons need saves beyond the Venus Lighthouse), the party-order array once Felix/Jenna/Sheba can join, and a check of Android. Tap-to-select was verified on Azahar (the hand moved to a tapped Djinn and the game showed its description).

## UI draw cost
The bottom screen is only redrawn when something changed (a hash of the game state and UI view), and twice in a row so both framebuffers match. A full redraw was optimised for the 3DS's column-major, rotated framebuffer: gradients, panels and rects write down contiguous columns, text and sprites blend with a packed RGB565 blend, the full-screen background and the tab bar are cached, and the map viewport writes straight into the framebuffer (and redraws at most every other painted frame). On the host a redraw went from 109-155 us to 37-64 us per tab with pixel-identical output; this is a relative measure only, nothing has been timed on a real 3DS yet.

- Before a game is started or loaded (title screen, intro, name entry: area id 0) the bottom screen shows a "Waiting for a game" panel and a "No game loaded yet" footer; the Menu tab still works so a state can be loaded.

## Speed
- The Menu tab footer shows a live timing readout (fps, ms per frame for state read `st`, emulation `emu`, top-screen blit `top`, map `map`, bottom UI `ui`). On an Old 3DS (or Azahar at its default 100% CPU clock) mGBA needs about 24 ms per frame, so the game runs near 35 fps; at a New 3DS clock (Azahar: Emulation > Configure > System > CPU clock speed 300%) `emu` drops to about 8 ms and it holds 60 fps.
- The top-screen copy is blocked by columns (about 3 ms instead of 6 ms in Fill mode) and the frame only waits for vblank when it finished early, which avoids snapping from 40 to 30 fps.
- Measured and found useless: mGBA frameskip (video is only about 1 ms), idle-loop detection, disabling audio.

## Start-up screens
The app always opens a themed game picker; a game whose ROM is not installed is shown greyed out ("Not installed - add its ROM with the patcher") and cannot be chosen, and the installed one is pre-selected. The picker (cloud background, ornate panels, character portraits; tap a game, or Up/Down and A; the last game played is marked). Error messages (missing ROM, wrong ROM) use the same look. Code: `src/launcher.c`.

## Two-core mode and the patcher
- New 3DS profile: the emulator runs on the main core and a worker on core 2 does the top-screen copy, map stitching and bottom-screen drawing for the previous frame (the picture lags one frame). Old 3DS profile (or any console without core 2): the same work runs inline, single core. The footer on the Menu tab shows `2c` when the worker is active.
- `profile.txt` in `3ds/GoldenSun3DS/` holds `new`, `old` or nothing (auto-detect). Measured in Azahar at its default 100% clock: 35 fps single core, 38 fps two cores (the emulator itself needs about 25 ms per frame there); a New 3DS clock (300%) holds 60.
- `tools/patcher/GoldenSun3DS-Patcher.ps1` (double-click `Install.bat`) is the installer: choose New or Old 3DS, the SD card, and your own ROM files; it checks the ROM game codes, copies the app and ROMs, writes `profile.txt` and first-run settings (Old 3DS defaults to the cheaper 1x top screen). `tools/make_dist.ps1` collects the patcher and the built `.3dsx` into `dist/GoldenSun3DS-Setup/` for sharing (no ROMs are included).
- Pacing (two-core mode): emulation runs on a 59.73 Hz clock of its own and never waits for the display. The worker paints, swaps and waits for the vblank; if it is still busy when the next frame is ready, that frame is emulated but not drawn (the footer shows `dN` dropped draws per second). A slow stretch therefore costs a few drawn frames, not half the speed. The audio queue is 8 buffers (about 94 ms).

## CIA output
The patcher can also build a single installable `.cia` (HOME Menu app) with your ROMs inside its RomFS (needs `bin\makerom.exe` and `bin\bannertool.exe`, included in `dist/GoldenSun3DS-Setup`). The app mounts the RomFS at start (`gsGameInitRomfs`), prefers `romfs:/golden_sun.gba`, `romfs:/lost_age.gba` and `romfs:/profile.txt` over the SD card copies, and keeps saves, states and settings on the SD card (`sdmc:/3ds/GoldenSun3DS/`, created on first run; the cartridge `.sav` path is now explicit because RomFS is read-only). The exheader (`tools/patcher/GoldenSun3DS.rsf`) asks for the 804 MHz clock, the L2 cache and core 2 on New 3DS. Not yet tried on a console; test the CIA with FBI or Azahar (File > Install CIA). A CIA contains your ROM: keep it for yourself.

## Tap to equip
On the Items tab, tapping a bag item (no gold bar) equips it through the game's own Item menu: the app opens Items for that member, walks to the item (pages flip with Right, five items per page), opens its menu and checks that the pointing hand sits on "Equip" (position about 125,38; it starts there for gear that can be equipped, on "Use" for a Herb), presses A, then backs out of the menus. Both screens hide the menus meanwhile: the top shows the last clean frame and the bottom shows only "Equipping..." and then "Equipped <item>". The game swaps the old piece itself and recalculates the stats (verified: Adept's Helm took max PP from 185 to 222). Items that cannot be equipped are left alone ("Cannot equip that"). Poking the equipped flag directly does not work (stats are not recalculated).

## Touch actions, message log, save slots, rewind
- **Message log** (Menu > Message log): the last 24 dialogue lines, newest first, taken from the game's text ring (`dialoglog.c`, fed from the worker with a copy of the 512-halfword ring at `0x0203396C`; Lost Age `0x02031E40`). Drag to scroll. Battle lines are not in that ring and are not logged yet.
- **Save slots** (Menu > Save slots): three cards with a screenshot and time each (`state.thb`, `state2.thb`, `state3.thb` next to the state files), Save and Load per card. This replaced the old Save state / Load state / State slot rows.
- **Rewind** (Menu > Rewind 5s): one in-memory snapshot per second, the last six kept; each press goes back up to five of them.
- **Tap actions** (all run through the game's own menus, hidden on both screens, with the pointing hand checked before anything is confirmed; the result is read back from the game's numbers):
  - Items tab: tap a bag item = Equip if it is gear, or Use: the party cards appear to choose who (the hand then steers to that member).
  - Psynergy tab: tap a blue name to cast it (healing Psynergy asks for a target the same way, others just cast).
  - Djinn tab: tap a Djinn to flip it between Set and Standby (the game's R button on the Djinn page).
- The map's Center button now centres on your character at every zoom level.

## Battle info, character screen, item info, speed, password keyboard
- **Enemy details**: each enemy row in the Battle panel shows its ATK / DEF / AGI (`A279 D87 S121`); the footer lists the three quickest combatants by Agility (a guide to the turn order, the game adds some randomness).
- **Character screen**: tap a card on the Party tab. Shows class (Lord, Champion, ...), level, ATK / DEF / AGI, HP / PP, EXP and how much is still needed for the next level (read from the ROM's per-character EXP table at `0x0807A830`, Lost Age `0x080B12C8`, 99 entries per character), and for each element the Djinn set/held and Power / Resist (record `+0x48`, order Venus, Mercury, Mars, Jupiter as on the game's Status page). Tap the screen edges for the next / previous member, **Game** opens the game's own Status page, **Back** returns.
- **Item descriptions**: hold your finger on an item in the Items tab for about half a second to see the game's description of it (text id = 117 + item id). A short tap still equips / uses it.
- **Speed**: hold **Y** (or **ZR** on a New 3DS) to fast-forward; **Menu > FF speed** sets the multiplier (2x, 3x, 4x), **Menu > Fast forward** latches it on, **Menu > Mute if slow** silences the sound whenever the game runs below about 54 fps (fast-forward is always silent). Saved in `settings.bin`.
- **Lost Age password keyboard**: on the "Waiting for a game" panel a **Password keyboard** button appears for The Lost Age. With the game on its password screen (title > Start > name > Transfer data > Password > level) tap the letters: the app steers the game's pointing hand to each key and presses A (`Del` presses B).
- **Patcher save import**: pick a `.sps` (SharkPort, e.g. from GameFAQs) or raw `.sav` for either game; it is converted to a 64 KB cartridge save and written next to the ROM (folder output) or next to the `.cia` (copy it to `sdmc:/3ds/GoldenSun3DS/`). An existing save is kept as `.bak`.
- Touch actions are quick: while one is pressing buttons in the hidden game menus the emulator runs four frames per loop (the presses are fed one per emulated frame, sound muted), the waits between list steps are shorter, and the menus are closed by pressing B until the game's own menu flag clears instead of a fixed number of presses (so a cast that closes the menu itself, such as Move, is no longer cancelled). On an emulator that is already at its speed limit (Azahar at 100% clock) the gain is limited by emulation speed; on a New 3DS the sequences take about 2 seconds.
- **Equip preview and Equip best.** Tapping an unequipped piece of gear in the Items tab first walks the game's item list to it and reads the stat change the game itself computes for the highlighted item (EWRAM `0x02038D04`: ATK, DEF, AGI as u32). A panel shows `529 > 323  -206` style changes with Equip / Cancel. "Equip best" (bottom right of the Items tab) scans every member's gear the same way (first pass: items whose ROM type byte is non-zero; later passes only the improvements), equips the biggest gain (sum of the three stat changes) and repeats until nothing improves. It is paced by emulation: in Azahar at 100% CPU it takes about a minute for four members, a New 3DS is several times quicker. Golden Sun only for now (the Lost Age preview address is not mapped yet, so it equips directly).
- **Autosave on by default, rotating save backups.** The state saved on exit (and every ~5 minutes in the field) is loaded at start; turn it off in Menu. The battery save is copied to `<game>.sav.1` ... `.5` at every start when it differs from the newest copy.
- **Djinn sets.** The "Sets" button on the Djinn tab (both games) opens presets (All Set / All Standby) and three save slots (Save stores the current Set/Standby layout, Apply restores it). Applying walks the game's Djinn pages once, toggling only the Djinn that differ (about 30 s for all 28 in Azahar at 100% clock; much quicker on a New 3DS), then reports "Djinn set applied". The slots live in `djinn.bin` beside the saves. In The Lost Age the game's page lists only the Djinn the four heroes hold (the other 36 stay as they are), and an All Standby of the 36 takes about 90 s in Azahar.
- **Speed and freezes.** Fast forward (hold Y / ZR, or the menu toggle) cannot be faster than the console can emulate: the app now measures the cost of a frame and never asks for more frames per loop than keep the picture updating about 30 times a second (before, a slow console showed one picture every few tenths of a second and no extra speed). When several frames run in one loop only the last is drawn, and when the game falls below ~54 fps every other frame is skipped (game logic and sound keep full rate; it tries full drawing again after ten good seconds). The map file and the autosave state are serialised in memory and written to the SD card in 16 KB slices from the main loop (`src/bgwrite.c`), and the rewind snapshots use fixed buffers, so none of them can stall a frame. The Menu footer shows `NNfps 2c gameNN eMM dN tN cNN [sk]`: `e` = milliseconds to emulate one frame, `c` = a start-up CPU benchmark in milliseconds (about 30 with the New 3DS 804 MHz clock active, about 90 without it), `sk` = automatic frame skip is on.

## Bestiary, Djinn details, save-slot notes
- **Bestiary** (Menu > Bestiary, both games, one list per game in `bestiary.bin`): every enemy you fight is recorded with its level, HP, ATK / DEF / AGI and how many battles it appeared in. Drag to scroll. It is written when a battle that added something ends, and on exit. The Menu tab now has six rows of buttons.
- **Djinn details** (Djinn tab): hold a Djinn for half a second to see its name, element, who holds it (Set or Standby) and the game's description of what it does in battle. In The Lost Age (no Set/Standby flip yet) a short tap shows the same panel. In Golden Sun a short tap still flips Set / Standby, now when you lift your finger.
- **Save-slot notes**: each slot card shows the party level and the area name from when it was saved, next to the date.
- Not added because they already exist or need a save that reaches the game's ending: the Lost Age password keyboard, item descriptions (hold) and the equip preview are already in; showing the password Golden Sun gives at its ending and a "continue from my Golden Sun ending" shortcut cannot be tested without such a save.

## Checking everything
- `python tools/selftest.py`: the headless checks (99).
- `powershell -ExecutionPolicy Bypass -File tools\azahar_regress.ps1`: Azahar pixel checks for the Lost Age Djinn flip and single-item equip (`-Only sets` adds the five-minute Djinn sets round trip).
- `powershell -ExecutionPolicy Bypass -File tools\check_all.ps1 [-WithSets]` runs both.
- Real-console timing: create an empty `perflog.txt` in `sdmc:/3ds/GoldenSun3DS/`; the app appends the footer's fps/timing line every ten seconds.
- `tools/azahar_smoke.ps1` takes `-Drags "x1,y1,x2,y2,wait;..."` for scrolling lists.
