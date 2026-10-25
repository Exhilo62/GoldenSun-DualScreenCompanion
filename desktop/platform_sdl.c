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
#include "../src/pickscreen.h"
#include "../src/platform.h"
#include "../src/ui.h"

#ifdef __ANDROID__
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <jni.h>
#endif

#define AUDIO_SAMPLES 512
#ifdef __ANDROID__
#define FULLSCREEN_FLAG SDL_WINDOW_FULLSCREEN   // immersive: no status or navigation bar
#else
#define FULLSCREEN_FLAG 0
#endif

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
// Which display shows what on a dual-screen device: normally this window (where the app was started) is the touch screen and the other display shows the
// GBA picture. Handhelds differ in which physical screen the app opens on, so the screens can be swapped (ZL on a controller, or the W key): then this
// window shows the picture and the other display shows the companion UI, and touches on it are sent back here by MainActivity.
// "Top screen: Fill": the 3:2 picture is stretched to 16:9. On a display wider than 16:9 (up to 21:9) it fills the whole display; on a taller one (4:3,
// portrait) it is a 16:9 box across the width; on a still wider one it is a 21:9 box.
static void fillBox(int W, int H, int* x, int* y, int* w, int* h) {
	float ta = (float) W / (float) (H ? H : 1);
	if (ta < 16.0f / 9.0f) ta = 16.0f / 9.0f;
	if (ta > 21.0f / 9.0f) ta = 21.0f / 9.0f;
	*w = W; *h = (int) (W / ta);
	if (*h > H) { *h = H; *w = (int) (H * ta); }
	*x = (W - *w) / 2; *y = (H - *h) / 2;
}

static bool swapScreens;
static bool topFill;      // the Menu tab's "Top screen: Fill": stretch the picture over its whole area instead of fitting it
static bool pickerMode;   // the game pick screen is showing (layout puts its two canvases where the game picture and the UI normally go)
static volatile bool surfaceStale;           // the other display must be redrawn even if the UI did not change (new surface, swap)
static volatile bool extDown;                // a finger is down on the other display (swapped mode), in 320x240 UI coordinates
static volatile int extTx, extTy;
static int extW, extH;                       // size of the other display's surface, kept for mapping its touches

#ifdef __ANDROID__
static ANativeWindow* topWin;
JNIEXPORT void JNICALL Java_com_exhilo_goldensundualscreen_MainActivity_nativeSetTopSurface(JNIEnv* env, jclass cls, jobject surface) {
	(void) cls;
	if (!topLock) topLock = SDL_CreateMutex();
	SDL_LockMutex(topLock);
	if (topWin) { ANativeWindow_release(topWin); topWin = NULL; }
	if (surface) topWin = ANativeWindow_fromSurface(env, surface);
	dualScreen = topWin != NULL;
	surfaceStale = true;
	extDown = false;
	SDL_UnlockMutex(topLock);
}

// Touch on the other display: action 0 = down / move, 1 = up; x, y are fractions of the surface. Mapped through the same letterbox the UI is drawn with.
JNIEXPORT void JNICALL Java_com_exhilo_goldensundualscreen_MainActivity_nativeSurfaceTouch(JNIEnv* env, jclass cls, jint action, jfloat fx, jfloat fy) {
	(void) env; (void) cls;
	if (action != 0 || extW <= 0 || extH <= 0) { extDown = false; return; }
	float sc = (float) extW / 320 < (float) extH / 240 ? (float) extW / 320 : (float) extH / 240;
	float x = (fx * extW - (extW - 320 * sc) / 2) / sc, y = (fy * extH - (extH - 240 * sc) / 2) / sc;
	if (x < 0 || y < 0 || x >= 320 || y >= 240) { extDown = false; return; }
	extTx = (int) x; extTy = (int) y;
	extDown = true;
}

// Nearest-neighbour fit of an RGB565 image (sw x sh, row stride in pixels) onto the other display, black bars where the shapes differ.
static void drawSurface(const uint16_t* src, int sw, int sh, int stride, bool stretch) {
	if (!topLock) return;
	SDL_LockMutex(topLock);
	if (topWin) {
		ANativeWindow_setBuffersGeometry(topWin, 0, 0, WINDOW_FORMAT_RGB_565);
		ANativeWindow_Buffer b;
		if (ANativeWindow_lock(topWin, &b, NULL) == 0) {
			if (b.format == WINDOW_FORMAT_RGB_565 && b.width > 0 && b.height > 0) {
				extW = b.width; extH = b.height;
				float sc = (float) b.width / sw < (float) b.height / sh ? (float) b.width / sw : (float) b.height / sh;
				int dw = (int) (sw * sc), dh = (int) (sh * sc), x0 = (b.width - dw) / 2, y0 = (b.height - dh) / 2;
				if (stretch) fillBox(b.width, b.height, &x0, &y0, &dw, &dh);   // "Top screen: Fill"
				static int xmap[4096];
				static int mapW = -1, mapS = -1;
				if (dw > 4096) dw = 4096;
				if (mapW != dw || mapS != sw) { for (int x = 0; x < dw; ++x) xmap[x] = x * sw / dw; mapW = dw; mapS = sw; }
				uint16_t* dst = b.bits;
				memset(dst, 0, (size_t) b.stride * b.height * 2);
				for (int y = 0; y < dh; ++y) {
					const uint16_t* row = src + (y * sh / dh) * stride;
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
static void drawSurface(const uint16_t* src, int sw, int sh, int stride, bool stretch) { (void) src; (void) sw; (void) sh; (void) stride; (void) stretch; }
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
enum { CT_DPAD, CT_A, CT_B, CT_L, CT_R, CT_START, CT_SELECT, CT_FF, CT_PADMODE, CT_COUNT };
typedef struct { SDL_Rect r; const char* label; } Control;
static Control ctl[CT_COUNT];
static SDL_Rect topDst, botDst;
static int outW = 640, outH = 800;
static bool ffToggle;
// Touch buttons: 0 = Auto (hidden while a game controller is connected), 1 = always shown, 2 = always hidden. A small chip below the FF button cycles it
// and stays on screen (dimmed) when the buttons are hidden; the choice is kept in padmode.txt in the data folder.
static int padMode;
static bool padsShown;   // the touch buttons are currently on screen and active
static int padsConnected(void);

static SDL_Rect rc(float x, float y, float w, float h) { return (SDL_Rect) {(int) x, (int) y, (int) w, (int) h}; }

static void layout(void) {
	SDL_GetRendererOutputSize(ren, &outW, &outH);
	const int W = outW, H = outH;
	topDst = (SDL_Rect) {0, 0, 0, 0};
	if (pickerMode) {   // banner (top canvas) and cards (bottom canvas), both 4:3
		botDst = (SDL_Rect) {0, 0, 0, 0};
		for (int i = 0; i < CT_COUNT; ++i) ctl[i].r = (SDL_Rect) {-1, -1, 0, 0};
		if (dualScreen) {   // only the window's own canvas is placed here; the other one goes to the other display
			float k = (float) W / 320 < (float) H / 240 ? (float) W / 320 : (float) H / 240;
			SDL_Rect r = rc((W - 320 * k) / 2, (H - 240 * k) / 2, 320 * k, 240 * k);
			if (swapScreens) {
				topDst = r;
				if (topFill) { int fx, fy, fw, fh; fillBox(W, H, &fx, &fy, &fw, &fh); topDst = (SDL_Rect) {fx, fy, fw, fh}; }   // the banner stretched to 16:9 like the game picture
			} else botDst = r;
		} else if (H >= W) {
			float k = (float) W / 320 < (float) H / 480 ? (float) W / 320 : (float) H / 480;
			float x0 = (W - 320 * k) / 2, y0 = (H - 480 * k) / 2;
			topDst = rc(x0, y0, 320 * k, 240 * k);
			botDst = rc(x0, y0 + 240 * k, 320 * k, 240 * k);
		} else {
			float a = (float) W / 640 < (float) H / 240 ? (float) W / 640 : (float) H / 240;
			float x0 = (W - 640 * a) / 2, y0 = (H - 240 * a) / 2;
			topDst = rc(x0, y0, 320 * a, 240 * a);
			botDst = rc(x0 + 320 * a, y0, 320 * a, 240 * a);
		}
		return;
	}
	if (dualScreen && swapScreens) {   // swapped: this window shows the picture alone, the other display has the UI
		float k = (float) W / 240 < (float) H / 160 ? (float) W / 240 : (float) H / 160;
		topDst = rc((W - 240 * k) / 2, (H - 160 * k) / 2, 240 * k, 160 * k);
		if (topFill) { int fx, fy, fw, fh; fillBox(W, H, &fx, &fy, &fw, &fh); topDst = (SDL_Rect) {fx, fy, fw, fh}; }
		botDst = (SDL_Rect) {0, 0, 0, 0};
		for (int i = 0; i < CT_COUNT; ++i) ctl[i].r = (SDL_Rect) {-1, -1, 0, 0};
	} else if (dualScreen) {   // this display is the touch screen alone: the UI as large as it fits, no on-screen buttons (the device has its own)
		float k = (float) W / 320 < (float) H / 240 ? (float) W / 320 : (float) H / 240;
		botDst = rc((W - 320 * k) / 2, (H - 240 * k) / 2, 320 * k, 240 * k);
		for (int i = 0; i < CT_COUNT; ++i) ctl[i].r = (SDL_Rect) {-1, -1, 0, 0};
	} else if (H >= W) {   // portrait
		float topH = topFill ? 180 : 160;   // Fill: the picture widened to 16:9 over the UI's full width
		float k = (float) W / 320;
		if (k * (topH + 240) > H * 0.62f) k = H * 0.62f / (topH + 240);
		int sw = (int) (320 * k), x0 = (W - sw) / 2;
		if (topFill) topDst = rc(x0, 0, 320 * k, 180 * k);
		else topDst = rc(x0 + 40 * k, 0, 240 * k, 160 * k);
		botDst = rc(x0, topH * k, sw, 240 * k);
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
		ctl[CT_PADMODE].r = rc(W / 2.0f - 1.3f * u, cy + 1.15f * u, 2.6f * u, 0.7f * u);
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
		ctl[CT_PADMODE].r = rc(W - m / 2 - 1.3f * u, 2.5f * u, 2.6f * u, 0.7f * u);
	}
	ctl[CT_A].label = "A"; ctl[CT_B].label = "B"; ctl[CT_L].label = "L"; ctl[CT_R].label = "R";
	ctl[CT_START].label = "START"; ctl[CT_SELECT].label = "SELECT"; ctl[CT_FF].label = "FF"; ctl[CT_PADMODE].label = padMode == 0 ? "PAD AUTO" : padMode == 1 ? "PAD ON" : "PAD OFF";
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
	{"11110", "10001", "10001", "10001", "10001", "10001", "11110"},   // D
	{"10001", "11001", "10101", "10101", "10011", "10001", "10001"},   // N
	{"01110", "10001", "10001", "10001", "10001", "10001", "01110"},   // O
	{"11110", "10001", "10001", "11110", "10000", "10000", "10000"},   // P
	{"10001", "10001", "10001", "10001", "10001", "10001", "01110"},   // U
};

static const char* const* glyph(char c) {
	const char* order = "ABCEFLRSTDNOPU";
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
		if (!padsShown && i != CT_PADMODE) continue;
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
		SDL_SetRenderDrawColor(ren, on ? 230 : 40, on ? 200 : 50, on ? 120 : 90, on ? 200 : (i == CT_PADMODE && !padsShown ? 70 : 150));
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
		SDL_SetRenderDrawColor(ren, 255, 255, 255, i == CT_PADMODE && !padsShown ? 120 : 220);
		int px = r.h / 12 > 1 ? r.h / 12 : 1, chars = (int) strlen(ctl[i].label);
		while (px > 1 && chars * 6 * px > r.w - 4) --px;   // the label must fit its button
		drawText(ctl[i].label, r.x + r.w / 2, r.y + r.h / 2, px);
	}
}

static bool inRect(const SDL_Rect* r, int x, int y) { return x >= r->x && x < r->x + r->w && y >= r->y && y < r->y + r->h; }

// ---- input ----------------------------------------------------------------------------------------------------------------
static int padsConnected(void) {
	int n = 0;
	for (int k = 0; k < 4; ++k) if (pads[k] && SDL_GameControllerGetAttached(pads[k])) ++n;
	return n;
}

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

static bool faceFlip;   // face buttons by position instead of by label (Select + ZL)
static const char* flipPath(void) {
	static char path[160];
	return gsAppFilePath("facebuttons.txt", path, sizeof(path));
}

static const char* swapPath(void) {
	static char path[160];
	return gsAppFilePath("screenswap.txt", path, sizeof(path));
}

static const char* padModePath(void) {
	static char path[160];
	return gsAppFilePath("padmode.txt", path, sizeof(path));
}

static bool swapPressed;
static bool backPressed;
static float tapX, tapY;
static int tapHold;

void plPollInput(PlInput* in) {
	loopThread = SDL_ThreadID();
	SDL_Event e;
	while (SDL_PollEvent(&e)) {
		if (e.type == SDL_QUIT) quitRequested = true;
		if (e.type == SDL_FINGERDOWN) { tapX = e.tfinger.x; tapY = e.tfinger.y; tapHold = 4; }   // a very quick tap must still be seen for a few frames
		if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_AC_BACK) backPressed = true;   // only the pick screen acts on Back; in the game it is ignored
		if (e.type == SDL_KEYDOWN && !e.key.repeat && e.key.keysym.scancode == SDL_SCANCODE_W) swapPressed = true;   // W: swap the two screens (an event, so a very short key press still counts)
		if (e.type == SDL_CONTROLLERDEVICEADDED || e.type == SDL_CONTROLLERDEVICEREMOVED) openPads();
	}
	padsShown = !dualScreen && (padMode == 1 || (padMode == 0 && padsConnected() == 0));
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
	bool swapKey = false, selectHeld = false, zlHeld = false;
	for (int p = 0; p < 4; ++p) {   // game controllers: east / south = A / B like the GBA's layout
		SDL_GameController* c = pads[p];
		if (!c || !SDL_GameControllerGetAttached(c)) continue;
		// Face buttons by the label the device reports (A = A, B = B, like the 3DS); Select + ZL flips A/B and X/Y for pads that report by position instead.
		if (SDL_GameControllerGetButton(c, faceFlip ? SDL_CONTROLLER_BUTTON_B : SDL_CONTROLLER_BUTTON_A)) keys |= 1 << GBA_KEY_A;
		if (SDL_GameControllerGetButton(c, faceFlip ? SDL_CONTROLLER_BUTTON_A : SDL_CONTROLLER_BUTTON_B)) keys |= 1 << GBA_KEY_B;
		if (SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_BACK)) selectHeld = true;
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
		if (SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_TRIGGERLEFT) > 16000) zlHeld = true;
		if (SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) > 16000 || SDL_GameControllerGetButton(c, faceFlip ? SDL_CONTROLLER_BUTTON_X : SDL_CONTROLLER_BUTTON_Y)) in->turbo = true;   // like the 3DS: Y or ZR
	}

	swapKey = zlHeld && !selectHeld;   // ZL swaps the two screens; Select + ZL flips the face-button layout
	static bool flipWas;
	bool flipNow = zlHeld && selectHeld;
	if (flipNow && !flipWas) {
		faceFlip = !faceFlip;
		FILE* ff = fopen(flipPath(), "w");
		if (ff) { fprintf(ff, "%d\n", faceFlip ? 1 : 0); fclose(ff); }
	}
	flipWas = flipNow;
	static bool swapWas;
	if (((swapKey && !swapWas) || swapPressed) && dualScreen) {
		swapPressed = false;
		swapScreens = !swapScreens;
		surfaceStale = true;
		extDown = false;
		FILE* sf = fopen(swapPath(), "w");
		if (sf) { fprintf(sf, "%d\n", swapScreens ? 1 : 0); fclose(sf); }
	}
	swapPressed = false;
	swapWas = swapKey;

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
	if (dualScreen && swapScreens && extDown) { in->touch = true; in->tx = extTx; in->ty = extTy; }   // a finger on the other display (the UI is there)
	static bool ffWas;
	bool ffNow = false, modeNow = false;
	static bool modeWas;
	for (int i = 0; i < np; ++i) {
		if (inRect(&botDst, px[i], py[i])) {
			if (!in->touch) { in->touch = true; in->tx = (px[i] - botDst.x) * GFX_W / botDst.w; in->ty = (py[i] - botDst.y) * GFX_H / botDst.h; }
			continue;
		}
		for (int c = 0; c < CT_COUNT; ++c) {
			if (!inRect(&ctl[c].r, px[i], py[i])) continue;
			if (!padsShown && c != CT_PADMODE) continue;
			switch (c) {
			case CT_PADMODE: modeNow = true; break;
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
	if (modeNow && !modeWas) {   // the PAD chip cycles Auto -> On -> Off
		padMode = (padMode + 1) % 3;
		FILE* pf = fopen(padModePath(), "w");
		if (pf) { fprintf(pf, "%d\n", padMode); fclose(pf); }
	}
	modeWas = modeNow;
	if (ffNow && !ffWas) ffToggle = !ffToggle;   // the FF button latches
	ffWas = ffNow;
	if (ffToggle) in->turbo = true;
	in->keys = keys;
	heldMask = keys;

	static int loops;   // test helper
	++loops;
	{   // test helpers: GS_SPAM="every;from-to;from-to" taps A for three loops out of every N inside the windows; GS_SCRIPT="loop:key,loop:key" presses
		// U D L R A B S(tart) E(select) for six loops at the given loop
		if (getenv("GS_TURBO")) in->turbo = true;   // test helper: run at fast-forward speed
	const char* spam = getenv("GS_SPAM");
		if (spam) {
			int every = atoi(spam);
			const char* w = strchr(spam, ';');
			bool inWin = false;
			while (every > 0 && w) {
				int f, t, used = 0;
				if (sscanf(w + 1, "%d-%d%n", &f, &t, &used) == 2) { if (loops >= f && loops <= t) inWin = true; w += 1 + used; } else break;
				w = strchr(w, ';');
			}
			if (every > 0 && inWin && (loops % every) < 3) in->keys |= 1u << GBA_KEY_A;
		}
		const char* sc = getenv("GS_SCRIPT");
		for (int at, used; sc && sscanf(sc, "%d:%n", &at, &used) == 1; ) {
			char key = sc[used];
			if (loops >= at && loops < at + 6) {
				switch (key) {
				case 'U': in->keys |= 1u << GBA_KEY_UP; break;
				case 'D': in->keys |= 1u << GBA_KEY_DOWN; break;
				case 'L': in->keys |= 1u << GBA_KEY_LEFT; break;
				case 'R': in->keys |= 1u << GBA_KEY_RIGHT; break;
				case 'A': in->keys |= 1u << GBA_KEY_A; break;
				case 'B': in->keys |= 1u << GBA_KEY_B; break;
				case 'S': in->keys |= 1u << GBA_KEY_START; break;
				case 'E': in->keys |= 1u << GBA_KEY_SELECT; break;
				}
			}
			sc += used + 1;
			if (*sc != ',') break;
			++sc;
		}
	}
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
	if (!dualScreen || swapScreens) SDL_UpdateTexture(topTex, NULL, j->top, 256 * 2);
	else drawSurface(j->top, 240, 160, 256, topFill);
	bool uiChanged = botDirty;
	if (botDirty) {
		for (int x = 0; x < GFX_W; ++x)
			for (int y = 0; y < GFX_H; ++y) rows[y * GFX_W + x] = privBot[x * GFX_H + (GFX_H - 1 - y)];
		if (!(dualScreen && swapScreens)) SDL_UpdateTexture(botTex, NULL, rows, GFX_W * 2);
		botDirty = false;
	}
	if (dualScreen && swapScreens && (uiChanged || surfaceStale)) { drawSurface(rows, GFX_W, GFX_H, GFX_W, false); surfaceStale = false; }
	SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
	SDL_RenderClear(ren);
	SDL_Rect topSrc = {0, 0, 240, 160};
	if (!dualScreen || swapScreens) SDL_RenderCopy(ren, topTex, &topSrc, &topDst);
	if (botDst.w > 0) SDL_RenderCopy(ren, botTex, NULL, &botDst);
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
	topFill = j->ui.topScale == TOP_FILL;
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

// ---- game pick screen ---------------------------------------------------------------------------------------------------
// The 3DS launcher's look (src/pickscreen.c): banner and game cards, plus "Add ROM file" (Android: the system file picker, see MainActivity.pickRom).
#ifdef __ANDROID__
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
#endif

static uint16_t pickTopCanvas[GFX_W * GFX_H], pickBotCanvas[GFX_W * GFX_H], pickTopRows[GFX_W * GFX_H], pickBotRows[GFX_W * GFX_H];

static void canvasToRows(const uint16_t* canvas, uint16_t* out) {
	for (int x = 0; x < GFX_W; ++x)
		for (int y = 0; y < GFX_H; ++y) out[y * GFX_W + x] = canvas[x * GFX_H + (GFX_H - 1 - y)];
}

static bool pickerLoop(GsGameId* out) {
	SDL_Texture* tTex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING, GFX_W, GFX_H);
	SDL_Texture* bTex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING, GFX_W, GFX_H);
	SDL_SetTextureScaleMode(tTex, SDL_ScaleModeNearest);
	SDL_SetTextureScaleMode(bTex, SDL_ScaleModeNearest);
	pickerMode = true;
	{   // the banner follows the Menu tab's "Top screen" setting (first byte of settings.bin; Fill when there is none yet)
		char sp[128];
		unsigned char b0 = TOP_FILL;
		FILE* sf = fopen(gsGamePath(GS_GAME_1, "settings.bin", sp, sizeof(sp)), "rb");
		if (sf) { if (fread(&b0, 1, 1, sf) != 1) b0 = TOP_FILL; fclose(sf); }
		topFill = b0 == TOP_FILL;
	}
	char lastPath[128];
	gsGamePath(GS_GAME_1, "lastgame.txt", lastPath, sizeof(lastPath));
	int last = -1;
	{ FILE* f = fopen(lastPath, "r"); int v; if (f) { if (fscanf(f, "%d", &v) == 1 && v >= 0 && v < GS_GAME_COUNT) last = v; fclose(f); } }
	bool have[2] = {gsGameRomExists(GS_GAME_1), gsGameRomExists(GS_GAME_2)};
	int sel = last >= 0 && have[last] ? last : have[0] ? 0 : have[1] ? 1 : PICK_ADD;
	char status[96] = "";
	bool adding = false, dirty = true, touchWas = false, upWas = false, downWas = false, okWas = false;
	int result = 0;   // 0 running, 1 chosen, -1 quit
	const GsGameInfo* gi[2] = {gsGameInfo(GS_GAME_1), gsGameInfo(GS_GAME_2)};
	while (result == 0) {
		PlInput in;
		plPollInput(&in);
		if (quitRequested || backPressed) { result = -1; break; }
		if (adding) {
#ifdef __ANDROID__
			int st = jniStaticInt("pickState");
			if (st != 1) {
				adding = false;
				have[0] = gsGameRomExists(GS_GAME_1); have[1] = gsGameRomExists(GS_GAME_2);
				snprintf(status, sizeof(status), "%s", st == 2 ? "ROM added" : st == 3 ? "Not a Golden Sun ROM (header check)" : st == 4 ? "Could not read that file" : "");
				dirty = true;
			}
#endif
		} else {
			bool up = (in.keys & (1u << GBA_KEY_UP)) != 0, down = (in.keys & (1u << GBA_KEY_DOWN)) != 0, ok = (in.keys & ((1u << GBA_KEY_A) | (1u << GBA_KEY_START))) != 0;
			int act = PICK_NONE;
			if (up && !upWas) { sel = (sel + PICK_COUNT - 1) % PICK_COUNT; dirty = true; status[0] = 0; }
			if (down && !downWas) { sel = (sel + 1) % PICK_COUNT; dirty = true; status[0] = 0; }
			if (ok && !okWas) act = sel;
			if (in.touch && !touchWas) { int h = pickHit(in.tx, in.ty); if (h != PICK_NONE) { sel = h; dirty = true; act = h; } }
			upWas = up; downWas = down; okWas = ok; touchWas = in.touch;
			if (act != PICK_NONE) {
				if (act < 2 && have[act]) { *out = (GsGameId) act; result = 1; }
				else {
#ifdef __ANDROID__
					status[0] = 0;
					jniPickRom();
					adding = true;
#else
					snprintf(status, sizeof(status), "%s", "Copy the ROM into the data folder (see README)");
#endif
					dirty = true;
				}
			}
		}
		if (dirty || surfaceStale) {
			pickDrawTop(pickTopCanvas, status[0] ? status : sel < 2 ? gi[sel]->name : "Add a ROM file");
			pickDrawBottom(pickBotCanvas, sel, last, have);
			canvasToRows(pickTopCanvas, pickTopRows);
			canvasToRows(pickBotCanvas, pickBotRows);
			SDL_UpdateTexture(tTex, NULL, pickTopRows, GFX_W * 2);
			SDL_UpdateTexture(bTex, NULL, pickBotRows, GFX_W * 2);
			if (dualScreen) drawSurface(swapScreens ? pickBotRows : pickTopRows, GFX_W, GFX_H, GFX_W, !swapScreens && topFill);   // the canvas that is not in this window
			dirty = false;
			surfaceStale = false;
		}
		layout();
		SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
		SDL_RenderClear(ren);
		if (topDst.w > 0) SDL_RenderCopy(ren, tTex, NULL, &topDst);
		if (botDst.w > 0) SDL_RenderCopy(ren, bTex, NULL, &botDst);
		if (shotPath && ++frameNo >= shotFrames) {
			SDL_Surface* sf = SDL_CreateRGBSurfaceWithFormat(0, outW, outH, 32, SDL_PIXELFORMAT_ARGB8888);
			SDL_RenderReadPixels(ren, NULL, SDL_PIXELFORMAT_ARGB8888, sf->pixels, sf->pitch);
			SDL_SaveBMP(sf, shotPath);
			SDL_FreeSurface(sf);
			quitRequested = true;
		}
		SDL_RenderPresent(ren);
		SDL_Delay(16);
	}
	pickerMode = false;
	SDL_DestroyTexture(tTex);
	SDL_DestroyTexture(bTex);
	if (result == 1) { FILE* f = fopen(lastPath, "w"); if (f) { fprintf(f, "%d", (int) *out); fclose(f); } }
	return result == 1;
}

bool plChooseGame(GsGameId* out) {
	const char* g = getenv("GS_GAME");
	const bool have[2] = {gsGameRomExists(GS_GAME_1), gsGameRomExists(GS_GAME_2)};
	if (g && (atoi(g) == 1 || atoi(g) == 2) && have[atoi(g) - 1]) { *out = (GsGameId) (atoi(g) - 1); return true; }
#ifndef __ANDROID__
	if (!getenv("GS_PICKER")) {   // desktop: straight into the game unless asked for the pick screen
		if (!have[0] && !have[1]) {
			char a[128], b[128], msg[300];
			snprintf(msg, sizeof(msg), "No ROM found. Put your ROM at:%c%s%c(Lost Age, optional:%c%s)", 10, gsGameRomPath(GS_GAME_1, a, sizeof(a)), 10, 10, gsGameRomPath(GS_GAME_2, b, sizeof(b)));
			plFatal(msg);
			return false;
		}
		*out = have[0] ? GS_GAME_1 : GS_GAME_2;
		return true;
	}
#endif
	return pickerLoop(out);
}

bool plInit(void) {
	uiShowHotkeys = true;   // this platform has hotkeys: the Menu tab gets a "Controls" list
	SDL_SetHint(SDL_HINT_ANDROID_TRAP_BACK_BUTTON, "1");   // Back must not close the app in the middle of a game
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
	win = SDL_CreateWindow("Golden Sun - Dual Screen Companion", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, ww, wh, SDL_WINDOW_RESIZABLE | FULLSCREEN_FLAG | (shotPath ? SDL_WINDOW_HIDDEN : 0));
	ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);   // no vsync: plPace holds the 59.73 Hz
	if (!win || !ren) { fprintf(stderr, "SDL: %s\n", SDL_GetError()); return false; }
	topTex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING, 256, 160);
	botTex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING, GFX_W, GFX_H);
	SDL_SetTextureScaleMode(topTex, SDL_ScaleModeNearest);
	SDL_SetTextureScaleMode(botTex, SDL_ScaleModeNearest);
	{
		FILE* pf = fopen(padModePath(), "r");
		int v;
		if (pf) { if (fscanf(pf, "%d", &v) == 1 && v >= 0 && v <= 2) padMode = v; fclose(pf); }
	}
	{
		FILE* sf = fopen(swapPath(), "r");
		int v;
		if (sf) { if (fscanf(sf, "%d", &v) == 1) swapScreens = v == 1; fclose(sf); }
	}
	{
		FILE* ff = fopen(flipPath(), "r");
		int v;
		if (ff) { if (fscanf(ff, "%d", &v) == 1) faceFlip = v == 1; fclose(ff); }
	}
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
