#include <stdio.h>
#include <string.h>
#include <time.h>

#include "art.h"
#include "gfx.h"
#include "gs_areas.h"
#include "gs_ram.h"
#include "gs_items.h"
#include "gs_abilities.h"
#include "gs_djinn.h"
#include "bestiary.h"
#include "dialoglog.h"
#include "gs_field.h"
#include "gs_classes.h"
#include "gs_itemdesc.h"
#include "mapview.h"
#include "ui.h"

#define C_BG_TOP   RGB(6, 12, 32)
#define C_BG_BOT   RGB(12, 24, 56)
#define C_PANEL_T  RGB(28, 50, 108)
#define C_PANEL_B  RGB(12, 26, 66)
#define C_CARD_T   RGB(20, 38, 86)
#define C_CARD_B   RGB(8, 18, 48)
#define C_GOLD     RGB(236, 188, 68)
#define C_GOLD_DIM RGB(150, 112, 48)
#define C_TEXT     RGB(244, 246, 255)
#define C_DIM      RGB(128, 140, 176)
#define C_ACTIVE_T RGB(86, 74, 40)
#define C_ACTIVE_B RGB(40, 32, 18)
#define C_HP_A     RGB(90, 240, 120)
#define C_HP_B     RGB(30, 150, 70)
#define C_PP_A     RGB(100, 190, 255)
#define C_PP_B     RGB(30, 90, 200)

enum { TAB_PARTY, TAB_PSYNERGY, TAB_DJINN, TAB_MAP, TAB_ITEMS, TAB_MENU, TAB_COUNT, TAB_BATTLE = TAB_COUNT };
static const char* const tabNames[TAB_COUNT] = {"Party", "Psynergy", "Djinn", "Map", "Items", "Menu"};
static const int tabIcons[TAB_COUNT] = {ICON_PARTY, ICON_PSYNERGY, ICON_DJINN, ICON_MAP, ICON_ITEMS, ICON_MENU};
static const uint16_t tabAccent[TAB_COUNT] = {RGB(236, 188, 68), RGB(80, 170, 255), RGB(255, 110, 70), RGB(110, 210, 130), RGB(220, 170, 100), RGB(190, 200, 225)};

#define TAB_X 2
#define TAB_W 50
#define TAB_GAP 3
#define TAB_Y 3
#define TAB_H 44
#define CONTENT_Y 50
#define CONTENT_H 158
#define FOOT_Y 210
#define FOOT_H 28

// Map tab geometry.
#define MAP_VX 6
#define MAP_VY 54
#define MAP_VW 308
#define MAP_VH 126
#define MAP_BTN_Y 184
#define MAP_BTN_H 20
#define LOG_TOP (CONTENT_Y + 28)
#define LOG_H (CONTENT_H - 32)
#define LOG_LINE 11
static int logTotalHeight(void);
#define KB_X0 6
#define KB_Y0 (CONTENT_Y + 32)
#define KB_W 16
#define KB_H 24
#define KB_SIDE_X (KB_X0 + 16 * KB_W + 6)
#define SLOT_CARD_X(i) (6 + (i) * 104)
#define SLOT_CARD_Y (CONTENT_Y + 30)
#define SLOT_CARD_H 122

// Menu tab buttons: two columns of five.
enum { MB_FAST, MB_SPEED, MB_SLOTS, MB_AUTOSAVE, MB_MUTE, MB_SCALE, MB_HIDEMENU, MB_LOG, MB_BEST, MB_REWIND, MB_CLEARMAP, MB_COUNT };
static const char* const menuLabels[MB_COUNT] = {"Fast forward", "FF speed", "Save slots", "Autosave", "Mute if slow", "Top screen", "Menus below", "Message log", "Bestiary", "Rewind 5s", "Clear map"};
#define MENU_W 150
#define MENU_H 23
#define MENU_Y0 56
#define MENU_STEP 25
#define MENU_BX(i) (6 + ((i) / 6) * 158)
#define MENU_BY(i) (MENU_Y0 + ((i) % 6) * MENU_STEP)

static bool inRect(int px, int py, int x, int y, int w, int h) {
	return px >= x && px < x + w && py >= y && py < y + h;
}

// ---- GBA menu mirror: live game frame on the bottom screen + touch buttons ----
typedef struct { int x, y, w, h; unsigned key; const char* label; } MirrorBtn;
static const MirrorBtn mirrorBtns[] = {
	{244, 4, 72, 34, UK_L, "L"}, {244, 42, 72, 34, UK_R, "R"}, {244, 80, 72, 34, UK_A, "A"}, {244, 118, 72, 34, UK_B, "B"},
	{4, 168, 56, 40, UK_LEFT, "<"}, {64, 168, 56, 40, UK_UP, "^"}, {124, 168, 56, 40, UK_DOWN, "v"}, {184, 168, 56, 40, UK_RIGHT, ">"},
};
#define MIRROR_BTNS ((int) (sizeof(mirrorBtns) / sizeof(mirrorBtns[0])))
#define MIRROR_CLOSE_X 244
#define MIRROR_CLOSE_Y 168
#define MIRROR_CLOSE_W 72
#define MIRROR_CLOSE_H 40
static const uint16_t* gbaFrame;
static size_t gbaStride;

void uiSetGbaFrame(const uint16_t* video, size_t stride) {
	gbaFrame = video;
	gbaStride = stride;
}

static void mirrorDraw(const UiView* v) {
	gRect(0, 0, UI_W, FOOT_Y, RGB(6, 10, 26));
	if (gbaFrame) gBlit(0, 4, gbaFrame, (int) gbaStride, 240, 160);
	for (int i = 0; i < 240; ++i) { gPx(i, 3, C_GOLD_DIM); gPx(i, 164, C_GOLD_DIM); }
	for (int i = 0; i < MIRROR_BTNS; ++i) {
		const MirrorBtn* b = &mirrorBtns[i];
		bool on = (v->touchKeys & b->key) != 0;
		gPanel(b->x, b->y, b->w, b->h, on ? C_ACTIVE_T : C_PANEL_T, on ? C_ACTIVE_B : C_PANEL_B, on ? C_GOLD : C_GOLD_DIM);
		gTextCentered(b->x + b->w / 2, b->y + b->h / 2 - 7, b->label, 2, on ? C_GOLD : C_TEXT);
	}
	gPanel(MIRROR_CLOSE_X, MIRROR_CLOSE_Y, MIRROR_CLOSE_W, MIRROR_CLOSE_H, RGB(120, 30, 30), RGB(60, 10, 14), RGB(255, 120, 90));
	gTextCentered(MIRROR_CLOSE_X + MIRROR_CLOSE_W / 2, MIRROR_CLOSE_Y + 16, "CLOSE", 1, C_TEXT);
}

void uiInit(UiView* v) {
	memset(v, 0, sizeof(*v));
	v->topScale = TOP_FILL;
	v->mapZoom = 1;
	v->speedMul = 3;
	v->autoSave = true;   // resume where you quit (settings.bin overrides)
}

bool uiShowsMap(const UiView* v) {
	return v->tab == TAB_MAP;
}

void uiToast(UiView* v, const char* msg) {
	snprintf(v->toast, sizeof(v->toast), "%s", msg);
	v->toastFrames = 90;
}

static const char* areaName(const GsState* st) {
	return st->mode == GS_MODE_BATTLE ? "Battle" : gsAreaName(st->areaGroup);
}

#define BEST_ROW 15
#define BEST_TOP (LOG_TOP + 10)
#define BEST_VIEW_H (CONTENT_H - 42)

// Which Djinn is under a touch (element 0..3, bit k in the owned mask); only Djinn the party owns respond. The Golden Sun layout is two columns of
// four names per element panel, The Lost Age (18 per element) is three columns of six.
static bool djinnHit(const GsState* st, int x, int y, int* eOut, int* kOut) {
	bool big = gsRam()->djinnCount > 7;
	int count = gsRam()->djinnCount;
	for (int e = 0; e < GS_ELEMENTS; ++e) {
		int px = 4 + (e & 1) * 158, py = CONTENT_Y + (e >> 1) * 80;
		for (int k = 0; k < count; ++k) {
			int cx = big ? px + 4 + (k / 6) * 50 : px + (k >= 4 ? 72 : 0), yy = big ? py + 16 + (k % 6) * 10 : py + 18 + (k % 4) * 14;
			bool hit = big ? inRect(x, y, cx, yy - 1, 49, 10) : inRect(x, y, cx, yy - 1, 70, 14);
			if (hit && ((st->djinnOwned[e] >> k) & 1)) { *eOut = e; *kOut = k; return true; }
		}
	}
	return false;
}

void uiUpdate(UiView* v, const GsState* st, bool down, int x, int y, UiOutput* out) {
	memset(out, 0, sizeof(*out));
	out->openMenu = MENU_NONE;
	out->openChar = -1;
	out->equipChar = out->equipIndex = -1;
	out->pickTarget = -1;
	out->eqAnswer = 0;
	out->loadApply = out->loadSave = -1;
	out->repeatRound = false;
	out->equipBest = -1;
	out->kbKey = -1;
	out->psyIndex = -1;
	out->djinnElem = -1;
	if (st->mode == GS_MODE_BATTLE) {
		if (st->battlePhase == GS_PH_COMMAND && !st->battleTop && st->battleCmd >= 0) v->lastCmd = st->battleCmd;
		if (st->battleAct != v->lastAct) { v->lastAct = st->battleAct; v->actHero = st->battleAct; v->actFrames = 110; }
		if (st->battleSel >= 0) v->actFrames = 0;
		if (v->actFrames > 0 && --v->actFrames == 0) v->actHero = -1;
	}
	if (v->toastFrames > 0) --v->toastFrames;
	if (v->descFrames > 0) --v->descFrames;
	if (v->djInfoFrames > 0) --v->djInfoFrames;
	if (v->mirror) {   // a GBA menu is up on the bottom screen: only the touch buttons matter
		bool edgeNow = down && !v->prevDown;
		v->prevDown = down;
		v->touchKeys = 0;
		v->dragging = false;
		if (!down) return;
		for (int i = 0; i < MIRROR_BTNS; ++i)
			if (inRect(x, y, mirrorBtns[i].x, mirrorBtns[i].y, mirrorBtns[i].w, mirrorBtns[i].h)) v->touchKeys |= mirrorBtns[i].key;
		out->keys = v->touchKeys;
		if (edgeNow && x >= 0 && x < 240 && y >= 4 && y < 164) { out->menuTap = true; out->tapX = x; out->tapY = y - 4; }
		if (edgeNow && inRect(x, y, MIRROR_CLOSE_X, MIRROR_CLOSE_Y, MIRROR_CLOSE_W, MIRROR_CLOSE_H)) out->closeMenu = true;
		return;
	}
	v->touchKeys = 0;
	if (st->mode == GS_MODE_BATTLE && !v->inBattle) {
		v->inBattle = true;
		v->lastAct = st->battleAct;
		v->lastCmd = -1;
		v->actHero = -1;
		v->actFrames = 0;
		v->tabBeforeBattle = v->tab;
		v->tab = TAB_BATTLE;
	} else if (st->mode != GS_MODE_BATTLE && v->inBattle) {
		v->inBattle = false;
		v->tab = v->tabBeforeBattle;
	}

	bool edge = down && !v->prevDown;
	v->prevDown = down;
	if (edge && v->tab == TAB_BATTLE && v->roundSaved && !v->roundRepeating && st->battleTop && inRect(x, y, 96, CONTENT_Y, 62, 13)) { out->repeatRound = true; return; }

	// Map panning (drag inside the viewport).
	if (v->tab == TAB_MAP) {
		if (edge && inRect(x, y, MAP_VX, MAP_VY, MAP_VW, MAP_VH)) { v->dragging = true; v->lastX = x; v->lastY = y; }
		if (down && v->dragging) {
			int zs = v->mapZoom == 2 ? 2 : 1;
			v->panX -= (x - v->lastX) * zs;
			v->panY -= (y - v->lastY) * zs;
			v->lastX = x; v->lastY = y;
		}
		if (!down) v->dragging = false;
	} else {
		v->dragging = false;
	}
	if (v->eqActive) {   // equip preview: Equip / Cancel
		if (edge) {
			if (inRect(x, y, 40, CONTENT_Y + 118, 100, 30)) out->eqAnswer = 1;
			else if (inRect(x, y, 180, CONTENT_Y + 118, 100, 30)) out->eqAnswer = -2;
		}
		return;
	}
	if (v->pickActive) {   // the game is waiting for a target: only the party cards and Cancel matter
		if (edge) {
			if (inRect(x, y, 254, CONTENT_Y + 5, 54, 17)) out->pickTarget = -2;
			else if (y >= CONTENT_Y + 28 && y < CONTENT_Y + CONTENT_H) {
				int c = (x - 4) / 79;
				if (x >= 4 && c >= 0 && c < st->partySize) out->pickTarget = c;
			}
		}
		return;
	}
	if (v->showSlots) {   // save slots: Save/Load per card, Back leaves; only a tab tap passes through
		if (edge && inRect(x, y, 254, CONTENT_Y + 5, 54, 17)) { v->showSlots = false; return; }
		if (edge) {
			for (int i = 0; i < 3; ++i) {
				int cx = SLOT_CARD_X(i), cy = SLOT_CARD_Y;
				if (inRect(x, y, cx + 5, cy + 92, 42, 24)) { v->stateSlot = i; out->saveState = true; return; }
				if (inRect(x, y, cx + 53, cy + 92, 42, 24)) { v->stateSlot = i; out->loadState = true; return; }
				if (inRect(x, y, cx, cy, 100, 90)) { v->stateSlot = i; return; }
			}
		}
		if (!(edge && y >= TAB_Y && y < TAB_Y + TAB_H)) return;
	}
	if (v->showKeys) {   // the password keyboard: keys steer the game's cursor (main), Close leaves
		if (edge && inRect(x, y, 262, CONTENT_Y + 5, 46, 17)) { v->showKeys = false; return; }
		if (edge && inRect(x, y, 210, CONTENT_Y + 5, 48, 17)) { out->kbKey = UKEY_DEL; return; }
		if (edge) {
			for (int r = 0; r < 4; ++r) {
				for (int c = 0; c < 16; ++c)
					if (inRect(x, y, KB_X0 + c * KB_W, KB_Y0 + r * KB_H, KB_W - 1, KB_H - 1)) { out->kbKey = r * 16 + c; return; }
			}
			if (inRect(x, y, KB_SIDE_X, KB_Y0, 40, KB_H - 1)) { out->kbKey = UKEY_SPACE; return; }
			if (inRect(x, y, KB_SIDE_X, KB_Y0 + KB_H, 40, KB_H - 1)) { out->kbKey = UKEY_RETURN; return; }
			if (inRect(x, y, KB_SIDE_X, KB_Y0 + 3 * KB_H, 40, KB_H - 1)) { out->kbKey = UKEY_FINISH; return; }
		}
		if (!(edge && y >= TAB_Y && y < TAB_Y + TAB_H)) return;
	}
	if (v->showLoad) {   // saved Djinn sets: presets, Save / Apply per slot, Back leaves; only a tab tap passes through
		if (edge && inRect(x, y, 262, CONTENT_Y + 5, 46, 17)) { v->showLoad = false; return; }
		if (edge) {
			if (inRect(x, y, 12, CONTENT_Y + 30, 146, 24)) { out->loadApply = 0; v->showLoad = false; return; }
			if (inRect(x, y, 162, CONTENT_Y + 30, 146, 24)) { out->loadApply = 1; v->showLoad = false; return; }
			for (int i = 0; i < 3; ++i) {
				int ry = CONTENT_Y + 62 + i * 30;
				if (inRect(x, y, 196, ry, 54, 24)) { out->loadSave = i; return; }
				if (inRect(x, y, 254, ry, 54, 24) && v->loadValid[i]) { out->loadApply = 2 + i; v->showLoad = false; return; }
			}
		}
		if (!(edge && y >= TAB_Y && y < TAB_Y + TAB_H)) return;
	}
	if (v->showChar) {   // character screen: Back leaves, Game opens the game's own Status page; only a tab tap passes through
		if (edge && inRect(x, y, 262, CONTENT_Y + 5, 46, 17)) { v->showChar = false; return; }
		if (edge && inRect(x, y, 210, CONTENT_Y + 5, 48, 17)) {
			if (st->mode == GS_MODE_FIELD) { out->openMenu = MENU_STATUS; out->openChar = v->charIdx; }
			v->showChar = false;
			return;
		}
		if (edge && y >= CONTENT_Y && y < CONTENT_Y + CONTENT_H) {   // a tap on the left/right half steps to the previous/next member
			if (x < 20) v->charIdx = (v->charIdx + st->partySize - 1) % st->partySize;
			else if (x > 300) v->charIdx = (v->charIdx + 1) % st->partySize;
			return;
		}
		if (!(edge && y >= TAB_Y && y < TAB_Y + TAB_H)) return;
	}
	if (v->showLog) {   // the message log: drag to scroll, Back to leave; only a tab tap passes through
		int maxScroll = logTotalHeight() - LOG_H;
		if (maxScroll < 0) maxScroll = 0;
		if (edge && inRect(x, y, 254, CONTENT_Y + 5, 54, 17)) { v->showLog = false; v->logDrag = false; return; }
		if (edge && inRect(x, y, 4, LOG_TOP, 312, LOG_H)) { v->logDrag = true; v->lastY = y; }
		if (down && v->logDrag) {
			v->logScroll -= y - v->lastY;
			v->lastY = y;
			if (v->logScroll < 0) v->logScroll = 0;
			if (v->logScroll > maxScroll) v->logScroll = maxScroll;
		}
		if (!down) v->logDrag = false;
		if (!(edge && y >= TAB_Y && y < TAB_Y + TAB_H)) return;
	}
	if (v->showBest) {   // the bestiary: drag to scroll, Back to leave; only a tab tap passes through
		int maxScroll = bestCount() * BEST_ROW - BEST_VIEW_H;
		if (maxScroll < 0) maxScroll = 0;
		if (edge && inRect(x, y, 254, CONTENT_Y + 5, 54, 17)) { v->showBest = false; v->bestDrag = false; return; }
		if (edge && inRect(x, y, 4, BEST_TOP, 312, BEST_VIEW_H)) { v->bestDrag = true; v->lastY = y; }
		if (down && v->bestDrag) {
			v->bestScroll -= y - v->lastY;
			v->lastY = y;
			if (v->bestScroll < 0) v->bestScroll = 0;
			if (v->bestScroll > maxScroll) v->bestScroll = maxScroll;
		}
		if (!down) v->bestDrag = false;
		if (!(edge && y >= TAB_Y && y < TAB_Y + TAB_H)) return;
	}
	if (v->tab != TAB_ITEMS) v->itemPending = false;
	if (v->itemPending) {   // a touch started on an item row: long press = description, short tap on release = equip / use
		if (down) {
			if (++v->itemHold == 24) { v->descId = v->itemId; v->descFrames = 220; v->itemPending = false; }
		} else {
			if (v->itemHold < 24 && !v->itemEquipped) { out->equipChar = v->itemWho; out->equipIndex = v->itemIdx; }
			v->itemPending = false;
		}
	}
	if (v->tab != TAB_DJINN) v->djPending = false;
	if (v->djPending) {   // a touch started on a Djinn: long press = details; on release a short tap flips Set / Standby (Golden Sun) or also shows details (Lost Age)
		if (down) {
			if (++v->djHold == 24) { v->djInfoE = v->djE; v->djInfoK = v->djK; v->djInfoFrames = 260; v->djPending = false; }
		} else {
			if (v->djHold < 24) {
				if (v->djToggle) { out->djinnElem = v->djE; out->djinnRow = v->djRow; out->djinnBit = v->djK; }
				else { v->djInfoE = v->djE; v->djInfoK = v->djK; v->djInfoFrames = 260; }
			}
			v->djPending = false;
		}
	}
	if (!edge) return;

	if (y >= TAB_Y && y < TAB_Y + TAB_H) {
		int t = (x - TAB_X) / (TAB_W + TAB_GAP);
		if (t >= 0 && t < TAB_COUNT) {
			// With menus on the bottom screen, tapping the tab you are already on opens the matching game screen.
			if (v->hideMenus && t == v->tab && st->mode == GS_MODE_FIELD) {
				static const int tabMenu[TAB_COUNT] = {MENU_STATUS, MENU_PSYNERGY, MENU_DJINN, MENU_NONE, MENU_ITEMS, MENU_NONE};
				out->openMenu = tabMenu[t];
			}
			v->tab = t;
			v->showLog = false;
			v->showBest = false;
			v->showSlots = false;
			v->showChar = false;
			v->showKeys = false;
			v->showLoad = false;
		}
		return;
	}
	if (!(st->partySize > 0 && st->areaGroup != 0) && v->tab != TAB_MENU && gsRam()->djinnCount > 7 && inRect(x, y, 90, CONTENT_Y + CONTENT_H - 36, 140, 28)) {
		v->showKeys = true;   // The Lost Age password screen: the touch keyboard
		return;
	}
	if (v->hideMenus && st->mode == GS_MODE_FIELD && inRect(x, y, 4, FOOT_Y, 204, FOOT_H)) {
		out->openMenu = MENU_BAR;   // footer: the quick menu bar
		return;
	}
	if (v->tab == TAB_MAP && y >= MAP_BTN_Y && y < MAP_BTN_Y + MAP_BTN_H + 3) {
		if (x < 160) { v->panX = v->panY = 0; }
		else { v->mapZoom = v->mapZoom == 1 ? 2 : 1; v->panX = v->panY = 0; }
		return;
	}
	if (v->tab == TAB_PARTY && y >= CONTENT_Y && y < CONTENT_Y + CONTENT_H) {   // a party card: the character screen
		int c = (x - 4) / 79;
		if (x >= 4 && c >= 0 && c < st->partySize) { v->showChar = true; v->charIdx = c; }
		return;
	}
	if (v->tab == TAB_PSYNERGY && st->mode == GS_MODE_FIELD && y >= CONTENT_Y + 42 && y < CONTENT_Y + 42 + 9 * 11 && x >= 8 && x < 312) {
		int who = v->psyChar < st->partySize ? v->psyChar : 0;
		int want = ((x - 8) / 102) * 9 + (y - (CONTENT_Y + 42)) / 11, usableBefore = 0;
		const GsMember* m = &st->party[who];
		for (int k = 0; k < m->abilityCount && k < 27; ++k) {
			unsigned id = m->abilities[k] & 0xFF;
			bool usable = gsAbilityFieldUsable(id);
			if (k == want) {
				if (usable) { out->psyChar = who; out->psyIndex = usableBefore; out->psyId = (int) id; }
				return;
			}
			if (usable) ++usableBefore;
		}
		return;
	}
	if (v->tab == TAB_PSYNERGY && y >= CONTENT_Y + 4 && y < CONTENT_Y + 24) {
		int c = (x - 8) / 76;
		if (x >= 8 && c >= 0 && c < st->partySize) {
			if (v->hideMenus && st->mode == GS_MODE_FIELD && c == v->psyChar) { out->openMenu = MENU_PSYNERGY; out->openChar = c; }   // tap the active name again: open its list in the game
			v->psyChar = c;
		}
		return;
	}
	if (v->tab == TAB_DJINN && st->mode == GS_MODE_FIELD && inRect(x, y, 108, CONTENT_Y + 2, 46, 14)) { v->showLoad = true; return; }
	if (v->tab == TAB_DJINN) {   // touch a Djinn: a short tap flips Set / Standby (in the field), a long press shows its details
		int e, k;
		if (djinnHit(st, x, y, &e, &k)) {
			int row = 0;
			for (int j = 0; j < k; ++j) if ((st->djinnOwned[e] >> j) & 1) ++row;   // the game's list is compact: owned Djinn only
			v->djPending = true; v->djHold = 0; v->djE = e; v->djK = k; v->djRow = row;
			v->djToggle = st->mode == GS_MODE_FIELD;
			return;
		}
	}
	if (v->tab == TAB_ITEMS && st->mode == GS_MODE_FIELD && inRect(x, y, 226, CONTENT_Y + CONTENT_H - 31, 82, 15)) { out->equipBest = v->itemChar < st->partySize ? v->itemChar : 0; return; }
	if (v->tab == TAB_ITEMS && st->mode == GS_MODE_FIELD && y >= CONTENT_Y + 28 && y < CONTENT_Y + 28 + 8 * 14 && x >= 8 && x < 312) {
		int who = v->itemChar < st->partySize ? v->itemChar : 0;
		int want = (x >= 160 ? 8 : 0) + (y - (CONTENT_Y + 28)) / 14, shown = 0;
		for (int k = 0; k < 15; ++k) {   // the visible row -> the item's position in the game's list
			const GsItem* it = &st->party[who].items[k];
			if (!it->id) continue;
			if (shown++ == want) {
				v->itemPending = true; v->itemHold = 0; v->itemWho = who; v->itemIdx = shown - 1; v->itemId = (int) it->id; v->itemEquipped = it->equipped;
				return;
			}
		}
		return;
	}
	if (v->tab == TAB_ITEMS && y >= CONTENT_Y + 4 && y < CONTENT_Y + 24) {
		int c = (x - 8) / 76;
		if (x >= 8 && c >= 0 && c < st->partySize) {
			if (v->hideMenus && st->mode == GS_MODE_FIELD && c == v->itemChar) { out->openMenu = MENU_ITEMS; out->openChar = c; }   // tap the active name again: open it in the game
			v->itemChar = c;
		}
		return;
	}
	if (v->tab == TAB_MENU) {
		for (int i = 0; i < MB_COUNT; ++i) {
			if (!inRect(x, y, MENU_BX(i), MENU_BY(i), MENU_W, MENU_H)) continue;
			switch (i) {
			case MB_FAST: v->fastForward = !v->fastForward; break;
			case MB_SPEED: v->speedMul = v->speedMul >= 4 ? 2 : v->speedMul + 1; break;
			case MB_MUTE: v->muteSlow = !v->muteSlow; break;
			case MB_SLOTS: v->showSlots = true; break;
			case MB_AUTOSAVE: v->autoSave = !v->autoSave; break;
			case MB_SCALE: v->topScale = (v->topScale + 1) % TOP_MODES; break;
			case MB_HIDEMENU: v->hideMenus = !v->hideMenus; break;
			case MB_LOG: v->showLog = true; v->logScroll = 0; break;
			case MB_BEST: v->showBest = true; v->bestScroll = 0; break;
			case MB_REWIND: out->rewind = true; break;
			case MB_CLEARMAP: out->clearMap = true; break;
			}
		}
	}
}


// ---- message log ----

// Greedy word wrap of `text` to `width` pixels; calls emit(line) for each line when draw is true. Returns the number of lines.
static int wrapLines(const char* text, int width, int scale, int y0, int x, uint16_t color, int clipTop, int clipBottom, bool draw) {
	char line[160];
	int n = 0, lines = 0;
	line[0] = 0;
	const char* p = text;
	while (*p) {
		const char* e = p;
		while (*e && *e != ' ') ++e;
		char word[96];
		int wl = (int) (e - p) < 95 ? (int) (e - p) : 95;
		memcpy(word, p, (size_t) wl);
		word[wl] = 0;
		char trial[200];
		snprintf(trial, sizeof(trial), n ? "%s %s" : "%s%s", line, word);
		if (n && gTextW(trial, scale) > width) {
			int y = y0 + lines * LOG_LINE;
			if (draw && y >= clipTop && y + LOG_LINE <= clipBottom) gText(x, y, line, scale, color);
			++lines;
			snprintf(line, sizeof(line), "%s", word);
		} else {
			snprintf(line, sizeof(line), "%s", trial);
		}
		n = 1;
		p = *e ? e + 1 : e;
	}
	if (n) {
		int y = y0 + lines * LOG_LINE;
		if (draw && y >= clipTop && y + LOG_LINE <= clipBottom) gText(x, y, line, scale, color);
		++lines;
	}
	return lines;
}

static int logTotalHeight(void) {
	int h = 0;
	for (int i = dlogCount() - 1; i >= 0; --i) h += wrapLines(dlogText(i), 280, 1, 0, 0, 0, 0, 0, false) * LOG_LINE + 8;
	return h;
}

static void logView(const UiView* v) {
	gPanel(4, CONTENT_Y, 312, CONTENT_H, C_CARD_T, C_CARD_B, C_GOLD);
	gPill(12, CONTENT_Y + 5, 150, 17, C_PANEL_T, C_PANEL_B, C_GOLD);
	gTextCentered(87, CONTENT_Y + 9, "Message log", 1, C_GOLD);
	gPanel(254, CONTENT_Y + 5, 54, 17, C_PANEL_T, C_PANEL_B, C_GOLD);
	gTextCentered(281, CONTENT_Y + 9, "Back", 1, C_TEXT);
	int top = LOG_TOP, bottom = LOG_TOP + LOG_H;
	if (!dlogCount()) {
		gTextCentered(160, top + 40, "No messages yet.", 1, C_DIM);
		gTextCentered(160, top + 56, "Lines from the game's message boxes appear here.", 0, C_DIM);
		return;
	}
	int y = top - v->logScroll;
	for (int i = dlogCount() - 1; i >= 0; --i) {   // newest first
		int lines = wrapLines(dlogText(i), 280, 1, y, 14, i == dlogCount() - 1 ? C_TEXT : RGB(190, 200, 230), top, bottom, true);
		int h = lines * LOG_LINE;
		if (y + h + 3 >= top && y + h + 3 < bottom) gRect(14, y + h + 3, 284, 1, C_GOLD_DIM);
		y += h + 8;
		if (y > bottom) break;
	}
}


// ---- save slots ----
typedef struct {
	bool hasState;   // the state file exists (older saves have no thumbnail)
	bool exists;
	char when[24];
	uint16_t px[SLOT_THUMB_W * SLOT_THUMB_H];
	char note[40];   // where the state was saved (older thumbnails have none)
} SlotInfo;
static SlotInfo slotInfo[3];
static unsigned slotsVersion;

static void thumbPath(char* out, size_t n, const char* dir, int slot) {
	if (slot <= 0) snprintf(out, n, "%sstate.thb", dir);
	else snprintf(out, n, "%sstate%d.thb", dir, slot + 1);
}

unsigned uiSlotsVersion(void) { return slotsVersion; }

void uiReloadSlots(const char* dir) {
	for (int i = 0; i < 3; ++i) {
		slotInfo[i].exists = false;
		slotInfo[i].note[0] = 0;
		char path[160];
		if (i <= 0) snprintf(path, sizeof(path), "%sstate.ss", dir);
		else snprintf(path, sizeof(path), "%sstate%d.ss", dir, i + 1);
		FILE* sf = fopen(path, "rb");
		slotInfo[i].hasState = sf != NULL;
		if (sf) fclose(sf);
		thumbPath(path, sizeof(path), dir, i);
		FILE* f = fopen(path, "rb");
		if (!f) continue;
		uint32_t hdr[4];
		if (fread(hdr, sizeof(hdr), 1, f) == 1 && hdr[0] == 0x31545347u && hdr[2] == SLOT_THUMB_W && hdr[3] == SLOT_THUMB_H &&
			fread(slotInfo[i].px, sizeof(slotInfo[i].px), 1, f) == 1) {
			size_t nn = fread(slotInfo[i].note, 1, sizeof(slotInfo[i].note) - 1, f);   // optional note after the pixels
			slotInfo[i].note[nn] = 0;
			time_t t = (time_t) hdr[1];
			struct tm* tmv = localtime(&t);
			if (tmv) strftime(slotInfo[i].when, sizeof(slotInfo[i].when), "%b %d  %H:%M", tmv);
			else snprintf(slotInfo[i].when, sizeof(slotInfo[i].when), "saved");
			slotInfo[i].exists = true;
		}
		fclose(f);
	}
	++slotsVersion;
}

bool uiWriteSlotThumb(const char* dir, int slot, const uint16_t* frame, size_t stride, const char* note) {
	char path[160];
	thumbPath(path, sizeof(path), dir, slot);
	FILE* f = fopen(path, "wb");
	if (!f) return false;
	uint32_t hdr[4] = {0x31545347u, (uint32_t) time(NULL), SLOT_THUMB_W, SLOT_THUMB_H};
	fwrite(hdr, sizeof(hdr), 1, f);
	static uint16_t px[SLOT_THUMB_W * SLOT_THUMB_H];
	for (int y = 0; y < SLOT_THUMB_H; ++y)
		for (int x = 0; x < SLOT_THUMB_W; ++x) px[y * SLOT_THUMB_W + x] = frame[(y * 3 + 1) * stride + x * 3 + 1];
	fwrite(px, sizeof(px), 1, f);
	if (note && note[0]) fwrite(note, 1, strnlen(note, 39), f);   // plain text, no terminator: the reader takes whatever follows the pixels
	fclose(f);
	return true;
}

static void slotsView(const UiView* v) {
	gPanel(4, CONTENT_Y, 312, CONTENT_H, C_CARD_T, C_CARD_B, C_GOLD);
	gPill(12, CONTENT_Y + 5, 150, 17, C_PANEL_T, C_PANEL_B, C_GOLD);
	gTextCentered(87, CONTENT_Y + 9, "Save slots", 1, C_GOLD);
	gPanel(254, CONTENT_Y + 5, 54, 17, C_PANEL_T, C_PANEL_B, C_GOLD);
	gTextCentered(281, CONTENT_Y + 9, "Back", 1, C_TEXT);
	for (int i = 0; i < 3; ++i) {
		int x = SLOT_CARD_X(i), y = SLOT_CARD_Y;
		bool cur = i == v->stateSlot;
		gPanel(x, y, 100, SLOT_CARD_H, cur ? C_ACTIVE_T : C_PANEL_T, cur ? C_ACTIVE_B : C_PANEL_B, cur ? C_GOLD : C_GOLD_DIM);
		char title[16];
		snprintf(title, sizeof(title), "Slot %d", i + 1);
		gTextCentered(x + 50, y + 5, title, 1, C_GOLD);
		int tx = x + 10, ty = y + 19;
		if (slotInfo[i].exists) {
			for (int py = 0; py < SLOT_THUMB_H; ++py)
				for (int px = 0; px < SLOT_THUMB_W; ++px) gPx(tx + px, ty + py, slotInfo[i].px[py * SLOT_THUMB_W + px]);
			char when[48];
			const char* area = slotInfo[i].note;
			char lv[12] = "";
			if (!strncmp(area, "Lv", 2)) {   // note = "Lv56 Area name": the level goes beside the date, the area gets its own line
				size_t n = strcspn(area, " ");
				snprintf(lv, sizeof(lv), "%.*s", (int) (n < sizeof(lv) - 1 ? n : sizeof(lv) - 1), area);
				area += n; if (*area == ' ') ++area;
			}
			snprintf(when, sizeof(when), "%s%s%s", slotInfo[i].when, lv[0] ? "  " : "", lv);
			gTextCentered(x + 50, ty + SLOT_THUMB_H + 2, when, 0, C_TEXT);
			if (area[0]) {
				char nt[40];
				snprintf(nt, sizeof(nt), "%s", area);
				for (size_t n = strlen(nt); n > 3 && gTextW(nt, 0) > 94; --n) nt[n - 1] = 0;
				gTextCentered(x + 50, ty + SLOT_THUMB_H + 12, nt, 0, C_GOLD);
			}
		} else {
			gRect(tx, ty, SLOT_THUMB_W, SLOT_THUMB_H, RGB(8, 14, 34));
			gTextCentered(x + 50, ty + 22, slotInfo[i].hasState ? "Saved" : "Empty", 1, C_DIM);
			if (slotInfo[i].hasState) gTextCentered(x + 50, ty + SLOT_THUMB_H + 4, "no preview", 0, C_DIM);
		}
		gPanel(x + 5, y + 92, 42, 24, C_PANEL_T, C_PANEL_B, C_GOLD);
		gTextCentered(x + 26, y + 99, "Save", 1, C_TEXT);
		gPanel(x + 53, y + 92, 42, 24, C_PANEL_T, C_PANEL_B, slotInfo[i].hasState ? C_GOLD : C_GOLD_DIM);
		gTextCentered(x + 74, y + 99, "Load", 1, slotInfo[i].hasState ? C_TEXT : C_DIM);
	}
}


// ---- touch keyboard (Lost Age password screen) ----
static const char* const kbRows[4] = {"ABCDEFGHJKLMNPQR", "STUVWXYZ23456789", "abcdefghijkmnpqr", "stuvwxyz!?#&$%+="};
static const int kbX[16] = {1, 7, 15, 23, 31, 47, 55, 63, 71, 79, 95, 103, 111, 119, 127, 143};   // hand x on each column
static const int kbY[4] = {115, 122, 130, 138};                                                    // hand y on each row

bool uiKeyTarget(int key, int* x, int* y) {
	if (key >= 0 && key < 64) { *x = kbX[key % 16]; *y = kbY[key / 16]; return true; }
	if (key == UKEY_SPACE) { *x = 159; *y = kbY[0]; return true; }
	if (key == UKEY_RETURN) { *x = 159; *y = kbY[1]; return true; }
	if (key == UKEY_FINISH) { *x = 159; *y = kbY[3]; return true; }
	return false;
}


static void keysView(void) {
	gPanel(4, CONTENT_Y, 312, CONTENT_H, C_CARD_T, C_CARD_B, C_GOLD);
	gPill(12, CONTENT_Y + 5, 190, 17, C_PANEL_T, C_PANEL_B, C_GOLD);
	gTextCentered(107, CONTENT_Y + 9, "Password keyboard", 1, C_GOLD);
	gPanel(210, CONTENT_Y + 5, 48, 17, C_PANEL_T, C_PANEL_B, C_GOLD_DIM);
	gTextCentered(234, CONTENT_Y + 9, "Del", 1, C_TEXT);
	gPanel(262, CONTENT_Y + 5, 46, 17, C_PANEL_T, C_PANEL_B, C_GOLD);
	gTextCentered(285, CONTENT_Y + 9, "Close", 1, C_TEXT);
	for (int r = 0; r < 4; ++r) {
		for (int c = 0; c < 16; ++c) {
			char ch[2] = {kbRows[r][c], 0};
			int x = KB_X0 + c * KB_W, y = KB_Y0 + r * KB_H;
			gPanel(x, y, KB_W - 1, KB_H - 1, C_PANEL_T, C_PANEL_B, (c / 5) % 2 ? C_GOLD_DIM : RGB(90, 120, 190));
			gTextCentered(x + (KB_W - 1) / 2, y + 8, ch, 1, C_TEXT);
		}
	}
	static const char* const side[4] = {"Space", "Return", "", "Finish"};
	for (int r = 0; r < 4; ++r) {
		if (!side[r][0]) continue;
		gPanel(KB_SIDE_X, KB_Y0 + r * KB_H, 40, KB_H - 1, C_ACTIVE_T, C_ACTIVE_B, C_GOLD);
		gTextCentered(KB_SIDE_X + 20, KB_Y0 + r * KB_H + 8, side[r], 0, C_GOLD);
	}
	gTextCentered(160, KB_Y0 + 4 * KB_H + 5, "Open the password screen in the game, then tap keys.", 0, C_DIM);
	gTextCentered(160, KB_Y0 + 4 * KB_H + 15, "Del = B (erase)   The letters follow the game's own grid.", 0, C_DIM);
}

// ---- drawing helpers ----

static void gauge(int x, int y, int w, int cur, int max, uint16_t a, uint16_t b) {
	gRect(x, y, w, 7, RGB(4, 8, 20));
	gRect(x + 1, y + 1, w - 2, 5, RGB(18, 28, 56));
	if (max > 0) {
		int fill = cur * (w - 2) / max;
		if (fill > w - 2) fill = w - 2;
		if (fill > 0) {
			gVGrad(x + 1, y + 1, fill, 5, a, b);
			gRect(x + 1, y + 1, fill, 1, gMix(a, RGB(255, 255, 255), 90));
		}
	}
}

static void tabButton(int i, const UiView* v) {
	int x = TAB_X + i * (TAB_W + TAB_GAP);
	bool on = v->tab == i;
	gPanel(x, TAB_Y, TAB_W, TAB_H, on ? C_ACTIVE_T : C_PANEL_T, on ? C_ACTIVE_B : C_PANEL_B, on ? C_GOLD : C_GOLD_DIM);
	if (on) gRect(x + 3, TAB_Y + TAB_H - 3, TAB_W - 6, 1, tabAccent[i]);
	const Sprite* ic = artIcon(tabIcons[i]);
	if (on) gSprite(x + (TAB_W - ic->w) / 2, TAB_Y + 1, ic);
	else gSpriteTint(x + (TAB_W - ic->w) / 2, TAB_Y + 1, ic, RGB(60, 80, 130), 90);
	gTextCentered(x + TAB_W / 2, TAB_Y + 33, tabNames[i], 1, on ? C_TEXT : C_DIM);
}

// HP gauge colours: green above half, amber to a quarter, red below.
static void hpColors(int cur, int max, uint16_t* a, uint16_t* b) {
	if (max <= 0 || cur * 2 > max) { *a = C_HP_A; *b = C_HP_B; }
	else if (cur * 4 > max) { *a = RGB(255, 220, 90); *b = RGB(190, 130, 20); }
	else { *a = RGB(255, 110, 100); *b = RGB(170, 30, 40); }
}

// Small status tag centred on x: red CURSE, grey DOWN (0 HP).
static void statusBadge(int x, int y, const GsMember* m) {
	const char* t = m->hp == 0 && m->maxHp > 0 ? "DOWN" : m->cursed ? "CURSE" : NULL;
	if (!t) return;
	x -= (gTextW(t, 0) + 6) / 2;   // x is the centre of the tag
	int w = gTextW(t, 0) + 6;
	bool down = t[0] == 'D';
	gPanel(x, y, w, 11, down ? RGB(90, 96, 112) : RGB(150, 30, 40), down ? RGB(50, 54, 66) : RGB(80, 10, 20), down ? RGB(170, 176, 190) : RGB(255, 120, 110));
	gText(x + 3, y + 2, t, 0, C_TEXT);
}

static void gaugeText(int x, int y, int w, int h, int cur, int max, uint16_t a, uint16_t b, const char* text);

static void partyCard(int x, int y, int w, int h, const GsMember* m, bool stats) {
	gPanel(x, y, w, h, C_CARD_T, C_CARD_B, m->present ? C_GOLD : C_GOLD_DIM);
	if (!m->present) {
		gTextCentered(x + w / 2, y + h / 2 - 3, "Empty", 1, C_DIM);
		return;
	}
	const Sprite* p = artPortraitRec(m->rec, m->name);
	int px = x + (w - PORTRAIT_W) / 2, py = y + 3;
	bool downed = m->hp == 0 && m->maxHp > 0;
	if (downed) gSpriteTint(px, py, p, RGB(70, 76, 96), 170);
	else gSprite(px, py, p);
	statusBadge(px + PORTRAIT_W / 2, py + PORTRAIT_H - 12, m);
	int ny = py + PORTRAIT_H + 2;
	gRect(px, ny, PORTRAIT_W, 14, RGB(5, 11, 28));
	char line[24];
	gTextShadow(px + 3, ny + 3, m->name[0] ? m->name : "?", 1, RGB(255, 238, 190));
	snprintf(line, sizeof(line), "Lv%u", (unsigned) m->level);
	gTextRight(px + PORTRAIT_W - 3, ny + 3, line, 1, RGB(150, 150, 255));
	uint16_t ha, hb;
	hpColors(m->hp, m->maxHp, &ha, &hb);
	snprintf(line, sizeof(line), "%u/%u", (unsigned) m->hp, (unsigned) m->maxHp);
	gaugeText(px, ny + 16, PORTRAIT_W, 13, m->hp, m->maxHp, ha, hb, line);
	snprintf(line, sizeof(line), "%u/%u", (unsigned) m->pp, (unsigned) m->maxPp);
	gaugeText(px, ny + 31, PORTRAIT_W, 13, m->pp, m->maxPp, C_PP_A, C_PP_B, line);
	if (stats) {
		int yy = ny + 52;
		gRect(px, yy - 4, PORTRAIT_W, 1, C_GOLD_DIM);
		static const char* const lab[3] = {"ATK", "DEF", "AGI"};
		unsigned val[3] = {m->atk, m->def, m->agi};
		for (int i = 0; i < 3; ++i) {
			gText(px + 3, yy + i * 14, lab[i], 1, C_DIM);
			snprintf(line, sizeof(line), "%u", val[i]);
			gTextRight(px + PORTRAIT_W - 3, yy + i * 14, line, 1, C_TEXT);
		}
	}
}
static void partyCards(const GsState* st, int y, int h, bool stats) {
	for (int i = 0; i < GS_PARTY_MAX; ++i) partyCard(4 + i * 79, y, 75, h, &st->party[i], stats);
}

// Gauge with the value drawn inside it.
static void gaugeText(int x, int y, int w, int h, int cur, int max, uint16_t a, uint16_t b, const char* text) {
	gRect(x, y, w, h, RGB(4, 8, 20));
	gRect(x + 1, y + 1, w - 2, h - 2, RGB(18, 28, 56));
	if (max > 0) {
		int fill = cur * (w - 2) / max;
		if (fill > w - 2) fill = w - 2;
		if (fill > 0) {
			gVGrad(x + 1, y + 1, fill, h - 2, a, b);
			gRect(x + 1, y + 1, fill, 1, gMix(a, RGB(255, 255, 255), 90));
		}
	}
	gTextCentered(x + w / 2, y + (h - 7) / 2, text, 0, C_TEXT);
}

static void enemyList(const GsState* st, int y, int rowH) {
	int n = st->enemyCount ? st->enemyCount : 1;
	gPanel(4, y, 312, 6 + rowH * n, C_CARD_T, C_CARD_B, RGB(255, 120, 90));
	if (!st->enemyCount) { gText(12, y + 5, "Enemy data unavailable", 1, C_DIM); return; }
	for (int i = 0; i < st->enemyCount; ++i) {
		const GsMember* e = &st->enemies[i];
		int yy = y + 3 + i * rowH;
		bool dead = e->hp == 0;
		bool targeted = st->battlePhase == GS_PH_TARGET && !st->battleTargetAlly && st->battleTarget == i;
		char line[32];
		if (targeted) {
			gRect(6, yy, 308, rowH, RGB(96, 72, 24));
			gRect(6, yy, 308, 1, RGB(255, 220, 110)); gRect(6, yy + rowH - 1, 308, 1, RGB(255, 220, 110));
			for (int k = 0; k < 4; ++k) gRect(9 + k, yy + rowH / 2 - 3 + k, 1, 7 - 2 * k, RGB(255, 235, 140));   // pointer
		} else if (i & 1) {
			gRect(7, yy, 306, rowH, RGB(14, 28, 62));
		}
		int ty = yy + (rowH - 7) / 2;
		gTextShadow(16, ty, e->name, 1, dead ? C_DIM : targeted ? RGB(255, 240, 170) : RGB(255, 170, 150));
		snprintf(line, sizeof(line), "Lv %u", (unsigned) e->level);
		gText(108, ty, line, 1, C_DIM);
		uint16_t ea, eb;
		hpColors(e->hp, e->maxHp, &ea, &eb);
		snprintf(line, sizeof(line), "%u/%u", (unsigned) e->hp, (unsigned) e->maxHp);
		int gh = rowH < 13 ? rowH - 1 : 11;
		gaugeText(150, yy + (rowH - gh) / 2, 70, gh, e->hp, e->maxHp, ea, eb, dead ? "Defeated" : line);
		char stats[24];
		snprintf(stats, sizeof(stats), "A%u D%u S%u", (unsigned) e->atk, (unsigned) e->def, (unsigned) e->agi);
		gText(226, ty, stats, 0, dead ? C_DIM : RGB(255, 190, 170));
	}
}

// Party card for battle: three sizes depending on the room (big portrait, face only, or no portrait).
static void battleCard(int x, int y, int w, int h, const GsMember* m, int mode) {  // mode: 0 none, 1 choosing, 2 acting, 3 target
	uint16_t edge = mode == 3 ? RGB(140, 255, 170) : mode == 1 ? RGB(255, 240, 150) : mode == 2 ? RGB(150, 235, 255) : m->present ? C_GOLD_DIM : RGB(90, 70, 40);
	if (mode) { gRect(x - 2, y - 2, w + 4, h + 4, gMix(edge, RGB(8, 14, 34), 150)); gRect(x - 1, y - 1, w + 2, h + 2, edge); }
	gPanel(x, y, w, h, mode ? C_ACTIVE_T : C_CARD_T, mode ? C_ACTIVE_B : C_CARD_B, edge);
	if (!m->present) { gTextCentered(x + w / 2, y + h / 2 - 3, "Empty", 1, C_DIM); return; }
	bool downed = m->hp == 0 && m->maxHp > 0;
	int variant = h >= 89 ? 2 : h >= 78 ? 1 : 0;      // full portrait, face only, or none
	char line[24];
	uint16_t ha, hb;
	hpColors(m->hp, m->maxHp, &ha, &hb);
	int px = x + (w - PORTRAIT_W) / 2, ny;
	if (variant) {
		int fh = variant == 2 ? PORTRAIT_H : 34;
		gSpriteClip(px, y + 3, artPortraitRec(m->rec, m->name), 0, 0, PORTRAIT_W, fh);
		if (downed) gRectA(px, y + 3, PORTRAIT_W, fh, RGB(40, 44, 60), 170);
		statusBadge(x + w / 2, y + 3 + fh - 12, m);
		ny = y + 3 + fh + 2;
	} else {
		ny = y + 5;
	}
	if (mode) {
		const char* t = mode == 3 ? "TARGET" : mode == 1 ? "TURN" : "ACT";
		int tw = gTextW(t, 0) + 8;
		gPill(x + 5, y + 5, tw, 11, mode == 3 ? RGB(40, 150, 80) : mode == 1 ? RGB(200, 160, 40) : RGB(40, 130, 160), mode == 3 ? RGB(10, 80, 40) : mode == 1 ? RGB(120, 90, 10) : RGB(10, 60, 90), edge);
		gText(x + 9, y + 8, t, 0, C_TEXT);
		if (!variant) ny = y + 18;
	}
	gRect(px, ny, PORTRAIT_W, 13, RGB(5, 11, 28));
	gTextShadow(px + 3, ny + 2, m->name, 1, downed ? C_DIM : mode ? C_TEXT : RGB(255, 238, 190));
	snprintf(line, sizeof(line), "Lv%u", (unsigned) m->level);
	gTextRight(px + PORTRAIT_W - 3, ny + 2, line, 1, RGB(150, 150, 255));
	snprintf(line, sizeof(line), "%u/%u", (unsigned) m->hp, (unsigned) m->maxHp);
	gaugeText(px, ny + 14, PORTRAIT_W, 11, m->hp, m->maxHp, ha, hb, line);
	snprintf(line, sizeof(line), "%u/%u", (unsigned) m->pp, (unsigned) m->maxPp);
	gaugeText(px, ny + 26, PORTRAIT_W, 11, m->pp, m->maxPp, C_PP_A, C_PP_B, line);
}

static const char* const kCmdNames[6] = {"Attack", "Psynergy", "Djinn", "Summon", "Item", "Defend"};

// Command strip: the six battle commands with the active one lit, plus a line about what is selected.
static void commandStrip(const GsState* st, const UiView* v, int y) {
	const int h = 28;
	gPanel(4, y, 312, h, C_CARD_T, C_CARD_B, C_GOLD_DIM);
	int x = 9;
	static const char* const kTopNames[3] = {"Fight", "Flee", "Status"};
	static const char* const kTopNamesTla[3] = {"Fight", "Switch", "Flee"};   // Lost Age round menu order
	const bool top = st->battlePhase == GS_PH_COMMAND && st->battleTop;
	for (int i = 0; i < (top ? 3 : 6); ++i) {
		if (!top && i == 3 && st->battleNoSummon) continue;   // the game hides Summon when none is available
		const char* label = top ? (gsRam()->battleLabel ? kTopNamesTla[i] : kTopNames[i]) : kCmdNames[i];
		int w = gTextW(label, 0) + 10;
		int shown = st->battlePhase == GS_PH_COMMAND ? st->battleCmd : v->lastCmd;
		bool cur = shown == i && st->battlePhase != GS_PH_NONE;
		uint16_t edge = !cur ? C_GOLD_DIM : st->battlePhase == GS_PH_COMMAND ? RGB(255, 240, 150) : st->battlePhase == GS_PH_LIST ? RGB(150, 235, 255) : RGB(255, 190, 100);
		gPanel(x, y + 3, w, 13, cur ? C_ACTIVE_T : C_PANEL_T, cur ? C_ACTIVE_B : C_PANEL_B, edge);
		gText(x + 5, y + 6, label, 0, cur ? C_TEXT : C_DIM);
		x += w + 4;
	}
	char line[80] = "";
	uint16_t c = C_DIM;
	if (st->battlePhase == GS_PH_LIST) {
		static const char* const hint[6] = {"", "Choose a Psynergy", "Choose a Djinn", "Choose a summon", "Choose an item", ""};
		int ci = v->lastCmd;
		if (st->battleText[0]) {
			const char* nm = (ci == 0 || ci == 1) ? gsAbilityByDescription(st->battleText, NULL) : ci == 2 ? gsDjinnByDescription(st->battleText) : NULL;
			if (nm) snprintf(line, sizeof(line), "%s: %s", nm, st->battleText);
			else snprintf(line, sizeof(line), "%s", st->battleText);
			if (gTextW(line, 0) > 296) snprintf(line, sizeof(line), "%s", st->battleText);
		}
		else snprintf(line, sizeof(line), "%s", ci >= 0 && ci < 6 ? hint[ci] : "Choose from the list");
		c = RGB(170, 240, 255);
	} else if (st->battlePhase == GS_PH_TARGET) {
		c = RGB(255, 225, 140);
		if (st->battleTarget < 0) snprintf(line, sizeof(line), "Choose a target");
		else if (st->battleTargetAlly) {
			const GsMember* a = &st->party[st->battleTarget];
			snprintf(line, sizeof(line), "Target: %s   HP %u/%u   PP %u/%u", a->name, (unsigned) a->hp, (unsigned) a->maxHp, (unsigned) a->pp, (unsigned) a->maxPp);
			c = RGB(170, 255, 190);
		} else {
			const GsMember* e = &st->enemies[st->battleTarget];
			snprintf(line, sizeof(line), "Target: %s   HP %u/%u", e->name, (unsigned) e->hp, (unsigned) e->maxHp);
		}
	} else if (top) {
		snprintf(line, sizeof(line), gsRam()->battleLabel ? "Round menu: Fight, Switch or Flee" : "Round menu: Fight, Flee or Status");
	} else if (st->battlePhase == GS_PH_COMMAND && st->battleSel >= 0) {
		snprintf(line, sizeof(line), "%s: choose a command", st->party[st->battleSel].name);
	} else {
		snprintf(line, sizeof(line), "...");
	}
	gText(10, y + 19, line, 0, c);
}

static void battlePanel(const GsState* st, const UiView* v) {
	int n = st->enemyCount ? st->enemyCount : 1;
	int rowH = n <= 2 ? 14 : n == 3 ? 11 : 10;
	int ey = CONTENT_Y + 15;
	// Slim ribbon: BATTLE + who is up.
	gPanel(4, CONTENT_Y, 312, 13, RGB(120, 30, 30), RGB(60, 10, 14), RGB(255, 120, 90));
	gTextShadow(12, CONTENT_Y + 3, "BATTLE", 1, C_TEXT);
	if (v->roundSaved && (st->battleTop || v->roundRepeating)) {   // on the round menu: replay the last round's commands
		gPanel(96, CONTENT_Y + 1, 62, 11, v->roundRepeating ? C_PANEL_T : C_ACTIVE_T, v->roundRepeating ? C_PANEL_B : C_ACTIVE_B, C_GOLD);
		gTextCentered(127, CONTENT_Y + 3, v->roundRepeating ? "Repeating" : "Repeat round", 0, C_GOLD);
	}
	char who[40] = "";
	int sel = st->battleSel, act = v->actFrames > 0 ? v->actHero : -1;
	if (sel >= 0 && sel < st->partySize) snprintf(who, sizeof(who), "%s is choosing", st->party[sel].name);
	else if (act >= 0 && act < st->partySize) snprintf(who, sizeof(who), "%s acts", st->party[act].name);
	if (who[0]) gTextRight(308, CONTENT_Y + 3, who, 1, sel >= 0 ? RGB(255, 240, 150) : RGB(150, 235, 255));
	enemyList(st, ey, rowH);
	int sy = ey + 6 + rowH * n + 3;
	commandStrip(st, v, sy);
	int top = sy + 28 + 3;
	int h = CONTENT_Y + CONTENT_H - top;
	if (h > 92) h = 92;
	for (int i = 0; i < GS_PARTY_MAX; ++i) {
		int mode = (st->battlePhase == GS_PH_TARGET && st->battleTargetAlly && st->battleTarget == i) ? 3 : i == sel ? 1 : (i == act && sel < 0) ? 2 : 0;
		battleCard(4 + i * 79, top, 75, h, &st->party[i], mode);
	}
}

static const uint16_t kElemColor[GS_ELEMENTS] = {RGB(220, 170, 70), RGB(255, 100, 70), RGB(190, 140, 255), RGB(90, 170, 255)};

static int popcount8(uint32_t v) {
	int n = 0;
	for (; v; v >>= 1) n += v & 1;
	return n;
}


// ---- character screen ----

// ---- saved Djinn sets ----
static void loadView(const GsState* st, const UiView* v) {
	gPanel(4, CONTENT_Y, 312, CONTENT_H, C_CARD_T, C_CARD_B, C_GOLD);
	gPill(12, CONTENT_Y + 5, 240, 17, C_PANEL_T, C_PANEL_B, C_GOLD);
	gTextCentered(132, CONTENT_Y + 9, "Djinn sets", 1, C_GOLD);
	gPanel(262, CONTENT_Y + 5, 46, 17, C_PANEL_T, C_PANEL_B, C_GOLD);
	gTextCentered(285, CONTENT_Y + 9, "Back", 1, C_TEXT);
	gPanel(12, CONTENT_Y + 30, 146, 24, C_ACTIVE_T, C_ACTIVE_B, C_GOLD);
	gTextCentered(85, CONTENT_Y + 38, "All Set", 1, C_GOLD);
	gPanel(162, CONTENT_Y + 30, 146, 24, C_PANEL_T, C_PANEL_B, C_GOLD_DIM);
	gTextCentered(235, CONTENT_Y + 38, "All Standby", 1, C_TEXT);
	for (int i = 0; i < 3; ++i) {
		int ry = CONTENT_Y + 62 + i * 30;
		char line[48];
		gPanel(12, ry, 180, 24, C_PANEL_T, C_PANEL_B, v->loadValid[i] ? C_GOLD_DIM : C_DIM);
		if (v->loadValid[i]) {
			int total = 0;
			for (int e = 0; e < GS_ELEMENTS; ++e) total += popcount8(v->loadSet[i][e]);
			snprintf(line, sizeof(line), "Set %d: %d Set", i + 1, total);
			gTextShadow(20, ry + 4, line, 1, C_TEXT);
			snprintf(line, sizeof(line), "Ve%d Ma%d Ju%d Me%d", popcount8(v->loadSet[i][0]), popcount8(v->loadSet[i][1]), popcount8(v->loadSet[i][2]), popcount8(v->loadSet[i][3]));
			gText(20, ry + 15, line, 0, C_DIM);
		} else {
			snprintf(line, sizeof(line), "Set %d: empty", i + 1);
			gTextShadow(20, ry + 8, line, 1, C_DIM);
		}
		gPanel(196, ry, 54, 24, C_PANEL_T, C_PANEL_B, C_GOLD);
		gTextCentered(223, ry + 8, "Save", 1, C_TEXT);
		gPanel(254, ry, 54, 24, v->loadValid[i] ? C_ACTIVE_T : C_PANEL_T, v->loadValid[i] ? C_ACTIVE_B : C_PANEL_B, v->loadValid[i] ? C_GOLD : C_DIM);
		gTextCentered(281, ry + 8, "Apply", 1, v->loadValid[i] ? C_GOLD : C_DIM);
	}
	(void) st;
}

static void charView(const GsState* st, const UiView* v) {
	static const char* const elem[GS_ELEMENTS] = {"Venus", "Mars", "Jupiter", "Mercury"};
	static const uint16_t ecol[GS_ELEMENTS] = {RGB(220, 170, 70), RGB(255, 100, 70), RGB(190, 140, 255), RGB(90, 170, 255)};
	int who = v->charIdx < st->partySize ? v->charIdx : 0;
	const GsMember* m = &st->party[who];
	gPanel(4, CONTENT_Y, 312, CONTENT_H, C_CARD_T, C_CARD_B, C_GOLD);
	char line[64];
	const char* cls = gsRam()->namesKnown ? gsClassName(m->classId) : "";
	snprintf(line, sizeof(line), cls[0] ? "%s  %s  Lv %u" : "%s  Lv %u%.0s", m->name, cls[0] ? cls : "", (unsigned) m->level, "");
	if (!cls[0]) snprintf(line, sizeof(line), "%s  Lv %u", m->name, (unsigned) m->level);
	gPill(12, CONTENT_Y + 5, 190, 17, C_PANEL_T, C_PANEL_B, C_GOLD);
	gTextCentered(107, CONTENT_Y + 9, line, 1, C_GOLD);
	gPanel(210, CONTENT_Y + 5, 48, 17, C_PANEL_T, C_PANEL_B, C_GOLD_DIM);
	gTextCentered(234, CONTENT_Y + 9, "Game", 1, C_TEXT);
	gPanel(262, CONTENT_Y + 5, 46, 17, C_PANEL_T, C_PANEL_B, C_GOLD);
	gTextCentered(285, CONTENT_Y + 9, "Back", 1, C_TEXT);
	gSprite(12, CONTENT_Y + 28, artPortrait(m->name));
	uint16_t ha, hb;
	hpColors(m->hp, m->maxHp, &ha, &hb);
	snprintf(line, sizeof(line), "%u/%u", (unsigned) m->hp, (unsigned) m->maxHp);
	gaugeText(12, CONTENT_Y + 78, 68, 11, m->hp, m->maxHp, ha, hb, line);
	snprintf(line, sizeof(line), "%u/%u", (unsigned) m->pp, (unsigned) m->maxPp);
	gaugeText(12, CONTENT_Y + 91, 68, 11, m->pp, m->maxPp, C_PP_A, C_PP_B, line);
	statusBadge(46, CONTENT_Y + 70, m);
	// stats and EXP
	snprintf(line, sizeof(line), "Attack %u    Defense %u    Agility %u", (unsigned) m->atk, (unsigned) m->def, (unsigned) m->agi);
	gTextShadow(92, CONTENT_Y + 30, line, 1, C_TEXT);
	snprintf(line, sizeof(line), "EXP %lu", (unsigned long) m->exp);
	gTextShadow(92, CONTENT_Y + 46, line, 1, C_TEXT);
	if (m->expNext) {
		snprintf(line, sizeof(line), "%lu to level %u", (unsigned long) m->expNext, (unsigned) m->level + 1);
		gTextRight(306, CONTENT_Y + 46, line, 1, C_GOLD);
		unsigned long span = (unsigned long) (m->expNext + (m->exp > m->expLevelStart ? m->exp - m->expLevelStart : 0));
		gauge(92, CONTENT_Y + 60, 214, (int) (m->exp > m->expLevelStart ? m->exp - m->expLevelStart : 0), (int) span, C_PP_A, C_PP_B);
	} else if (m->level >= 99) {
		gTextRight(306, CONTENT_Y + 46, "Max level", 1, C_GOLD);
	}
	// elements: Djinn held (set/total), power and resist, in the game's order
	static const int order[4] = {0, 3, 1, 2};   // Venus, Mercury, Mars, Jupiter
	for (int k = 0; k < 4; ++k) {
		int e = order[k], x = 92 + k * 53, y = CONTENT_Y + 76;
		gPanel(x, y, 50, 74, C_PANEL_T, C_PANEL_B, ecol[e]);
		gTextCentered(x + 25, y + 4, elem[e], 0, ecol[e]);
		const Sprite* mascot = artDjinn(e);
		if (mascot) gSprite(x + 25 - mascot->w / 2, y + 12, mascot);
		snprintf(line, sizeof(line), "%d/%d", popcount8(m->djinnSet[e]), popcount8(m->djinnOwned[e]));
		gTextCentered(x + 25, y + 40, line, 0, popcount8(m->djinnOwned[e]) ? C_TEXT : C_DIM);
		snprintf(line, sizeof(line), "Pow %u", (unsigned) m->elemPow[e]);
		gTextCentered(x + 25, y + 51, line, 0, C_TEXT);
		snprintf(line, sizeof(line), "Res %u", (unsigned) m->elemRes[e]);
		gTextCentered(x + 25, y + 62, line, 0, C_DIM);
	}
	if (st->partySize > 1) {   // the screen edges step to the neighbouring member
		gTextShadow(5, CONTENT_Y + 100, "<", 1, C_GOLD);
		gTextShadow(305, CONTENT_Y + 100, ">", 1, C_GOLD);
	}
}

static void psynergyTab(const GsState* st, const UiView* v) {
	int who = v->psyChar < st->partySize ? v->psyChar : 0;
	const GsMember* m = &st->party[who];
	gPanel(4, CONTENT_Y, 312, CONTENT_H, C_CARD_T, C_CARD_B, C_GOLD_DIM);
	for (int i = 0; i < st->partySize; ++i) {
		bool on = i == who;
		gPanel(8 + i * 76, CONTENT_Y + 4, 72, 20, on ? C_ACTIVE_T : C_PANEL_T, on ? C_ACTIVE_B : C_PANEL_B, on ? C_GOLD : C_GOLD_DIM);
		gTextCentered(8 + i * 76 + 36, CONTENT_Y + 11, st->party[i].name, 1, on ? C_TEXT : C_DIM);
	}
	char line[32];
	snprintf(line, sizeof(line), "PP %u/%u", (unsigned) m->pp, (unsigned) m->maxPp);
	gText(12, CONTENT_Y + 30, line, 1, C_TEXT);
	gauge(80, CONTENT_Y + 29, 112, m->pp, m->maxPp, C_PP_A, C_PP_B);
	for (int e = 0; e < GS_ELEMENTS; ++e) {   // Djinn held: set/total per element
		int x = 204 + e * 28, own = popcount8(m->djinnOwned[e]), set = popcount8(m->djinnSet[e]);
		for (int j = -2; j <= 2; ++j)
			for (int i = -2; i <= 2; ++i)
				if (i * i + j * j <= 5) gPx(x + i, CONTENT_Y + 33 + j, own ? kElemColor[e] : RGB(30, 40, 70));
		snprintf(line, sizeof(line), "%d/%d", set, own);
		gText(x + 5, CONTENT_Y + 30, line, 0, own ? C_TEXT : C_DIM);
	}
	int shown = 0;
	for (int k = 0; k < m->abilityCount && shown < 27; ++k) {
		uint16_t a = m->abilities[k];
		int col = shown / 9, row = shown % 9;
		int x = 10 + col * 102, y = CONTENT_Y + 42 + row * 11;
		uint16_t c = gsAbilityFieldUsable(a & 0xFF) ? RGB(150, 220, 255) : (a & 0x8000) ? RGB(255, 226, 150) : C_DIM;
		gRect(x - 2, y - 1, 98, 10, row & 1 ? RGB(14, 28, 66) : RGB(18, 34, 76));
		const Sprite* ic = artPsyIconSmall(a & 0xFF);   // the game's own list icon (field-usable Psynergy); others get a dot in the type colour
		if (ic) gSprite(x, y, ic); else gRect(x + 2, y + 3, 3, 3, c);
		gTextShadow(x + 11, y, gsAbilityName(a & 0xFF), 1, c);
		++shown;
	}
	if (!shown) gTextCentered(160, CONTENT_Y + 90, "No Psynergy learned", 1, C_DIM);
	gText(14, CONTENT_Y + CONTENT_H - 11, "Blue = tap to cast   Gold = battle only", 0, C_DIM);
}

// ---- bestiary ----
static void bestView(const UiView* v) {
	gPanel(4, CONTENT_Y, 312, CONTENT_H, C_CARD_T, C_CARD_B, C_GOLD);
	gPill(12, CONTENT_Y + 5, 150, 17, C_PANEL_T, C_PANEL_B, C_GOLD);
	char title[32];
	snprintf(title, sizeof(title), "Bestiary  %d", bestCount());
	gTextCentered(87, CONTENT_Y + 9, title, 1, C_GOLD);
	gPanel(254, CONTENT_Y + 5, 54, 17, C_PANEL_T, C_PANEL_B, C_GOLD);
	gTextCentered(281, CONTENT_Y + 9, "Back", 1, C_TEXT);
	if (!bestCount()) {
		gTextCentered(160, BEST_TOP + 30, "No enemies recorded yet.", 1, C_DIM);
		gTextCentered(160, BEST_TOP + 46, "Every enemy you fight is added here with its stats.", 0, C_DIM);
		return;
	}
	gText(14, LOG_TOP - 1, "Enemy", 0, C_DIM);
	gTextRight(148, LOG_TOP - 1, "Lv", 0, C_DIM);
	gTextRight(190, LOG_TOP - 1, "HP", 0, C_DIM);
	gTextRight(228, LOG_TOP - 1, "ATK", 0, C_DIM);
	gTextRight(262, LOG_TOP - 1, "DEF", 0, C_DIM);
	gTextRight(294, LOG_TOP - 1, "AGI", 0, C_DIM);
	gTextRight(310, LOG_TOP - 1, "x", 0, C_DIM);
	int top = BEST_TOP, bottom = BEST_TOP + BEST_VIEW_H;
	for (int i = 0; i < bestCount(); ++i) {
		int y = top + i * BEST_ROW - v->bestScroll;
		if (y < top || y + BEST_ROW > bottom) continue;
		const BestEntry* e = bestAt(i);
		gRect(8, y, 296, BEST_ROW - 1, i & 1 ? RGB(14, 28, 66) : RGB(18, 34, 76));
		char num[12];
		gTextShadow(12, y + 2, e->name, 1, C_TEXT);
		snprintf(num, sizeof(num), "%u", (unsigned) e->level); gTextRight(148, y + 2, num, 1, C_TEXT);
		snprintf(num, sizeof(num), "%u", (unsigned) e->maxHp); gTextRight(190, y + 2, num, 1, RGB(150, 235, 160));
		snprintf(num, sizeof(num), "%u", (unsigned) e->atk); gTextRight(228, y + 2, num, 1, RGB(255, 200, 140));
		snprintf(num, sizeof(num), "%u", (unsigned) e->def); gTextRight(262, y + 2, num, 1, RGB(160, 200, 255));
		snprintf(num, sizeof(num), "%u", (unsigned) e->agi); gTextRight(294, y + 2, num, 1, RGB(255, 235, 150));
		snprintf(num, sizeof(num), "%u", (unsigned) e->seen); gTextRight(310, y + 2, num, 1, C_DIM);
	}
	if (bestCount() * BEST_ROW > BEST_VIEW_H) gText(14, CONTENT_Y + CONTENT_H - 11, "Drag to scroll", 0, C_DIM);
}

static const char* const djinnNames[GS_ELEMENTS][7] = {
	{"Flint", "Granite", "Quartz", "Vine", "Sap", "Ground", "Bane"},
	{"Forge", "Fever", "Corona", "Scorch", "Ember", "Flash", "Torch"},
	{"Gust", "Breeze", "Zephyr", "Smog", "Kite", "Squall", "Luff"},
	{"Fizz", "Sleet", "Mist", "Spritz", "Hail", "Tonic", "Dew"},
};

static const char* const djinnNamesTla[GS_ELEMENTS][18] = {   // The Lost Age: 7 Golden Sun Djinn then 11 new ones per element (decoded from its ROM text, tools/ta_textdec.py)
	{"Flint", "Granite", "Quartz", "Vine", "Sap", "Ground", "Bane", "Echo", "Iron", "Steel", "Mud", "Flower", "Meld", "Petra", "Salt", "Geode", "Mold", "Crystal"},
	{"Forge", "Fever", "Corona", "Scorch", "Ember", "Flash", "Torch", "Cannon", "Spark", "Kindle", "Char", "Coal", "Reflux", "Core", "Tinder", "Shine", "Fury", "Fugue"},
	{"Gust", "Breeze", "Zephyr", "Smog", "Kite", "Squall", "Luff", "Breath", "Blitz", "Ether", "Waft", "Haze", "Wheeze", "Aroma", "Whorl", "Gasp", "Lull", "Gale"},
	{"Fizz", "Sleet", "Mist", "Spritz", "Hail", "Tonic", "Dew", "Fog", "Sour", "Spring", "Shade", "Chill", "Steam", "Rime", "Gel", "Eddy", "Balm", "Serac"},
};

static void djinnTab(const GsState* st, const UiView* v) {
	static const char* const elem[GS_ELEMENTS] = {"Venus", "Mars", "Jupiter", "Mercury"};
	static const uint16_t ec[GS_ELEMENTS] = {RGB(220, 170, 70), RGB(255, 100, 70), RGB(190, 140, 255), RGB(90, 170, 255)};
	int count = gsRam()->djinnCount, big = count > 7;   // The Lost Age: 18 per element, laid out as 3 columns x 6 rows in the tiny font
	for (int e = 0; e < GS_ELEMENTS; ++e) {
		int x = 4 + (e & 1) * 158, y = CONTENT_Y + (e >> 1) * 80;
		gPanel(x, y, 154, 77, C_CARD_T, C_CARD_B, C_GOLD_DIM);
		int owned = popcount8(st->djinnOwned[e]);
		char line[24];
		snprintf(line, sizeof(line), "%s %d/%d", elem[e], owned, count);
		gTextShadow(x + 6, y + 4, line, 1, ec[e]);
		if (e == 0) {   // opens the saved Djinn sets
			gPanel(x + 104, y + 2, 46, 14, C_ACTIVE_T, C_ACTIVE_B, C_GOLD);
			gTextCentered(x + 127, y + 5, "Sets", 0, C_GOLD);
		}
		const Sprite* mascot = big ? NULL : artDjinn(e);   // the element's Djinn creature from the game's Status screen (no room in the 18-Djinn layout)
		if (mascot) gSprite(x + 127, y + 44, mascot);
		for (int k = 0; k < count; ++k) {
			int cx = big ? x + 4 + (k / 6) * 50 : x + (k >= 4 ? 72 : 0), yy = big ? y + 16 + (k % 6) * 10 : y + 18 + (k % 4) * 14;
			bool have = (st->djinnOwned[e] >> k) & 1, set = (st->djinnSet[e] >> k) & 1;
			uint16_t c = !have ? C_DIM : set ? C_TEXT : RGB(255, 90, 90);
			int r = big ? 3 : 5, ox = big ? 3 : 8, oy = big ? 3 : 3;
			for (int j = 0; j < r; ++j)
				for (int i = 0; i < r; ++i)
					if (big || (i - 2) * (i - 2) + (j - 2) * (j - 2) <= 5) gPx(cx + ox + i - (big ? 3 : 0), yy + (big ? 1 : 1) + j, have ? (set ? ec[e] : gMix(ec[e], RGB(20, 20, 40), 140)) : RGB(30, 40, 70));
			(void) oy;
			char generic[16];
			const char* nm = "???";
			if (have) {
				if (big) nm = djinnNamesTla[e][k];
				else if (gsRam()->namesKnown) nm = djinnNames[e][k];
				else { snprintf(generic, sizeof(generic), "Djinn %d", k + 1); nm = generic; }
			}
			gText(cx + (big ? 6 : 17), yy - (big ? 2 : 0), nm, 1, c);
		}
	}
	if (v->djInfoFrames > 0 && v->djInfoK >= 0) {   // details from a long press (or a tap in The Lost Age)
		int e = v->djInfoE, k = v->djInfoK;
		int y0 = CONTENT_Y + CONTENT_H - 64;
		gPanel(10, y0, 300, 58, C_PANEL_T, C_PANEL_B, C_GOLD);
		char line[64];
		snprintf(line, sizeof(line), "%s  (%s)", gsDjinnName(e, k), elem[e]);
		gTextShadow(18, y0 + 4, line, 1, ec[e]);
		line[0] = 0;
		for (int p = 0; p < st->partySize; ++p) {
			if (!((st->party[p].djinnOwned[e] >> k) & 1)) continue;
			bool set = (st->party[p].djinnSet[e] >> k) & 1;
			snprintf(line, sizeof(line), "Held by %s - %s", st->party[p].name, set ? "Set" : "Standby");
			break;
		}
		if (line[0]) gText(18, y0 + 18, line, 0, C_DIM);
		const char* d = gsDjinnDescription(e, k);
		wrapLines(d[0] ? d : "No description.", 280, 1, y0 + 30, 18, C_TEXT, CONTENT_Y, CONTENT_Y + CONTENT_H, true);
	}
}

// 13px glyphs for the map buttons (gold line art).
static void glyphCrosshair(int cx, int cy, uint16_t c) {
	for (int j = -5; j <= 5; ++j) for (int i = -5; i <= 5; ++i) { int d = i * i + j * j; if (d >= 12 && d <= 20) gPx(cx + i, cy + j, c); }
	gRect(cx - 7, cy, 4, 1, c); gRect(cx + 4, cy, 4, 1, c); gRect(cx, cy - 7, 1, 4, c); gRect(cx, cy + 4, 1, 4, c);
	gPx(cx, cy, c);
}
static void glyphMagnifier(int cx, int cy, uint16_t c) {
	for (int j = -4; j <= 4; ++j) for (int i = -4; i <= 4; ++i) { int d = i * i + j * j; if (d >= 9 && d <= 16) gPx(cx - 2 + i, cy - 2 + j, c); }
	for (int k = 0; k < 5; ++k) { gPx(cx + 2 + k, cy + 2 + k, c); gPx(cx + 3 + k, cy + 2 + k, c); }
}
static void mapTab(const GsState* st, const UiView* v) {
	gPanel(4, CONTENT_Y, 312, 134, C_CARD_T, C_CARD_B, C_GOLD);
	if (!gsRam()->hasMap) {   // player position / camera not located for this game yet
		gTextCentered(UI_W / 2, CONTENT_Y + 50, areaName(st), 2, C_GOLD);
		gTextCentered(UI_W / 2, CONTENT_Y + 78, "Live map is not available for this game yet.", 1, C_DIM);
		return;
	}
	mapDraw(MAP_VX, MAP_VY, MAP_VW, MAP_VH, v->mapZoom, v->panX, v->panY);
	{
		const char* nm = areaName(st);
		int w = gTextW(nm, 1) + 26;
		gPill(MAP_VX + 3, MAP_VY + 2, w, 17, RGB(26, 52, 110), RGB(10, 24, 62), C_GOLD);
		gTextShadow(MAP_VX + 14, MAP_VY + 7, nm, 1, RGB(255, 238, 190));
	}
	if (st->areaGroup == 2) {   // the overworld scrolls differently and is not stitched
		gTextCentered(MAP_VX + MAP_VW / 2, MAP_VY + MAP_VH / 2 - 4, "WORLD MAP IS NOT MAPPED", 1, RGB(150, 160, 190));
	}
	for (int i = 0; i < 2; ++i) {
		int x = 4 + i * 158, w = 154;
		gPanel(x, MAP_BTN_Y, w, MAP_BTN_H + 3, C_PANEL_T, C_PANEL_B, C_GOLD);
		const char* t = i == 0 ? "Center" : (v->mapZoom == 1 ? "Zoom: detail" : "Zoom: overview");
		int cy = MAP_BTN_Y + 11;
		if (i == 0) glyphCrosshair(x + 17, cy, C_GOLD);
		else glyphMagnifier(x + 16, cy, C_GOLD);
		gTextShadow(x + 30, MAP_BTN_Y + 8, t, 1, C_TEXT);
	}
}

static void itemsTab(const GsState* st, const UiView* v) {
	int who = v->itemChar < st->partySize ? v->itemChar : 0;
	gPanel(4, CONTENT_Y, 312, CONTENT_H, C_CARD_T, C_CARD_B, C_GOLD_DIM);
	for (int i = 0; i < st->partySize; ++i) {
		bool on = i == who;
		gPanel(8 + i * 76, CONTENT_Y + 4, 72, 20, on ? C_ACTIVE_T : C_PANEL_T, on ? C_ACTIVE_B : C_PANEL_B, on ? C_GOLD : C_GOLD_DIM);
		gTextCentered(8 + i * 76 + 36, CONTENT_Y + 11, st->party[i].name, 1, on ? C_TEXT : C_DIM);
	}
	int shown = 0;
	for (int k = 0; k < 15; ++k) {
		const GsItem* it = &st->party[who].items[k];
		if (!it->id) continue;
		int col = shown / 8, row = shown % 8;
		int x = 12 + col * 152, y = CONTENT_Y + 30 + row * 14;
		gRect(x - 2, y - 2, 146, 13, row & 1 ? RGB(14, 28, 66) : RGB(18, 34, 76));
		if (it->equipped) gRect(x, y + 1, 3, 7, C_GOLD);
		const Sprite* ic = artItemIcon(it->id);   // 16x16 with transparent padding, so neighbouring 14px rows may overlap by 2px
		if (ic) gSprite(x + 5, y - 1, ic);
		gTextShadow(x + (ic ? 23 : 7), y + 1, gsItemName(it->id), 1, C_TEXT);
		if (it->qty > 1) {
			char q[8];
			snprintf(q, sizeof(q), "x%u", (unsigned) it->qty);
			gTextRight(x + 140, y + 1, q, 1, C_GOLD);
		}
		++shown;
	}
	if (!shown) gTextCentered(160, CONTENT_Y + 70, "No items", 1, C_DIM);
	char line[40];
	snprintf(line, sizeof(line), "EXP %lu", (unsigned long) st->party[who].exp);
	gTextRight(306, CONTENT_Y + CONTENT_H - 12, line, 1, C_DIM);
	gText(14, CONTENT_Y + CONTENT_H - 12, "Gold = equipped  Tap = equip  Hold = info", 0, C_DIM);
	gPanel(226, CONTENT_Y + CONTENT_H - 31, 82, 15, C_ACTIVE_T, C_ACTIVE_B, C_GOLD);   // sits above the EXP line and below the last item row
	gTextCentered(267, CONTENT_Y + CONTENT_H - 29, "Equip best", 1, C_GOLD);   // body font with room for the q and p tails; acts on the member shown above
	if (v->descFrames > 0) {   // description popup from a long press
		gPanel(10, CONTENT_Y + CONTENT_H - 56, 300, 50, C_PANEL_T, C_PANEL_B, C_GOLD);
		gTextShadow(18, CONTENT_Y + CONTENT_H - 51, gsItemName((unsigned) v->descId), 1, C_GOLD);
		const char* d = gsItemDescription((unsigned) v->descId);
		wrapLines(d[0] ? d : "No description.", 280, 1, CONTENT_Y + CONTENT_H - 37, 18, C_TEXT, CONTENT_Y, CONTENT_Y + CONTENT_H, true);
	}
}

static void menuTab(const UiView* v) {
	for (int i = 0; i < MB_COUNT; ++i) {
		int x = MENU_BX(i), y = MENU_BY(i);
		bool on = (i == MB_FAST && v->fastForward) || (i == MB_HIDEMENU && v->hideMenus) || (i == MB_AUTOSAVE && v->autoSave) || (i == MB_MUTE && v->muteSlow);
		gPanel(x, y, MENU_W, MENU_H, on ? C_ACTIVE_T : C_PANEL_T, on ? C_ACTIVE_B : C_PANEL_B, on ? C_GOLD : C_GOLD_DIM);
		const char* val = i == MB_FAST ? (v->fastForward ? "On" : "Off") : i == MB_SCALE ? (v->topScale == TOP_FILL ? "Fill" : "1x") : i == MB_HIDEMENU ? (v->hideMenus ? "On" : "Off") : i == MB_AUTOSAVE ? (v->autoSave ? "On" : "Off") : i == MB_MUTE ? (v->muteSlow ? "On" : "Off") : i == MB_SPEED ? (const char* const[]) {"2x", "2x", "3x", "4x"}[v->speedMul < 2 ? 1 : v->speedMul > 4 ? 3 : v->speedMul - 1] : "";
		gTextShadow(x + 9, y + 6, menuLabels[i], 1, C_TEXT);
		if (val[0]) gTextRight(x + MENU_W - 9, y + 6, val, 1, C_GOLD);
	}
}

// Before a game is started or loaded there is no party to show: the title screen, intro and name entry all read as "no party".
static bool gameStarted(const GsState* st) { return st->partySize > 0 && st->areaGroup != 0; }

static void waitingPanel(void) {
	gPanel(4, CONTENT_Y, 312, CONTENT_H, C_CARD_T, C_CARD_B, C_GOLD);
	int cx = UI_W / 2, y = CONTENT_Y + CONTENT_H / 2 - 46;
	gSprite(cx - artIcon(ICON_MAP)->w / 2, y, artIcon(ICON_MAP));
	gTextCentered(cx, y + 42, "Waiting for a game", 2, C_GOLD);
	gTextCentered(cx, y + 62, "Start a new game on the top screen,", 1, C_TEXT);
	gTextCentered(cx, y + 76, "or open Menu and tap Save slots.", 1, C_DIM);
	if (gsRam()->djinnCount > 7) {   // The Lost Age: the transfer password is typed on a letter grid
		gPanel(90, CONTENT_Y + CONTENT_H - 36, 140, 28, C_PANEL_T, C_PANEL_B, C_GOLD);
		gTextCentered(160, CONTENT_Y + CONTENT_H - 27, "Password keyboard", 1, C_GOLD);
	}
}

static const char* footerNote;   // set by uiDraw: replaces the footer's second line (the timing readout on the Menu tab)

// Gold octagonal coin like the game's menu icon: a thin dark outline, a faceted rim (bright top and bottom-left, dark right), and a golden face
// with a vertical gradient and a soft gloss at the upper left.
static void drawCoin(int cx, int cy, int r) {
	static const uint16_t rimN = RGB(254, 240, 170), rimNW = RGB(254, 247, 215), rimW = RGB(244, 205, 70), rimSW = RGB(214, 130, 8);
	static const uint16_t rimS = RGB(250, 226, 120), rimSE = RGB(190, 116, 10), rimE = RGB(158, 94, 8), rimNE = RGB(232, 172, 28);
	for (int j = -r; j <= r; ++j) {
		for (int i = -r; i <= r; ++i) {
			int ax = i < 0 ? -i : i, ay = j < 0 ? -j : j;
			int m = ax > ay ? ax : ay;
			int diag = ((ax + ay) * 181) >> 8;   // (|x| + |y|) / sqrt 2
			int d = m > diag ? m : diag;
			if (d > r) continue;
			uint16_t c;
			if (d >= r) {                                                        // outline: dark brown, darker towards the bottom right
				int t = 128 + (i + j) * 10;
				if (t < 0) t = 0; else if (t > 255) t = 255;
				c = gMix(RGB(120, 72, 8), RGB(58, 30, 2), t);
			} else if (d >= r - 2) {                                             // rim, one shade per octagon side
				if (m >= diag) c = ay >= ax ? (j < 0 ? rimN : rimS) : (i < 0 ? rimW : rimE);
				else c = i < 0 ? (j < 0 ? rimNW : rimSW) : (j < 0 ? rimNE : rimSE);
			} else {                                                             // face
				int t = (j + r) * 255 / (2 * r);
				c = gMix(RGB(253, 219, 88), RGB(243, 176, 34), t);
				int band = i + j;
				if (band <= -r / 2 && band >= -r && d < r - 3) c = gMix(c, RGB(254, 250, 226), band <= -r / 2 - 1 ? 190 : 110);   // gloss
				if (d == r - 2 || (ax + ay > r + 1 && i > 0 && j > 0)) c = gMix(c, RGB(205, 128, 8), 70);                         // shaded lower right
			}
			gPx(cx + i, cy + j, c);
		}
	}
}

static void footer(const GsState* st) {
	if (!gameStarted(st)) {
		gPanel(4, FOOT_Y, 312, FOOT_H, C_CARD_T, C_CARD_B, C_GOLD);
		gSprite(6, FOOT_Y + (FOOT_H - ICON_SMALL) / 2, artIconSmall(ICON_MAP));
		gTextShadow(42, FOOT_Y + 5, gsGameName(), 1, C_GOLD);
		gText(42, FOOT_Y + 17, gsGameMapped() ? "No game loaded yet" : "RAM map not set up", 0, C_DIM);
		return;
	}
	gPanel(4, FOOT_Y, 204, FOOT_H, C_CARD_T, C_CARD_B, C_GOLD);
	gSprite(6, FOOT_Y + (FOOT_H - ICON_SMALL) / 2, artIconSmall(ICON_MAP));
	gTextShadow(42, FOOT_Y + 5, areaName(st), 1, C_GOLD);
	char id[48];
	if (st->mode == GS_MODE_BATTLE) {
		// Who is quickest: the three highest Agility among the living (a guide to the turn order, the game adds some randomness).
		const GsMember* all[GS_PARTY_MAX + GS_ENEMY_MAX];
		int n = 0;
		for (int i = 0; i < GS_PARTY_MAX; ++i) if (st->party[i].present && st->party[i].hp > 0) all[n++] = &st->party[i];
		for (int i = 0; i < st->enemyCount; ++i) if (st->enemies[i].hp > 0) all[n++] = &st->enemies[i];
		for (int a = 0; a < n; ++a) for (int b = a + 1; b < n; ++b) if (all[b]->agi > all[a]->agi) { const GsMember* t = all[a]; all[a] = all[b]; all[b] = t; }
		char first[3][12] = {"", "", ""};
		for (int k = 0; k < 3 && k < n; ++k) snprintf(first[k], sizeof(first[k]), "%.5s %u", all[k]->name, (unsigned) all[k]->agi);
		snprintf(id, sizeof(id), "AGI %s>%s>%s", first[0], first[1], first[2]);
	}
	else snprintf(id, sizeof(id), "Map %03X   Party Lv %u", (unsigned) st->areaGroup, (unsigned) st->party[0].level);
	gText(42, FOOT_Y + 17, footerNote ? footerNote : id, 0, footerNote ? C_GOLD : C_DIM);
	gPanel(212, FOOT_Y, 104, FOOT_H, C_CARD_T, C_CARD_B, C_GOLD);
	drawCoin(228, FOOT_Y + 14, 8);
	char line[24];
	snprintf(line, sizeof(line), "%lu", (unsigned long) st->coins);
	int cs = gTextW(line, 2) <= 62 ? 2 : 1;
	gTextRight(308, FOOT_Y + (cs == 2 ? 8 : 10), line, cs, C_GOLD);
}

// The tab bar only changes when the active tab does, so each variant is drawn once and then copied.
#define BAR_H 48
static uint16_t barCache[TAB_COUNT + 1][UI_W * BAR_H];
static bool barValid[TAB_COUNT + 1];

static void drawTabBar(const UiView* v) {
	int variant = v->tab < TAB_COUNT ? v->tab : TAB_COUNT;
	if (barValid[variant]) { gRestoreRegion(0, 0, UI_W, BAR_H, barCache[variant]); return; }
	for (int i = 0; i < TAB_COUNT; ++i) tabButton(i, v);
	gSaveRegion(0, 0, UI_W, BAR_H, barCache[variant]);
	barValid[variant] = true;
}

void uiDraw(uint16_t* framebuffer, const GsState* st, const UiView* v) {
	gBegin(framebuffer);
	if (v->mirror) {
		mirrorDraw(v);
		footer(st);
		return;
	}
	footerNote = v->tab == TAB_MENU && v->perf[0] ? v->perf : NULL;
	gSetBackgroundImage(artBackground());
	gCornerSprite = artCorner();
	gBackground(C_BG_TOP, C_BG_BOT);
	drawTabBar(v);
	if (v->showKeys) {   // before the "waiting for a game" panel: the password screen is exactly when no game is loaded yet
		keysView();
		footer(st);
		return;
	}
	if (!gameStarted(st) && v->tab != TAB_MENU) {
		waitingPanel();
		footer(st);
		return;
	}
	if (v->showLoad) {
		loadView(st, v);
		footer(st);
		return;
	}
	if (v->showChar && !v->pickActive) {
		charView(st, v);
		footer(st);
		return;
	}
	if (v->eqActive) {
		gPanel(4, CONTENT_Y, 312, CONTENT_H, C_CARD_T, C_CARD_B, C_GOLD);
		gPill(12, CONTENT_Y + 5, 296, 17, C_PANEL_T, C_PANEL_B, C_GOLD);
		gTextCentered(160, CONTENT_Y + 9, v->eqLabel, 1, C_GOLD);
		static const char* const lab[3] = {"Attack", "Defense", "Agility"};
		for (int i = 0; i < 3; ++i) {
			int y = CONTENT_Y + 34 + i * 26, d = v->eqNew[i] - v->eqCur[i];
			char line[48];
			gPanel(30, y, 260, 22, C_PANEL_T, C_PANEL_B, C_GOLD_DIM);
			gTextShadow(40, y + 7, lab[i], 1, C_TEXT);
			snprintf(line, sizeof(line), "%d  >  %d", v->eqCur[i], v->eqNew[i]);
			gTextCentered(168, y + 7, line, 1, C_TEXT);
			snprintf(line, sizeof(line), d > 0 ? "+%d" : "%d", d);
			gTextRight(280, y + 7, d ? line : "-", 1, d > 0 ? RGB(120, 230, 120) : d < 0 ? RGB(255, 120, 110) : C_DIM);
		}
		gPanel(40, CONTENT_Y + 118, 100, 30, C_ACTIVE_T, C_ACTIVE_B, C_GOLD);
		gTextCentered(90, CONTENT_Y + 127, "Equip", 1, C_GOLD);
		gPanel(180, CONTENT_Y + 118, 100, 30, C_PANEL_T, C_PANEL_B, C_GOLD_DIM);
		gTextCentered(230, CONTENT_Y + 127, "Cancel", 1, C_TEXT);
		footer(st);
		return;
	}
	if (v->pickActive) {
		gPanel(4, CONTENT_Y, 312, CONTENT_H, C_CARD_T, C_CARD_B, C_GOLD);
		char title[40];
		snprintf(title, sizeof(title), "Use %s on whom?", v->pickLabel);
		gPill(12, CONTENT_Y + 5, 230, 17, C_PANEL_T, C_PANEL_B, C_GOLD);
		gTextCentered(127, CONTENT_Y + 9, title, 1, C_GOLD);
		gPanel(254, CONTENT_Y + 5, 54, 17, C_PANEL_T, C_PANEL_B, C_GOLD);
		gTextCentered(281, CONTENT_Y + 9, "Cancel", 1, C_TEXT);
		partyCards(st, CONTENT_Y + 28, CONTENT_H - 28, false);
		footer(st);
		return;
	}
	if (v->showSlots) {
		slotsView(v);
		footer(st);
		return;
	}
	if (v->showLog) {
		logView(v);
		footer(st);
		return;
	}
	if (v->showBest) {
		bestView(v);
		footer(st);
		return;
	}
	switch (v->tab) {
	case TAB_PARTY: partyCards(st, CONTENT_Y, CONTENT_H, true); break;
	case TAB_PSYNERGY: psynergyTab(st, v); break;
	case TAB_DJINN: djinnTab(st, v); break;
	case TAB_MAP: mapTab(st, v); break;
	case TAB_ITEMS: itemsTab(st, v); break;
	case TAB_MENU: menuTab(v); break;
	case TAB_BATTLE: battlePanel(st, v); break;
	}
	footer(st);
	if (v->toastFrames > 0) {
		int w = gTextW(v->toast, 2) + 24;
		gPanel((UI_W - w) / 2, 100, w, 28, C_PANEL_T, C_PANEL_B, C_GOLD);
		gTextCentered(UI_W / 2, 108, v->toast, 2, C_GOLD);
	}
}
