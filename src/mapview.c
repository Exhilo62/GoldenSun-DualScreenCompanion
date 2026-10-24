#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gfx.h"
#include "gs_areas.h"
#include "mapview.h"
#include "mapdecode.h"
#include "art.h"

#define C_LABEL_EDGE RGB(210, 165, 60)

static MapSlot slots[MAP_SLOTS];
static MapSlot* cur;
static unsigned tick, version;
static int lastCamX = -9999, lastCamY = -9999;
static uint32_t lastKey;
static unsigned savedVersion;
static int lastId;               // map id of the last frame seen (0 = none yet)
static MapSlot* exitSlot;        // slot the player was last painted into
static int pendingFrom;          // id we came from, until the arrival door is recorded

void mapInit(void) {
	memset(slots, 0, sizeof(slots));
	cur = NULL;
	exitSlot = NULL;
	lastId = pendingFrom = 0;
	lastCamX = lastCamY = -9999;
}

void mapForgetTransition(void) {
	lastId = pendingFrom = 0;
	exitSlot = NULL;
}

static void addPoi(MapSlot* s, int x, int y, int toId) {
	for (int i = 0; i < s->poiCount; ++i) {
		if (s->pois[i].toId == toId && abs(s->pois[i].x - x) < 10 && abs(s->pois[i].y - y) < 10) return;
	}
	if (s->poiCount < MAP_POIS) s->pois[s->poiCount++] = (MapPoi) {x, y, toId};
}

static MapSlot* newSlot(uint32_t key, int camX, int camY) {
	MapSlot* v = NULL;
	for (int i = 0; i < MAP_SLOTS; ++i) {
		if (!slots[i].used) { v = &slots[i]; break; }
		if (!v || slots[i].lastUse < v->lastUse) v = &slots[i];
	}
	if (!v->px) v->px = malloc(MAP_W * MAP_H * sizeof(uint16_t));
	if (!v->px) return NULL;
	memset(v->px, 0, MAP_W * MAP_H * sizeof(uint16_t));
	v->used = true;
	v->key = key;
	v->originX = camX - MAP_W;
	v->originY = camY - MAP_H;
	v->minX = MAP_W; v->minY = MAP_H; v->maxX = -1; v->maxY = -1;
	v->hasPlayer = false;
	v->poiCount = 0;
	return v;
}

static bool slotCovers(const MapSlot* s, int camX, int camY) {
	int cx = (camX - s->originX) / 2, cy = (camY - s->originY) / 2;
	return cx >= 0 && cy >= 0 && cx + 120 < MAP_W && cy + 80 < MAP_H;
}

static MapSlot* pickSlot(const GsState* st) {
	bool jump = abs(st->camX - lastCamX) > 200 || abs(st->camY - lastCamY) > 200;
	if (cur && st->areaKey == cur->key && !jump && slotCovers(cur, st->camX, st->camY)) return cur;
	// Returning to an area we already mapped?
	for (int i = 0; i < MAP_SLOTS; ++i) {
		if (slots[i].used && slots[i].key == st->areaKey && slotCovers(&slots[i], st->camX, st->camY)) return &slots[i];
	}
	return newSlot(st->areaKey, st->camX, st->camY);
}

static int lum(uint16_t c) {
	return ((c >> 11) << 3) + (((c >> 5) & 63) << 2) + ((c & 31) << 3);
}

// The party HUD (HP box top-right + icon bar bottom) only shows after pressing A; its panel is teal (0,99,132).
static bool teal(uint16_t c) {
	int r = (c >> 11) << 3, g = ((c >> 5) & 63) << 2, b = (c & 31) << 3;
	return r < 24 && abs(g - 99) < 20 && abs(b - 132) < 20;
}

static bool hudVisible(const uint16_t* v, size_t stride) {
	return teal(v[10 * stride + 160]) && teal(v[28 * stride + 220]);
}

// ROM mode: when the cartridge's map tables are recognised the whole area is drawn from the ROM (mapdecode.c) and nothing is stitched.
static bool romMode;
static int romId = -1;      // map id the ROM canvas was made for (-1 = none yet)
static int romSkipPend;     // frames the blend setting disagreed with the drawn canvas
static int romPx, romPy;    // canvas coordinates of the player marker

bool mapRomActive(void) { return romMode; }

static void romEnter(const GsState* st, int psx, int psy) {
	bool skip = ((st->bldcnt >> 6) & 3) == 1 && (st->bldcnt & 2);   // the game alpha blends BG1 (a glow layer in houses): leave it out
	romMode = mdEnter(st->areaGroup, st->camX, st->camY, psx, psy, skip);
	romSkipPend = 0;
	++version;
}

void mapUpdate(const uint16_t* video, size_t stride, const GsState* st) {
	if (st->mode != GS_MODE_FIELD) return;
	if (st->areaGroup != 2 && st->playerX == 0 && st->playerY == 0) return;  // transition frames (map 0x1FE etc.)
	if (mdAvailable() && st->areaGroup != 2) {
		int psx = st->hasSprite ? st->sprX + 16 : 120, psy = st->hasSprite ? st->sprY + 26 : 90;
		if (st->areaGroup != romId) {
			romId = st->areaGroup;
			romEnter(st, psx, psy);
		} else if (romMode) {
			bool skip = ((st->bldcnt >> 6) & 3) == 1 && (st->bldcnt & 2);
			if (skip != mdCurrent()->skipLayer2) { if (++romSkipPend >= 20) romEnter(st, psx, psy); } else romSkipPend = 0;
		}
		if (romMode) {
			const MdMap* m = mdCurrent();
			int lx = ((st->camX - m->offX) % m->extX + m->extX) % m->extX, ly = ((st->camY - m->offY) % m->extY + m->extY) % m->extY;   // layer-local, wrapping
			romPx = (lx + psx - m->originX) / 2;
			romPy = (ly + psy - m->originY) / 2;
			++version;
			return;
		}
	} else {
		romMode = false;
		if (st->areaGroup == 2) romId = -1;
	}
	// A new map id means the player went through a door: mark it on both sides.
	if (st->areaGroup != lastId) {
		if (lastId && exitSlot && lastId != 2) addPoi(exitSlot, exitSlot->playerX, exitSlot->playerY, st->areaGroup);
		pendingFrom = lastId;
		lastId = st->areaGroup;
		++version;
	}
	if (st->areaGroup == 2) { exitSlot = NULL; return; }  // world map: scrolling/affine view, camera and player RAM do not follow the usual rules
	// Skip fades / blank frames.
	int sum = 0;
	for (int i = 0; i < 16; ++i) sum += lum(video[((i * 10) % 160) * stride + (i * 37) % 240]);
	if (sum / 16 < 100) return;

	MapSlot* s = pickSlot(st);
	if (!s) return;
	bool changed = s != cur;
	lastCamX = st->camX; lastCamY = st->camY;
	cur = s;
	s->lastUse = ++tick;

	int baseX = st->camX - s->originX, baseY = st->camY - s->originY;
	bool hud = hudVisible(video, stride);
	// Player screen position estimate (camera has a dead zone, so y is approximate).
	int psx = 120, psy = 90;
	if (st->hasSprite) { psx = st->sprX + 16; psy = st->sprY + 26; }

	// Reject frames that disagree strongly with what we already have (menus, dialogs, lightning flashes).
	int seen = 0, bad = 0;
	for (int y = 4; y < 160; y += 8) {
		for (int x = 4; x < 240; x += 8) {
			int cx = (baseX + x) >> 1, cy = (baseY + y) >> 1;
			if ((unsigned) cx >= MAP_W || (unsigned) cy >= MAP_H) continue;
			uint16_t old = s->px[cy * MAP_W + cx];
			if (!old) continue;
			uint16_t nw = video[y * stride + x];
			int d = abs(((old >> 11) - (nw >> 11)) * 8) + abs((((old >> 5) & 63) - ((nw >> 5) & 63)) * 4) + abs(((old & 31) - (nw & 31)) * 8);
			++seen;
			if (d > 150) ++bad;
		}
	}
	if (seen > 40 && bad * 100 / seen > 35) return;
	(void) changed;

	// A dialogue or message window (the same teal panel as the HUD, but one block) is skipped by its bounding box. Without this,
	// a message that is already open when the area loads (or while standing still) is stitched into the map as junk.
	int dx0 = 999, dx1 = -1, dy0 = 999, dy1 = -1, tealN = 0;
	if (!hud) {
		for (int y = 0; y < 160; y += 2) {
			for (int x = 0; x < 240; x += 2) {
				if (!teal(video[y * stride + x])) continue;
				++tealN;
				if (x < dx0) dx0 = x;
				if (x > dx1) dx1 = x;
				if (y < dy0) dy0 = y;
				if (y > dy1) dy1 = y;
			}
		}
		int area = ((dx1 - dx0) / 2 + 1) * ((dy1 - dy0) / 2 + 1);
		if (tealN < 60 || tealN * 100 < area * 35) dx1 = -1;   // too little, or too scattered: not a window
	}
	for (int y = 0; y < 160; y += 2) {
		for (int x = 0; x < 240; x += 2) {
			if (dx1 >= 0 && x >= dx0 - 4 && x <= dx1 + 4 && y >= dy0 - 4 && y <= dy1 + 4) continue;
			// HUD overlays and the player's own sprite never go into the map.
			if (hud && x >= 128 && y < 38) continue;
			if (hud && y >= 108 && x >= 50 && x < 232) continue;
			if (abs(x - psx) < 18 && y > psy - 36 && y < psy + 8) continue;
			int cx = (baseX + x) >> 1, cy = (baseY + y) >> 1;
			if ((unsigned) cx >= MAP_W || (unsigned) cy >= MAP_H) continue;
			// Other sprites (followers, NPCs) only fill cells nothing else has covered yet, so they never stick as ghosts.
			bool onSprite = false;
			for (int k = 0; k < st->spriteCount && !onSprite; ++k) {
				onSprite = x >= st->sprites[k].x - 2 && x < st->sprites[k].x + st->sprites[k].w + 2 &&
					y >= st->sprites[k].y - 2 && y < st->sprites[k].y + st->sprites[k].h + 2;
			}
			if (onSprite && s->px[cy * MAP_W + cx]) continue;
			// Average the 2x2 block this half-resolution cell covers (point sampling made dithered grass and text look like noise).
			const uint16_t* q = video + y * stride + x;
			uint16_t q0 = q[0], q1 = q[1], q2 = q[stride], q3 = q[stride + 1];
			unsigned rr = ((q0 >> 11) + (q1 >> 11) + (q2 >> 11) + (q3 >> 11) + 2) >> 2, gg = (((q0 >> 5) & 63) + ((q1 >> 5) & 63) + ((q2 >> 5) & 63) + ((q3 >> 5) & 63) + 2) >> 2,
				bb = ((q0 & 31) + (q1 & 31) + (q2 & 31) + (q3 & 31) + 2) >> 2;
			uint16_t c = (uint16_t) (rr << 11 | gg << 5 | bb);
			s->px[cy * MAP_W + cx] = c ? c : 1;
			if (cx < s->minX) s->minX = cx;
			if (cx > s->maxX) s->maxX = cx;
			if (cy < s->minY) s->minY = cy;
			if (cy > s->maxY) s->maxY = cy;
		}
	}
	s->playerX = (st->camX + psx - s->originX) >> 1;
	s->playerY = (st->camY + psy - s->originY) >> 1;
	s->hasPlayer = true;
	lastKey = st->areaKey;
	if (pendingFrom) { addPoi(s, s->playerX, s->playerY, pendingFrom); pendingFrom = 0; }
	exitSlot = s;
	++version;
}

unsigned mapVersion(void) {
	return version;
}

const MapSlot* mapCurrent(void) {
	return cur;
}

void mapClearCurrent(void) {
	++version;
	if (cur && cur->px) {
		memset(cur->px, 0, MAP_W * MAP_H * sizeof(uint16_t));
		cur->minX = MAP_W; cur->minY = MAP_H; cur->maxX = -1; cur->maxY = -1;
		cur->poiCount = 0;
	}
}

bool mapDirty(void) {
	return version != savedVersion;
}

// File: "GSMAP002", u32 slot count, then per explored slot: 11 x i32 header, POIs (3 x i32 each), bounding-box rows of u16.
// The saved map as one memory block (format GSMAP002), so it can be written in the background. Returns a malloc'd buffer (caller frees) or NULL.
uint8_t* mapSerialize(size_t* outSize) {
	uint32_t n = 0;
	size_t total = 12;
	for (int i = 0; i < MAP_SLOTS; ++i) {
		const MapSlot* s = &slots[i];
		if (!s->used || !s->px || s->maxX < s->minX) continue;
		++n;
		total += 44 + 12 * (size_t) s->poiCount + 2 * (size_t) (s->maxX - s->minX + 1) * (size_t) (s->maxY - s->minY + 1);
	}
	uint8_t* buf = malloc(total);
	if (!buf) return NULL;
	uint8_t* p = buf;
	memcpy(p, "GSMAP002", 8); p += 8;
	memcpy(p, &n, 4); p += 4;
	for (int i = 0; i < MAP_SLOTS; ++i) {
		const MapSlot* s = &slots[i];
		if (!s->used || !s->px || s->maxX < s->minX) continue;
		int32_t hdr[11] = {(int32_t) s->key, s->originX, s->originY, s->minX, s->minY, s->maxX, s->maxY, s->playerX, s->playerY, s->hasPlayer, s->poiCount};
		memcpy(p, hdr, sizeof(hdr)); p += sizeof(hdr);
		for (int k = 0; k < s->poiCount; ++k) {
			int32_t q[3] = {s->pois[k].x, s->pois[k].y, s->pois[k].toId};
			memcpy(p, q, sizeof(q)); p += sizeof(q);
		}
		for (int y = s->minY; y <= s->maxY; ++y) {
			size_t row = (size_t) (s->maxX - s->minX + 1) * sizeof(uint16_t);
			memcpy(p, &s->px[y * MAP_W + s->minX], row); p += row;
		}
	}
	*outSize = (size_t) (p - buf);
	savedVersion = version;
	return buf;
}

bool mapSave(const char* path) {
	size_t n = 0;
	uint8_t* buf = mapSerialize(&n);
	if (!buf) return false;
	FILE* f = fopen(path, "wb");
	bool ok = f && fwrite(buf, 1, n, f) == n;
	if (f) ok = fclose(f) == 0 && ok;
	free(buf);
	return ok;
}

bool mapLoad(const char* path) {
	FILE* f = fopen(path, "rb");
	if (!f) return false;
	char magic[8];
	uint32_t n = 0;
	bool ok = fread(magic, 1, 8, f) == 8 && !memcmp(magic, "GSMAP002", 8) && fread(&n, sizeof(n), 1, f) == 1 && n <= MAP_SLOTS;
	for (uint32_t i = 0; i < MAP_SLOTS; ++i) { if (slots[i].px) memset(slots[i].px, 0, MAP_W * MAP_H * sizeof(uint16_t)); slots[i].used = false; slots[i].poiCount = 0; }
	cur = NULL;
	for (uint32_t i = 0; ok && i < n; ++i) {
		int32_t hdr[11];
		if (fread(hdr, sizeof(hdr), 1, f) != 1) { ok = false; break; }
		int minX = hdr[3], minY = hdr[4], maxX = hdr[5], maxY = hdr[6], pc = hdr[10];
		if (minX < 0 || minY < 0 || maxX >= MAP_W || maxY >= MAP_H || maxX < minX || maxY < minY || pc < 0 || pc > MAP_POIS) { ok = false; break; }
		MapSlot* s = &slots[i];
		if (!s->px) s->px = calloc(MAP_W * MAP_H, sizeof(uint16_t));
		if (!s->px) { ok = false; break; }
		s->used = true; s->key = (uint32_t) hdr[0]; s->originX = hdr[1]; s->originY = hdr[2];
		s->minX = minX; s->minY = minY; s->maxX = maxX; s->maxY = maxY;
		s->playerX = hdr[7]; s->playerY = hdr[8]; s->hasPlayer = hdr[9] != 0; s->poiCount = pc; s->lastUse = i;
		for (int k = 0; k < pc; ++k) {
			int32_t p[3];
			if (fread(p, sizeof(p), 1, f) != 1) { ok = false; break; }
			s->pois[k] = (MapPoi) {p[0], p[1], p[2]};
		}
		for (int y = minY; ok && y <= maxY; ++y) {
			if (fread(&s->px[y * MAP_W + minX], sizeof(uint16_t), maxX - minX + 1, f) != (size_t) (maxX - minX + 1)) ok = false;
		}
	}
	fclose(f);
	if (!ok) {   // never keep a half-read file
		for (int i = 0; i < MAP_SLOTS; ++i) { slots[i].used = false; slots[i].poiCount = 0; slots[i].maxX = -1; }
	}
	tick = n;
	++version;
	savedVersion = version;
	return ok;
}

// Textured dark-blue background for unexplored space, with a faint compass rose, built once per viewport size.
static uint16_t unexp[320 * 160];
static int unexpW, unexpH;

static void buildUnexplored(int w, int h) {
	if (w > 320 || h > 160) return;
	const uint16_t* bg = artBackground();
	const Sprite* cp = artCompass();
	int cx0 = w - 8 - 64, cy0 = (h - 64) / 2;
	for (int i = 0; i < w; ++i) {
		for (int j = 0; j < h; ++j) {
			uint16_t c = bg[(j + 40) * 320 + i];
			int ci = i - cx0, cj = j - cy0;
			if (ci >= 0 && ci < 64 && cj >= 0 && cj < 64) {
				int k = cj * 64 + ci, a = cp->alpha[k] * 3 / 4;
				if (a > 4) {
					uint16_t s = cp->col[k];
					int r = ((c >> 11) * (255 - a) + (s >> 11) * a) / 255;
					int g = (((c >> 5) & 63) * (255 - a) + ((s >> 5) & 63) * a) / 255;
					int b = ((c & 31) * (255 - a) + (s & 31) * a) / 255;
					c = (uint16_t) ((r << 11) | (g << 5) | b);
				}
			}
			unexp[i * h + j] = c;
		}
	}
	unexpW = w; unexpH = h;
}

static void drawPoi(int X, int Y, bool near) {
	// Gold four-point star with a dark outline.
	uint16_t gold = near ? RGB(255, 240, 150) : RGB(244, 196, 76), dark = RGB(18, 10, 4);
	for (int dy = -9; dy <= 9; ++dy) for (int dx = -9; dx <= 9; ++dx) {
		float a = fabsf((float) dx), b = fabsf((float) dy);
		float s = powf(a, 0.6667f) + powf(b, 0.6667f);
		if (s < powf(8.5f, 0.6667f)) gPx(X + dx, Y + dy, dark);
	}
	for (int dy = -8; dy <= 8; ++dy) for (int dx = -8; dx <= 8; ++dx) {
		float a = fabsf((float) dx), b = fabsf((float) dy);
		float s = powf(a, 0.6667f) + powf(b, 0.6667f);
		if (s < powf(7.0f, 0.6667f)) gPx(X + dx, Y + dy, (dx < 0 && dy < 0) ? RGB(255, 252, 210) : gold);
	}
}


static void drawPlayerMarker(int vx, int vy, int mx, int my) {
	int X = vx + mx, Y = vy + my;
	uint16_t gold = RGB(255, 214, 80), dark = RGB(20, 12, 4);
	for (int dy = -6; dy <= 6; ++dy) {   // dark outline diamond, then gold diamond on top
		int w = 6 - abs(dy);
		for (int k = -w; k <= w; ++k) gPx(X + k, Y + dy, dark);
	}
	for (int dy = -4; dy <= 4; ++dy) {
		int w = 4 - abs(dy);
		for (int k = -w; k <= w; ++k) gPx(X + k, Y + dy, gold);
	}
	gPx(X, Y, RGB(255, 255, 255));
}

static void mapDrawRom(int vx, int vy, int vw, int vh, int zoom, int panX, int panY) {
	const MdMap* m = mdCurrent();
	int zs = zoom == 2 ? 2 : 1;
	int cx = romPx + panX, cy = romPy + panY;
	int left = cx - (vw * zs) / 2, top = cy - (vh * zs) / 2;
	for (int i = 0; i < vw; ++i) {
		int mx = left + i * zs;
		bool inx = (unsigned) mx < (unsigned) m->w;
		uint16_t* col = gPixelPtr(vx + i, vy);
		for (int j = 0; j < vh; ++j) {
			int my = top + j * zs;
			uint16_t c = (inx && (unsigned) my < (unsigned) m->h) ? m->px[my * m->w + mx] : 0;
			col[-j] = c ? c : unexp[i * vh + j];
		}
	}
	int mx = (romPx - left) / zs, my = (romPy - top) / zs;
	if (mx >= 2 && mx < vw - 2 && my >= 2 && my < vh - 2) drawPlayerMarker(vx, vy, mx, my);
}

void mapDraw(int vx, int vy, int vw, int vh, int zoom, int panX, int panY) {
	const MapSlot* s = cur;
	if (unexpW != vw || unexpH != vh) buildUnexplored(vw, vh);
	if (romMode && mdCurrent()) { mapDrawRom(vx, vy, vw, vh, zoom, panX, panY); return; }
	if (!s || s->maxX < 0) {
		for (int i = 0; i < vw; ++i) {
			uint16_t* col = gPixelPtr(vx + i, vy);
			for (int j = 0; j < vh; ++j) col[-j] = unexp[i * vh + j];
		}
		gTextCentered(vx + vw / 2, vy + vh / 2 - 8, "NOTHING EXPLORED YET", 1, RGB(150, 160, 190));
		gTextCentered(vx + vw / 2, vy + vh / 2 + 6, "WALK AROUND TO FILL THE MAP", 1, RGB(100, 110, 140));
		return;
	}
	int zs = zoom == 2 ? 2 : 1;
	// Centre on the player (plus any pan) at every zoom level, so the Center button always brings you back to your character;
	// without a known player position the explored bounds are used.
	int cx = (s->hasPlayer ? s->playerX : (s->minX + s->maxX) / 2) + panX;
	int cy = (s->hasPlayer ? s->playerY : (s->minY + s->maxY) / 2) + panY;
	int left = cx - (vw * zs) / 2, top = cy - (vh * zs) / 2;
	// One framebuffer column at a time (columns are contiguous in the rotated 3DS framebuffer).
	for (int i = 0; i < vw; ++i) {
		int mx = left + i * zs;
		bool inx = (unsigned) mx < MAP_W;
		uint16_t* col = gPixelPtr(vx + i, vy);   // top pixel; moving down decrements the pointer
		for (int j = 0; j < vh; ++j) {
			int my = top + j * zs;
			uint16_t c = (inx && (unsigned) my < MAP_H) ? s->px[my * MAP_W + mx] : 0;
			col[-j] = c ? c : unexp[i * vh + j];
		}
	}
	// Only the player is marked: door pins and their labels were removed (the stored door data is kept in the map file but not drawn).
	if (s->hasPlayer) {
		int mx = (s->playerX - left) / zs, my = (s->playerY - top) / zs;
		if (mx >= 2 && mx < vw - 2 && my >= 2 && my < vh - 2) {
			int X = vx + mx, Y = vy + my;
			uint16_t gold = RGB(255, 214, 80), dark = RGB(20, 12, 4);
			for (int dy = -6; dy <= 6; ++dy) {   // dark outline diamond, then gold diamond on top
				int w = 6 - abs(dy);
				for (int k = -w; k <= w; ++k) gPx(X + k, Y + dy, dark);
			}
			for (int dy = -4; dy <= 4; ++dy) {
				int w = 4 - abs(dy);
				for (int k = -w; k <= w; ++k) gPx(X + k, Y + dy, gold);
			}
			gPx(X, Y, RGB(255, 255, 255));
		}
	}
}
