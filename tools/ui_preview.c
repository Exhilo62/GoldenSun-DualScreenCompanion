// Host preview: renders the bottom UI from a RAM dump (probe.exe "dump" format) to PPM files.
// usage: ui_preview dump.bin out_prefix [battle] [replay.bin]
//   replay.bin = repeated records: int32 camX, camY, playerX, playerY, areaGroup, sprX, sprY (-1 = none), then 240*160 RGB565 pixels.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/gs_ram.h"
#include "../src/gs_state.h"
#include "../src/mapview.h"
#include "../src/ui.h"
#include "../src/bestiary.h"
#include "../src/dialoglog.h"

static int forceBattle;
static unsigned char ram[0x8000 + 0x40000];

static uint32_t rd(void* ctx, uint32_t addr, int size) {
	(void) ctx;
	if (addr == 0x04000000) return forceBattle ? 1 : 0;  // DISPCNT
	if (!((addr >= 0x02000000 && addr < 0x02040000) || (addr >= 0x03000000 && addr < 0x03008000))) return 0;
	uint32_t off = (addr >= 0x02000000 && addr < 0x02040000) ? 0x8000 + (addr - 0x02000000) : addr - 0x03000000;
	uint32_t v = 0;
	for (int i = 0; i < size; ++i) v |= (uint32_t) ram[off + i] << (8 * i);
	return v;
}

static void writePpm(const char* path, const uint16_t* fb) {
	FILE* f = fopen(path, "wb");
	fprintf(f, "P6\n%d %d\n255\n", UI_W, UI_H);
	for (int y = 0; y < UI_H; ++y) {
		for (int x = 0; x < UI_W; ++x) {
			uint16_t c = fb[x * UI_H + (UI_H - 1 - y)];
			fputc(((c >> 11) & 31) * 255 / 31, f);
			fputc(((c >> 5) & 63) * 255 / 63, f);
			fputc((c & 31) * 255 / 31, f);
		}
	}
	fclose(f);
}

int main(int argc, char** argv) {
	if (argc < 3) return 1;
	FILE* f = fopen(argv[1], "rb");
	if (!f || fread(ram, 1, sizeof ram, f) != sizeof ram) return 2;
	fclose(f);
	static uint16_t fb[UI_W * UI_H];
	if (getenv("GS_TLA")) { gsSetRam(&gsRamTla); gsSetGame("Golden Sun: The Lost Age", true); }   // The Lost Age address table
	GsState st;
	forceBattle = argc > 3 && !strcmp(argv[3], "battle");
	gsReadState(&st, rd, NULL);
	for (int i = 0; i < GS_PARTY_MAX; ++i)   // the dump has no ROM, so the EXP table is unavailable: fake it so the layout can be checked
		if (st.party[i].present && !st.party[i].expNext) { st.party[i].expLevelStart = st.party[i].exp - 30000; st.party[i].expNext = 94000; }
	mapInit();
	const char* replay = argc > 4 ? argv[4] : (argc > 3 && strcmp(argv[3], "battle") ? argv[3] : NULL);
	if (replay) {
		FILE* r = fopen(replay, "rb");
		static uint16_t frame[240 * 160];
		int rec[7], n = 0;
		while (r && fread(rec, sizeof rec, 1, r) == 1 && fread(frame, sizeof frame, 1, r) == 1) {
			GsState s = st;
			s.camX = rec[0]; s.camY = rec[1]; s.playerX = rec[2]; s.playerY = rec[3]; s.areaGroup = rec[4];
			s.areaKey = (uint32_t) rec[4] << 16;
			s.hasSprite = rec[5] >= 0; s.sprX = rec[5]; s.sprY = rec[6];
			s.mode = GS_MODE_FIELD;
			mapUpdate(frame, 240, &s);
			st.camX = s.camX; st.camY = s.camY; st.playerX = s.playerX; st.playerY = s.playerY; st.areaGroup = s.areaGroup;
			++n;
		}
		if (r) fclose(r);
		printf("replayed %d frames\n", n);
	}
	UiView v;
	uiInit(&v);
	for (int tab = 0; tab < 7; ++tab) {
		v.tab = tab;
		v.toastFrames = tab == 5 ? 30 : 0;
		if (tab == 5) { v.fastForward = true; strcpy(v.toast, "State saved"); }
		if (tab == 6 && forceBattle && st.battleSel < 0) { v.actHero = 2; v.actFrames = 50; }
		if (tab == 6 && getenv("LASTCMD")) v.lastCmd = atoi(getenv("LASTCMD"));
		uiDraw(fb, &st, &v);
		char path[512];
		snprintf(path, sizeof path, "%s%d.ppm", argv[2], tab);
		writePpm(path, fb);
	}
	{   // message log: fill it from a synthetic ring and render it
		static uint16_t ring[DLOG_RING_WORDS];
		const char* msgs[] = {"Welcome to Vale! The Elder is waiting for you at the Sanctum. Please hurry.", "It's a huge rock that Isaac's strength can't even budge.", "Garet: Isaac, we have to find another way up the mountain."};
		int w = 0;
		for (int m = 0; m < 3; ++m) {
			for (const char* c = msgs[m]; *c; ++c) ring[w++] = (uint16_t) *c;
			ring[w++] = 2; ring[w++] = 0;
			dlogPoll(ring, w);
			ring[0] = 0;   // the message leaves the ring before the next one arrives
			for (int i = 0; i < w; ++i) ring[i] = 0;
			w = 0;
			dlogPoll(ring, DLOG_RING_WORDS);
		}
		UiView lv = v;
		lv.tab = 5; lv.showLog = true; lv.logScroll = 0;
		uiDraw(fb, &st, &lv);
		char path[512];
		snprintf(path, sizeof path, "%slog.ppm", argv[2]);
		writePpm(path, fb);
		printf("log entries -> %d (want 3)\n", dlogCount());
		lv.showLog = false; lv.showSlots = true; lv.stateSlot = 1;
		uiDraw(fb, &st, &lv);
		snprintf(path, sizeof path, "%sslots.ppm", argv[2]);
		writePpm(path, fb);
	}
	// Touch tests: tab selection, map pan, map zoom button.
	UiOutput o;
	v.tab = 0;
	uiUpdate(&v, &st, true, 70, 20, &o);
	printf("tap tab 1 -> tab=%d (want 1)\n", v.tab);
	uiUpdate(&v, &st, false, 0, 0, &o);
	v.tab = 3;
	uiUpdate(&v, &st, true, 100, 100, &o);
	uiUpdate(&v, &st, true, 80, 90, &o);
	printf("drag map -> pan=(%d,%d) (want 20,10)\n", v.panX, v.panY);
	uiUpdate(&v, &st, false, 0, 0, &o);
	uiUpdate(&v, &st, true, 220, 190, &o);
	printf("zoom btn -> zoom=%d (want 2)\n", v.mapZoom);
	v.tab = 3; v.panX = 12; v.panY = -7;
	uiUpdate(&v, &st, false, 0, 0, &o);
	uiUpdate(&v, &st, true, 80, 190, &o);
	printf("center btn -> pan=(%d,%d) (want 0,0)\n", v.panX, v.panY);
	// Menu-on-bottom-screen touch paths.
	v.mirror = false; v.hideMenus = true; v.tab = 0;
	uiUpdate(&v, &st, false, 0, 0, &o);
	uiUpdate(&v, &st, true, 4 + 2 * 79 + 20, 120, &o);
	printf("party card tap opens the character screen -> showChar=%d charIdx=%d (want showChar=1 charIdx=2)\n", v.showChar, v.charIdx);
	{   // render the character screen
		uiDraw(fb, &st, &v);
		char path[512];
		snprintf(path, sizeof path, "%schar.ppm", argv[2]);
		writePpm(path, fb);
	}
	uiUpdate(&v, &st, false, 0, 0, &o);
	uiUpdate(&v, &st, true, 234, 50 + 12, &o);
	printf("character screen Game button -> openMenu=%d openChar=%d (want openMenu=%d openChar=2)\n", o.openMenu, o.openChar, MENU_STATUS);
	uiUpdate(&v, &st, false, 0, 0, &o);
	v.tab = 2;
	uiUpdate(&v, &st, true, 4 + 2 * 53 + 2 * 3 + 20, 20, &o);
	printf("active Djinn tab tap opens Djinn -> openMenu=%d (want %d)\n", o.openMenu, MENU_DJINN);
	uiUpdate(&v, &st, false, 0, 0, &o);
	v.tab = 1;
	uiUpdate(&v, &st, true, 8 + 76 + 30, 50 + 12, &o);
	uiUpdate(&v, &st, false, 0, 0, &o);
	uiUpdate(&v, &st, true, 8 + 76 + 30, 50 + 12, &o);
	printf("psynergy row tap opens that member -> openChar=%d (want 1)\n", o.openChar);
	uiUpdate(&v, &st, false, 0, 0, &o);
	v.hideMenus = false; v.tab = 0;
	uiUpdate(&v, &st, true, 4 + 79 + 20, 120, &o);
	printf("card tap does nothing with the option off -> openMenu=%d (want %d)\n", o.openMenu, MENU_NONE);
	uiUpdate(&v, &st, false, 0, 0, &o);
	v.hideMenus = true; v.mirror = true;
	uiUpdate(&v, &st, true, 120, 64, &o);
	printf("mirror frame tap -> menuTap=%d tapY=%d (want 1 tapY=60)\n", o.menuTap, o.tapY);
	uiUpdate(&v, &st, false, 0, 0, &o);
	uiUpdate(&v, &st, true, 280, 60, &o);
	printf("mirror R button -> keys=%u (want %u)\n", o.keys, (unsigned) UK_R);
	uiUpdate(&v, &st, false, 0, 0, &o);
	uiUpdate(&v, &st, true, 280, 188, &o);
	printf("mirror CLOSE -> closeMenu=%d (want 1)\n", o.closeMenu);
	// Menu tab buttons (two columns of five, Fast forward / FF speed hidden): autosave toggles, Save slots opens the cards, Message log opens the log.
	v.mirror = false; v.tab = 5; v.showChar = false;
	uiUpdate(&v, &st, false, 0, 0, &o);
	uiUpdate(&v, &st, true, 50, 56 + 1 * 30 + 10, &o);
	printf("menu row Autosave -> auto=%d (want 0)\n", v.autoSave);
	uiUpdate(&v, &st, false, 0, 0, &o);
	uiUpdate(&v, &st, true, 50, 56 + 0 * 30 + 10, &o);
	printf("menu row Save slots -> showSlots=%d (want 1)\n", v.showSlots);
	uiUpdate(&v, &st, false, 0, 0, &o);
	uiUpdate(&v, &st, true, 6 + 104 + 26, 50 + 30 + 100, &o);   // slot 2 card, Save button
	printf("slot 2 Save -> saveState=%d slot=%d (want 1 slot=1)\n", o.saveState, v.stateSlot);
	uiUpdate(&v, &st, false, 0, 0, &o);
	uiUpdate(&v, &st, true, 6 + 208 + 74, 50 + 30 + 100, &o);   // slot 3 card, Load button
	printf("slot 3 Load -> loadState=%d slot=%d (want 1 slot=2)\n", o.loadState, v.stateSlot);
	uiUpdate(&v, &st, false, 0, 0, &o);
	uiUpdate(&v, &st, true, 280, 50 + 12, &o);
	printf("slots Back -> showSlots=%d (want 0)\n", v.showSlots);
	uiUpdate(&v, &st, false, 0, 0, &o);
	uiUpdate(&v, &st, true, 220, 56 + 0 * 30 + 10, &o);
	printf("menu row Message log -> showLog=%d (want 1)\n", v.showLog);
	uiUpdate(&v, &st, false, 0, 0, &o);
	uiUpdate(&v, &st, true, 160, 120, &o);
	uiUpdate(&v, &st, true, 160, 100, &o);
	uiUpdate(&v, &st, false, 0, 0, &o);
	printf("log drag -> scroll=%d (want 0)\n", v.logScroll);
	uiUpdate(&v, &st, true, 280, 50 + 12, &o);
	printf("log Back -> showLog=%d (want 0)\n", v.showLog);
	uiUpdate(&v, &st, false, 0, 0, &o);
	{   // Bestiary: three fake enemies, open from the Menu tab, scroll is clamped, Back leaves
		GsState bs; memset(&bs, 0, sizeof(bs)); bs.mode = GS_MODE_BATTLE; bs.enemyCount = 3;
		const char* nm[3] = {"Rat", "Bat", "Wolf"};
		for (int i = 0; i < 3; ++i) { bs.enemies[i].present = true; snprintf(bs.enemies[i].name, sizeof(bs.enemies[i].name), "%s", nm[i]); bs.enemies[i].level = (uint8_t) (3 + i); bs.enemies[i].maxHp = (uint16_t) (20 + 10 * i); bs.enemies[i].atk = 9; bs.enemies[i].def = 5; bs.enemies[i].agi = 11; }
		bestReset(); bestObserve(&bs);
	}
	uiUpdate(&v, &st, true, 220, 56 + 1 * 30 + 10, &o);
	printf("menu row Bestiary -> showBest=%d (want 1)\n", v.showBest);
	uiUpdate(&v, &st, false, 0, 0, &o);
	{ uiDraw(fb, &st, &v); char bp[512]; snprintf(bp, sizeof bp, "%sbest.ppm", argv[2]); writePpm(bp, fb); }
	uiUpdate(&v, &st, true, 160, 120, &o);
	uiUpdate(&v, &st, true, 160, 40, &o);
	uiUpdate(&v, &st, false, 0, 0, &o);
	printf("bestiary drag clamps -> scroll=%d (want 0)\n", v.bestScroll);
	uiUpdate(&v, &st, true, 280, 50 + 12, &o);
	printf("bestiary Back -> showBest=%d (want 0)\n", v.showBest);
	uiUpdate(&v, &st, false, 0, 0, &o);
	uiUpdate(&v, &st, true, 220, 56 + 2 * 30 + 10, &o);
	printf("menu row Rewind -> rewind=%d (want 1)\n", o.rewind);
	// Fast forward and FF speed are hidden: the first row is Save slots, and nothing changes the speed settings; Mute if slow still toggles.
	v.tab = 5; v.showSlots = false; v.showLog = false;
	{ int mul = v.speedMul; bool ff = v.fastForward;
	  uiUpdate(&v, &st, false, 0, 0, &o);
	  uiUpdate(&v, &st, true, 50, 56 + 0 * 30 + 10, &o);   // first row (Save slots opens the cards; it must not touch the speed)
	  v.showSlots = false;
	  printf("menu has no speed buttons -> unchanged=%d (want 1)\n", v.speedMul == mul && v.fastForward == ff); }
	uiUpdate(&v, &st, false, 0, 0, &o);
	uiUpdate(&v, &st, true, 50, 56 + 2 * 30 + 10, &o);
	printf("menu row Mute if slow -> muteSlow=%d (want 1)\n", v.muteSlow);
	// Password keyboard: Return key and a letter key.
	v.showKeys = true; v.showChar = false; v.pickActive = false;
	uiUpdate(&v, &st, false, 0, 0, &o);
	uiUpdate(&v, &st, true, 6 + 16 * 16 + 6 + 20, 50 + 32 + 24 + 10, &o);
	printf("keyboard Return -> kbKey=%d (want %d)\n", o.kbKey, UKEY_RETURN);
	uiUpdate(&v, &st, false, 0, 0, &o);
	uiUpdate(&v, &st, true, 6 + 3 * 16 + 8, 50 + 32 + 2 * 24 + 10, &o);
	printf("keyboard letter -> kbKey=%d (want 35)\n", o.kbKey);
	{ uiDraw(fb, &st, &v); char path[512]; snprintf(path, sizeof path, "%skeys.ppm", argv[2]); writePpm(path, fb); }
	v.showKeys = false;
	{   // The password keyboard must be drawn even with no game loaded (that is the password screen): it used to be hidden by the "waiting" panel.
		static uint16_t fbA[UI_W * UI_H], fbB[UI_W * UI_H];
		GsState empty; memset(&empty, 0, sizeof(empty));
		UiView kv; uiInit(&kv); kv.tab = 0;
		uiDraw(fbA, &empty, &kv);
		kv.showKeys = true;
		uiDraw(fbB, &empty, &kv);
		printf("password keyboard drawn with no game -> differs=%d (want 1)\n", memcmp(fbA, fbB, sizeof(fbA)) != 0);
	}
	// Touch actions: tap a castable Psynergy (Cure is the first of Isaac's list), a Djinn, and the target picker.
	v.tab = 1; v.psyChar = 0; v.hideMenus = false; v.mirror = false;
	uiUpdate(&v, &st, false, 0, 0, &o);
	uiUpdate(&v, &st, true, 120, 50 + 42 + 5, &o);   // first row of the second column
	printf("psynergy tap -> psyIndex=%d psyId=%d (want psyIndex=0 psyId=90)\n", o.psyIndex, o.psyId);
	v.tab = 2;
	uiUpdate(&v, &st, false, 0, 0, &o);
	uiUpdate(&v, &st, true, 4 + 72 + 10, 50 + 18 + 4, &o);   // Venus, 5th Djinn (Sap): row 4 in the compact list
	uiUpdate(&v, &st, false, 0, 0, &o);   // a short tap acts on release
	printf("djinn tap -> elem=%d row=%d bit=%d (want elem=0 row=4 bit=4)\n", o.djinnElem, o.djinnRow, o.djinnBit);
	uiUpdate(&v, &st, false, 0, 0, &o);
	uiUpdate(&v, &st, true, 4 + 72 + 10, 50 + 18 + 4, &o);   // long press on the same Djinn: details, no flip
	for (int i = 0; i < 30; ++i) uiUpdate(&v, &st, true, 4 + 72 + 10, 50 + 18 + 4, &o);
	uiUpdate(&v, &st, false, 0, 0, &o);
	printf("djinn long press -> info=%d bit=%d flip=%d (want info=1 bit=4 flip=0)\n", v.djInfoFrames > 0, v.djInfoK, o.djinnElem >= 0);
	{ uiDraw(fb, &st, &v); char dp[512]; snprintf(dp, sizeof dp, "%sdjinninfo.ppm", argv[2]); writePpm(dp, fb); }
	v.djInfoFrames = 0;
	v.pickActive = true; snprintf(v.pickLabel, sizeof(v.pickLabel), "Herb");
	uiUpdate(&v, &st, false, 0, 0, &o);
	uiUpdate(&v, &st, true, 4 + 79 + 20, 120, &o);
	printf("picker tap -> pickTarget=%d (want 1)\n", o.pickTarget);
	uiDraw(fb, &st, &v);
	{ char path[512]; snprintf(path, sizeof path, "%spick.ppm", argv[2]); writePpm(path, fb); }
	uiUpdate(&v, &st, false, 0, 0, &o);
	uiUpdate(&v, &st, true, 281, 50 + 12, &o);
	printf("picker Cancel -> pickTarget=%d (want -2)\n", o.pickTarget);
	return 0;
}
