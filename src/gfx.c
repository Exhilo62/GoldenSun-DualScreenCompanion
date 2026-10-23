#include <math.h>
#include <string.h>

#include "font_data.h"
#include "gfx.h"

static uint16_t* fb;
#ifdef GFX_STATS
unsigned long gStatPx, gStatBlend, gStatRectCalls;
#define STAT(x) (++(x))
#else
#define STAT(x) ((void) 0)
#endif

// RGB565 blend with a 5-bit alpha (0..32): the packed-field trick does red/blue and green in one multiply.
static inline uint16_t blend565(uint16_t bg, uint16_t fg, unsigned a5) {
	uint32_t b = ((uint32_t) bg | ((uint32_t) bg << 16)) & 0x07E0F81Fu;
	uint32_t f = ((uint32_t) fg | ((uint32_t) fg << 16)) & 0x07E0F81Fu;
	uint32_t r = ((((f - b) * a5) >> 5) + b) & 0x07E0F81Fu;
	return (uint16_t) (r | (r >> 16));
}

uint16_t* gPixelPtr(int x, int y) {
	return &fb[x * GFX_H + (GFX_H - 1 - y)];
}

void gBegin(uint16_t* framebuffer) {
	fb = framebuffer;
}

uint16_t gMix(uint16_t a, uint16_t b, int t) {
	int ar = a >> 11, ag = (a >> 5) & 63, ab = a & 31;
	int br = b >> 11, bg = (b >> 5) & 63, bb = b & 31;
	int r = (ar * (256 - t) + br * t) >> 8;
	int g = (ag * (256 - t) + bg * t) >> 8;
	int bl = (ab * (256 - t) + bb * t) >> 8;
	return (uint16_t) ((r << 11) | (g << 5) | bl);
}

void gPx(int x, int y, uint16_t c) {
	STAT(gStatPx);
	if ((unsigned) x < GFX_W && (unsigned) y < GFX_H) {
		fb[x * GFX_H + (GFX_H - 1 - y)] = c;
	}
}

uint16_t gGet(int x, int y) {
	if ((unsigned) x < GFX_W && (unsigned) y < GFX_H) {
		return fb[x * GFX_H + (GFX_H - 1 - y)];
	}
	return 0;
}

// Copies a w x h RGB565 image (row-major, `stride` pixels per row) to (x, y); one column of the rotated framebuffer at a time.
void gBlit(int x, int y, const uint16_t* src, int stride, int w, int h) {
	if (x < 0 || y < 0 || x + w > GFX_W || y + h > GFX_H) return;
	for (int i = 0; i < w; ++i) {
		uint16_t* dst = fb + (x + i) * GFX_H + (GFX_H - 1 - y);
		const uint16_t* col = src + i;
		for (int j = 0; j < h; ++j) {
			*dst-- = *col;
			col += stride;
		}
	}
}

void gPxA(int x, int y, uint16_t c, int a) {
	STAT(gStatBlend);
	if (a <= 7) return;
	if (a >= 248) { gPx(x, y, c); return; }
	if ((unsigned) x < GFX_W && (unsigned) y < GFX_H) {
		uint16_t* p = &fb[x * GFX_H + (GFX_H - 1 - y)];
		*p = blend565(*p, c, (unsigned) a >> 3);
	}
}

// Fills run down a column of the rotated framebuffer, so they are contiguous in memory.
void gRect(int x, int y, int w, int h, uint16_t c) {
	STAT(gStatRectCalls);
	if (x < 0) { w += x; x = 0; }
	if (y < 0) { h += y; y = 0; }
	if (x + w > GFX_W) w = GFX_W - x;
	if (y + h > GFX_H) h = GFX_H - y;
	if (w <= 0 || h <= 0) return;
	uint16_t* col = &fb[x * GFX_H + (GFX_H - y - h)];
	for (int i = 0; i < w; ++i, col += GFX_H) {
		for (int k = 0; k < h; ++k) col[k] = c;
	}
}

void gRectA(int x, int y, int w, int h, uint16_t c, int a) {
	if (a <= 7) return;
	if (x < 0) { w += x; x = 0; }
	if (y < 0) { h += y; y = 0; }
	if (x + w > GFX_W) w = GFX_W - x;
	if (y + h > GFX_H) h = GFX_H - y;
	if (w <= 0 || h <= 0) return;
	unsigned a5 = a >= 248 ? 32 : (unsigned) a >> 3;
	uint16_t* col = &fb[x * GFX_H + (GFX_H - y - h)];
	for (int i = 0; i < w; ++i, col += GFX_H) {
		for (int k = 0; k < h; ++k) col[k] = blend565(col[k], c, a5);
	}
}

void gVGrad(int x, int y, int w, int h, uint16_t top, uint16_t bottom) {
	static uint16_t colors[GFX_H];
	if (h > GFX_H) h = GFX_H;
	for (int j = 0; j < h; ++j) colors[j] = gMix(top, bottom, h > 1 ? j * 256 / (h - 1) : 0);
	int j0 = 0;
	if (x < 0) { w += x; x = 0; }
	if (y < 0) { j0 = -y; h += y; y = 0; }
	if (x + w > GFX_W) w = GFX_W - x;
	if (y + h > GFX_H) h = GFX_H - y;
	if (w <= 0 || h <= 0) return;
	uint16_t* col = &fb[x * GFX_H + (GFX_H - y - h)];
	for (int i = 0; i < w; ++i, col += GFX_H) {
		for (int k = 0; k < h; ++k) col[k] = colors[j0 + h - 1 - k];
	}
}

static float hash2(int x, int y) {
	unsigned h = (unsigned) x * 374761393u + (unsigned) y * 668265263u;
	h = (h ^ (h >> 13)) * 1274126177u;
	return ((h ^ (h >> 16)) & 0xFFFF) / 65535.0f;
}

static float vnoise(float x, float y) {
	int xi = (int) floorf(x), yi = (int) floorf(y);
	float fx = x - xi, fy = y - yi;
	fx = fx * fx * (3 - 2 * fx); fy = fy * fy * (3 - 2 * fy);
	float a = hash2(xi, yi), b = hash2(xi + 1, yi), c = hash2(xi, yi + 1), d = hash2(xi + 1, yi + 1);
	return (a + (b - a) * fx) + ((c + (d - c) * fx) - (a + (b - a) * fx)) * fy;
}

// Whole-screen vertical gradient, rendered once and copied afterwards.
static const uint16_t* bgImage;
void gSetBackgroundImage(const uint16_t* image) { bgImage = image; }

void gBackground(uint16_t top, uint16_t bottom) {
	static uint16_t* cache;
	static uint16_t cachedTop, cachedBottom;
	static const uint16_t* cachedImage;
	if (bgImage && (!cache || cachedImage != bgImage)) {   // bake the row-major image into the column-major cache once
		static uint16_t imgStorage[GFX_W * GFX_H];
		for (int x = 0; x < GFX_W; ++x) {
			for (int y = 0; y < GFX_H; ++y) imgStorage[x * GFX_H + (GFX_H - 1 - y)] = bgImage[y * GFX_W + x];
		}
		cache = imgStorage;
		cachedImage = bgImage;
		cachedTop = cachedBottom = 0xFFFF;
	}
	if (bgImage) {
		memcpy(fb, cache, GFX_W * GFX_H * sizeof(uint16_t));
		return;
	}
	if (!cache || cachedImage || cachedTop != top || cachedBottom != bottom) {
		cachedImage = NULL;
		static uint16_t storage[GFX_W * GFX_H];
		uint16_t* saved = fb;
		cache = storage;
		fb = storage;
		gVGrad(0, 0, GFX_W, GFX_H, top, bottom);
		// Soft cloud texture plus a vignette, baked once.
		for (int x = 0; x < GFX_W; ++x) {
			for (int y = 0; y < GFX_H; ++y) {
				float n = 0.5f * vnoise(x / 46.0f, y / 46.0f) + 0.3f * vnoise(x / 19.0f + 7, y / 19.0f) + 0.2f * vnoise(x / 8.0f, y / 8.0f + 3);
				float dx = (x - GFX_W / 2) / (float) (GFX_W / 2), dy = (y - GFX_H / 2) / (float) (GFX_H / 2);
				float vig = 1.0f - 0.38f * (dx * dx * 0.7f + dy * dy * 0.7f);
				uint16_t* p = &storage[x * GFX_H + (GFX_H - 1 - y)];
				int r = ((*p >> 11) << 3), g = (((*p >> 5) & 63) << 2), b = ((*p & 31) << 3);
				float lift = (n - 0.5f) * 30.0f;
				r = (int) ((r + lift * 0.35f) * vig); g = (int) ((g + lift * 0.7f) * vig); b = (int) ((b + lift * 1.1f) * vig);
				r = r < 0 ? 0 : r > 255 ? 255 : r; g = g < 0 ? 0 : g > 255 ? 255 : g; b = b < 0 ? 0 : b > 255 ? 255 : b;
				*p = RGB(r, g, b);
			}
		}
		fb = saved;
		cachedTop = top;
		cachedBottom = bottom;
	}
	memcpy(fb, cache, GFX_W * GFX_H * sizeof(uint16_t));
}

void gSaveRegion(int x, int y, int w, int h, uint16_t* buf) {
	for (int i = 0; i < w; ++i) memcpy(buf + i * h, &fb[(x + i) * GFX_H + (GFX_H - y - h)], h * sizeof(uint16_t));
}

void gRestoreRegion(int x, int y, int w, int h, const uint16_t* buf) {
	for (int i = 0; i < w; ++i) memcpy(&fb[(x + i) * GFX_H + (GFX_H - y - h)], buf + i * h, h * sizeof(uint16_t));
}

const Sprite* gCornerSprite;

void gPanel(int x, int y, int w, int h, uint16_t top, uint16_t bottom, uint16_t outline) {
	uint16_t bronze = RGB(120, 82, 36);
	if (w < 6 || h < 6) return;
	// Fill column by column with 2px chamfered corners so the panel reads as rounded.
	static uint16_t colors[GFX_H];
	int hh = h > GFX_H ? GFX_H : h;
	for (int j = 0; j < hh; ++j) colors[j] = gMix(top, bottom, hh > 1 ? j * 256 / (hh - 1) : 0);
	for (int i = 0; i < w; ++i) {
		int xx = x + i;
		if ((unsigned) xx >= GFX_W) continue;
		int skip = (i == 0 || i == w - 1) ? 2 : (i == 1 || i == w - 2) ? 1 : 0;
		int j0 = skip, j1 = hh - skip;      // rows [j0, j1)
		int ys = y + j0, ye = y + j1;
		if (ys < 0) { j0 -= ys; ys = 0; }
		if (ye > GFX_H) ye = GFX_H;
		if (ye <= ys) continue;
		uint16_t* col = &fb[xx * GFX_H + (GFX_H - ye)];
		for (int k = 0; k < ye - ys; ++k) col[k] = colors[j0 + (ye - ys) - 1 - k];
	}
	gRect(x + 2, y, w - 4, 1, outline);
	gRect(x + 2, y + h - 1, w - 4, 1, outline);
	gRect(x, y + 2, 1, h - 4, outline);
	gRect(x + w - 1, y + 2, 1, h - 4, outline);
	gPx(x + 1, y + 1, outline); gPx(x + w - 2, y + 1, outline);
	gPx(x + 1, y + h - 2, outline); gPx(x + w - 2, y + h - 2, outline);
	// Inner bronze line for the beveled look.
	gRect(x + 3, y + 1, w - 6, 1, bronze);
	gRect(x + 3, y + h - 2, w - 6, 1, bronze);
	gRect(x + 1, y + 3, 1, h - 6, bronze);
	gRect(x + w - 2, y + 3, 1, h - 6, bronze);
	if (w >= 60 && h >= 44) {   // ornate flourishes from the mockup on the larger panels
		if (gCornerSprite) gOrnateCorners(x, y, w, h, gCornerSprite);
	}
}

void gPill(int x, int y, int w, int h, uint16_t top, uint16_t bottom, uint16_t edge) {
	int mid = (h - 1) / 2;
	for (int j = 0; j < h; ++j) {
		int d = j < mid ? mid - j : j - mid;     // 0 on the middle row: the tips extend furthest out
		int x0 = x + d, x1 = x + w - 1 - d;
		uint16_t c = gMix(top, bottom, h > 1 ? j * 256 / (h - 1) : 0);
		gRect(x0, y + j, x1 - x0 + 1, 1, c);
		gPx(x0, y + j, edge); gPx(x1, y + j, edge);
		if (j == 0 || j == h - 1) gRect(x0, y + j, x1 - x0 + 1, 1, edge);
	}
}

static void spriteRegion(int x, int y, const Sprite* s, int sx, int sy, int sw, int sh) {
	if (sx < 0) { x -= sx; sw += sx; sx = 0; }
	if (sy < 0) { y -= sy; sh += sy; sy = 0; }
	if (sx + sw > s->w) sw = s->w - sx;
	if (sy + sh > s->h) sh = s->h - sy;
	int i0 = x < 0 ? -x : 0, j0 = y < 0 ? -y : 0;
	int i1 = x + sw > GFX_W ? GFX_W - x : sw, j1 = y + sh > GFX_H ? GFX_H - y : sh;
	for (int i = i0; i < i1; ++i) {
		uint16_t* dst = &fb[(x + i) * GFX_H + (GFX_H - 1 - (y + j0))];
		for (int j = j0; j < j1; ++j, --dst) {
			int idx = (sy + j) * s->w + sx + i;
			int a = s->alpha[idx];
			if (a >= 248) *dst = s->col[idx];
			else if (a > 7) *dst = blend565(*dst, s->col[idx], (unsigned) a >> 3);
		}
	}
}

void gSprite(int x, int y, const Sprite* s) {
	spriteRegion(x, y, s, 0, 0, s->w, s->h);
}

void gSpriteFlip(int x, int y, const Sprite* s, bool flipX, bool flipY) {
	for (int j = 0; j < s->h; ++j) {
		int sj = flipY ? s->h - 1 - j : j;
		for (int i = 0; i < s->w; ++i) {
			int si = flipX ? s->w - 1 - i : i;
			int idx = sj * s->w + si, a = s->alpha[idx];
			if (a > 7) gPxA(x + i, y + j, s->col[idx], a);
		}
	}
}

void gOrnateCorners(int x, int y, int w, int h, const Sprite* c) {
	// The corner art includes its own frame lines, so it sits flush with the panel's outer edge.
	gSpriteFlip(x - 1, y - 1, c, false, true);
	gSpriteFlip(x + w - c->w + 1, y - 1, c, true, true);
	gSpriteFlip(x - 1, y + h - c->h + 1, c, false, false);
	gSpriteFlip(x + w - c->w + 1, y + h - c->h + 1, c, true, false);
}

void gSpriteClip(int x, int y, const Sprite* s, int sx, int sy, int sw, int sh) {
	spriteRegion(x, y, s, sx, sy, sw, sh);
}

void gSpriteTint(int x, int y, const Sprite* s, uint16_t tint, int t) {
	for (int j = 0; j < s->h; ++j) {
		for (int i = 0; i < s->w; ++i) {
			int a = s->alpha[j * s->w + i];
			if (a) gPxA(x + i, y + j, gMix(s->col[j * s->w + i], tint, t), a);
		}
	}
}

static const GFont* pickFont(int scale) {
	return scale <= 0 ? &font_tiny : scale == 1 ? &font_small : &font_large;
}

int gTextW(const char* s, int scale) {
	const GFont* f = pickFont(scale);
	int w = 0;
	for (; *s; ++s) {
		int c = (unsigned char) *s;
		if (c < 32 || c > 126) c = '?';
		w += f->adv[c - 32];
	}
	return w;
}

static void glyph(int x, int y, const GFont* f, int ch, uint16_t c) {
	if (ch < 32 || ch > 126) ch = '?';
	int idx = ch - 32, w = f->adv[idx] + 2;
	const uint8_t* d = f->data + f->off[idx];
	int x0 = x - 1, y0 = y - f->capTop;
	int i0 = x0 < 0 ? -x0 : 0, i1 = x0 + w > GFX_W ? GFX_W - x0 : w;
	int j0 = y0 < 0 ? -y0 : 0, j1 = y0 + f->h > GFX_H ? GFX_H - y0 : f->h;
	for (int i = i0; i < i1; ++i) {
		uint16_t* dst = &fb[(x0 + i) * GFX_H + (GFX_H - 1 - (y0 + j0))];
		for (int j = j0; j < j1; ++j, --dst) {
			unsigned a = d[j * w + i];
			if (a < 16) continue;
			*dst = a >= 248 ? c : blend565(*dst, c, a >> 3);
		}
	}
}

// y is the top of the capital letters; descenders hang below. scale: 0 tiny, 1 body, 2 heading.
void gText(int x, int y, const char* s, int scale, uint16_t c) {
	const GFont* f = pickFont(scale);
	for (; *s; ++s) {
		int ch = (unsigned char) *s;
		glyph(x, y, f, ch, c);
		if (ch < 32 || ch > 126) ch = '?';
		x += f->adv[ch - 32];
	}
}

void gTextShadow(int x, int y, const char* s, int scale, uint16_t c) {
	gText(x + 1, y + 1, s, scale, RGB(4, 8, 20));
	gText(x, y, s, scale, c);
}

void gTextCentered(int cx, int y, const char* s, int scale, uint16_t c) {
	gTextShadow(cx - gTextW(s, scale) / 2, y, s, scale, c);
}

void gTextRight(int rx, int y, const char* s, int scale, uint16_t c) {
	gTextShadow(rx - gTextW(s, scale), y, s, scale, c);
}
