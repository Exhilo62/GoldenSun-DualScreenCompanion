#include <stdio.h>
#include <string.h>

#include "art.h"
#include "gfx.h"
#include "gs_game.h"
#include "pickscreen.h"
#include "version.h"

#define L_BG_TOP   RGB(6, 12, 32)
#define L_BG_BOT   RGB(12, 24, 56)
#define L_PANEL_T  RGB(28, 50, 108)
#define L_PANEL_B  RGB(12, 26, 66)
#define L_CARD_T   RGB(20, 38, 86)
#define L_CARD_B   RGB(8, 18, 48)
#define L_GOLD     RGB(236, 188, 68)
#define L_GOLD_DIM RGB(150, 112, 48)
#define L_TEXT     RGB(244, 246, 255)
#define L_DIM      RGB(128, 140, 176)
#define L_ACTIVE_T RGB(86, 74, 40)
#define L_ACTIVE_B RGB(40, 32, 18)

#define CARD_X 10
#define CARD_W 300
#define CARD_H 74
#define CARD_Y(i) (30 + (i) * 84)
#define ADD_Y 200
#define ADD_H 32

static void beginScene(uint16_t* fb) {
	gBegin(fb);
	gSetBackgroundImage(artBackground());
	gCornerSprite = artCorner();
	gBackground(L_BG_TOP, L_BG_BOT);
}

void pickDrawTop(uint16_t* canvas, const char* sub) {
	beginScene(canvas);
	const Sprite* logo = artLogo();
	int y = 60;
	gPanel(50, y, 220, logo->h + 62, L_CARD_T, L_CARD_B, L_GOLD);
	gSprite(160 - logo->w / 2, y + 10, logo);
	const char* tag = "Dual Screen Companion";
	int pw = gTextW(tag, 1) + 30;
	gPill(160 - pw / 2, y + logo->h + 18, pw, 16, L_PANEL_T, L_PANEL_B, L_GOLD);
	gTextCentered(160, y + logo->h + 22, tag, 1, L_TEXT);
	if (sub) gTextCentered(160, y + logo->h + 40, sub, 0, L_DIM);
	gTextCentered(160, 208, "v" APP_VERSION " " APP_STAGE, 1, L_DIM);
}

static void gameCard(int i, bool sel, bool last, bool installed, const char* line1, const char* line2, const char* who, const char* portrait) {
	int x = CARD_X, y = CARD_Y(i), w = CARD_W, h = CARD_H;
	if (!installed) {   // not installed: dim; tapping it opens the ROM picker
		gPanel(x, y, w, h, sel ? RGB(30, 36, 64) : RGB(16, 24, 48), sel ? RGB(14, 18, 36) : RGB(8, 12, 28), sel ? L_GOLD_DIM : RGB(70, 76, 100));
		gSpriteTint(x + 8, y + (h - PORTRAIT_H) / 2, artPortrait(portrait), RGB(10, 16, 36), 190);
		gTextShadow(x + 84, y + 12, line1, 2, RGB(96, 104, 134));
		if (line2[0]) gTextShadow(x + 84, y + 32, line2, 1, RGB(96, 104, 134));
		gText(x + 84, y + 48, "No ROM yet - tap to add one", 0, RGB(120, 128, 160));
		return;
	}
	gPanel(x, y, w, h, sel ? L_ACTIVE_T : L_PANEL_T, sel ? L_ACTIVE_B : L_PANEL_B, sel ? L_GOLD : L_GOLD_DIM);
	gSprite(x + 8, y + (h - PORTRAIT_H) / 2, artPortrait(portrait));
	gTextShadow(x + 84, y + 12, line1, 2, sel ? L_GOLD : L_TEXT);
	if (line2[0]) gTextShadow(x + 84, y + 32, line2, 1, L_TEXT);
	gText(x + 84, y + 48, who, 0, L_DIM);
	if (last) {
		gPill(x + w - 74, y + 6, 68, 14, L_CARD_T, L_CARD_B, L_GOLD_DIM);
		gTextCentered(x + w - 40, y + 9, "LAST PLAYED", 0, L_GOLD);
	}
}

void pickDrawBottom(uint16_t* canvas, int sel, int last, const bool installed[2]) {
	beginScene(canvas);
	const GsGameInfo* g1 = gsGameInfo(GS_GAME_1);
	const GsGameInfo* g2 = gsGameInfo(GS_GAME_2);
	static char l2b[40];
	const char* colon = strstr(g2->name, ": ");
	snprintf(l2b, sizeof(l2b), "%s", colon ? colon + 2 : g2->name);
	gTextCentered(160, 10, "Choose your adventure", 1, L_TEXT);
	gameCard(0, sel == PICK_GAME1, last == 0, installed[0], g1->name, "", "Isaac, Garet, Ivan and Mia", "Isaac");
	gameCard(1, sel == PICK_GAME2, last == 1, installed[1], "Golden Sun", l2b, "Felix, Jenna, Sheba and Piers", "Felix");
	bool addSel = sel == PICK_ADD;
	gPanel(CARD_X, ADD_Y, CARD_W, ADD_H, addSel ? L_ACTIVE_T : L_CARD_T, addSel ? L_ACTIVE_B : L_CARD_B, addSel ? L_GOLD : L_GOLD_DIM);
	gTextCentered(160, ADD_Y + 6, "Add ROM file", 1, addSel ? L_GOLD : L_TEXT);
	gTextCentered(160, ADD_Y + 20, "Your ROMs stay on this device", 0, L_DIM);
}

int pickHit(int x, int y) {
	for (int i = 0; i < 2; ++i)
		if (x >= CARD_X && x < CARD_X + CARD_W && y >= CARD_Y(i) && y < CARD_Y(i) + CARD_H) return i == 0 ? PICK_GAME1 : PICK_GAME2;
	if (x >= CARD_X && x < CARD_X + CARD_W && y >= ADD_Y && y < ADD_Y + ADD_H) return PICK_ADD;
	return PICK_NONE;
}
