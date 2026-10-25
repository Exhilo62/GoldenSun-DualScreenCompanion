// SDL2 platform layer (Windows and Android): the GBA picture and the companion UI in one window, laid out for the window's shape, with on-screen
// buttons for the GBA keys (touch), a keyboard, game controllers and the mouse. Jobs run inline. The game itself is in src/app.c.
// Test helpers: GS_SHOT=file.bmp GS_FRAMES=n saves a picture of the window after n frames and quits; GS_GAME=1|2 picks the game;
// GS_TOUCH="x,y,firstLoop,loops[;...]" presses the 320x240 bottom screen without a mouse; GS_SIZE=WxH sets the window size.
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

#ifdef __ANDROID__
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <jni.h>
#endif

#define AUDIO_SAMPLES 512

static void quietLog(struct mLogger* l, int category, enum mLogLevel level, const char* format, va_list args) { (void) l; (void) category; (void) level; (void) format; (void) args; }
static struct mLogger quietLogger = {quietLog};

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
static SDL_GameController* pads[4];

// Dual-screen devices (Android handhelds with a second display): MainActivity shows a Presentation on the other display and hands its Surface in
// here; the GBA picture is then drawn straight onto it and this window becomes the touch screen alone.
static SDL_mutex* topLock;
static bool dualScreen;
#ifdef __ANDROID__
static ANativeWindow* topWin;
JNIEXPORT void JNICALL Java_com_exhilo_goldensundualscreen_MainActivity_nativeSetTopSurface(JNIEnv* env, jclass cls, jobject surface) {
	(void) cls;
	if (!topLock) topLock = SDL_CreateMutex();
	SDL_LockMutex(topLock);
	if (topWin) { ANativeWindow_release(topWin); topWin = NULL; }
	if (surface) topWin = ANativeWindow_fromSurface(env, surface);
	dualScreen = topWin != NULL;
	SDL_UnlockMutex(topLock);
}

// Nearest-neighbour fit of the 240x160 picture (RGB565, stride 256) onto the other display, black bars where the shapes differ.
static void drawTopSurface(const uint16_t* src) {
	if (!topLock) return;
	SDL_LockMutex(topLock);
	if (topWin) {
		ANativeWindow_setBuffersGeometry(topWin, 0, 0, WINDOW_FORMAT_RGB_565);
		ANativeWindow_Buffer b;
		if (ANativeWindow_lock(topWin, &b, NULL) == 0) {
			if (b.format == WINDOW_FORMAT_RGB_565 && b.width > 0 && b.height > 0) {
				float sc = (float) b.width / 240 < (float) b.height / 160 ? (float) b.width / 240 : (float) b.height / 160;
				int dw = (int) (240 * sc), dh = (int) (160 * sc), x0 = (b.width - dw) / 2, y0 = (b.height - dh) / 2;
				static int xmap[4096];
				static int mapW = -1;
				if (dw > 4096) dw = 4096;
				if (mapW != dw) { for (int x = 0; x < dw; ++x) xmap[x] = x * 240 / dw; mapW = dw; }
				uint16_t* dst = b.bits;
				memset(dst, 0, (size_t) b.stride * b.height * 2);
				for (int y = 0; y < dh; ++y) {
					const uint16_t* row = src + (y * 160 / dh) * 256;
					uint16_t* d = dst + (size_t) (y0 + y) * b.stride + x0;
					for (int x = 0; x < dw; ++x) d[x] = row[xmap[x]];
				}
			}
			ANativeWindow_unlockAndPost(topWin);
		}
	}
	SDL_UnlockMutex(topLock);
}
#else
static void drawTopSurface(const uint16_t* src) { (void) src; }
#endif

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

// ---- layout and on-screen buttons -------------------------------------------------------------------------------------------
// Portrait: the two screens stacked at the top, the buttons in the space below. Landscape: the screens side by side in the middle, the buttons in a
// column on each side. Everything is in window pixels.
enum { CT_DPAD, CT_A, CT_B, CT_L, CT_R, CT_START, CT_SELECT, CT_FF, CT_COUNT };
typedef struct { SDL_Rect r; const char* label; } Control;
static Control ctl[CT_COUNT];
static SDL_Rect topDst, botDst;
static int outW = 640, outH = 800;
static bool ffToggle;

static SDL_Rect rc(float x, float y, float w, float h) { return (SDL_Rect) {(int) x, (int) y, (int) w, (int) h}; }

static void layout(void) {
	SDL_GetRendererOutputSize(ren, &outW, &outH);
	const int W = outW, H = outH;
	topDst = (SDL_Rect) {0, 0, 0, 0};
	if (dualScreen) {   // this display is the touch screen alone: the UI as large as it fits, no on-screen buttons (the device has its own)
		float k = (float) W / 320 < (float) H / 240 ? (float) W / 320 : (float) H / 240;
		botDst = rc((W - 320 * k) / 2, (H - 240 * k) / 2, 320 * k, 240 * k);
		for (int i = 0; i < CT_COUNT; ++i) ctl[i].r = (SDL_Rect) {-1, -1, 0, 0};
	} else if (H >= W) {   // portrait
		float k = (float) W / 320;
		if (k * 400 > H * 0.62f) k = H * 0.62f / 400;
		int sw = (int) (320 * k), x0 = (W - sw) / 2;
		topDst = rc(x0 + 40 * k, 0, 240 * k, 160 * k);
		botDst = rc(x0, 160 * k, sw, 240 * k);
		int cy = botDst.y + botDst.h, ch = H - cy;
		float u = ch / 6.0f < W / 8.0f ? ch / 6.0f : W / 8.0f;   // one button unit
		if (u < 8) u = 8;
		float mid = cy + ch / 2.0f;
		ctl[CT_DPAD].r = rc(u * 0.5f, mid - 1.5f * u, 3 * u, 3 * u);
		ctl[CT_A].r = rc(W - 1.7f * u, mid - 1.6f * u, 1.4f * u, 1.4f * u);
		ctl[CT_B].r = rc(W - 3.4f * u, mid - 0.5f * u, 1.4f * u, 1.4f * u);
		ctl[CT_L].r = rc(u * 0.5f, cy + 0.1f * u, 2 * u, 0.9f * u);
		ctl[CT_R].r = rc(W - 2.5f * u, cy + 0.1f * u, 2 * u, 0.9f * u);
		ctl[CT_START].r = rc(W / 2.0f + u / 6, cy + ch - 1.2f * u, 2.2f * u, 0.8f * u);
		ctl[CT_SELECT].r = rc(W / 2.0f - u / 6 - 2.2f * u, cy + ch - 1.2f * u, 2.2f * u, 0.8f * u);
		ctl[CT_FF].r = rc(W / 2.0f - u, cy + 0.1f * u, 2 * u, 0.9f * u);
	} else {        // landscape
		float m = W / 6.0f;
		float a = (W - 2 * m) / 560;
		if (a * 240 > H) a = (float) H / 240;
		float x0 = (W - 560 * a) / 2, y0 = (H - 240 * a) / 2;
		topDst = rc(x0, y0 + 40 * a, 240 * a, 160 * a);
		botDst = rc(x0 + 240 * a, y0, 320 * a, 240 * a);
		float u = m / 4 < H / 7.0f ? m / 4 : H / 7.0f;
		if (u < 8) u = 8;
		ctl[CT_DPAD].r = rc(m / 2 - 1.5f * u, H / 2.0f - 0.5f * u, 3 * u, 3 * u);
		ctl[CT_A].r = rc(W - m / 2 + 0.1f * u, H / 2.0f - 1.2f * u, 1.4f * u, 1.4f * u);
		ctl[CT_B].r = rc(W - m / 2 - 1.5f * u, H / 2.0f + 0.3f * u, 1.4f * u, 1.4f * u);
		ctl[CT_L].r = rc(m / 2 - u, u / 3, 2 * u, 0.9f * u);
		ctl[CT_R].r = rc(W - m / 2 - u, u / 3, 2 * u, 0.9f * u);
		ctl[CT_START].r = rc(W - m / 2 - 1.1f * u, H - 1.4f * u, 2.2f * u, 0.8f * u);
		ctl[CT_SELECT].r = rc(m / 2 - 1.1f * u, H - 1.4f * u, 2.2f * u, 0.8f * u);
		ctl[CT_FF].r = rc(W - m / 2 - u, 1.5f * u, 2 * u, 0.9f * u);
	}
	ctl[CT_A].label = "A"; ctl[CT_B].label = "B"; ctl[CT_L].label = "L"; ctl[CT_R].label = "R";
	ctl[CT_START].label = "START"; ctl[CT_SELECT].label = "SELECT"; ctl[CT_FF].label = "FF";
}

// A 5x7 font for the few letters the buttons need: seven rows of five columns.
static const char* const glyphRows[][7] = {
	{"01110", "10001", "10001", "11111", "10001", "10001", "10001"},   // A
	{"11110", "10001", "10001", "11110", "10001", "10001", "11110"},   // B
	{"01110", "10001", "10000", "10000", "10000", "10001", "01110"},   // C
	{"11111", "10000", "10000", "11110", "10000", "10000", "11111"},   // E
	{"11111", "10000", "10000", "11110", "10000", "10000", "10000"},   // F
	{"10000", "10000", "10000", "10000", "10000", "10000", "11111"},   // L
	{"11110", "10001", "10001", "11110", "10100", "10010", "10001"},   // R
	{"01111", "10000", "10000", "01110", "00001", "00001", "11110"},   // S
	{"11111", "00100", "00100", "00100", "00100", "00100", "00100"},   // T
};

static const char* const* glyph(char c) {
	const char* order = "ABCEFLRST";
	const char* p = strchr(order, c);
	return p && c ? glyphRows[p - order] : NULL;
}

static void drawText(const char* s, int cx, int cy, int px) {
	int n = (int) strlen(s), w = n * 6 * px - px;
	int x = cx - w / 2, y = cy - 7 * px / 2;
	for (; *s; ++s, x += 6 * px) {
		const char* const* g = glyph(*s);
		if (!g) continue;
		for (int r = 0; r < 7; ++r)
			for (int c = 0; c < 5; ++c)
				if (g[r][c] == '1') { SDL_Rect d = {x + c * px, y + r * px, px, px}; SDL_RenderFillRect(ren, &d); }
	}
}

static void fillCircle(int cx, int cy, int r) {
	for (int dy = -r; dy <= r; ++dy) {
		int dx = (int) SDL_sqrtf((float) (r * r - dy * dy));
		SDL_RenderDrawLine(ren, cx - dx, cy + dy, cx + dx, cy + dy);
	}
}

static uint32_t heldMask;   // GBA keys currently pressed (for the highlight)

static void drawControls(void) {
	SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
	for (int i = 0; i < CT_COUNT; ++i) {
		SDL_Rect r = ctl[i].r;
		bool on = false;
		switch (i) {
		case CT_A: on = heldMask & (1u << GBA_KEY_A); break;
		case CT_B: on = heldMask & (1u << GBA_KEY_B); break;
		case CT_L: on = heldMask & (1u << GBA_KEY_L); break;
		case CT_R: on = heldMask & (1u << GBA_KEY_R); break;
		case CT_START: on = heldMask & (1u << GBA_KEY_START); break;
		case CT_SELECT: on = heldMask & (1u << GBA_KEY_SELECT); break;
		case CT_FF: on = ffToggle; break;
		}
		SDL_SetRenderDrawColor(ren, on ? 230 : 40, on ? 200 : 50, on ? 120 : 90, on ? 200 : 150);
		if (i == CT_DPAD) {
			const int dp = r.w / 3;
			SDL_Rect h = {r.x, r.y + dp, r.w, dp}, v = {r.x + dp, r.y, dp, r.h};
			SDL_RenderFillRect(ren, &h); SDL_RenderFillRect(ren, &v);
			SDL_SetRenderDrawColor(ren, 255, 255, 255, 170);
			const int dirs[4][2] = {{0, -1}, {0, 1}, {-1, 0}, {1, 0}};
			for (int d = 0; d < 4; ++d) {   // an arrow head in each arm
				int cx = r.x + r.w / 2 + dirs[d][0] * dp, cy = r.y + r.h / 2 + dirs[d][1] * dp, s = dp / 4;
				int tipx = cx + dirs[d][0] * s, tipy = cy + dirs[d][1] * s;
				SDL_RenderDrawLine(ren, tipx, tipy, cx - dirs[d][0] * s - dirs[d][1] * s, cy - dirs[d][1] * s - dirs[d][0] * s);
				SDL_RenderDrawLine(ren, tipx, tipy, cx - dirs[d][0] * s + dirs[d][1] * s, cy - dirs[d][1] * s + dirs[d][0] * s);
			}
			continue;
		}
		if (i == CT_A || i == CT_B) fillCircle(r.x + r.w / 2, r.y + r.h / 2, r.w / 2);
		else SDL_RenderFillRect(ren, &r);
		SDL_SetRenderDrawColor(ren, 255, 255, 255, 220);
		int px = r.h / 12 > 1 ? r.h / 12 : 1, chars = (int) strlen(ctl[i].label);
		while (px > 1 && chars * 6 * px > r.w - 4) --px;   // the label must fit its button
		drawText(ctl[i].label, r.x + r.w / 2, r.y + r.h / 2, px);
	}
}

static bool inRect(const SDL_Rect* r, int x, int y) { return x >= r->x && x < r->x + r->w && y >= r->y && y < r->y + r->h; }

// ---- input ----------------------------------------------------------------------------------------------------------------
static void openPads(void) {
	for (int i = 0; i < SDL_NumJoysticks(); ++i) {
		if (!SDL_IsGameController(i)) continue;
		SDL_JoystickID want = SDL_JoystickGetDeviceInstanceID(i);
		bool have = false;
		for (int k = 0; k < 4; ++k) if (pads[k] && SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(pads[k])) == want) have = true;
		if (have) continue;
		for (int k = 0; k < 4; ++k) if (!pads[k] || !SDL_GameControllerGetAttached(pads[k])) { pads[k] = SDL_GameControllerOpen(i); break; }
	}
}

// Leaving the foreground (Android): SDL raises the event on the emulation thread from inside the event poll, i.e. between frames, so the app can
// save right there. Events from any other thread are ignored.
static SDL_threadID loopThread;
static int SDLCALL lifecycleWatch(void* ud, SDL_Event* e) {
	(void) ud;
	if (e->type == SDL_APP_WILLENTERBACKGROUND && loopThread && SDL_ThreadID() == loopThread) appSaveNow();
	return 0;
}

static float tapX, tapY;
static int tapHold;

void plPollInput(PlInput* in) {
	loopThread = SDL_ThreadID();
	SDL_Event e;
	while (SDL_PollEvent(&e)) {
		if (e.type == SDL_QUIT) quitRequested = true;
		if (e.type == SDL_FINGERDOWN) { tapX = e.tfinger.x; tapY = e.tfinger.y; tapHold = 4; }   // a very quick tap must still be seen for a few frames
		if (e.type == SDL_CONTROLLERDEVICEADDED || e.type == SDL_CONTROLLERDEVICEREMOVED) openPads();
	}
	layout();
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
	in->turbo = k[SDL_SCANCODE_TAB] != 0;
	for (int p = 0; p < 4; ++p) {   // game controllers: east / south = A / B like the GBA's layout
		SDL_GameController* c = pads[p];
		if (!c || !SDL_GameControllerGetAttached(c)) continue;
		if (SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_B)) keys |= 1 << GBA_KEY_A;
		if (SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_A)) keys |= 1 << GBA_KEY_B;
		if (SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_LEFTSHOULDER)) keys |= 1 << GBA_KEY_L;
		if (SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER)) keys |= 1 << GBA_KEY_R;
		if (SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_START)) keys |= 1 << GBA_KEY_START;
		if (SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_BACK)) keys |= 1 << GBA_KEY_SELECT;
		if (SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_DPAD_UP)) keys |= 1 << GBA_KEY_UP;
		if (SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_DPAD_DOWN)) keys |= 1 << GBA_KEY_DOWN;
		if (SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_DPAD_LEFT)) keys |= 1 << GBA_KEY_LEFT;
		if (SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_DPAD_RIGHT)) keys |= 1 << GBA_KEY_RIGHT;
		int lx = SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_LEFTX), ly = SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_LEFTY);
		if (lx < -16000) keys |= 1 << GBA_KEY_LEFT;
		if (lx > 16000) keys |= 1 << GBA_KEY_RIGHT;
		if (ly < -16000) keys |= 1 << GBA_KEY_UP;
		if (ly > 16000) keys |= 1 << GBA_KEY_DOWN;
		if (SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) > 16000) in->turbo = true;
	}

	// pointers: every finger on the touch screen plus the mouse button
	int px[12], py[12], np = 0;
	int winW, winH;
	SDL_GetWindowSize(win, &winW, &winH);
	for (int d = 0; d < SDL_GetNumTouchDevices() && np < 10; ++d) {
		SDL_TouchID id = SDL_GetTouchDevice(d);
		for (int f = 0; f < SDL_GetNumTouchFingers(id) && np < 10; ++f) {
			SDL_Finger* fg = SDL_GetTouchFinger(id, f);
			if (fg) { px[np] = (int) (fg->x * outW); py[np] = (int) (fg->y * outH); ++np; }
		}
	}
	if (tapHold > 0) { if (np == 0) { px[np] = (int) (tapX * outW); py[np] = (int) (tapY * outH); ++np; } --tapHold; }
	int mx, my;
	if ((SDL_GetMouseState(&mx, &my) & SDL_BUTTON_LMASK) && np < 11) { px[np] = mx * outW / (winW ? winW : 1); py[np] = my * outH / (winH ? winH : 1); ++np; }
	in->touch = false;
	in->tx = in->ty = 0;
	static bool ffWas;
	bool ffNow = false;
	for (int i = 0; i < np; ++i) {
		if (inRect(&botDst, px[i], py[i])) {
			if (!in->touch) { in->touch = true; in->tx = (px[i] - botDst.x) * GFX_W / botDst.w; in->ty = (py[i] - botDst.y) * GFX_H / botDst.h; }
			continue;
		}
		for (int c = 0; c < CT_COUNT; ++c) {
			if (!inRect(&ctl[c].r, px[i], py[i])) continue;
			switch (c) {
			case CT_A: keys |= 1 << GBA_KEY_A; break;
			case CT_B: keys |= 1 << GBA_KEY_B; break;
			case CT_L: keys |= 1 << GBA_KEY_L; break;
			case CT_R: keys |= 1 << GBA_KEY_R; break;
			case CT_START: keys |= 1 << GBA_KEY_START; break;
			case CT_SELECT: keys |= 1 << GBA_KEY_SELECT; break;
			case CT_FF: ffNow = true; break;
			case CT_DPAD: {
				int dx = px[i] - (ctl[c].r.x + ctl[c].r.w / 2), dy = py[i] - (ctl[c].r.y + ctl[c].r.h / 2), t = ctl[c].r.w / 6;
				if (dx < -t) keys |= 1 << GBA_KEY_LEFT;
				if (dx > t) keys |= 1 << GBA_KEY_RIGHT;
				if (dy < -t) keys |= 1 << GBA_KEY_UP;
				if (dy > t) keys |= 1 << GBA_KEY_DOWN;
				break;
			}
			}
		}
	}
	if (ffNow && !ffWas) ffToggle = !ffToggle;   // the FF button latches
	ffWas = ffNow;
	if (ffToggle) in->turbo = true;
	in->keys = keys;
	heldMask = keys;

	static int loops;   // test helper
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
	if (dualScreen) drawTopSurface(j->top);
	else SDL_UpdateTexture(topTex, NULL, j->top, 256 * 2);
	if (botDirty) {
		for (int x = 0; x < GFX_W; ++x)
			for (int y = 0; y < GFX_H; ++y) rows[y * GFX_W + x] = privBot[x * GFX_H + (GFX_H - 1 - y)];
		SDL_UpdateTexture(botTex, NULL, rows, GFX_W * 2);
		botDirty = false;
	}
	SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
	SDL_RenderClear(ren);
	SDL_Rect topSrc = {0, 0, 240, 160};
	if (!dualScreen) SDL_RenderCopy(ren, topTex, &topSrc, &topDst);
	SDL_RenderCopy(ren, botTex, NULL, &botDst);
	if (!dualScreen) drawControls();
	if (shotPath && ++frameNo >= shotFrames) {
		SDL_Surface* sf = SDL_CreateRGBSurfaceWithFormat(0, outW, outH, 32, SDL_PIXELFORMAT_ARGB8888);
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

#ifdef __ANDROID__
// Start-up menu: Play (each ROM that is present) and Add ROM, which opens the system file picker (MainActivity.pickRom). Back = quit.
static int jniStaticInt(const char* name) {
	JNIEnv* env = SDL_AndroidGetJNIEnv();
	jobject act = SDL_AndroidGetActivity();
	jclass cls = (*env)->GetObjectClass(env, act);
	jmethodID m = (*env)->GetStaticMethodID(env, cls, name, "()I");
	int v = m ? (*env)->CallStaticIntMethod(env, cls, m) : 4;
	(*env)->DeleteLocalRef(env, cls);
	(*env)->DeleteLocalRef(env, act);
	return v;
}

static void jniPickRom(void) {
	JNIEnv* env = SDL_AndroidGetJNIEnv();
	jobject act = SDL_AndroidGetActivity();
	jclass cls = (*env)->GetObjectClass(env, act);
	jmethodID m = (*env)->GetStaticMethodID(env, cls, "pickRom", "()V");
	if (m) (*env)->CallStaticVoidMethod(env, cls, m);
	(*env)->DeleteLocalRef(env, cls);
	(*env)->DeleteLocalRef(env, act);
}

static void note(const char* text) { SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "Golden Sun - Dual Screen Companion", text, win); }

bool plChooseGame(GsGameId* out) {
	const char* g = getenv("GS_GAME");
	for (;;) {
		const bool have[2] = {gsGameRomExists(GS_GAME_1), gsGameRomExists(GS_GAME_2)};
		if (g && (atoi(g) == 1 || atoi(g) == 2) && have[atoi(g) - 1]) { *out = (GsGameId) (atoi(g) - 1); return true; }
		SDL_MessageBoxButtonData b[3];
		int nb = 0, game[3];
		for (int i = 0; i < 2; ++i) if (have[i]) { b[nb] = (SDL_MessageBoxButtonData) {0, nb, i ? "Play The Lost Age" : "Play Golden Sun"}; game[nb++] = i; }
		b[nb] = (SDL_MessageBoxButtonData) {SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, nb, "Add ROM file"};
		game[nb++] = -1;
		const SDL_MessageBoxData mb = {SDL_MESSAGEBOX_INFORMATION, win, "Golden Sun - Dual Screen Companion",
			have[0] || have[1] ? "Pick a game, or add a ROM. Your ROMs stay on this device (back button: quit)."
			                   : "No ROM yet. Add your own Golden Sun or Golden Sun: The Lost Age ROM (.gba). It is copied into this app and never leaves your device.",
			nb, b, NULL};
		int id = -1;
		if (SDL_ShowMessageBox(&mb, &id) != 0) return false;
		if (id < 0) return false;   // dismissed with back
		if (game[id] >= 0) { *out = (GsGameId) game[id]; return true; }
		jniPickRom();
		while (jniStaticInt("pickState") == 1) { SDL_PumpEvents(); SDL_Delay(100); }
		int st = jniStaticInt("pickState");
		if (st == 2) note("ROM added.");
		else if (st == 3) note("That file is not Golden Sun or Golden Sun: The Lost Age (checked the cartridge code in the ROM header).");
		else if (st == 4) note("Could not read that file.");
	}
}
#else
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
#endif

bool plInit(void) {
	SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");   // fingers are read directly (several at once: buttons plus the touch screen)
	SDL_SetHint(SDL_HINT_MOUSE_TOUCH_EVENTS, "0");
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) != 0) { fprintf(stderr, "SDL: %s\n", SDL_GetError()); return false; }
#ifdef __ANDROID__
	{   // ROMs, saves and maps live in the app's own folder on shared storage (visible in a file manager, removed with the app)
		const char* p = SDL_AndroidGetExternalStoragePath();
		if (!p) p = SDL_AndroidGetInternalStoragePath();
		if (p) setenv("GS_DATA", p, 0);
	}
#endif
	mLogSetDefaultLogger(&quietLogger);
	shotPath = getenv("GS_SHOT");
	shotFrames = getenv("GS_FRAMES") ? atoi(getenv("GS_FRAMES")) : 120;
	int ww = 640, wh = 800;
	if (getenv("GS_SIZE")) sscanf(getenv("GS_SIZE"), "%dx%d", &ww, &wh);
	win = SDL_CreateWindow("Golden Sun - Dual Screen Companion", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, ww, wh, SDL_WINDOW_RESIZABLE | (shotPath ? SDL_WINDOW_HIDDEN : 0));
	ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);   // no vsync: plPace holds the 59.73 Hz
	if (!win || !ren) { fprintf(stderr, "SDL: %s\n", SDL_GetError()); return false; }
	topTex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING, 256, 160);
	botTex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING, GFX_W, GFX_H);
	SDL_SetTextureScaleMode(topTex, SDL_ScaleModeNearest);
	SDL_SetTextureScaleMode(botTex, SDL_ScaleModeNearest);
	openPads();
	layout();
#ifdef __ANDROID__
	SDL_AddEventWatch(lifecycleWatch, NULL);
#endif
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
