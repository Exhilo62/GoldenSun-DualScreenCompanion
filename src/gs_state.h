// Game state snapshot read from GBA memory; consumed by the UI.
#ifndef GS_STATE_H
#define GS_STATE_H
#include <stdint.h>
#include <stdbool.h>

#define GS_PARTY_MAX 4
#define GS_ENEMIES_MAX 5

// Element order used by the UI and the djinn masks.
enum { GS_VENUS, GS_MARS, GS_JUPITER, GS_MERCURY, GS_ELEMENTS };

typedef struct {
	uint16_t id;      // 0 = empty slot
	uint8_t qty;
	bool equipped;
} GsItem;

#define GS_ABILITY_MAX 40

typedef struct {
	bool present;
	uint8_t rec;      // character record index (Isaac 0 .. Piers 7); 0xFF for enemies. Names are player-editable, this is not.
	char name[16];
	uint8_t level;
	uint16_t hp, maxHp, pp, maxPp;
	uint16_t atk, def, agi;
	bool cursed;
	uint32_t exp;
	GsItem items[15];
	uint8_t classId;
	uint16_t elemPow[4], elemRes[4];   // element order Venus, Mars, Jupiter, Mercury
	uint32_t expLevelStart, expNext;    // EXP at which the current level began / still needed for the next one (0 = unknown or max level)
	uint8_t abilityCount;
	uint16_t abilities[GS_ABILITY_MAX];   // learned list in game order: bit 15 battle, bit 14 field-only utility, low byte = ability id
	uint32_t djinnOwned[4], djinnSet[4];  // per character, element order GS_VENUS..GS_MERCURY (bit n = nth Djinn)
} GsMember;

typedef enum { GS_MODE_FIELD, GS_MODE_BATTLE } GsMode;
typedef enum { GS_PH_NONE, GS_PH_COMMAND, GS_PH_LIST, GS_PH_TARGET } GsBattlePhase;

typedef struct {
	uint32_t areaKey;          // changes when the area (map) changes
	int areaGroup;             // GS_AREA_A byte: 3 = Vale outdoors
	int camX, camY;            // scroll of the visible frame
	uint16_t bldcnt;           // alpha-blend control as last written by the game (set by the caller from the I/O registers)
	int playerX, playerY;      // world position
	bool hasSprite;            // leader sprite found in OAM
	int sprX, sprY;            // its top-left on the visible frame
	bool handFound;            // the menu pointing-hand sprite (tile 0x104/0x10C, 16x16) was found
	int handX, handY;          // its top-left on the GBA screen
	int spriteCount;           // visible OAM objects (screen rectangles), used to keep characters out of the map
	struct { short x, y, w, h; } sprites[40];
	bool menuOpen;             // quick menu / pause menu showing on the GBA screen
	GsMode mode;  // BATTLE when the GBA is in BG mode 1
	int partySize;
	uint32_t coins;
	uint32_t djinnOwned[GS_ELEMENTS]; // bit n = nth Djinn of the element held by anyone in the party
	uint32_t djinnSet[GS_ELEMENTS];   // subset that is set (owned and not on standby)
	GsMember party[GS_PARTY_MAX];
	int battleSel, battleAct;  // battle only: hero choosing / last acting (0..3) or -1
	int battleDepth;           // battle: raw menu depth byte, -1 when unknown (it blips to 0 for one frame about every 24 frames on the command bar)
	bool battleNoSummon;       // battle: the hero bar has no Summon icon (five commands)
	bool battleTop;            // battle: the round menu (Fight / Flee / Status, battleCmd 0..2) is up instead of a hero's command bar
	int battleCmd;             // battle: highlighted command 0..5 (Attack, Psynergy, Djinn, Summon, Item, Defend) or -1
	GsBattlePhase battlePhase; // battle: command bar / ability list / choosing a target (NONE while it is not the party's input)
	int battleTarget;          // battle: target index while TARGET (enemy index, or party index if battleTargetAlly), else -1
	bool battleTargetAlly;     // the target is a party member (items, healing)
	char battleText[56];       // battle: ability description (LIST) or target name (TARGET)
	int enemyCount;            // battle only: valid enemy records found
	GsMember enemies[GS_ENEMIES_MAX];
} GsState;

// Reads `size` (1, 2 or 4) bytes at a GBA bus address.
typedef uint32_t (*GsMemRead)(void* ctx, uint32_t addr, int size);

// The active game: UI footer name, and whether its RAM map is verified. When it is not, gsReadState returns an
// empty "no game info" state (only the BG mode) instead of reading Golden Sun addresses that would be wrong.
void gsSetGame(const char* name, bool mapped);
const char* gsGameName(void);
bool gsGameMapped(void);

void gsReadState(GsState* out, GsMemRead rd, void* ctx);
#endif
