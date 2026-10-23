#include <math.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "art.h"
#include "gs_ram.h"
#include "art_data.h"

typedef struct {
	float r, g, b, a;  // colour + coverage of one sample
} Sample;

typedef Sample (*ShadeFn)(float u, float v, const void* ctx);

static float clamp01(float x) { return x < 0 ? 0 : x > 1 ? 1 : x; }
static float lerp(float a, float b, float t) { return a + (b - a) * t; }
static Sample col(float r, float g, float b) { Sample s = {clamp01(r), clamp01(g), clamp01(b), 1}; return s; }
static Sample colv(const float* c, float k) { return col(c[0] * k, c[1] * k, c[2] * k); }
static Sample colmix(const float* a, const float* b, float t) { return col(lerp(a[0], b[0], t), lerp(a[1], b[1], t), lerp(a[2], b[2], t)); }
static Sample none(void) { Sample s = {0, 0, 0, 0}; return s; }
static bool inCircle(float u, float v, float cx, float cy, float r) {
	float du = u - cx, dv = v - cy;
	return du * du + dv * dv < r * r;
}
static bool inEllipse(float u, float v, float cx, float cy, float rx, float ry) {
	float du = (u - cx) / rx, dv = (v - cy) / ry;
	return du * du + dv * dv < 1;
}
static bool inBox(float u, float v, float x0, float y0, float x1, float y1) {
	return u >= x0 && u <= x1 && v >= y0 && v <= y1;
}
// Distance from (u,v) to the segment (ax,ay)-(bx,by).
static float segDist(float u, float v, float ax, float ay, float bx, float by) {
	float dx = bx - ax, dy = by - ay, t = ((u - ax) * dx + (v - ay) * dy) / (dx * dx + dy * dy);
	t = clamp01(t);
	float px = ax + dx * t - u, py = ay + dy * t - v;
	return sqrtf(px * px + py * py);
}
// Four-pointed (astroid) star of radius r; thin > 1 makes the points sharper.
static bool inStar4(float u, float v, float cx, float cy, float r) {
	float a = fabsf(u - cx) / r, b = fabsf(v - cy) / r;
	return powf(a, 0.6667f) + powf(b, 0.6667f) < 1.0f;
}
static void rot(float* u, float* v, float a) {
	float c = cosf(a), s = sinf(a), x = *u * c - *v * s, y = *u * s + *v * c;
	*u = x; *v = y;
}

// Render a w*h sprite by averaging 3x3 samples of fn over unit coordinates mapped by (cx,cy,scale).
static Sprite* render(int w, int h, float cx, float cy, float scale, ShadeFn fn, const void* ctx) {
	Sprite* s = malloc(sizeof(Sprite));
	s->w = w; s->h = h;
	s->col = malloc(w * h * sizeof(uint16_t));
	s->alpha = malloc(w * h);
	for (int y = 0; y < h; ++y) {
		for (int x = 0; x < w; ++x) {
			float r = 0, g = 0, b = 0, a = 0;
			for (int sy = 0; sy < 3; ++sy) {
				for (int sx = 0; sx < 3; ++sx) {
					float u = (x + (sx + 0.5f) / 3 - cx) / scale;
					float v = (y + (sy + 0.5f) / 3 - cy) / scale;
					Sample p = fn(u, v, ctx);
					r += p.r * p.a; g += p.g * p.a; b += p.b * p.a; a += p.a;
				}
			}
			int i = y * w + x;
			if (a > 0) {
				s->col[i] = RGB((int) (r / a * 255), (int) (g / a * 255), (int) (b / a * 255));
				s->alpha[i] = (uint8_t) (a / 9 * 255);
			} else {
				s->col[i] = 0;
				s->alpha[i] = 0;
			}
		}
	}
	return s;
}

// ===================== Icons (unit space -1..1, y down) =====================

// ---- Party: four coloured figures with spiky/long hair silhouettes ----
static bool figureHead(float u, float v, float cx, float cy, int kind) {
	if (inCircle(u, v, cx, cy, 0.25f)) return true;                       // face
	if (kind == 0 || kind == 1) {                                         // spiky hair
		for (int i = -2; i <= 2; ++i) {
			float a = (i * 0.5f) + (kind == 0 ? 0.25f : 0), len = 0.44f - fabsf(i) * 0.06f;
			if (segDist(u, v, cx, cy - 0.05f, cx + sinf(a) * len, cy - 0.05f - cosf(a) * len) < 0.1f - fabsf(i) * 0.012f) return true;
		}
		return inCircle(u, v, cx, cy - 0.06f, 0.3f);
	}
	if (kind == 2) return inCircle(u, v, cx, cy - 0.05f, 0.31f) || inEllipse(u, v, cx, cy + 0.12f, 0.33f, 0.33f);   // long hair
	return inCircle(u, v, cx, cy - 0.05f, 0.3f) || inCircle(u, v, cx + 0.04f, cy - 0.36f, 0.08f);                 // tuft
}

static Sample __attribute__((unused)) iconParty(float u, float v, const void* ctx) {
	(void) ctx;
	static const float cx[4] = {-0.74f, -0.25f, 0.25f, 0.74f};
	static const float top[4][3] = {{1.0f, 0.88f, 0.4f}, {1.0f, 0.55f, 0.35f}, {0.5f, 0.75f, 1.0f}, {0.55f, 0.95f, 0.55f}};
	static const float bot[4][3] = {{0.78f, 0.55f, 0.15f}, {0.7f, 0.22f, 0.12f}, {0.2f, 0.4f, 0.8f}, {0.15f, 0.55f, 0.2f}};
	static const int kind[4] = {0, 1, 2, 3};
	static const int order[4] = {0, 3, 1, 2};          // outer figures first, inner ones in front
	for (int k = 3; k >= 0; --k) {
		int i = order[k];
		float hy = -0.22f + (i == 0 || i == 3 ? 0.06f : 0);
		bool body = inEllipse(u, v, cx[i], hy + 0.78f, 0.34f, 0.46f) && v < 0.95f;
		if (figureHead(u, v, cx[i], hy, kind[i]) || body) {
			float t = clamp01((v + 0.8f) / 1.7f);
			float shade = (u > cx[i] + 0.1f) ? 0.88f : 1.0f;
			return col(lerp(top[i][0], bot[i][0], t) * shade, lerp(top[i][1], bot[i][1], t) * shade, lerp(top[i][2], bot[i][2], t) * shade);
		}
	}
	return none();
}

// ---- Psynergy: glowing swirl orb cupped by a hand ----
static Sample __attribute__((unused)) iconPsynergy(float u, float v, const void* ctx) {
	(void) ctx;
	// orb with spiral, upper right
	float ou = u - 0.28f, ov = v + 0.3f, r = sqrtf(ou * ou + ov * ov);
	if (r < 0.52f) {
		float th = atan2f(ov, ou), d = 9;
		for (int k = 0; k < 3; ++k) {
			float rr = (th + 3.14159f + 6.28318f * k) * 0.07f;
			float dd = fabsf(r - rr);
			if (dd < d) d = dd;
		}
		if (d < 0.05f) return col(0.85f, 1.0f, 1.0f);
		float glow = 1.0f - r / 0.52f;
		return col(lerp(0.1f, 0.45f, glow), lerp(0.5f, 0.85f, glow), 1.0f);
	}
	if (r < 0.64f) return col(0.2f, 0.55f, 1.0f);   // outer glow ring
	// hand: palm + fingers pointing up-right
	float hu = u + 0.38f, hv = v - 0.34f;
	rot(&hu, &hv, 0.55f);
	bool palm = inEllipse(hu, hv, 0, 0.05f, 0.3f, 0.26f);
	bool fingers = false;
	for (int i = 0; i < 4 && !fingers; ++i) {
		float fx = -0.2f + i * 0.14f, tip = -0.62f + (i == 1 || i == 2 ? -0.1f : 0);
		fingers = segDist(hu, hv, fx, -0.1f, fx, tip) < 0.06f;
	}
	bool thumb = segDist(hu, hv, 0.22f, 0.12f, 0.46f, -0.1f) < 0.07f;
	if (palm || fingers || thumb) {
		float t = clamp01((hv + 0.7f) / 1.0f);
		return col(lerp(0.75f, 0.2f, t), lerp(0.95f, 0.5f, t), 1.0f);
	}
	// sparkles
	if (inStar4(u, v, -0.78f, -0.62f, 0.14f) || inStar4(u, v, 0.8f, 0.5f, 0.12f) || inStar4(u, v, -0.15f, -0.82f, 0.1f)) return col(0.8f, 0.95f, 1.0f);
	return none();
}

// ---- Djinn: four elemental orbs around a gold compass star ----
static Sample orb(float u, float v, float cx, float cy, const float* c) {
	float du = u - cx, dv = v - cy, r = sqrtf(du * du + dv * dv);
	if (r > 0.34f) return none();
	float hl = clamp01(1.0f - sqrtf((du + 0.1f) * (du + 0.1f) + (dv + 0.12f) * (dv + 0.12f)) / 0.3f);
	float edge = clamp01((r - 0.22f) / 0.12f);
	return col(c[0] * (1 - 0.45f * edge) + 0.5f * hl * hl, c[1] * (1 - 0.45f * edge) + 0.5f * hl * hl, c[2] * (1 - 0.45f * edge) + 0.5f * hl * hl);
}

static Sample __attribute__((unused)) iconDjinn(float u, float v, const void* ctx) {
	(void) ctx;
	static const float venus[3] = {0.78f, 0.5f, 0.22f}, mars[3] = {0.95f, 0.28f, 0.14f}, mercury[3] = {0.22f, 0.5f, 0.95f}, jupiter[3] = {0.3f, 0.75f, 0.42f};
	Sample s;
	s = orb(u, v, -0.4f, -0.4f, venus); if (s.a) return s;
	s = orb(u, v, 0.4f, -0.4f, mars); if (s.a) return s;
	s = orb(u, v, -0.4f, 0.4f, mercury); if (s.a) return s;
	s = orb(u, v, 0.4f, 0.4f, jupiter); if (s.a) return s;
	float r = sqrtf(u * u + v * v);
	if (r > 0.78f && r < 0.85f && fabsf(u) > 0.2f) return col(0.95f, 0.8f, 0.35f);          // ring segments
	if (inStar4(u, v, 0, 0, 0.98f)) return col(1.0f, 0.88f, 0.4f);                              // compass star behind
	return none();
}

// ---- Map: parchment scroll with green land and a red compass star ----
static Sample __attribute__((unused)) iconMap(float u, float v, const void* ctx) {
	(void) ctx;
	bool roll = inEllipse(u, v, -0.8f, 0, 0.15f, 0.7f) || inEllipse(u, v, 0.8f, 0, 0.15f, 0.7f);
	bool sheet = inBox(u, v, -0.8f, -0.62f, 0.8f, 0.62f);
	if (roll) {
		float t = clamp01((u + 1) / 0.4f);
		return col(0.74f - 0.2f * (u > 0 ? 1 - clamp01((u - 0.65f) / 0.3f) : t), 0.52f, 0.28f);
	}
	if (!sheet) return none();
	if (inStar4(u, v, 0.42f, -0.08f, 0.28f)) return col(0.9f, 0.18f, 0.2f);
	bool land = inCircle(u, v, -0.32f, -0.05f, 0.3f) || inCircle(u, v, 0.0f, 0.12f, 0.3f) || inCircle(u, v, -0.5f, 0.22f, 0.2f) || inCircle(u, v, 0.18f, -0.22f, 0.22f);
	if (land && !inCircle(u, v, 0.42f, -0.08f, 0.2f)) return col(0.38f, 0.62f, 0.26f);
	float t = clamp01((v + 0.62f) / 1.24f);
	return col(lerp(0.98f, 0.86f, t), lerp(0.88f, 0.7f, t), lerp(0.6f, 0.42f, t));
}

// ---- Items: leather satchel with a gold buckle ----
static Sample __attribute__((unused)) iconItems(float u, float v, const void* ctx) {
	(void) ctx;
	// strap arc over the top
	float sr = sqrtf(u * u + (v + 0.2f) * (v + 0.2f));
	if (v < -0.35f && sr > 0.58f && sr < 0.7f && fabsf(u) < 0.55f) return col(0.45f, 0.26f, 0.12f);
	float flapEdge = 0.12f + 0.2f * (1 - (u / 0.78f) * (u / 0.78f));
	bool body = inBox(u, v, -0.78f, -0.3f, 0.78f, 0.82f) && !(fabsf(u) > 0.62f && v > 0.66f && (fabsf(u) - 0.62f) * (fabsf(u) - 0.62f) + (v - 0.66f) * (v - 0.66f) > 0.04f);
	bool flap = inBox(u, v, -0.78f, -0.45f, 0.78f, flapEdge);
	if (inBox(u, v, -0.1f, flapEdge - 0.06f, 0.1f, flapEdge + 0.16f)) {                      // buckle
		if (inBox(u, v, -0.04f, flapEdge, 0.04f, flapEdge + 0.1f)) return col(0.35f, 0.2f, 0.08f);
		return col(1.0f, 0.82f, 0.3f);
	}
	if (flap) {
		float edge = flapEdge - v;
		if (edge < 0.04f || fabsf(fabsf(u) - 0.78f) < 0.04f) return col(0.32f, 0.17f, 0.07f);   // stitched rim
		return col(lerp(0.66f, 0.5f, clamp01((v + 0.45f) / 0.6f)), lerp(0.36f, 0.26f, clamp01((v + 0.45f) / 0.6f)), 0.16f);
	}
	if (body) {
		float t = clamp01((v + 0.3f) / 1.1f);
		return col(lerp(0.58f, 0.34f, t), lerp(0.32f, 0.18f, t), lerp(0.14f, 0.07f, t));
	}
	return none();
}

// ---- Menu: gold gear around a compass, with a second small gear ----
static bool gearShape(float u, float v, float cx, float cy, float r, int teeth) {
	float du = u - cx, dv = v - cy, d = sqrtf(du * du + dv * dv), th = atan2f(dv, du);
	float edge = sinf(teeth * th) > 0 ? r : r * 0.82f;
	return d < edge;
}

static Sample __attribute__((unused)) iconMenu(float u, float v, const void* ctx) {
	(void) ctx;
	float t = clamp01((v + 0.9f) / 1.8f);
	// compass at the centre of the big gear
	float du = u + 0.15f, dv = v + 0.1f, r = sqrtf(du * du + dv * dv);
	if (r < 0.3f) {
		if (inStar4(u, v, -0.15f, -0.1f, 0.3f)) return col(1.0f, 0.95f, 0.7f);
		return col(0.26f, 0.17f, 0.08f);
	}
	if (r < 0.38f) return col(0.5f, 0.32f, 0.12f);
	if (gearShape(u, v, -0.15f, -0.1f, 0.66f, 8) && r > 0.38f) return col(lerp(1.0f, 0.72f, t), lerp(0.82f, 0.52f, t), lerp(0.36f, 0.16f, t));
	float su = u - 0.55f, sv = v - 0.5f, sr = sqrtf(su * su + sv * sv);
	if (sr < 0.14f) return col(0.26f, 0.17f, 0.08f);
	if (gearShape(u, v, 0.55f, 0.5f, 0.36f, 6)) return col(lerp(1.0f, 0.8f, t), lerp(0.84f, 0.6f, t), lerp(0.4f, 0.22f, t));
	return none();
}

static Sprite* icons[ICON_COUNT];
static Sprite* iconsSmall[ICON_COUNT];

// Sprite wrapper around data baked from the reference mockups (tools/genart.ps1); the arrays stay read-only.
static Sprite* fromBaked(const ArtImage* im) {
	Sprite* s = malloc(sizeof(Sprite));
	s->w = im->w; s->h = im->h;
	s->col = (uint16_t*) im->col;
	if (im->alpha) {
		s->alpha = (uint8_t*) im->alpha;
	} else {
		s->alpha = malloc(im->w * im->h);
		memset(s->alpha, 255, im->w * im->h);
	}
	return s;
}

const Sprite* artIcon(int id) {
	if (id < 0 || id >= ICON_COUNT) return NULL;
	if (!icons[id]) icons[id] = fromBaked(&artBakedIconBig[id]);
	return icons[id];
}

const Sprite* artIconSmall(int id) {
	if (id < 0 || id >= ICON_COUNT) return NULL;
	if (!iconsSmall[id]) iconsSmall[id] = fromBaked(&artBakedIconSmall[id]);
	return iconsSmall[id];
}

// ===================== Portraits =====================

typedef struct {
	const char* name;
	float skin[3], hair[3], hairHi[3], hairLo[3], eye[3], cloth[3], clothLo[3], accent[3], bgTop[3], bgBot[3];
	int who;   // 0 Isaac, 1 Garet, 2 Ivan, 3 Mia, 4 Jenna, 5 Sheba, 6 Felix, 7 unknown
} Look;

static const Look looks[] = {
	{"Isaac", {0.98f, 0.8f, 0.64f}, {0.86f, 0.6f, 0.2f}, {1.0f, 0.82f, 0.4f}, {0.55f, 0.32f, 0.1f}, {0.2f, 0.45f, 0.95f}, {0.2f, 0.4f, 0.8f}, {0.1f, 0.2f, 0.5f}, {0.95f, 0.8f, 0.2f}, {0.08f, 0.16f, 0.4f}, {0.03f, 0.07f, 0.2f}, 0},
	{"Garet", {0.96f, 0.74f, 0.58f}, {0.82f, 0.32f, 0.16f}, {1.0f, 0.55f, 0.3f}, {0.5f, 0.15f, 0.08f}, {0.5f, 0.26f, 0.12f}, {0.55f, 0.4f, 0.2f}, {0.3f, 0.2f, 0.1f}, {0.95f, 0.75f, 0.3f}, {0.08f, 0.16f, 0.4f}, {0.03f, 0.07f, 0.2f}, 1},
	{"Ivan", {0.98f, 0.82f, 0.66f}, {0.9f, 0.78f, 0.2f}, {1.0f, 0.95f, 0.5f}, {0.6f, 0.5f, 0.1f}, {0.2f, 0.65f, 0.3f}, {0.2f, 0.5f, 0.28f}, {0.1f, 0.3f, 0.15f}, {0.6f, 0.4f, 0.75f}, {0.08f, 0.16f, 0.4f}, {0.03f, 0.07f, 0.2f}, 2},
	{"Mia", {0.99f, 0.84f, 0.72f}, {0.15f, 0.7f, 0.78f}, {0.45f, 0.95f, 0.95f}, {0.05f, 0.42f, 0.55f}, {0.25f, 0.5f, 0.95f}, {0.92f, 0.92f, 0.96f}, {0.6f, 0.62f, 0.8f}, {0.6f, 0.4f, 0.85f}, {0.08f, 0.16f, 0.4f}, {0.03f, 0.07f, 0.2f}, 3},
	{"Jenna", {0.98f, 0.8f, 0.66f}, {0.9f, 0.42f, 0.18f}, {1.0f, 0.65f, 0.35f}, {0.6f, 0.22f, 0.08f}, {0.5f, 0.3f, 0.15f}, {0.78f, 0.28f, 0.24f}, {0.45f, 0.1f, 0.1f}, {0.95f, 0.85f, 0.5f}, {0.08f, 0.16f, 0.4f}, {0.03f, 0.07f, 0.2f}, 4},
	{"Sheba", {0.98f, 0.82f, 0.68f}, {0.62f, 0.85f, 0.45f}, {0.85f, 1.0f, 0.65f}, {0.35f, 0.6f, 0.25f}, {0.3f, 0.55f, 0.35f}, {0.9f, 0.9f, 0.78f}, {0.55f, 0.65f, 0.5f}, {0.4f, 0.7f, 0.45f}, {0.08f, 0.16f, 0.4f}, {0.03f, 0.07f, 0.2f}, 5},
	{"Felix", {0.94f, 0.74f, 0.58f}, {0.5f, 0.32f, 0.2f}, {0.75f, 0.52f, 0.34f}, {0.3f, 0.18f, 0.1f}, {0.2f, 0.4f, 0.55f}, {0.35f, 0.42f, 0.55f}, {0.2f, 0.25f, 0.35f}, {0.7f, 0.7f, 0.75f}, {0.08f, 0.16f, 0.4f}, {0.03f, 0.07f, 0.2f}, 6},
	{"?", {0.8f, 0.75f, 0.7f}, {0.5f, 0.52f, 0.6f}, {0.7f, 0.72f, 0.8f}, {0.3f, 0.32f, 0.4f}, {0.3f, 0.4f, 0.6f}, {0.4f, 0.45f, 0.6f}, {0.2f, 0.24f, 0.35f}, {0.6f, 0.6f, 0.7f}, {0.08f, 0.16f, 0.4f}, {0.03f, 0.07f, 0.2f}, 7},
};
#define LOOK_COUNT ((int) (sizeof(looks) / sizeof(looks[0])))
static Sprite* portraits[LOOK_COUNT];

// Spike table per hairstyle: {angle from straight up (radians, + = right), length, half-width (radians)}.
typedef struct { float ang, len, w; } Spike;
static const Spike spikesIsaac[] = {{-1.1f, 0.2f, 0.4f}, {-0.7f, 0.26f, 0.32f}, {-0.3f, 0.34f, 0.3f}, {0.15f, 0.3f, 0.3f}, {0.6f, 0.22f, 0.3f}, {0.95f, 0.14f, 0.3f}};
static const Spike spikesGaret[] = {{-1.0f, 0.26f, 0.26f}, {-0.6f, 0.4f, 0.2f}, {-0.2f, 0.5f, 0.2f}, {0.2f, 0.52f, 0.2f}, {0.6f, 0.42f, 0.2f}, {1.0f, 0.26f, 0.26f}};
static const Spike spikesIvan[] = {{0.05f, 0.2f, 0.12f}, {-0.5f, 0.07f, 0.4f}, {0.55f, 0.07f, 0.4f}};
static const Spike spikesFelix[] = {{-0.8f, 0.2f, 0.3f}, {-0.3f, 0.3f, 0.25f}, {0.2f, 0.32f, 0.25f}, {0.7f, 0.2f, 0.3f}};

static float hairRadius(const Look* L, float ang) {
	const Spike* sp = NULL; int n = 0; float base = 0.56f;
	switch (L->who) {
	case 0: sp = spikesIsaac; n = 6; base = 0.54f; break;
	case 1: sp = spikesGaret; n = 6; base = 0.5f; break;
	case 2: sp = spikesIvan; n = 3; base = 0.58f; break;
	case 6: sp = spikesFelix; n = 4; base = 0.52f; break;
	default: base = 0.58f; break;
	}
	float r = base;
	for (int i = 0; i < n; ++i) {
		float d = fabsf(ang - sp[i].ang);
		if (d < sp[i].w) r += sp[i].len * powf(1.0f - d / sp[i].w, 1.3f);
	}
	return r;
}

static Sample portraitFn(float u, float v, const void* ctx) {
	const Look* L = ctx;
	int who = L->who;
	float hx = 0.04f, hy = -0.14f;
	bool longHair = who == 3 || who == 4 || who == 5;
	// ---- eyes, brows, nose, mouth (3/4 view: features sit slightly right of centre) ----
	for (int side = -1; side <= 1; side += 2) {
		float ex = 0.04f + side * 0.2f, ey = 0.1f, rx = side < 0 ? 0.1f : 0.088f;
		if (inEllipse(u, v, ex, ey, rx, 0.12f)) {
			float lid = (v - ey) / 0.12f;
			if (lid < -0.62f) return col(0.12f, 0.08f, 0.1f);                                   // upper lid line
			if (inCircle(u, v, ex + side * 0.012f, ey + 0.015f, 0.082f)) {
				if (inCircle(u, v, ex + side * 0.012f, ey + 0.02f, 0.04f)) return col(0.04f, 0.04f, 0.1f);
				if (inCircle(u, v, ex - 0.035f, ey - 0.04f, 0.026f)) return col(1, 1, 1);        // highlight
				float t = clamp01((v - ey + 0.08f) / 0.17f);
				return col(L->eye[0] * (0.7f + 0.5f * t), L->eye[1] * (0.7f + 0.5f * t), L->eye[2] * (0.7f + 0.5f * t));
			}
			return col(0.98f, 0.97f, 1.0f);
		}
		float tilt = who == 1 ? side * 0.05f : 0.0f;                                            // Garet: angry brows
		if (fabsf(v - (ey - 0.19f - tilt * (0.2f - fabsf(u - 0.04f)) * 5)) < 0.026f && fabsf(u - ex) < 0.12f) return colv(L->hairLo, 1.0f);
	}
	if (inBox(u, v, 0.04f, 0.2f, 0.08f, 0.27f)) return col(L->skin[0] * 0.8f, L->skin[1] * 0.7f, L->skin[2] * 0.66f);   // nose shadow
	if (inBox(u, v, -0.02f, 0.35f, 0.14f, 0.375f)) return col(0.78f, 0.3f, 0.3f);                                    // mouth
	// ---- headband (Mia) ----
	float dxh = u - hx, dyh = v - hy;
	float hairAng = atan2f(dxh, -dyh);                // 0 = up, + = right
	float hr = sqrtf(dxh * dxh + dyh * dyh);
	if (who == 3) {
		float bandV = -0.3f + 0.16f * (u - hx) * (u - hx);
		if (fabsf(v - bandV) < 0.05f && hr < 0.62f && u > -0.5f && u < 0.54f) return col(0.96f, 0.96f, 1.0f);
		if (inCircle(u, v, -0.5f, -0.2f, 0.07f)) return col(0.9f, 0.85f, 0.7f);                // ribbon knot
	}
	// ---- front hair ----
	float bangs = -0.05f + 0.07f * sinf(u * 11.0f + 1.0f) + (longHair ? 0.05f : 0.0f) + (who == 2 ? 0.06f : 0.0f);
	bool hairTop = hr < hairRadius(L, hairAng) && v < bangs + 0.0f;
	bool sideLocks = fabsf(u - hx) > 0.34f && fabsf(u - hx) < 0.58f && v > -0.12f && v < (longHair ? 0.95f : 0.18f) && hr < 0.95f;
	if (longHair && (fabsf(u - hx) > 0.36f && fabsf(u - hx) < 0.7f && v > -0.1f && v < 0.98f)) sideLocks = true;
	if (hairTop || sideLocks) {
		float l = clamp01(0.5f + (-(u - hx) * 0.6f - (v - hy) * 0.7f));                           // light from the upper left
		float strand = 0.5f + 0.5f * sinf(hairAng * 11.0f + hr * 7.0f);
		Sample c = colmix(L->hairLo, L->hairHi, clamp01(l * 0.85f + strand * 0.15f - 0.05f));
		if (hairTop && v > bangs - 0.04f) { c.r *= 0.8f; c.g *= 0.8f; c.b *= 0.8f; }
		return c;
	}
	// ---- ears ----
	if (inEllipse(u, v, hx - 0.43f, 0.08f, 0.07f, 0.1f)) return col(L->skin[0] * 0.92f, L->skin[1] * 0.8f, L->skin[2] * 0.74f);
	// ---- face ----
	if (inEllipse(u, v, hx, 0.02f, 0.4f, 0.5f)) {
		float sh = 1.0f - 0.18f * clamp01((u - hx - 0.1f) * 2.0f) - 0.12f * clamp01((bangs + 0.08f - v) * 4.0f);
		if (inEllipse(u, v, hx - 0.26f, 0.27f, 0.07f, 0.045f) || inEllipse(u, v, hx + 0.3f, 0.27f, 0.06f, 0.04f)) return col(L->skin[0] * 1.0f, L->skin[1] * 0.78f * sh, L->skin[2] * 0.72f * sh);   // blush
		return col(L->skin[0] * sh, L->skin[1] * sh, L->skin[2] * sh);
	}
	// ---- neck ----
	if (inBox(u, v, hx - 0.16f, 0.4f, hx + 0.16f, 0.7f)) return col(L->skin[0] * 0.8f, L->skin[1] * 0.72f, L->skin[2] * 0.68f);
	// ---- accessories in front of the clothes ----
	if (who == 0) {                      // Isaac: chain over the left shoulder
		for (int i = 0; i < 7; ++i) {
			float cu = -0.95f + i * 0.1f, cv = 0.56f + 0.16f * sinf(i * 0.6f);
			if (inCircle(u, v, cu, cv, 0.05f)) return col(0.6f + 0.1f * (i & 1), 0.62f, 0.68f);
		}
	}
	if (who == 2) {                      // Ivan: staff with a ring head
		if (segDist(u, v, 1.05f, 1.0f, 1.18f, -0.1f) < 0.07f) return col(0.52f, 0.32f, 0.14f);
		float rr = sqrtf((u - 1.19f) * (u - 1.19f) + (v + 0.16f) * (v + 0.16f));
		if (rr > 0.08f && rr < 0.17f) return col(0.62f, 0.4f, 0.18f);
	}
	// ---- clothes ----
	bool shoulders = inEllipse(u, v, hx, 1.12f, 1.05f, 0.62f);
	if (shoulders) {
		float t = clamp01((v - 0.55f) / 0.5f);
		Sample cloth = colmix(L->cloth, L->clothLo, t);
		if (who == 0) {                  // yellow scarf wrapped at the neck
			float sv = 0.62f + 0.07f * sinf(u * 9.0f);
			if (v > 0.5f && v < sv + 0.2f && fabsf(u - hx) < 0.62f) { float sl = clamp01((v - 0.5f) / 0.3f); return col(L->accent[0] * (1 - 0.35f * sl), L->accent[1] * (1 - 0.35f * sl), L->accent[2] * (1 - 0.6f * sl)); }
		}
		if (who == 1) {                  // gold shoulder armor
			bool pad = inEllipse(u, v, hx - 0.62f, 0.78f, 0.4f, 0.28f) || inEllipse(u, v, hx + 0.62f, 0.78f, 0.4f, 0.28f);
			if (pad) { float hl = clamp01(0.6f - (v - 0.62f) * 2.0f); return col(0.8f + 0.2f * hl, 0.58f + 0.25f * hl, 0.2f + 0.15f * hl); }
		}
		if (who == 2) {                  // green cloak with a purple collar
			if (v > 0.5f && v < 0.72f && fabsf(u - hx) < 0.34f) return col(L->accent[0], L->accent[1], L->accent[2]);
		}
		if (who == 3) {                  // white robe with a violet gem collar
			if (v > 0.52f && v < 0.7f && fabsf(u - hx) < 0.3f) return col(L->accent[0] * 0.9f, L->accent[1] * 0.85f, L->accent[2]);
		}
		return cloth;
	}
	// ---- long hair behind the shoulders (and the back of short hair) ----
	if (longHair && inEllipse(u, v, hx, 0.15f, 0.72f, 0.95f)) return colv(L->hairLo, 1.05f);
	if (!longHair && hr < hairRadius(L, hairAng) + 0.05f && v < 0.12f) return colv(L->hairLo, 1.0f);
	// ---- background: deep navy with a soft glow behind the head ----
	float t = clamp01((v + 1.0f) / 2.0f);
	float glow = clamp01(1.0f - sqrtf((u - hx) * (u - hx) + (v + 0.1f) * (v + 0.1f)) / 1.3f);
	return col(lerp(L->bgTop[0], L->bgBot[0], t) + 0.03f * glow, lerp(L->bgTop[1], L->bgBot[1], t) + 0.06f * glow, lerp(L->bgTop[2], L->bgBot[2], t) + 0.14f * glow);
}

const Sprite* artPortraitRec(int rec, const char* name) {
	static Sprite* byRec[8];
	if (rec < 0 || rec >= 8) return artPortrait(name);
	if (!byRec[rec]) byRec[rec] = fromBaked(&artBakedPortrait[rec]);   // baked order == record order
	return byRec[rec];
}

const Sprite* artPortrait(const char* name) {
	// The four Golden Sun heroes come from the reference mockup, the four Lost Age characters from the game's own portraits; anything else falls back to the procedural faces below.
	static Sprite* baked[8];
	for (int i = 0; name && i < 8; ++i) {
		if (!strcmp(name, artBakedPortraitNames[i])) {
			if (!baked[i]) baked[i] = fromBaked(&artBakedPortrait[i]);
			return baked[i];
		}
	}
	int idx = LOOK_COUNT - 1;
	for (int i = 0; i < LOOK_COUNT - 1; ++i) {
		if (name && !strcmp(name, looks[i].name)) { idx = i; break; }
	}
	if (!portraits[idx]) portraits[idx] = render(PORTRAIT_W, PORTRAIT_H, PORTRAIT_W / 2.0f - 2, PORTRAIT_H / 2.0f + 2, 26.0f, portraitFn, &looks[idx]);
	return portraits[idx];
}

const Sprite* artItemIcon(unsigned id) {
	static Sprite* cache[2][512];
	int g = gsRam() == &gsRamTla;   // the two games have different id -> icon tables
	const uint16_t* ids = g ? artItemIconIdsTla : artItemIconIds;
	const ArtImage* imgs = g ? artItemIconImagesTla : artItemIconImages;
	int lo = 0, hi = (g ? artItemIconCountTla : artItemIconCount) - 1;
	while (lo <= hi) {
		int mid = (lo + hi) / 2;
		if (ids[mid] == id) {
			if (!cache[g][mid]) cache[g][mid] = fromBaked(&imgs[mid]);
			return cache[g][mid];
		}
		if (ids[mid] < id) lo = mid + 1; else hi = mid - 1;
	}
	return NULL;
}

// Box-filters a 16x16 image to 8x8 (alpha-weighted).
static Sprite* halve(const ArtImage* im) {
	Sprite* sp = malloc(sizeof(Sprite));
	sp->w = 8; sp->h = 8;
	sp->col = malloc(8 * 8 * sizeof(uint16_t));
	sp->alpha = malloc(8 * 8);
	for (int y = 0; y < 8; ++y) {
		for (int x = 0; x < 8; ++x) {
			unsigned r = 0, g = 0, b = 0, a = 0;
			for (int j = 0; j < 2; ++j) {
				for (int i = 0; i < 2; ++i) {
					int k = (2 * y + j) * 16 + 2 * x + i;
					unsigned al = im->alpha ? im->alpha[k] : 255, c = im->col[k];
					r += ((c >> 11) & 31) * al; g += ((c >> 5) & 63) * al; b += (c & 31) * al; a += al;
				}
			}
			if (a) { r /= a; g /= a; b /= a; }
			sp->col[y * 8 + x] = (uint16_t) ((r << 11) | (g << 5) | b);
			sp->alpha[y * 8 + x] = (uint8_t) (a / 4);
		}
	}
	return sp;
}

const Sprite* artPsyIconSmall(unsigned id) {
	static Sprite* cache[2][128];
	static bool tried[2][128];
	int g = gsRam() == &gsRamTla;
	const uint16_t* ids = g ? artPsyIconIdsTla : artPsyIconIds;
	const ArtImage* imgs = g ? artPsyIconImagesTla : artPsyIconImages;
	int lo = 0, hi = (g ? artPsyIconCountTla : artPsyIconCount) - 1;
	while (lo <= hi) {
		int mid = (lo + hi) / 2;
		if (ids[mid] == id) {
			if (!tried[g][mid]) { cache[g][mid] = halve(&imgs[mid]); tried[g][mid] = true; }
			return cache[g][mid];
		}
		if (ids[mid] < id) lo = mid + 1; else hi = mid - 1;
	}
	return NULL;
}

const Sprite* artDjinn(int element) {
	static Sprite* cache[4];
	if (element < 0 || element >= 4) return NULL;   // same creatures in both games
	if (!cache[element]) cache[element] = fromBaked(&artDjinnImages[element]);
	return cache[element];
}

const Sprite* artCorner(void) {
	static Sprite* s;
	if (!s) s = fromBaked(&artBakedCorner);
	return s;
}

const Sprite* artLogo(void) {
	static Sprite* s;
	if (!s) s = fromBaked(&artBakedLogo);
	return s;
}

const Sprite* artCompass(void) {
	static Sprite* s;
	if (!s) s = fromBaked(&artBakedCompass);
	return s;
}

const uint16_t* artBackground(void) {
	return artBakedBackground;
}
