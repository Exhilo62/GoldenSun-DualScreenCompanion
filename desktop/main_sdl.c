// Desktop spike for the Android port: the GBA picture on top and the companion UI below, in one SDL2 window.
// usage: gs_desktop rom.gba [save.sav]      keys: arrows, Z = A, X = B, A = L, S = R, Enter = Start, Backspace = Select; the mouse is the touch screen.
// Touch actions that press buttons in the game (open menu, equip, cast ...) are not wired up yet: this proves the shared core outside the 3DS.
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <mgba/core/core.h>
#include <mgba/internal/gba/gba.h>
#include <mgba/internal/gba/input.h>
#include <mgba-util/vfs.h>
#include <mgba/core/serialize.h>

#include "../src/art.h"
#include "../src/gfx.h"
#include "../src/gs_areas.h"
#include "../src/gs_game.h"
#include "../src/gs_ram.h"
#include "../src/gs_state.h"
#include "../src/mapdecode.h"
#include "../src/mapview.h"
#include "../src/ui.h"

#define T(m) do { if (getenv("GS_TRACE")) { fprintf(stderr, "%s\n", m); fflush(stderr); } } while (0)

static uint32_t memRead(void* ctx, uint32_t addr, int size) {
	struct mCore* core = ctx;
	return size == 1 ? core->busRead8(core, addr) : size == 2 ? core->busRead16(core, addr) : core->busRead32(core, addr);
}

int main(int argc, char** argv) {
	T("start");
	if (argc < 2) { fprintf(stderr, "usage: gs_desktop rom.gba [save.sav]\n"); return 1; }
	FILE* rf = fopen(argv[1], "rb");
	if (!rf) { fprintf(stderr, "cannot open %s\n", argv[1]); return 2; }
	uint8_t* rom = malloc(0x2000000);
	size_t romN = fread(rom, 1, 0x2000000, rf);
	fclose(rf);
	int game = romN > 0xB0 && memcmp(rom + 0xAC, "AGF", 3) == 0 ? 2 : 1;   // The Lost Age is AGF*, Golden Sun is AGS*

	T("romread");
	struct mCore* core = mCoreFind(argv[1]);
	T("found");
	if (!core) { fprintf(stderr, "not a GBA ROM\n"); return 3; }
	core->init(core);
	T("init");
	mCoreInitConfig(core, NULL);
	static mColor video[256 * 160];
	core->setVideoBuffer(core, video, 256);
	mCoreLoadFile(core, argv[1]);
	T("load");
	if (argc > 2) mCoreLoadSaveFile(core, argv[2], false);
	core->reset(core);
	T("reset");
	// test helpers: GS_STATE=file.state loads a save state, GS_TAB=n opens a tab, GS_SHOT=file.bmp GS_FRAMES=n saves a picture of the window and quits
	if (getenv("GS_STATE")) {
		struct VFile* vf = VFileOpen(getenv("GS_STATE"), O_RDONLY);
		if (vf) { mCoreLoadStateNamed(core, vf, SAVESTATE_SAVEDATA | SAVESTATE_RTC); vf->close(vf); }
	}
	const char* shotPath = getenv("GS_SHOT");
	int shotFrames = getenv("GS_FRAMES") ? atoi(getenv("GS_FRAMES")) : 120, frameNo = 0;

	gsSetGame(game == 2 ? "Golden Sun: The Lost Age" : "Golden Sun", true);
	gsSetRam(game == 2 ? &gsRamTla : &gsRamGs1);
	gsAreasUseBuiltin(true);
	gsAreasSelectGame(game == 2 ? 1 : 0);
	mapInit();
	T("mapInit");
	mdInit(rom, romN, game);
	T("mdInit");
	gSetBackgroundImage(artBackground());

	T("pre-sdl");
	if (SDL_Init(SDL_INIT_VIDEO) != 0) { fprintf(stderr, "SDL: %s\n", SDL_GetError()); return 4; }
	SDL_Window* win = SDL_CreateWindow("Golden Sun - Dual Screen Companion (desktop spike)", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 640, 800, SDL_WINDOW_RESIZABLE | (shotPath ? SDL_WINDOW_HIDDEN : 0));
	SDL_Renderer* ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
	SDL_RenderSetLogicalSize(ren, 640, 800);
	SDL_Texture* topTex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING, 240, 160);
	SDL_Texture* botTex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING, GFX_W, GFX_H);
	SDL_SetTextureScaleMode(topTex, SDL_ScaleModeNearest);
	SDL_SetTextureScaleMode(botTex, SDL_ScaleModeNearest);

	static GsState gs;
	static UiView ui;
	uiInit(&ui);
	if (getenv("GS_TAB")) ui.tab = atoi(getenv("GS_TAB"));
	static uint16_t fb[GFX_W * GFX_H];       // the companion UI, in the 3DS framebuffer layout (column-major, flipped)
	static uint16_t rows[GFX_W * GFX_H];     // the same, row-major, for the texture
	static uint16_t frame565[256 * 160];     // the GBA picture as RGB565, the format the map stitcher expects
	const SDL_Rect topDst = {80, 0, 480, 320}, botDst = {0, 320, 640, 480};
	bool quit = false;
	while (!quit) {
		SDL_Event e;
		while (SDL_PollEvent(&e)) if (e.type == SDL_QUIT) quit = true;
		const Uint8* k = SDL_GetKeyboardState(NULL);
		uint32_t keys = 0;
		if (k[SDL_SCANCODE_Z]) keys |= 1 << GBA_KEY_A;
		if (k[SDL_SCANCODE_X]) keys |= 1 << GBA_KEY_B;
		if (k[SDL_SCANCODE_A]) keys |= 1 << GBA_KEY_L;
		if (k[SDL_SCANCODE_S]) keys |= 1 << GBA_KEY_R;
		if (k[SDL_SCANCODE_RETURN]) keys |= 1 << GBA_KEY_START;
		if (k[SDL_SCANCODE_BACKSPACE]) keys |= 1 << GBA_KEY_SELECT;
		if (k[SDL_SCANCODE_UP]) keys |= 1 << GBA_KEY_UP;
		if (k[SDL_SCANCODE_DOWN]) keys |= 1 << GBA_KEY_DOWN;
		if (k[SDL_SCANCODE_LEFT]) keys |= 1 << GBA_KEY_LEFT;
		if (k[SDL_SCANCODE_RIGHT]) keys |= 1 << GBA_KEY_RIGHT;
		core->setKeys(core, keys);
		core->runFrame(core);
	T("ran");

		gsReadState(&gs, memRead, core);
	T("gsRead");
		gs.bldcnt = ((struct GBA*) core->board)->memory.io[0x50 >> 1];
		for (int y = 0; y < 160; ++y)
			for (int x = 0; x < 240; ++x) {
				mColor c = video[y * 256 + x];
				frame565[y * 256 + x] = (uint16_t) ((((c & 0xFF) >> 3) << 11) | ((((c >> 8) & 0xFF) >> 2) << 5) | (((c >> 16) & 0xFF) >> 3));
			}
		if (gs.mode == GS_MODE_FIELD && !gs.menuOpen) mapUpdate(frame565, 256, &gs);

		int mx, my;
		Uint32 mb = SDL_GetMouseState(&mx, &my);
		float lx, ly;
		SDL_RenderWindowToLogical(ren, mx, my, &lx, &ly);
		int tx = (int) ((lx - botDst.x) / 2), ty = (int) ((ly - botDst.y) / 2);
		bool down = (mb & SDL_BUTTON_LMASK) && tx >= 0 && tx < GFX_W && ty >= 0 && ty < GFX_H;
		UiOutput out;
		memset(&out, 0, sizeof(out));
		out.openMenu = -1; out.openChar = -1; out.equipBest = -1; out.loadApply = -1; out.loadSave = -1; out.pickTarget = -1; out.psyIndex = -1; out.djinnElem = -1; out.kbKey = -1; out.equipIndex = -1;
		uiUpdate(&ui, &gs, down, tx, ty, &out);
	T("uiUpdate");   // actions that press game buttons are the next step of the port
		uiDraw(fb, &gs, &ui);
	T("uiDraw");
		for (int x = 0; x < GFX_W; ++x)
			for (int y = 0; y < GFX_H; ++y) rows[y * GFX_W + x] = fb[x * GFX_H + (GFX_H - 1 - y)];

		SDL_UpdateTexture(topTex, NULL, video, 256 * 4);   // pitch of the 256-wide video buffer
		SDL_UpdateTexture(botTex, NULL, rows, GFX_W * 2);
		SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
		SDL_RenderClear(ren);
		SDL_Rect topSrc = {0, 0, 240, 160};
		SDL_RenderCopy(ren, topTex, &topSrc, &topDst);
		SDL_RenderCopy(ren, botTex, NULL, &botDst);
		if (shotPath && ++frameNo >= shotFrames) {   // save what was just drawn, then leave
			SDL_Surface* sf = SDL_CreateRGBSurfaceWithFormat(0, 640, 800, 32, SDL_PIXELFORMAT_ARGB8888);
			SDL_RenderReadPixels(ren, NULL, SDL_PIXELFORMAT_ARGB8888, sf->pixels, sf->pitch);
			SDL_SaveBMP(sf, shotPath);
			SDL_FreeSurface(sf);
			quit = true;
		}
		SDL_RenderPresent(ren);
	}
	core->deinit(core);
	SDL_Quit();
	return 0;
}
