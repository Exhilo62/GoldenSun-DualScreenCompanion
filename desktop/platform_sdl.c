// SDL2 platform layer (Windows now, Android next): one window with the GBA picture on top and the companion UI below, mouse / finger as the touch
// screen, keyboard as the buttons. Jobs run inline. The game itself is in src/app.c.
// Test helpers: GS_SHOT=file.bmp GS_FRAMES=n saves a picture of the window after n frames and quits; GS_GAME=1|2 picks the game.
#include <SDL.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <mgba/core/core.h>
#include <mgba-util/audio-buffer.h>
#include <mgba/core/interface.h>
#include <mgba/core/log.h>
#include <mgba/internal/gba/input.h>

#include "../src/app.h"
#include "../src/gfx.h"
#include "../src/gs_game.h"
#include "../src/platform.h"
#include "../src/ui.h"

#define WIN_W 640
#define WIN_H 800
#define AUDIO_SAMPLES 512
static void quietLog(struct mLogger* l, int category, enum mLogLevel level, const char* format, va_list args) { (void) l; (void) category; (void) level; (void) format; (void) args; }
static struct mLogger quietLogger = {quietLog};
static const SDL_Rect topDst = {80, 0, 480, 320}, botDst = {0, 320, 640, 480};

static SDL_Window* win;
static SDL_Renderer* ren;
static SDL_Texture *topTex, *botTex;
static bool quitRequested;
static const char* shotPath;
static int shotFrames, frameNo;
static uint16_t privBot[GFX_W * GFX_H];   // the 3DS framebuffer layout: column-major, flipped
static uint16_t rows[GFX_W * GFX_H];
static bool botDirty;
static SDL_AudioDeviceID audioDev;
static volatile bool audioMute;

uint64_t plTicks(void) { return SDL_GetPerformanceCounter() * 1000000ull / SDL_GetPerformanceFrequency(); }   // microseconds

// ---- audio ----------------------------------------------------------------------------------------------------------------
static void postAudioBuffer(struct mAVStream* stream, struct mAudioBuffer* buffer) {
	(void) stream;
	if (!audioDev || audioMute) { mAudioBufferClear(buffer); return; }
	static int16_t tmp[AUDIO_SAMPLES * 2];
	int n = (int) mAudioBufferAvailable(buffer);
	while (n > 0) {
		int c = n < AUDIO_SAMPLES ? n : AUDIO_SAMPLES;
		mAudioBufferRead(buffer, tmp, (size_t) c);
		if (SDL_GetQueuedAudioSize(audioDev) < 32768u * 4 * 12 / 100) SDL_QueueAudio(audioDev, tmp, (Uint32) c * 4);   // keep under ~120 ms queued
		n -= c;
	}
}

static void audioRateChanged(struct mAVStream* stream, unsigned rate) { (void) stream; (void) rate; }

void plAudioInit(struct mCore* core) {
	core->setAudioBufferSize(core, AUDIO_SAMPLES);
	if (SDL_InitSubSystem(SDL_INIT_AUDIO) == 0) {
		SDL_AudioSpec want = {0}, got;
		want.freq = 32768; want.format = AUDIO_S16SYS; want.channels = 2; want.samples = 1024;
		audioDev = SDL_OpenAudioDevice(NULL, 0, &want, &got, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
		if (audioDev) SDL_PauseAudioDevice(audioDev, 0);
	}
	static struct mAVStream stream;
	stream.postAudioBuffer = postAudioBuffer;
	stream.audioRateChanged = audioRateChanged;
	core->setAVStream(core, &stream);
}
void plAudioMute(bool mute) { audioMute = mute; }
bool plHasSound(void) { return audioDev != 0; }
bool plAudioSkipped(void) { return false; }

// ---- input ----------------------------------------------------------------------------------------------------------------
void plPollInput(PlInput* in) {
	SDL_Event e;
	while (SDL_PollEvent(&e)) if (e.type == SDL_QUIT) quitRequested = true;
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
	in->keys = keys;
	in->turbo = k[SDL_SCANCODE_TAB] != 0;
	int mx, my;
	Uint32 mb = SDL_GetMouseState(&mx, &my);
	float lx, ly;
	SDL_RenderWindowToLogical(ren, mx, my, &lx, &ly);
	in->tx = (int) ((lx - botDst.x) * GFX_W / botDst.w);
	in->ty = (int) ((ly - botDst.y) * GFX_H / botDst.h);
	in->touch = (mb & SDL_BUTTON_LMASK) && in->tx >= 0 && in->tx < GFX_W && in->ty >= 0 && in->ty < GFX_H;
	static int loops;   // test helper: GS_TOUCH="x,y,firstLoop,loops[;...]" presses the touch screen without a mouse
	++loops;
	const char* sp = getenv("GS_TOUCH");   // several presses can be chained with ';'
	for (int tx, ty, t0, tn, used; sp && sscanf(sp, "%d,%d,%d,%d%n", &tx, &ty, &t0, &tn, &used) == 4; ) {
		if (loops >= t0 && loops < t0 + tn) { in->touch = true; in->tx = tx; in->ty = ty; }
		sp += used;
		if (*sp != ';') break;
		++sp;
	}
}

// ---- frame pipeline (inline) -----------------------------------------------------------------------------------------------
bool plTwoCore(void) { return false; }
bool plJobSlotFree(void) { return true; }

static void present(Job* j) {
	SDL_UpdateTexture(topTex, NULL, j->top, 256 * 2);
	if (botDirty) {
		for (int x = 0; x < GFX_W; ++x)
			for (int y = 0; y < GFX_H; ++y) rows[y * GFX_W + x] = privBot[x * GFX_H + (GFX_H - 1 - y)];
		SDL_UpdateTexture(botTex, NULL, rows, GFX_W * 2);
		botDirty = false;
	}
	SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
	SDL_RenderClear(ren);
	SDL_Rect topSrc = {0, 0, 240, 160};
	SDL_RenderCopy(ren, topTex, &topSrc, &topDst);
	SDL_RenderCopy(ren, botTex, NULL, &botDst);
	if (shotPath && ++frameNo >= shotFrames) {
		SDL_Surface* sf = SDL_CreateRGBSurfaceWithFormat(0, WIN_W, WIN_H, 32, SDL_PIXELFORMAT_ARGB8888);
		SDL_RenderReadPixels(ren, NULL, SDL_PIXELFORMAT_ARGB8888, sf->pixels, sf->pitch);
		SDL_SaveBMP(sf, shotPath);
		SDL_FreeSurface(sf);
		quitRequested = true;
	}
	SDL_RenderPresent(ren);
}

void plJobSubmit(Job* j) {
	if (appProcessJob(j, privBot)) botDirty = true;
	present(j);
}

void plJobFinish(Job* j) { appProcessJob(j, privBot); }   // the final map save

uint64_t plPace(void) {
	static uint64_t deadline;
	uint64_t now = plTicks(), slept = 0;
	if (!deadline) deadline = now;
	deadline += (uint64_t) (1000000.0 / 59.7275);
	if (now + 1000 < deadline) { SDL_Delay((Uint32) ((deadline - now) / 1000)); slept = plTicks() - now; }
	else if (now > deadline + 200000) deadline = now;
	return slept;
}

int plTakeVblankWaits(void) { return 0; }
unsigned plCpuMs(void) { return 0; }
void plPerfExtra(char* out, size_t n) { if (n) out[0] = 0; }

// ---- start-up, game choice, errors -----------------------------------------------------------------------------------------
bool plRunning(void) { return !quitRequested; }

void plExit(void) {
	if (audioDev) SDL_CloseAudioDevice(audioDev);
	SDL_Quit();
}

void plFatal(const char* msg) {
	fprintf(stderr, "%s\n", msg);
	SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Golden Sun - Dual Screen Companion", msg, win);
}

bool plChooseGame(GsGameId* out) {
	const bool have[2] = {gsGameRomExists(GS_GAME_1), gsGameRomExists(GS_GAME_2)};
	if (!have[0] && !have[1]) {
		char a[128], b[128], msg[300];
		snprintf(msg, sizeof(msg), "No ROM found. Put your ROM at:\n%s\n(Lost Age, optional:\n%s)", gsGameRomPath(GS_GAME_1, a, sizeof(a)), gsGameRomPath(GS_GAME_2, b, sizeof(b)));
		plFatal(msg);
		return false;
	}
	int pick = have[0] ? 0 : 1;
	const char* g = getenv("GS_GAME");
	if (g && (atoi(g) == 1 || atoi(g) == 2) && have[atoi(g) - 1]) pick = atoi(g) - 1;
	*out = (GsGameId) pick;
	return true;
}

bool plInit(void) {
	if (SDL_Init(SDL_INIT_VIDEO) != 0) { fprintf(stderr, "SDL: %s\n", SDL_GetError()); return false; }
	mLogSetDefaultLogger(&quietLogger);
	shotPath = getenv("GS_SHOT");
	shotFrames = getenv("GS_FRAMES") ? atoi(getenv("GS_FRAMES")) : 120;
	win = SDL_CreateWindow("Golden Sun - Dual Screen Companion", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WIN_W, WIN_H, SDL_WINDOW_RESIZABLE | (shotPath ? SDL_WINDOW_HIDDEN : 0));
	ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);   // no vsync: plPace holds the 59.73 Hz
	if (!win || !ren) { fprintf(stderr, "SDL: %s\n", SDL_GetError()); return false; }
	SDL_RenderSetLogicalSize(ren, WIN_W, WIN_H);
	topTex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING, 256, 160);
	botTex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING, GFX_W, GFX_H);
	SDL_SetTextureScaleMode(topTex, SDL_ScaleModeNearest);
	SDL_SetTextureScaleMode(botTex, SDL_ScaleModeNearest);
	gsGameInitRomfs();
	gsGameEnsureDir(GS_GAME_1);
	return true;
}

int main(int argc, char** argv) {
	(void) argc; (void) argv;
	if (!plInit()) return 1;
	int r = appRun();
	plExit();
	return r;
}
