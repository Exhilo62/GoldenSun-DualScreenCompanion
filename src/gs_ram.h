// Golden Sun (USA/Europe, AGSE) RAM map. See docs/RAM_MAP.md for what is verified.
#ifndef GS_RAM_H
#define GS_RAM_H
// GBA DISPCNT BG mode: field/menus use mode 0, battles use mode 1 (verified on the Vermin fight).
#define GS_REG_DISPCNT 0x04000000u
// Menu open flag: bit 0x40 is set while the quick menu (A) or any pause-menu screen is up (clear in field, dialogue, battle).
#define GS_MENU_FLAGS  0x02000060u
#define GS_MENU_BIT    0x40
#define GS_PARTY_MASK  0x02000040u  // u8: popcount = party size (verified)
#define GS_PARTY_ORDER 0x0200001Cu  // u8[4] record indices in party order, 0xFF-terminated (verified by poking)
// Area identity / camera / player position (verified by diffing walks; see docs/RAM_MAP.md)
#define GS_AREA_A      0x02000400u  // u8: 3 outdoors in Vale, 7/8 in houses
#define GS_AREA_B      0x02000408u  // u8
#define GS_AREA_C      0x0200041Au  // u8
#define GS_CAM_X       0x03001AD4u  // u16 camera scroll, clamps (Vale: 960..1200); screen left = camX
#define GS_CAM_Y       0x03001AD6u  // u16
#define GS_PLAYER_X    0x02030EC6u  // u16 world x (camX = playerX + 843 when unclamped)
#define GS_PLAYER_Y    0x02030ECEu  // u16 world y
#define GS_OAM         0x07000000u  // 128 sprite entries; the leader is the 32x32 sprite nearest the estimated player position
#define GS_COINS       0x02000250u  // u32 (verified by poke: menu showed the poked value)
#define GS_CHAR_BASE   0x02000500u  // character records, EWRAM
#define GS_CHAR_STRIDE 0x14Cu
#define GS_CHAR_COUNT  8
#define GS_OFF_NAME    0x00  // char[15], ASCII, NUL padded
#define GS_OFF_LEVEL   0x0F  // u8
#define GS_OFF_MAXHP   0x34  // u16
#define GS_OFF_MAXPP   0x36  // u16
#define GS_OFF_HP      0x38  // u16
#define GS_OFF_PP      0x3A  // u16
#define GS_OFF_CURSE 0x130 // u8: non-zero = cursed (status screen shows "Curse"; any non-zero value does, verified by poking 1..32)
#define GS_OFF_EXP     0x124 // u32 (verified by poke)
#define GS_OFF_ABILITIES 0x5C // 4-byte entries until a zero: 0x8000|id battle Psynergy, 0x4000|id field utility, plain id for the rest (names: gs_abilities.c)
#define GS_OFF_CLASS 0x129   // u8 class id (Lord = 4, Champion = 13, Magister = 23, Paragon = 33; text group 7 index = 65 + id)
#define GS_OFF_ELEM  0x48    // 4 x (u16 power, u16 resist) in the order Venus, Mercury, Mars, Jupiter (matches the Status screen)
#define GS_EXP_STRIDE 0x18C  // ROM: per-character table of 99 u32 cumulative EXP thresholds (entry L = EXP needed for level L+1)
#define GS_OFF_ATK     0x3C  // u16 (verified against the status screen: 25)
#define GS_OFF_DEF     0x3E  // u16 (13)
#define GS_OFF_AGI     0x40  // u16 (13)
#define GS_OFF_ITEMS   0xD8  // 15 x u16: bits 0-8 item id, 0x200 equipped, bits 11-15 = quantity-1 (verified by poking)
#define GS_ITEM_SLOTS  15
// Enemy records in battle use the character record layout (name, level, max HP/PP, HP/PP at the same offsets).
// Verified: Vermin at 0x02030878, then Bat at +0x14C in a two-enemy fight.
#define GS_ENEMY_BASE  0x02030878u
#define GS_ENEMY_MAX   5
// Battle turn indices (verified on two fights, 2 and 4 party members):
// SEL = hero whose command is being entered (0..3 only during command entry; other values otherwise).
// ACT = hero that last acted; it changes when the next hero acts and goes stale during enemy turns.
#define GS_BATTLE_SEL  0x03007DECu  // u8
#define GS_BATTLE_ACT  0x020306A0u  // u8
// Battle command UI (verified on a Lv27 fight; see docs/RAM_MAP.md):
// CMD = highlighted command, 0 Attack, 1 Psynergy, 2 Djinn, 3 Summon, 4 Item, 5 Defend (valid while commands are entered).
// DESC = UTF-16 description of the highlighted ability, present only while an ability list is open.
// TNAME = UTF-16 name of the targeted enemy, present only while a target is being chosen; TIDX = its index.
// DEPTH = menu level while commands are entered: 3 command bar, 0 ability/item list, 2 choosing a target (verified for
// Attack, Psynergy, Djinn, Summon and Item, with enemy and ally targets). TIDX is the target index (enemy or ally).
#define GS_BATTLE_DEPTH 0x03007C46u  // u8
#define GS_BATTLE_TOP   0x03007D84u  // u8: 0 on the Fight / Flee / Status round menu, 1 on a hero's command bar, other values inside lists (verified on two fights)
#define GS_BATTLE_CMD   0x02035C08u  // u8
#define GS_BATTLE_CMDS  0x02035BF8u  // u8: number of commands on the hero bar, 5 (no Summon icon) or 6 (verified on four fights)
#define GS_BATTLE_DESC  0x03007D00u  // UTF-16, up to 48 chars
#define GS_BATTLE_TNAME 0x03007CC0u  // UTF-16
#define GS_BATTLE_TIDX  0x03007C8Cu  // u8
// Djinn (verified on a Lv27 save with 25 Djinn; Flint and Torch toggled to standby in the menu):
// each character record has one byte per element, bit n = nth Djinn of that element in canonical order.
// OWNED = Djinn this character holds; SET = held and not on standby (standby = owned & ~set).
// Byte order inside the record is Venus, Mercury, Mars, Jupiter (4-byte stride).
#define GS_OFF_DJINN_VENUS   0xF8
#define GS_OFF_DJINN_MERCURY 0xFC
#define GS_OFF_DJINN_MARS    0x100
#define GS_OFF_DJINN_JUPITER 0x104
#define GS_OFF_DJINN_SET_DELTA 0x10  // SET byte = OWNED byte + 0x10

// Per-game RAM addresses. 0 = not located yet for that game (the reader skips what it cannot find).
// Record-internal offsets (GS_OFF_*) and the record stride are shared: verified identical in The Lost Age.
#include <stdint.h>
#include <stdbool.h>
typedef struct {
	uint32_t areaA, camX, camY, playerX, playerY;
	uint32_t coins, charBase, enemyBase, menuFlags;
	uint32_t partyMask;   // 0: party size = length of the order array
	uint32_t partyOrder;
	uint32_t battleSel, battleAct, battleDepth, battleTop, battleCmd, battleDesc, battleTidx;
	uint32_t battleCmdCount;   // u8: 5 or 6 icons on the hero command bar (Summon is left out when no Summon is available, so Item and Defend move up one); 0 = unknown
	bool hasMap;          // position/camera known, so the map stitcher can run
	uint8_t djinnCount;   // Djinn per element (bits used in each 32-bit owned/set mask): 7 in Golden Sun, 18 in The Lost Age
	uint32_t itemTable;   // ROM: 44-byte item records, id * 44 (+2 = type, 0 = consumable); 0 = unknown
	uint32_t eqPreview;   // EWRAM: the item list's stat preview for the highlighted gear (u32 each: ATK, DEF, AGI at +0, +4, +8); 0 = unknown
	uint32_t expTable;    // ROM address of character 0's EXP threshold table (0 = unknown)
	uint32_t textBuf;     // the text engine's 512-halfword ring (dialogue lines); also where list descriptions are written
	bool namesKnown;      // item / Psynergy / Djinn name tables are valid for this game (they are Golden Sun's)
	uint32_t battleLabel; // UTF-16: text of the highlighted battle command ("Fight", "Attack", "Psynergy"...); when set, replaces depth-3/cmd-index reading (The Lost Age)
	uint32_t battleTname; // UTF-16: name of the targeted enemy while a target is being chosen
	uint8_t camLag;       // the camera in RAM runs this many frames ahead of the picture (The Lost Age: 1); the stitcher then uses the value read just before the last emulated frame
	bool eqPreviewIndirect; // eqPreview is the address of a pointer to the preview (The Lost Age keeps it in a heap object that moves: the pointer sits at 0x03000000, the ATK / DEF / AGI words 8 bytes in)
	uint32_t partyOrderAlt; // EWRAM: a second copy of the party order the game keeps while the save-data array is still empty (The Lost Age opening); 0 = none
} GsRam;
extern const GsRam gsRamGs1, gsRamTla;
void gsSetRam(const GsRam* r);
const GsRam* gsRam(void);
#endif
