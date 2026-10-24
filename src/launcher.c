// Start-up screens (game picker and error messages) drawn with the same panels, fonts and art as the companion UI.
#include <3ds.h>
#include <stdio.h>
#include <string.h>

#include "art.h"
#include "gfx.h"
#include "gs_game.h"
#include "launcher.h"
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

static uint16_t topCanvas[GFX_W * GFX_H];

static void beginScene(uint16_t* fb) {
	gBegin(fb);
	gSetBackgroundImage(artBackground());
	gCornerSprite = artCorner();
	gBackground(L_BG_TOP, L_BG_BOT);
}

// The top screen is 400 wide: the 320-wide canvas goes in the middle and the 40 columns on each side mirror the canvas edge,
// which continues the cloud texture without visible streaks.
static void copySides(void) {
	uint16_t* fb = (uint16_t*) gfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL);
	for (int x = 0; x < 40; ++x) {
		memcpy(fb + x * GFX_H, topCanvas + (39 - x) * GFX_H, GFX_H * sizeof(uint16_t));
		memcpy(fb + (360 + x) * GFX_H, topCanvas + (GFX_W - 1 - x) * GFX_H, GFX_H * sizeof(uint16_t));
	}
}

static void presentTop(void) {
	uint16_t* fb = (uint16_t*) gfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL);
	memcpy(fb + 40 * GFX_H, topCanvas, sizeof(topCanvas));
}

static void titleBanner(int y, const char* sub) {
	const Sprite* logo = artLogo();
	gPanel(50, y, 220, logo->h + 62, L_CARD_T, L_CARD_B, L_GOLD);
	gSprite(160 - logo->w / 2, y + 10, logo);
	const char* tag = "Dual Screen Companion";
	int pw = gTextW(tag, 1) + 30;
	gPill(160 - pw / 2, y + logo->h + 18, pw, 16, L_PANEL_T, L_PANEL_B, L_GOLD);
	gTextCentered(160, y + logo->h + 22, tag, 1, L_TEXT);
	if (sub) gTextCentered(160, y + logo->h + 40, sub, 0, L_DIM);
	gTextCentered(160, 208, "v" APP_VERSION " " APP_STAGE, 1, L_DIM);   // version on the pick screen
	gTextCentered(160, 222, "build " __DATE__ " " __TIME__, 1, L_DIM);   // tells two installs of the same version apart
}

static void drawTop(const char* sub) {
	beginScene(topCanvas);
	copySides();   // sides come from the bare background, before any frame is drawn on it
	titleBanner(60, sub);
	presentTop();
}

static void present(void) {
	gfxFlushBuffers();
	gfxSwapBuffers();
	gspWaitForVBlank();
}

// Draws one top and one bottom frame into both framebuffer halves (the 3DS swaps buffers, so draw twice).
static void paintGameCard(int x, int y, int w, int h, bool sel, bool last, bool installed, int game) {
	const Sprite* face = artCardFace(game);
	const Sprite* logo = artCardLogo(game);
	int fx = x + 7, fy = y + (h - face->h) / 2;
	int lx = x + 196 - logo->w / 2, ly = y + 3;   // banner right of the portrait, clear of the LAST PLAYED tag below it
	if (!installed) {   // not installed: dim, no highlight, and a hint on how to add it
		gPanel(x, y, w, h, RGB(16, 24, 48), RGB(8, 12, 28), RGB(70, 76, 100));
		gSpriteTint(fx, fy, face, RGB(10, 16, 36), 190);
		gSpriteTint(lx, ly, logo, RGB(10, 16, 36), 190);
		gText(x + 76, y + 62, "Not installed - add its ROM with the patcher", 0, RGB(120, 128, 160));
		return;
	}
	gPanel(x, y, w, h, sel ? L_ACTIVE_T : L_PANEL_T, sel ? L_ACTIVE_B : L_PANEL_B, sel ? L_GOLD : L_GOLD_DIM);
	gSprite(fx, fy, face);
	gSprite(lx, ly, logo);
	if (last) {
		gPill(x + w - 74, y + 58, 68, 14, L_CARD_T, L_CARD_B, L_GOLD_DIM);
		gTextCentered(x + w - 40, y + 61, "LAST PLAYED", 0, L_GOLD);
	}
}

static void drawBottom(int sel, int last, const bool installed[2]) {
	uint16_t* fb = (uint16_t*) gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, NULL, NULL);
	beginScene(fb);
	gTextCentered(160, 10, "Choose your adventure", 1, L_TEXT);
	for (int i = 0; i < 2; ++i) paintGameCard(10, 30 + i * 84, 300, 74, sel == i, last == i, installed[i], i);
	gPanel(10, 200, 300, 32, L_CARD_T, L_CARD_B, L_GOLD_DIM);
	gTextCentered(160, 207, "Tap a game, or Up/Down and A", 1, L_TEXT);
	gTextCentered(160, 220, "START: exit", 0, L_DIM);
}

int launcherPick(int last, const bool installed[2]) {
	gSetBackgroundImage(artBackground());
	const GsGameInfo* g1 = gsGameInfo(GS_GAME_1);
	const GsGameInfo* g2 = gsGameInfo(GS_GAME_2);
	int sel = (last == 1 && installed[1]) || !installed[0] ? 1 : 0, pick = -1, painted = 0;
	bool redraw = true;
	while (aptMainLoop() && pick < 0) {
		hidScanInput();
		u32 d = hidKeysDown();
		if ((d & (KEY_DUP | KEY_DDOWN | KEY_CPAD_UP | KEY_CPAD_DOWN)) && installed[sel ^ 1]) { sel ^= 1; redraw = true; }   // only onto a game that is installed
		if (d & KEY_A) pick = sel;
		if (d & KEY_START) return -1;
		if (d & KEY_TOUCH) {
			touchPosition t;
			hidTouchRead(&t);
			for (int i = 0; i < 2; ++i) {
				int y = 30 + i * 84;
				if (installed[i] && t.px >= 10 && t.px < 310 && t.py >= y && t.py < y + 74) { sel = i; pick = i; }
			}
		}
		if (redraw) { redraw = false; painted = 2; }
		if (painted > 0) {
			--painted;
			drawTop(sel == 0 ? g1->name : g2->name);
			drawBottom(sel, last, installed);
		}
		present();
	}
	return pick;
}

void launcherMessage(const char* msg) {
	char buf[320];
	snprintf(buf, sizeof(buf), "%s", msg);
	for (int frame = 0; frame < 2; ++frame) {
		drawTop("Something needs attention");
		uint16_t* fb = (uint16_t*) gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, NULL, NULL);
		beginScene(fb);
		gPanel(10, 24, 300, 160, L_CARD_T, L_CARD_B, RGB(255, 120, 90));
		gTextCentered(160, 34, "Cannot start", 2, RGB(255, 170, 150));
		int y = 62;
		for (char* line = strtok(buf, "\n"); line && y < 170; line = strtok(NULL, "\n")) {
			gTextCentered(160, y, line, 1, L_TEXT);
			y += 14;
		}
		gPanel(10, 196, 300, 30, L_PANEL_T, L_PANEL_B, L_GOLD);
		gTextCentered(160, 205, "Press START to exit", 1, L_GOLD);
		present();
		snprintf(buf, sizeof(buf), "%s", msg);   // strtok consumed the buffer
	}
	while (aptMainLoop()) {
		hidScanInput();
		if (hidKeysDown() & KEY_START) break;
		gspWaitForVBlank();
	}
}
