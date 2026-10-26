# Golden Sun: The Lost Age (groundwork)

Status: the app runs The Lost Age with a partial info UI (Party, Psynergy PP, Items by id, Menu, battle enemy list).
Addresses are in `gsRamTla` (src/gs_state.c); anything not located is 0 and skipped, so nothing from Golden Sun is read for it.
Live map, Djinn/item/Psynergy names and most battle-menu details are still off (see the list below).

## Using it
- Put the ROM at `sdmc:/3ds/GoldenSun3DS/lost_age.gba` (Azahar: `%APPDATA%/Azahar/sdmc/3ds/GoldenSun3DS/`). Not bundled.
- With only one ROM present the app starts it directly; with both it asks (A = Golden Sun, B = The Lost Age) and remembers the last choice in `lastgame.txt`.
- The ROM header game code is checked (`AGS*` / `AGF*`, any region) so a mis-named file gives a clear message.
- Each game has its own files: Golden Sun keeps `state.ss`, `auto.ss`, `maps.bin`, `areas.txt` in the app folder (unchanged);
  The Lost Age uses the same names in `lost_age/`. `settings.bin` is shared. Cartridge saves sit next to the ROM (`lost_age.sav`).

## Carrying a save over from Golden Sun
The game itself does this: after the Golden Sun ending it shows a password; The Lost Age asks for it (or a Game Link
cable). Enter the password in The Lost Age's own menu. This is untested here, because no available save gets past the
Venus Lighthouse.

## Turning on the info tabs (to do when a ROM and saves are available)
1. Set `mapped` to true for the game in `src/gs_game.c` only after the steps below.
2. Move the addresses out of `gs_ram.h` into a per-game table (they are `#define`s used by `gs_state.c`, `app.c` and the host tools). Start by copying Golden Sun's values.
3. Re-verify, in this order (same method as RAM_MAP.md; scripts in `tools/`): character record base and stride, name/level/HP offsets, party mask and order array (`tools/partyscan.py`), coins, map id and player/camera, menu flag bit, Djinn bytes, battle selectors (`tools/battleorder.py`), enemy records.
4. Expect differences: new party (Felix, Jenna, Sheba, Piers), more Djinn per element, ship/world-map rules, and the item and ability id tables (`gs_items.c`, `gs_abilities.c`) need extending.
5. Add Lost Age portraits/area names; `areas.txt` in `lost_age/` already works for names without a rebuild.

## Findings so far (new game, no saves; ROM `AGFE`, tools/ta_newgame.py boots to Jenna at the Venus Lighthouse)
Verified against the status screen (Jenna Lv5, HP 65, PP 43, EXP 297, 200 coins):
- **Character records use Golden Sun's layout and stride (`0x14C`) but a different base: `0x02000A50`.** Record order is Felix, Jenna, Sheba, Piers, then four empty records (Isaac/Garet/Ivan/Mia are presumably added at transfer time). Name `+0x00`, level `+0x0F`, max HP/PP `+0x34/+0x36`, HP/PP `+0x38/+0x3A`, ATK/DEF/AGI `+0x3C/+0x3E/+0x40`, EXP `+0x124` and the item array `+0xD8` (item `0x10B4` = Herb x3, as in Golden Sun) all matched.
- **Coins are at `0x02000250`**, same as Golden Sun.
- Piers already has Lv 18 stats at new game (a pre-filled record).
Not the same as Golden Sun:
- `0x02000040` holds `0x20` with only Jenna in the party, so the party mask seems to use global character ids (Jenna = 5), not record indices. Unconfirmed; needs a save with several members.
- The Golden Sun party-order address (`0x0200001C`) is all zero here, and `0x02000400` (map id) is 0.
- `0x02030EC6/CE` (player X/Y) read a constant value that does not follow movement, and `0x03001AD4/D6` (camera) read 0, so these are probably different in this game.
Still to find: party order array, map id, player position and camera, menu flag (`0x02000060` bit `0x40` in Golden Sun), battle selectors and enemy base, Djinn bytes (Jenna's are 0 at new game).
Method that works: `GS_ROM=build/ta.gba` makes `tools/gs.py` and every script that uses it run The Lost Age; diff `dump` snapshots between states that differ in one thing. With a real mid-game save this is a short job.


## Update: five real saves (Lv 40-99, Oct 22) - what is verified
Tools: `tools/ta_savefield.py <tag>` (boots build/tasaves/ta.<tag>.sav, saves build/taf_<tag>.state), `tools/ta_findbattle.py`, `GS_TLA=1 build/stateprobe.exe ta.gba - state` prints what the app reads.
- **Characters:** 8 records from `0x02000520`, stride `0x14C`, order Isaac, Garet, Ivan, Mia, Felix, Jenna, Sheba, Piers. (The new-game state had Felix at `0x02000A50`, which is record 4 of the same table.) Offsets `+0x00/0x0F/0x34/0x36/0x38/0x3A/0x3C..0x40/0xD8/0x124` identical to Golden Sun.
- **Party order array:** `0x0200001C`, u8 record indices, `0xFF`-terminated (`04 05 06 07`; `04 05 01 07` in a save with Garet in slot 3; `00 01 02 03` style when Isaac's group leads). Party size = its length. `0x02000040` is NOT the party size here: it is `0xFF` (all characters unlocked).
- **Coins** `0x02000250`; **menu flag** `0x02000060` (0 -> `0x50`, bit `0x40` as in Golden Sun).
- **Map id:** u16 at `0x02000420`, duplicated at `0x02000428` (Golden Sun's `0x02000400` block shifted by `+0x20`). Names are not known; the footer shows "Area 0xNNN" and `lost_age/areas.txt` can name them.
- **Battle:** acting hero `0x03007E48` = record index (Felix 4, Jenna 5, Garet 1, Piers 7 verified over one round); enemy records from `0x020308C8` (Fire Bird, HP 303 -> 71 after an attack).
Not found / not trusted yet:
- Player X/Y and camera (so no live map). `0x02030ECE` follows vertical movement and `0x02030ECA` looks like X, but `0x02030EC6` is a constant 514 and the camera is unlocated.
- Battle depth/command/description/target addresses (Golden Sun's `0x03007C46` etc. do not apply), last-actor address.
- Djinn: the bytes at record `+0xF8..` read `0xFF` for most characters, which looks like all-owned padding, and the names are unknown. The Djinn tab shows "Djinn n" placeholders (Djinn names are the one name table still missing for this game).
- (Item and Psynergy names: solved, see "Text decoding".)
- Curse flag at `+0x130` is assumed to match (Piers reads cursed in one save; not confirmed).

## Portraits and curse flag
- All eight characters (Isaac, Garet, Ivan, Mia too, since the first-game ones were switched to match) use the game's own 32x32 status-screen portraits (`assets/reference/<name>_game.png`, captured with tools in this session, backdrop swapped for the app's navy gradient) baked by `tools/genart.ps1` into `art_data.c` at the same 68x46 size as the mockup portraits. Cards pick the portrait by character record index (`GsMember.rec`), so renamed characters ("Feliz") still get the right face. Replace those PNGs (and rerun genart.ps1) if painted art in the Golden Sun mockup style becomes available.
- The curse flag at record `+0x130` is confirmed for The Lost Age: Piers' status screen shows "Curse" in the save where the app reads cursed=1.

## Item icons and Djinn sprites (both games)
- `tools/itemicons.py` pokes item ids into a character (low 9 bits = id; ids above 511 collide with the equipped flag), opens the Items menu and crops the 5x3 icon grid. Golden Sun: 191 icons (exactly the 191 named items; unused ids show a coin). The Lost Age: 371 icons (ids up to 460; its table differs from Golden Sun's, e.g. id 7 is another sword), its item names are now decoded too (below).
- The four Djinn element creatures come from the Status screen; they are identical in both games.
- Raw crops live in `assets/reference/{gs1,tla}_icons/`; `python tools/genicons.py` bakes them into `src/icon_data.c`. The Lost Age capture: `GS_ROM=build/ta.gba ICON_STATE=taf_2636.state ICON_REC=0x2000A50 ICON_PREFIX=ta_ python tools/itemicons.py 1 511`.

## Text decoding (item and Psynergy names, descriptions)
- The Lost Age uses the same Huffman text scheme as Golden Sun. Its decoder overlay is at ROM `0x0803843C` (`tools/ta_textdec_iwram.bin`, 1900 bytes) and runs from IWRAM `0x03003EEC`; the init routine is at `0x03004030` (Golden Sun: `0x0300402C`, the overlay has 4 extra bytes). `tools/ta_textdec.py GROUP` dumps a group (text id = group << 8 | index).
- **Item names:** text id = `0x25F` + item id (groups 2-4). Sol Blade is id 10, Herb 180, Cosmos Shield 357. 372 named ids up to 460.
- **Ability names:** text id = `0x5A7` + ability id (same ids as Golden Sun: Cure 90, Ragnarok 192; 317 names). **Descriptions:** text id = `0x885` + ability id.
- `tools/gen_ta_names.py` writes the two Lost Age tables into `gs_items.c` / `gs_abilities.c` from `build/ta_names.json`.
- Not yet decoded: Djinn names (probably a later group) and area names.

## Psynergy icons
`tools/psyicons.py` steps the cursor through each member's field Psynergy list, identifies the highlighted ability from its description (Golden Sun description buffer `0x0203396C`, Lost Age `0x02031E40`), and crops each icon from a frame where it is not under the hand cursor. Only field-usable Psynergy appear in that menu (24 icons for Golden Sun, 30 for The Lost Age); battle-only Psynergy get a small coloured dot in the app. Baked by `tools/genicons.py`; shown at 8x8 in the dense Psynergy list.

## Area names, live map (Oct 22)
- **Map labels:** both games keep an internal label per map id in their text (Lost Age: text id `0xE00 + 186 + map id`, group 14-15; Golden Sun: `map id + 0xA07`, group 10). Map 0 TITLE, 1 UTIL, 2 WORLD in both. Verified: Lost Age `0x10F` = PROX_C (Prox), `0x11F` = MAR_O03 (Mars Lighthouse), `0x134` = ANEMOS06 (Anemos Sanctum); Golden Sun `0xB1` = B_MICHI01 (Road to Babi Lighthouse), `0x13` = SOLENT (Sol Entrance), `0x9` = HAISIN (Vale Sanctum), `0xB5` = V_ENT01.
- **Lost Age names:** `tools/gen_ta_areas.py` writes `src/gs_areas_tla.inc` (314 maps; 239 mapped to the player-facing location names from text group 14, indices 88-170, by label prefix such as PROX*, MAR_*, DELY*, KAIJIN*; the rest show a readable internal label; OTMP placeholders stay "Area 0xNNN"). The prefix to location links follow the order of the two lists and are confirmed for Prox, Mars Lighthouse, Anemos Sanctum and Venus Lighthouse; others (Garoh, Mikasalla, Loho, Suhalla Gate) are inferred from order and may be off.
- **Golden Sun names corrected from the labels:** `0xA7`/`0xA8` are Suhalla (not Lalivero), `0xA9`/`0xAA` are SHR_M (Suhalla Gate), `0xB0` Babi's Lighthouse, `0xB2`/`0xB4` Lalivero, `0x69` Lunpa house.
- **Camera:** Lost Age camera scroll is `0x020322F6` (X) and `0x020322FE` (Y), one pixel per frame (`0x03001124`/`0x0300112C` also track X). `0x02030ECA/CE` are not pixel positions (ECE creeps while walking sideways), so the reader derives the player's world position from camera + leader sprite. The live map stitches (a town was assembled from a random walk) and the Map tab shows the marker; some frames show 1-2 px vertical streaks, probably camera and image a frame apart.

## Battle UI, camera fix, descriptions (Oct 23)
- **Battle addresses (verified over a full round and every menu phase with `tools/ta_battlephases.py` / `stateprobe`):** menu depth `0x03007DD3` (3 = hero command bar, 0 = list open, 2 = round menu or target choice), bar-or-round flag `0x03007DB8` (1 on the hero bar, 0 otherwise), description `0x03007D34` (UTF-16), targeted enemy name `0x03007CF4` (UTF-16, present only during target choice), highlighted command text `0x02035708` (UTF-16: round menu Fight / Switch / Flee; hero bar Attack / Psynergy / Djinn / Summon / Item / Defend, so no cursor index is needed and the five-icon bar without Summon needs no special case). Acting hero `0x03007E48` (record index) as before. Not found: ally-target index and "last acted" (the highlight for party targets is skipped). Round menu order here is Fight, Switch, Flee (Golden Sun: Fight, Flee, Status).
- **Camera correction:** the real picture scroll is IWRAM `0x0300112C` (X) / `0x0300112E` (Y), *not* `0x020322F6/FE` (that is a smoother 1 px/frame counter that never matched the picture; it caused the streaks). The value in RAM leads the picture by exactly one frame (`GsRam.camLag = 1`): stitching with the camera read one frame earlier cut the overlap disagreement from 31.8% to 3.5% (`GS_MISMATCH=1 GS_CAMLAG=n maptest`, `GS_MISLOG=1` prints it per frame). The app reads the camera right before the last emulated frame of each loop, so fast-forward (several frames per loop) stays aligned. Player X/Y candidates at `0x03001124/26`.
- **Descriptions:** Lost Age ability descriptions are text id `0x885` + ability id; Djinn descriptions start at `0x9B1` with 20 slots per element (Venus, Mercury, Mars, Jupiter; 18 used). `tools/gen_ta_descs.py` writes both tables, so battle lists show the ability or Djinn name next to the description. Summon names are ability ids 380-412 (Venus, Ramses, Cybele, Judgment, Zagan, Haures, Charon, Mercury, Nereid, Neptune, Boreas, Moloch, Coatlicue, Azul, Mars, Kirin, Tiamat, Meteor, Megaera, Ulysses, Daedalus, Iris, Jupiter, Atalanta, Procne, Thor, Flora, Eclipse, Catastrophe). Enemy names come from the enemy record in RAM, so they need no table.
- **Area-name check:** the first half of the game's location list (Venus Lighthouse, Suhalla Gate, Daila, Kandorean Temple, Shrine of the Sea God, Dehkan Plateau, Madra, Mikasalla, Garoh, Yampi Desert, Air's Rock, Alhafra, Naribwe, Kibombo) is in the same order as the map labels, and Jupiter Lighthouse, Loho, Northern Reaches, Prox follow label order too; many prefixes spell the name (TAUPO, AQUAR, GAIAR, IZUMO, CHAM, ANGKOR, GND, HES, KLT, MAGMA, SHARMAN, TRYROAD, TAKARA, ANEMOS). Unverified by walking: nothing contradicts them.

## Verification (Oct 23)
- **In the real app (Azahar, `tools/azahar_smoke.ps1 -Game 2 -State ...`):** Items tab (names + icons), battle panel in every phase (round menu Fight/Switch/Flee, hero bar, Psynergy list with description, target choice with the enemy's HP). Golden Sun Psynergy tab re-checked through the same harness.
- **Fast-forward and the map:** simulated headlessly with `GS_BATCH=4 maptest` (stitch only the last frame of each batch of four, as the app does): camera read just before the last frame = 4.3% overlapping-pixel disagreement; stale (read at the start of the batch) = 29.8%; read after the batch = 22.4%. The app uses the first. An in-app walk test was inconclusive (the character walked out of the town through a door in both runs), so it is not yet confirmed visually under fast-forward on screen.
- **Regression tests:** `tools/selftest.py` now has a Lost Age section (party order, renamed lead, 18 Djinn, every battle phase, description and target name, camera alignment normal and batched). It skips itself if `build/ta.gba` is missing. 72 checks pass.

- **Djinn tap-to-flip (verified in Azahar):** the Lost Age Djinn page has one column per party member, so the app finds the holder's column (first member whose owned mask has the Djinn), presses Right that many times, then Down by the number of that member's Djinn sorted before it (ascending element, bit), and presses R. Checked with Echo (column 0, row 0) and Piers's Spring (column 3, row 2). A quick tap flips on release; a hold of 24+ frames shows the Djinn details. Djinn Sets now work in the Lost Age too: toggles are sorted by holder column then row, the cursor goes back to the top of a column before each move, and only Djinn that the four heroes hold are touched (verified: Save, All Standby, Apply returns the exact layout).
- **Ally-target index:** `0x03007CC0` is the party-order index while choosing an ally target (menu depth 2, top 255).
- **Single-item equip (verified in Azahar):** the Lost Age stat preview lives at `0x02039238` (ATK, DEF, AGI as u32; it holds the current stats for anything that is not new gear, so equal values mean "no preview"), and after A on a gear item the cursor starts on **Equip** with the hand at (134, 38) (Golden Sun: 125, 38). Tapping Piers's Lightning Sword now equips it and moves the gold marker off the Hestia Blade. The Lost Age only rewrites the preview when the cursor lands, so the app reads it without clearing it first and waits longer (the item page opens slowly); a tap then shows the Equip/Cancel popup (Attack 445 > 429 for the Lightning Sword). The `eqPreview`, `itemTable` (`0x080B2364`) and `textBuf` fields of the Lost Age RAM table were out of line before, which this fixed.
- **Fast-forward (verified in Azahar):** holding Y runs `speedMul` frames per loop; the password menu text typed out five lines in the time the unpressed run typed two.
- **Item ids above 460:** text id `0x25F + 461` is `???` and the ones after it are the placeholders "Venus/Mercury/Mars Djinni", not real bag items, so the name table stops at 460.
- **"Last acted" marker: not found.** A RAM diff over a full round (`bp_fight.state`, A pressed every 48 frames, a dump every 6 frames) gave 122 noisy candidates; the only monotonic one is `0x02006A9D` (1 to 4 across the round), which looks like the command-entry slot, not the acting hero. The feature is only a brief portrait highlight, so the Lost Age skips it.
- **Memory:** with `perflog.txt` the log also prints the free application heap and free linear memory (Azahar: about 31.7 MB linear free with the Lost Age loaded); check the same line on a real console.

## Feature parity with Golden Sun (audit, Oct 24, checked in Azahar with `-Game 2` and the saved Lost Age states)
| Feature | The Lost Age |
|---|---|
| Party cards (HP/PP/stats, curse badge, portraits) | works |
| Character screen (EXP to next level, Djinn Pow/Res) | works; **class name now shown** (new table `gs_classes_tla.inc`, ids from text groups 11 / 12) |
| Psynergy list, tap to cast ("Use Cure on whom?") | works |
| Djinn tab, tap Set / Standby, Djinn sets (All Set, All Standby, 3 slots) | works (sets overlay opens; Save / Apply use the same column-walking as single Djinn) |
| Items list, icons, long-press description | works |
| Equip preview, "Equip best" | **fixed Oct 26**: the preview now reads (see below); on a saved state with the best sword, tunic and boots in the bag, the scan picks a better item; not yet checked on a console |
| Map tab | **now drawn from the ROM** (Yallam, Anemos Sanctum checked), see MAP_DATA.md; the world map still uses the stitched map |
| Battle panel (phases, enemy HP/ATK, turn order, descriptions) | works (earlier verification) |
| Repeat round, Message log, Bestiary | same code as Golden Sun; **Repeat round still not exercised in a Lost Age fight** (scripted fights from a saved state kept ending or stalling before a second round menu) |
| Save slots, autosave, rewind, top-screen modes, "Menus below", "Mute if slow" | shared code, unaffected |
| Password keyboard | Lost Age only |

## Party list in the opening (Oct 25)
A new game without a password leaves the save-data party list (`0x0200001C`) all zeros until the opening is over, so reading it gave four copies of record 0 (Isaac, Lv28, the default clear-data party). The game keeps its active party in a second copy at `0x02000458` (8 entries, the first four are the party; in every good state it matches the save-data list), and in the opening that copy is the real list (Felix, Jenna, Mia at the Venus Lighthouse; Jenna alone once she leads). `gs_state.c` now takes the leading distinct entries of the save-data list and, when that list is empty or repeats an entry, the leading distinct entries of the second copy (`GsRam.partyOrderAlt`). Found by running a new game on the desktop build (scripted keys) and diffing memory against saved states; checked on the desktop build at the Lighthouse (cards match the game's own menu) and with selftest (110/110).

## Area names (Oct 26)
`src/gs_areas_tla.inc` is generated by `tools/gen_ta_areas.py`: the map labels are text ids `0xE00 + 186 + map id` and the player-facing location list is `0xE00 + 88 .. 170`. The map table has no location field (checked: entry `set, 0, code file, flags`; a brute-force search for a map -> location table found none). Labels are mapped to locations by prefix; the families in the last pass are inferred, see ANDROID_PORT.md. 314 of 325 ids are named; the rest are the placeholder maps.

## Equip best / item preview (Oct 26)
Symptom: Equip best did nothing in The Lost Age even with better gear in the bag. The scan reads the stat preview (ATK, DEF, AGI as u32 words) that the game shows beside the highlighted item. Golden Sun keeps it at a fixed address; The Lost Age keeps it in a heap object whose address changes with the state (`0x02039254`, `0x02038CF8`, `0x0203950C` in four saves), and fills it only about 60-90 frames after the cursor lands. Fixes: `GsRam.eqPreviewIndirect` (the address is a pointer at `0x03000000`, the words start 8 bytes in, never written through), longer waits in the scan (`app.c`, `djinnCount > 7` branches). Test state `taf_unequipped.state` made by taking Sol Blade, Erinyes Tunic and Dragon Boots off through the game's own menu; the scan then read +300 ATK for the sword and the candidates were found.
