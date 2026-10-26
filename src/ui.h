// Bottom-screen UI: rendering + touch handling. Input to the game uses the console's real controls, so there are
// no on-screen action buttons.
#ifndef UI_HEADER_H
#define UI_HEADER_H
#include <stdbool.h>
#include <stdint.h>

#include "gs_state.h"

#define UI_W 320
#define UI_H 240

// Virtual GBA buttons produced by the touch mirror (main.c maps them to the core's key mask).
enum { UK_A = 1, UK_B = 2, UK_L = 4, UK_R = 8, UK_UP = 16, UK_DOWN = 32, UK_LEFT = 64, UK_RIGHT = 128 };
// Game menu screens the info tabs can open (UiOutput.openMenu).
enum { MENU_NONE = -1, MENU_PSYNERGY = 0, MENU_DJINN = 1, MENU_ITEMS = 2, MENU_STATUS = 3, MENU_BAR = 4 };

enum { TOP_1X, TOP_FILL, TOP_MODES };  // 1:1 centred | stretched to the full 400x240 (nearest)

typedef struct {
	int tab;
	int itemChar;          // whose inventory the ITEMS tab shows
	bool showTouchOpts;    // the settings panel replaces the tab content (Android and desktop builds only)
	int optsPage;          // which page of the settings panel
	unsigned padOptsTick;  // bumped whenever an option changes so the panel is redrawn
	bool showHotkeys;      // the controls / hotkeys list replaces the tab content (Android and desktop builds only)
	bool showKeys;         // the touch keyboard for the Lost Age password screen
	bool roundSaved;       // set by main: a command round is recorded and can be repeated
	bool roundRepeating;   // set by main: the recorded round is being replayed
	bool showLoad;         // Djinn tab: the saved Djinn sets (presets and three slots)
	uint32_t loadSet[3][4];   // saved "Set" masks per element, slots 1..3 (persisted by main)
	bool loadValid[3];
	bool showChar;         // the character screen (class, EXP, elements) replaces the tab content
	int charIdx;           // party slot shown there
	bool itemPending;      // Items tab: a touch is down on an item; a short tap equips it on release, a long press shows its description
	int itemHold, itemWho, itemIdx, itemId;
	bool itemEquipped;
	int descId, descFrames;   // item description popup
	bool eqActive;         // set by main: the game showed the stat change for the highlighted gear; Equip / Cancel decide
	char eqLabel[40];
	int eqCur[3], eqNew[3];   // attack, defense, agility now / after
	bool pickActive;       // set by main: a game menu is waiting for a target, the party cards are shown to choose one
	char pickLabel[24];    // "Herb", "Cure": what is being used
	bool showSlots;        // the save-slot cards replace the tab content
	bool showLog;          // the message log replaces the tab content
	bool showBest, bestDrag;   // the bestiary (enemies met) replaces the tab content; a drag is scrolling it
	int bestScroll;        // pixels scrolled down from the top of the list
	bool djPending, djToggle;   // Djinn tab: a touch is down on a Djinn; djToggle = a short tap flips Set/Standby (Golden Sun), else a tap shows details
	int djHold, djE, djK, djRow;
	int djInfoFrames, djInfoE, djInfoK;   // Djinn detail popup
	bool logDrag;          // a drag is scrolling the log
	int logScroll;         // pixels scrolled down from the newest message
	int psyChar;           // whose Psynergy list the PSYNERGY tab shows
	int tabBeforeBattle;
	bool inBattle;
	int lastCmd;                       // battle: last command highlighted on the command bar
	int lastAct, actHero, actFrames;   // battle: which hero just acted (cleared after a while)
	int speedMul;          // fast-forward multiplier 2..4
	bool muteSlow;         // silence the sound while the game runs below full speed (it stutters otherwise)
	bool fastForward;
	int topScale;          // TOP_1X or TOP_FILL
	int stateSlot;         // 0..2: which state file Save/Load state use
	bool autoSave;         // save an automatic state on exit (and every few minutes) and load it at start
	bool hideMenus;        // GBA menus go to the bottom screen (touch buttons); the top screen holds the last clean field frame
	bool mirror;           // set by main: a GBA menu is open and hideMenus is on, so the bottom screen mirrors it
	unsigned touchKeys;    // UK_* buttons currently held on the touch overlay (for highlighting)
	int mapZoom;           // 1 = detail, 2 = overview
	int panX, panY;        // map pan in canvas pixels
	bool prevDown;
	bool dragging;
	int lastX, lastY;
	int toastFrames;
	char perf[72];         // timing summary from main (shown in the footer while the Menu tab is open)
	char toast[24];
} UiView;

typedef struct {
	bool saveState;
	bool loadState;
	bool clearMap;
	int openMenu;          // MENU_* to open in the game (tap an active tab, a character card or the footer), or MENU_NONE
	int openChar;          // with openMenu: party member whose page to show (0..3), or -1 for the screen's first page
	bool closeMenu;        // back out of every GBA menu
	unsigned keys;         // UK_* held through the touch overlay
	bool menuTap;          // a tap landed on the mirrored GBA frame at (tapX, tapY) in GBA pixels
	int tapX, tapY;
	int equipBest;         // Items tab: "Equip best" tapped for this party member (-1 = not tapped)
	bool repeatRound;      // battle: "Repeat" tapped on the round menu
	int loadApply;         // Djinn sets: 0 all Set, 1 all Standby, 2..4 saved slot 1..3; -1 = none
	int loadSave;          // slot 0..2 to overwrite with the current Set/Standby, -1 = none
	int eqAnswer;          // with the equip preview up: 1 = Equip, -2 = Cancel, 0 = nothing
	int pickTarget;        // with the picker up: party member tapped (0..3), -2 = cancelled, -1 = nothing
	int psyChar, psyIndex, psyId;   // Psynergy tab: tap a castable ability (member, index in the game's field list, ability id), else psyIndex -1
	int djinnElem, djinnRow, djinnBit;   // Djinn tab: tap a Djinn (element 0..3, row in the game's list, bit in the owned mask), else djinnElem -1
	int kbKey;             // touch keyboard: key tapped (see uiKeyTarget), or -1
	bool rewind;           // Menu tab: step back a few seconds
	bool swapScreens;      // Settings: "Swap screens" tapped (dual-screen devices only)
	int backup;            // Settings: 1 = export the saves, 2 = import them (Android)
	int equipChar, equipIndex;   // Items tab: tap on a bag item = equip it. Member 0..3 and the item's position in that member's list (game order), or -1
} UiOutput;

// Save-slot cards: each state file has a small screenshot and a timestamp next to it (<dir>state.thb, state2.thb, state3.thb).
#define SLOT_THUMB_W 80
#define SLOT_THUMB_H 53
void uiReloadSlots(const char* dir);            // re-reads the three .thb files
unsigned uiSlotsVersion(void);                   // changes after every reload
// 240x160 game frame -> thumbnail + time; `note` (e.g. "Lv56 Mars Lighthouse", may be NULL) is stored after the pixels and shown under the thumbnail
bool uiWriteSlotThumb(const char* dir, int slot, const uint16_t* frame, size_t stride, const char* note);
// Touch keyboard for the Lost Age password screen: keys 0..63 are the 4x16 character grid (row * 16 + column), then the side keys.
enum { UKEY_SPACE = 100, UKEY_RETURN = 101, UKEY_FINISH = 102, UKEY_DEL = 103 };
// Where the game's pointing hand sits (GBA pixels) when its cursor is on that key; false for an unknown key. UKEY_DEL has no position (B).
bool uiKeyTarget(int key, int* x, int* y);
void uiInit(UiView* v);
void uiUpdate(UiView* v, const GsState* st, bool down, int x, int y, UiOutput* out);
// True when the visible tab shows the live map (so map updates require a redraw).
bool uiShowsMap(const UiView* v);
// Set by the platform layers that have hotkeys (Android, desktop): adds a "Controls" button to the Menu tab. The 3DS build leaves it off.
extern bool uiShowHotkeys;
// Set by the platform while two physical displays are in use: adds a "Swap screens" button to the Menu tab.
extern bool uiShowSwap;
// Touch-button options (Android / desktop): set by the platform, edited in the Menu tab's "Touch buttons" panel (which shares the slot of "Swap screens"
// and is shown while only one display is in use).
extern bool uiShowTouchOpts;   // set by platforms that have the Settings panel (Android, desktop)
extern int uiMaxSpeed;         // highest fast-forward multiplier on this platform (3DS 4; Android / desktop 16)
extern int uiPadMode;      // 0 Auto (hidden while a controller is connected), 1 always shown, 2 always hidden
extern int uiPadSize;      // 0 small, 1 medium, 2 large
extern int uiPadOpacity;   // 0 low, 1 medium, 2 high
extern bool uiPadHaptic;   // a short vibration when an on-screen button is pressed
void uiToast(UiView* v, const char* msg);
// Renders into a 320x240 RGB565 framebuffer in 3DS layout: fb[x * 240 + (239 - y)].
void uiDraw(uint16_t* fb, const GsState* st, const UiView* v);
// Live GBA frame (RGB565, `stride` pixels per row) used by the menu mirror.
void uiSetGbaFrame(const uint16_t* video, size_t stride);
#endif
