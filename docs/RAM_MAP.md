# Golden Sun (USA, Europe) RAM map

Found with `tools/probe.c` + `tools/gs.py` (headless mGBA, scripted input, screenshots, RAM write/dump).
Game code `AGSE`. All addresses are EWRAM (0x02xxxxxx). Little-endian.

## Verified
| What | Address | Notes |
|---|---|---|
| Character record n | `0x02000500 + n*0x14C` | 8 slots: Isaac, Garet, Ivan, Mia, Felix, Jenna, Sheba, PC07 |
| name | `+0x00` | ASCII, up to 15 bytes |
| level | `+0x0F` | u8 |
| max HP / max PP | `+0x34` / `+0x36` | u16 |
| current HP / PP | `+0x38` / `+0x3A` | u16. Poked in RAM, menu and overworld HUD followed |

| Coins | `0x02000250` | u32. Poked 0x777, menu showed 1911 |
| EXP | record `+0x124` | u32. Poked 0x55, menu showed 85 |
| ATK / DEF / AGI | record `+0x3C` / `+0x3E` / `+0x40` | u16, matches the status screen (Isaac 25/13/13) |
| Inventory | record `+0xD8` | u16 entries: low 9 bits item id, 0x200 equipped. Isaac: 0x20F, 0x259, 0x27F, 0x10B4 = Light Blade, Cotton Shirt, Padded Gloves, Herb x3. Names are in compressed text |
| Party size | `0x02000040` (u8) | popcount = number of members (1 = Isaac, 3 after Garet joins). Poking single bits or 0xF changes the party size only |
| Battle flag | `DISPCNT` `0x04000000` bits 0-2 | BG mode is 1 in battle (`0x1741`/`0x7741`), 0 in field/menus (`0x1F40`). Verified across 9 states |
| Live HP/PP in battle | character records | HUD matches records during the Vermin fight; they update live |

## Observations
- Random encounters happen in the grass around Vale after Garet joins (first fight: Vermin, 7 HP candidate at `0x02035C18`, not verified).
- Isaac coordinates are not at `0x03001AD4`; those are camera/scroll values that clamp.
- Event flags live around `0x02000040`-`0x02000150` (changed when Garet joined: `+0x40` 1->3, `+0x60` 16->80, `+0x146` 17->209).

## Camera, area and sprites (used by the Map tab)
- Camera scroll: X `0x03001AD4`, Y `0x03001AD6` (u16). The visible frame's top-left corner is (camX, camY) in world pixels; X clamps (Vale: 960..1200). Stitching 400 frames this way reproduced Vale cleanly.
- Area: bytes `0x02000400` (3 = Vale outdoors, 7/8 = Vale houses), `0x02000408`, `0x0200041A` form an area key.
- Player sprite: the visible 32x32 OAM sprite (shape square, size 2) nearest `(playerX + 963 - camX, playerY - 192 - camY)`; the camera has a dead zone so the formula alone is off by up to 16 px.
- The party HUD (teal panel at the top-right and an icon bar at the bottom) only shows after pressing A; the map skips those zones only when it detects them.

## Dialogue text and world position
- Dialogue text is decoded into RAM as UTF-16LE around `0x02033800`-`0x02034800` while a message is queued/shown (`tools/dialogue.py` reads it). Names such as the character names are plain ASCII elsewhere.
- Isaac world position: X `0x02030EC6`, Y `0x02030ECE` (u16, ~1 unit per frame walked; `0x03001AD4/6` are camera values that clamp).

## Item and enemy names
`src/gs_items.c` maps item ids 1..268 to names. Built by poking ids into Isaac's inventory and reading the item list (anchors: 15 Machete, 89 Cotton Shirt, 127 Padded Gloves, 180 Herb all matched the real inventory). Names are compressed in the ROM, so this table came from the screen. Ids from about 270 up are enemy names. They do not need a table: during battle the enemy record holds its name as plain ASCII.

## Enemy records (battle)
- Same layout as a character record (name char[15] at +0, level +0x0F, max HP +0x34, HP +0x38). Verified for one enemy: Vermin Lv1 20/20 at `0x02030878`.
- Further enemies follow at the same `0x14C` stride (verified: Vermin 20/20 + Bat 17/17 in one fight, state `es_x_b1.state`; `tools/enemyscan.py` finds such fights). `gsReadState` stops at the first invalid record. The record persists outside battle, so it is only read in BG mode 1.

## Battle turn indices
- `0x03007DEC` (u8): the hero whose command is being entered, 0..3, during command entry only (other values such as 16, 146 or 192 otherwise). Verified with 2 and 4 party members: values 0,1,2,3 in order while Isaac, Garet, Ivan, Mia chose.
- `0x020306A0` (u8): the hero that last acted. It changes when the next hero acts (Ivan=2 then Isaac=0 in a fight where Ivan was faster) and keeps its old value during enemy turns, so the UI highlights a hero only when the value changes and fades the highlight after ~2 s.

## Battle command UI (verified on a Lv27 fight, Isaac choosing)
- `0x02035C08` (u8): highlighted command while commands are being entered: 0 Attack, 1 Psynergy, 2 Djinn, 3 Summon, 4 Item, 5 Defend (confirmed by stepping the cursor with D-pad right/left, wraps).
- `0x03007D00` (UTF-16): description of the highlighted ability, present only while an ability list is open (e.g. "Strike with a massive sword." for Ragnarok); otherwise it holds junk or zero.
- `0x03007CC0` (UTF-16): name of the targeted enemy, present only while a target is being chosen ("Kobold", then "Warrior Bee" as the target moves); empty otherwise. `0x03007C8C` (u8) is the target index (0 for Kobold, 1 for Warrior Bee).
- **Menu depth: `0x03007C46` (u8)**: 3 = command bar, 0 = an ability/Djinn/Summon/Item list is open, 2 = choosing a target. Verified for Attack, Psynergy, Djinn, Summon and Item (enemy and ally targets), and on a second hero. The UI uses this instead of guessing from the text buffers (those go stale).
- Inside the Djinn list the command cursor `0x02035C08` holds scrambled values (167, 165), so the UI remembers the last command seen on the bar (depth 3).
- Target index `0x03007C8C` indexes the party for ally targets (items, healing; e.g. Herb: 0 = Isaac, 1 = Garet) and the enemy list for enemy targets. Enemy targets also leave the enemy's UTF-16 name in IWRAM `0x03007C00..0x03007D00` (its exact spot shifts by menu), which is how enemy vs ally targets are told apart.
- Lists show a description for the highlighted entry at `0x03007D00` for Psynergy ("Strike with a massive sword."), Djinn ("Strike a blow that can cleave stone.") and Items ("Replenishes 50 HP"). The Summon list showed no description here.
- The old guess (description present = list, name present = targeting) is superseded by the depth variable. The ability names themselves are only decoded for a single frame (see below), so the list shows the description and the highlighted command chip, not the ability name. Djinn/Item lists were not checked.

## Story progress notes (for finding Djinn / more battles)
- The opening dialogue tells Isaac to go **south** to the plaza (Dora: "just go south to get to the plaza"). Garet blocks the north stairs. A flood-fill (`tools/bfs.py`) from the post-join state finds a closed region of 1607 cells with no walk-through exits; a "huge rock" in the south says Isaac cannot budge it, and Garet's chest says "Garet's chest of valuables".
- Opening: name entry -> house scene with Jenna -> free roam in rainy Vale. Garet joins on the ledge NE of the start (flags `+0x40` 1->3). Walking toward the stairs north, Garet says not to go to Mt. Aleph, so the next story trigger is elsewhere.
- Random walks (tools/autoplay.py, tools/autowalk.py) from the post-join state only covered Vale's outskirts and found Vermin fights; no Djinn or new map found yet. Flint needs a story step I have not identified.

## Djinn (verified)
Source: a Lv27 save (Venus Lighthouse entrance, 25 Djinn), loaded with `tools/sps2sav.py` + `probe.exe rom sav` (or `GS_SAV=... python tools/gs.py`-based scripts).
- Per character record: one byte per element, bit n = nth Djinn of that element in canonical order (below). OWNED bytes at `+0xF8` Venus, `+0xFC` Mercury, `+0x100` Mars, `+0x104` Jupiter; SET bytes are the same + `0x10`. Standby = owned & ~set (toggled in the menu with R: Flint -> Venus SET 0x3F->0x3E; Torch -> Garet Mars SET 0x5D->0x1D).
- Canonical order: Venus Flint Granite Quartz Vine Sap Ground Bane; Mars Forge Fever Corona Scorch Ember Flash Torch; Jupiter Gust Breeze Zephyr Smog Kite Squall Luff; Mercury Fizz Sleet Mist Spritz Hail Tonic Dew.
- Observed holdings (Isaac Venus 0x3F, Garet Mars 0x5D + Luff on Jupiter 0x40, Ivan Jupiter 0x3F, Mia Mercury 0x5F) matched the in-game Djinn screen exactly. The element letter names printed beside the record offsets (`+0x118..`) look like per-element counts; unused.
- The party has no separate order array here: record order Isaac, Garet, Ivan, Mia matched the menu with all four present.

## Not yet known (need gameplay states)
- ~~Party order array~~: found, see below.
- Battle sub-phase (command menu vs animation) and enemy data. Enemy HP candidate above.
- Stats after +0x3A (ATK/DEF/AGI/LCK order), EXP, equipment, inventory (`+0xD8` looks like u16 item entries).

## Method
1. `python tools/gs.py`-based scripts drive `build/probe.exe`; `save/load` keep states in `build/*.state`.
2. Search known on-screen values (HP 30/30, PP 13/13, name) with the `dump` command, then confirm by `w16` poke + screenshot.
3. For flags: dump before/after an event and diff.

## Map id (verified on three saves)
- `0x02000400` is a u16 map id (duplicated at `0x02000408`); `0x02000402` is the entrance/spawn id used with it. Vale outdoors = 3, Vale houses = 7/8, Venus Lighthouse entrance = `0xB5` (from the save's own note), a later lighthouse room = `0xBA` (name unknown).
- Names are in compressed ROM text, so `gsAreaName()` in `src/gs_areas.c` only lists ids identified by hand (plus anything in the optional `areas.txt` on the SD card). No public id list was found by web search.
- Save sources: GameFAQs SharkPort saves converted with `tools/sps2sav.py`. Save A: Lv 27, 25 Djinn. Save B: Lv 54, all 28 Djinn (Bane, Fever, Flash, Tonic, Luff bit positions confirmed on it).
- Player screen position is `playerX + Kx - camX` with a per-map constant (Vale Kx=963/Ky=-192, Venus Lighthouse area about 0/540), so `gsReadState` learns it by voting over the 32x32 OAM sprites on frames where the player moved. Map `0x1FE` with player (0,0) is a transition frame and is skipped.

## Menus and layers (top-screen hiding experiment)
- Menu flag: `0x02000060` bit `0x40` is set while the quick menu (A) or a pause-menu screen (Djinn, Items...) is open; clear in the field, in plain dialogue (bit `0x10` only) and in battle. It rises one frame before the panels draw and clears one frame before they disappear.
- Layers (mGBA `enableVideoLayer`): BG0 = menu windows, text and the HP panel; BG2/BG3 = the world; OBJ = characters AND the quick-menu icon bar (so OBJ cannot be switched off without losing the player). The game rebuilds its shadow OAM (`0x0300347C`) every frame, so single sprites cannot be dropped by writing OAM from outside.
- The app therefore hides top-screen menus by holding the last menu-free frame (Menu tab > "Top screen menus": Hidden). Only the quick menu and the Djinn page were checked; other screens (shops, Psynergy casting) may not set the bit.
- Menu flag survey (Lv 54 save): bit `0x40` of `0x02000060` stays set on the quick bar and on the Psynergy, Djinn, Items and Status screens and their sub-lists; it clears when the last menu closes. Shops, the save screen and Psynergy casting in the field were not reachable from the saves, so they are unchecked.
- Quick bar: the cursor starts on Psynergy every time it opens, moves with D-pad left/right (wraps), and OAM sprite 0 (the enlarged icon) sits at x = 32 + 24 * index. The shoulder buttons are NOT used on the bar. Verified key script: A (hold 3 frames, wait 14), D-pad right x n (3 on, 6 off), A opens Psynergy/Djinn/Items/Status for n = 0..3; B x 6 closes everything.
- Psynergy lists render real ability names in the "Which Psynergy?" screen, which is a way to build an ability-name table later.

## Map ids identified so far (`tools/mapscan.py` random-walks a state and screenshots each new id)
| id | what | source |
|---|---|---|
| `0x002` | World map (player RAM reads 32768,1 and the camera 0,0 here, so the Map tab does not stitch it) | walked out of the Venus Lighthouse grounds |
| `0x003` | Vale outdoors | first save |
| `0x007`/`0x008` | Vale house interiors | first save |
| `0x0B5` | Venus Lighthouse grounds (forest path to the entrance) | save note |
| `0x0B6` | Venus Lighthouse entrance hall (four statues) | seen |
| `0x0BA` | Venus Lighthouse upper room with the Venus emblem floor | late save |
| `0x1FE` | transition frame between maps (player 0,0) | seen |

## Ability names and game text (solved by running the game's own decoder)
- The ROM keeps all text as an order-1 Huffman stream, grouped by `group << 8 | index`; the group table is at ROM `0x080736B8` (42 groups of 8 bytes: data pointer, length-table pointer). The game decodes text with two ARM routines it copies into IWRAM `0x03003EEC` (init at `0x0300402C` with r1 = text id, next symbol at `0x03003EEC`, returns 0 at the end). `tools/textdec.py` dumps strings by calling those routines from the headless probe (`loadbin`/`callarm`, plus `bpdump`/`bp`/`steps`/`trace` for finding them).
- Group 3 holds monster names (index 0..50) and ability names: **ability id = index - 51** (Quake = 3, Cure = 90, Ragnarok = 192). Group 5 holds the ability descriptions at index = id + 58 ("Restore 70 HP." for Cure). Group 6 holds Djinn descriptions, group 1 and 2 equipment text, group 0 save-file messages. The app embeds names and descriptions in `src/gs_abilities.c`.
- A character's learned abilities are the 4-byte entries at record `+0x5C` (`0x8000|id` battle Psynergy, `0x4000|id` field utility such as Carry/Halt/Lift, plain id for Move, Mind Read, Reveal, Retreat), ended by a zero. The list is rebuilt from class, level and Djinn when the Psynergy menu opens, so it is output, not something to poke.
- The Psynergy tab shows the named list per member. In battle the command strip prefixes the highlighted ability's description with its name when the description is unique.

## Status flags
- Record `+0x130` (u8): non-zero = cursed. Verified by poking 0 (status screen says "Normal") and 1, 2, 3, 4, 5, 6, 8, 16, 32 (all say "Curse"); Garet has it set in the Lv 54 save (a cursed item) while Isaac/Ivan/Mia read 0. Other ailments (poison, sleep, ...) were not located. The Party and Psynergy tabs show a CURSE badge, or DOWN when HP is 0.

## Saves in goldensunsaves/ (11 SharkPort .sps from GameFAQs; convert with tools/sps2sav.py)
- Boot with `GS_SAV`, tap A through the title menu (Continue) and B out of menus: tools/savefield.py. Resulting field states: Lv 2 world map (id 0x02), Lv 21-27 (ids 0xA7 desert town, 0xB5 Lighthouse grounds), Lv 40 (0xBB lighthouse exterior), Lv 52-54 (0xBA lighthouse top, all Djinn), Lv 99 cheat saves (Isaac only, ids 3 and 8). Several names are edited (Julie, Anita, Vanir, Cura, xcode, Czar, San); one save is a cheat/hack one.
- Character records, in order: Isaac, Garet, Ivan, Mia, Felix, Jenna, Sheba, "PC07". The party mask at `0x02000040` is a bit per record index (0xF = first four; 0x1 = Isaac only even though all four records have data), so unused records (Jenna Lv 1-3, Felix and Sheba 0) are not party members.
- Record `+0x5C` holds `0x8000|id` ability entries (and `0x4000|id` entries) but poking them does not change the Psynergy list the menu shows (the list is rebuilt from class/level/Djinn), so names still cannot be recovered that way. All item ids in these saves are already in gs_items.c.
- 4-member battles (Willowisp x3 + Recluse, Orc Captain x2) render correctly in the Battle panel.

## Battle round menu (Fight / Flee / Status)
- Each round starts with a three-option menu before any hero's command bar: 0 Fight, 1 Flee, 2 Status (wraps). It shares depth `0x03007C46` = 3 and the cursor `0x02035C08` with the hero bar, so the byte `0x03007D84` tells them apart: 0 on the round menu, 1 on the hero bar, other values (83) inside lists. Verified on two fights, including backing out of a list with B. The command strip shows Fight / Flee / Status while it is up.

## Map ids found by mapscan over all 11 saves (Oct 22)
| id | what | confidence |
|---|---|---|
| `0x009` | Vale Sanctum (in-game banner) | verified |
| `0x013` | Sol Sanctum entrance (banner "Sol Entrance") | verified |
| `0x0B3` | Lalivero Sanctum (banner) | verified |
| `0x0A7`, `0x0B2` | Lalivero streets (same tileset, purple water) | likely |
| `0x0A8` | Lalivero house | likely |
| `0x0BB` | Venus Lighthouse tower exterior | likely |
| `0x004` | cliff path near Sol Sanctum, unnamed | unknown |
| `0x069` | cave house interior, unnamed | unknown |
| `0x0A9`, `0x0AA` | desert cliff paths with a door, unnamed | unknown |

## Party order array (verified Oct 22)
`0x0200001C`: u8[4] of character record indices in party order, terminated by `0xFF` (2 members: `00 01 FF FF`; 4 members: `00 01 02 03 FF`). A mirror copy sits at `0x02000438`. Poking `02 01 00 03` into 4034's state reordered the in-game HUD to Ivan, Garet, Isaac, Mia. The 2340 save naturally holds `03 00 02 01` (Mia leads). `gs_state.c` now maps slot -> record through it (falls back to record order if the array's length disagrees with the mask), including per-member Djinn. Battle selectors (`0x03007DEC` etc.) are party slots, not record ids. Felix/Jenna/Sheba joining is still untested; no save reaches that point.

Correction: `0x03007DEC` and `0x020306A0` are character **record indices**, not party slots (tools/battleorder.py: party order 2 1 0 3, the choosing sequence 0,2,1 showed Isaac, Ivan, Garet portraits). `gs_state.c` maps them through the order array. The ally-target index `0x03007C8C` has not been checked against a reordered party.
| `0x0B1` | "Road to Babi Lighthouse" (banner) | verified |
| `0x0B0` | desert approach with two statues, near 0xB1, unnamed | unknown |

## Hero command bar: five or six icons
- `0x02035BF8` (u8) is the number of icons on a hero's command bar: 6 normally, 5 when no Summon is available (the game then drops the Summon icon and the cursor `0x02035C08` runs 0 Attack, 1 Psynergy, 2 Djinn, 3 Item, 4 Defend). Verified on four fights (a Lv 54 party with every Djinn set has no Summon; a Lv 27 party with a spare Djinn has it). The reader maps the five-icon indices back to the six-slot order and the strip hides the Summon chip.
- The depth byte `0x03007C46` blips to 0 for exactly one frame every 24 frames while a hero's command bar (or the round menu) is up, which read as "an ability list is open" and flipped the bottom strip for a frame. `app.c` only believes depth 0 after three frames in a row; a real list holds 0 continuously (checked over 60 frames).

## Item menu automation (tap to equip)
- Item list: five rows per page, three pages ("123" header). Down wraps within the page; Right/Left flips pages. Opening an item shows a 3x2 grid (Use, Equip, Details / Give, Remove, Drop); the hand starts on the first enabled entry (Equip for gear in the bag at about x 125, y 38; Use at about x 95, y 37 for a Herb; Remove at about x 127, y 44 for worn gear). Pressing A on Equip swaps with the worn piece and the game recalculates stats. Poking the 0x200 flag in the item word changes nothing the game uses.

## Menus driven by touch actions
- Djinn page: four columns (Venus, Mars, Jupiter, Mercury) of owned Djinn only, hand starts at the first Venus Djinn; D-pad Right moves to the next column, Down to the next row; the R shoulder button flips Set/Standby for the highlighted one (djinnSet bit changes at once).
- Target prompts ("Use it on whom?", for items and healing Psynergy): the hand sits above the party at y about 23 with x about -1, 23, 47, 71 for members 0..3; Right moves to the next member and wraps. The prompt starts on member 0.
- Field Psynergy menu list (only castable ones): Growth 12, Douse 33, Whirlwind 78, Cure..Revive 90-101, Move..Retreat 140-149; healing ids 90-101 ask for a target. Five per page, pages flip with Right.
- Message text ring: 512 halfwords at `0x0203396C`, `<3>` = new line, `<2>` = end of a box, `<0>` between strings; a message is written twice and stale text stays until overwritten.

## Character record: class, elements, EXP (Golden Sun, Lost Age identical)
- `+0x129` (u8) class id: 4 Lord, 13 Champion, 23 Magister, 33 Paragon; name = text group 7 index 65 + id (`src/gs_classes.c`). Changes with the Djinn that are set.
- `+0x48`: four (u16 power, u16 resist) pairs in the order Venus, Mercury, Mars, Jupiter (Isaac Lv 54: 169/169, 78/88, 76/86, 77/87 as the Status page shows). Enemy records carry the same block and ATK / DEF / AGI at `+0x3C/3E/40` (Willowisp Lv 25: 279 / 87 / 121).
- EXP thresholds: ROM `0x0807A830` (Lost Age `0x080B12C8`), one table of 99 u32 per character record, stride `0x18C`; entry L = total EXP needed for level L+1 (entry 0 is 0). EXP to next level = `table[level] - exp`.
- Item descriptions: text id `117 + item id` (group 0 for ids up to 138, group 1 above), full text such as "Long Sword: Unleashes Titan Blade". Only the part after the colon ends up in the text ring while an item is highlighted (the type word is substituted by the text engine).

## Lost Age password keyboard (title > Start > name > "Transfer data" > Password > level)
- Grid, 4 rows: `ABCDEFGHJKLMNPQR`, `STUVWXYZ23456789`, `abcdefghijkmnpqr`, `stuvwxyz!?#&$%+=` plus Space (row 0), Return (row 1), Finish (row 3) in a side column. Hand x per column: 1, 7, 15, 23, 31, 47, 55, 63, 71, 79, 95, 103, 111, 119, 127, 143, side column 159; hand y per row: 115, 122, 130, 138. Right from the side column wraps to column 0. A types the key, B erases.

## Item table and equip preview (Golden Sun)
- ROM item records: 44 bytes each at `0x0807B6A8 + id * 44`. Byte +2 is the type (0 consumable, 1 weapon, 2 armour, 3 gloves/shield, 4 helm, ...), u16 at +8 is a stat value (not the same scale as the displayed total; the game applies class and level scaling). Entry 0 is empty.
- Stat preview while an item is highlighted in the Items list: u32 at `0x02038D04` (ATK), `0x02038D08` (DEF), `0x02038D0C` (AGI) = the stats after equipping it. It is stale for items that cannot be equipped, so the app writes `0xFFFFFFFF` first and treats an unchanged sentinel as "no preview". Needs about 4 frames after the cursor moves. Found by tracing writes (`probe trace`) to the value; the writer reads the item record above.
- The item list keeps the cursor's row when flipping pages (clamped to the last row of a short page); Down wraps within the page.
