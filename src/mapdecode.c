#include "mapdecode.h"
#include <stdlib.h>
#include <string.h>

// USA / Europe Golden Sun (AAGSE) and The Lost Age (BAGFE). See docs/MAP_DATA.md.
#define CELLS 128                // the cell table is 128 x 128
// per game: master file table (u32 ROM pointers), map table (8 bytes per map id: u16 graphics set, ...), graphics set table (12 bytes per set: six u16
// file numbers, relative to fileBase: container, palette, three tile images, one more), number of map ids
static struct { uint32_t mft, mftCount, mapTable, setTable, fileBase, mapIds; } cfg;
#define MFT_OFF cfg.mft
#define MFT_COUNT cfg.mftCount
#define MAP_TABLE_OFF cfg.mapTable
#define SET_TABLE_OFF cfg.setTable
#define MAP_IDS cfg.mapIds
#define FILE_BASE cfg.fileBase

static const uint8_t* rom;
static size_t romSize;
static MdMap cur;
static bool curValid;
static bool noCrop;
static int gapR = 1;

static uint32_t rd16(uint32_t o) { return rom[o] | (uint32_t) rom[o + 1] << 8; }
static uint32_t rd32(uint32_t o) { return rd16(o) | rd16(o + 2) << 16; }

bool mdInit(const uint8_t* r, size_t size, int game) {
	mdForget();
	rom = NULL;
	romSize = 0;
	if (!r || size < 0x400000) return false;
	if (game == 1) {
		cfg.mft = 0x320000; cfg.mftCount = 975; cfg.mapTable = 0x9F1AC; cfg.setTable = 0x13784; cfg.fileBase = 296; cfg.mapIds = 205;
	} else if (game == 2 && size >= 0x1000000) {
		cfg.mft = 0x680000; cfg.mftCount = 1723; cfg.mapTable = 0xF17AC; cfg.setTable = 0x2F380; cfg.fileBase = 620; cfg.mapIds = 325;
	} else {
		return false;
	}
	rom = r;
	romSize = size;
	// the file table must point at itself, and the map table must hold the entries known from each game's own maps
	bool ok = rd32(MFT_OFF) == 0x08000000u && rd32(MFT_OFF + 4) == 0x08000000u + MFT_OFF;
	if (ok && game == 1) ok = rd16(MAP_TABLE_OFF + 8 * 3) == 2 && rd16(MAP_TABLE_OFF + 8 * 7) == 6;
	if (ok && game == 2) ok = rd16(MAP_TABLE_OFF + 8 * 5) == 4 && rd16(MAP_TABLE_OFF + 8 * 0x8B) == 133;
	if (!ok) {
		rom = NULL;
		romSize = 0;
	}
	return ok;
}

bool mdAvailable(void) { return rom != NULL; }
void mdSetNoCrop(bool on) { noCrop = on; }
void mdSetGap(int r) { gapR = r < 0 ? 0 : r; }
const MdMap* mdCurrent(void) { return curValid ? &cur : NULL; }

void mdForget(void) {
	free(cur.px);
	memset(&cur, 0, sizeof(cur));
	curValid = false;
}

// ---- decompression (formats 0, 1 and 2) ------------------------------------------------------------------------------------------------

typedef struct {
	const uint8_t* d;
	size_t n, p;
	uint32_t buf;
	int cnt;
} Bits;

static uint32_t bget(Bits* b, int k) {
	while (b->cnt < k) {
		uint32_t v = b->p < b->n ? b->d[b->p] : 0;
		++b->p;
		b->buf |= v << b->cnt;
		b->cnt += 8;
	}
	uint32_t r = b->buf & ((1u << k) - 1);
	b->buf >>= k;
	b->cnt -= k;
	return r;
}

static bool lzCopy(uint8_t* out, size_t cap, size_t* len, size_t dist, size_t n) {
	if (dist == 0 || dist > *len || *len + n > cap) return false;
	for (size_t i = 0; i < n; ++i, ++*len) out[*len] = out[*len - dist];
	return true;
}

size_t mdDecompress(const uint8_t* in, size_t inSize, uint8_t* out, size_t cap) {
	if (inSize < 2) return 0;
	int fmt = in[0];
	size_t len = 0, p = 1;
	if (fmt == 1) {
		for (;;) {
			if (p >= inSize) return 0;
			unsigned flags = in[p++];
			for (int bit = 7; bit >= 0; --bit) {
				if (!((flags >> bit) & 1)) {
					if (p >= inSize || len >= cap) return 0;
					out[len++] = in[p++];
					continue;
				}
				if (p + 1 >= inSize) return 0;
				unsigned b0 = in[p], b1 = in[p + 1];
				p += 2;
				unsigned dist = ((b0 & 0xF0) << 4) | b1, n = b0 & 15;
				if (n == 0) {
					if (dist == 0) return len;
					if (p >= inSize) return 0;
					n = in[p++] + 16u;
				}
				if (!lzCopy(out, cap, &len, dist, n + 1)) return 0;
			}
		}
	}
	if (fmt != 0 && fmt != 2) return 0;
	Bits b = {in, inSize, p, 0, 0};
	uint8_t mtf[16];
	for (int i = 0; i < 16; ++i) mtf[i] = (uint8_t) i;
	for (;;) {
		if (b.p > inSize + 8) return 0;
		if (bget(&b, 1)) {
			if (len >= cap) return 0;
			if (fmt == 0) {
				out[len++] = (uint8_t) bget(&b, 8);
			} else {
				int w = bget(&b, 1) ? 2 : (bget(&b, 1) ? 3 : 4);   // "1" -> 2-bit indices, "01" -> 3 bits, "00" -> 4 bits
				unsigned v = 0;
				for (int half = 0; half < 2; ++half) {
					unsigned i = bget(&b, w);
					uint8_t t = mtf[i];
					memmove(mtf + 1, mtf, i);
					mtf[0] = t;
					v |= (unsigned) t << (4 * half);
				}
				out[len++] = (uint8_t) v;
			}
			continue;
		}
		unsigned n = 2;
		while (n < 6 && bget(&b, 1)) ++n;
		if (n == 6) {
			if (bget(&b, 1)) {
				unsigned k = bget(&b, 2);
				if (k) {
					n = k + 7;
				} else {
					k = bget(&b, 7);
					if (k == 0) return len;
					n = k + 10;
				}
			} else {
				n = 6 + bget(&b, 1);
			}
		}
		unsigned d;
		if (bget(&b, 1)) {
			d = bget(&b, 5);
		} else {
			int r6 = (int) len - 33;
			if (r6 < 0 || r6 >= 2048) {
				d = bget(&b, 12) + 32;
			} else {
				int bits = 0;
				while ((r6 >> bits) != 0) ++bits;
				d = bget(&b, bits) + 32;
			}
		}
		if (!lzCopy(out, cap, &len, d + 1, n)) return 0;
	}
}

// ---- map drawing -----------------------------------------------------------------------------------------------------------------------

static uint32_t fileAddr(int n) {
	if (n < 0 || n >= (int) MFT_COUNT) return 0;
	uint32_t a = rd32(MFT_OFF + 4u * (uint32_t) n) & 0x1FFFFFFu;
	return a < romSize ? a : 0;
}

static size_t loadBlob(uint32_t addr, uint8_t* out, size_t cap) {
	if (!addr || addr >= romSize) return 0;
	return mdDecompress(rom + addr, romSize - addr, out, cap);
}

static uint16_t rgb565(unsigned c) {   // BGR555 -> RGB565
	unsigned r = c & 31, g = (c >> 5) & 31, b = (c >> 10) & 31;
	return (uint16_t) ((r << 11) | (((g << 1) | (g >> 4)) << 5) | b);
}

// Draws one 16x16 block of layer k at layer-local cell (cx, cy) into col/has (256 entries, row-major), over what is already there.
static void drawBlock(uint16_t* col, uint8_t* has, const uint16_t* dict, int dictN, const uint16_t* ids, const uint16_t* pal, int palN, const uint8_t* vram,
                      int cb, int cellX, int cellY) {
	unsigned id = ids[(cellY & 127) * CELLS + (cellX & 127)];
	if ((int) id * 4 + 4 > dictN) return;
	for (int q = 0; q < 4; ++q) {
		unsigned e = dict[id * 4 + q];
		unsigned tile = e & 0x3FF, bank = e >> 12;
		bool hf = e & 0x400, vf = e & 0x800;
		unsigned addr = (unsigned) cb * 0x4000u + tile * 32u;
		if (addr + 32 > 0x10000u) continue;
		int tx = (q & 1) * 8, ty = (q >> 1) * 8;
		for (int py = 0; py < 8; ++py) {
			const uint8_t* row = vram + addr + (vf ? 7 - py : py) * 4;
			for (int px = 0; px < 8; ++px) {
				int sx = hf ? 7 - px : px;
				unsigned ci = (sx & 1) ? row[sx >> 1] >> 4 : row[sx >> 1] & 15;
				if (!ci) continue;
				unsigned idx = bank * 16 + ci;
				if ((int) idx >= palN) continue;
				int o = (ty + py) * 16 + tx + px;
				col[o] = pal[idx];
				has[o] = 1;
			}
		}
	}
}

bool mdEnter(int mapId, int camX, int camY, int psx, int psy, bool skipLayer2) {
	mdForget();
	if (!rom || mapId < 0 || mapId >= (int) MAP_IDS) return false;
	unsigned gs = rd16(MAP_TABLE_OFF + 8u * (unsigned) mapId);
	if (gs > 400) return false;
	uint32_t files[6];
	for (int i = 0; i < 6; ++i) files[i] = fileAddr(FILE_BASE + (int) rd16(SET_TABLE_OFF + 12u * gs + 2u * (unsigned) i));
	if (!files[0] || !files[1] || !files[2] || !files[3] || !files[4] || files[0] + 0x40 > romSize) return false;
	const uint8_t* hb = rom + files[0];
	int w = hb[2] / 2, h = hb[3] / 2;
	if (w < 1 || h < 1 || w > CELLS || h > CELLS) return false;
	int ox[3], oy[3];
	for (int k = 0; k < 3; ++k) { ox[k] = hb[12 + 8 * k] / 2; oy[k] = hb[13 + 8 * k] / 2; }

	bool ok = false;
	uint8_t* blob = malloc(65536);
	uint16_t* dict = NULL;
	uint16_t* ids = malloc(CELLS * CELLS * sizeof(uint16_t));
	uint16_t* pal = NULL;
	uint8_t* vram = calloc(1, 0x10000);
	uint16_t* canvas = NULL;
	uint8_t* occ = NULL;
	if (!blob || !ids || !vram) goto done;

	// block dictionary
	size_t n0 = loadBlob(files[0] + rd32(files[0] + 0x24), blob, 65536);
	if (n0 < 3) goto done;
	int dictN = (int) ((n0 - 1) >> 1);
	dict = malloc((size_t) dictN * 2);
	if (!dict) goto done;
	{
		int mode = blob[0];
		unsigned prev = 0;
		for (int i = 0; i < dictN; ++i) {
			unsigned v = mode == 1 ? (unsigned) (blob[1 + i] << 8 | blob[1 + dictN + i]) : (unsigned) (blob[1 + 2 * i] | blob[2 + 2 * i] << 8);
			if (mode != 0) { prev ^= v; v = prev; }
			dict[i] = (uint16_t) v;
		}
	}
	// cell table: only the block ids (low 12 bits) are needed
	if (loadBlob(files[0] + rd32(files[0] + 0x2C), blob, 65536) != 65536) goto done;
	{
		unsigned nxt = 0;
		for (int i = 0; i < CELLS * CELLS; ++i) {
			unsigned v = blob[0x8000 + 2 * i] | blob[0x8001 + 2 * i] << 8;
			if ((v & 0xFFF) == 0xFFF) v = (v + 0xF001u + nxt++) & 0xFFFF;
			ids[i] = (uint16_t) (v & 0xFFF);
		}
	}
	// palette and the three tile images
	{
		size_t np = loadBlob(files[1], blob, 65536);
		if (np < 32) goto done;
		int palN = (int) (np / 2);
		pal = malloc((size_t) palN * 2);
		if (!pal) goto done;
		for (int i = 0; i < palN; ++i) pal[i] = rgb565(blob[2 * i] | blob[2 * i + 1] << 8);
		for (int k = 0; k < 3; ++k) {
			uint8_t* img = malloc(65536);
			if (!img) goto done;
			size_t ni = loadBlob(files[2 + k], img, 65536);
			if (ni > 0x4000) ni = 0x4000;
			memcpy(vram + 0x4000 + 0x4000 * k, img, ni);
			free(img);
		}
		int cw = w * 8, ch = h * 8;
		canvas = calloc((size_t) cw * ch, 2);
		occ = calloc((size_t) w * h, 1);
		if (!canvas || !occ) goto done;
		for (int cy = 0; cy < h; ++cy) {
			for (int cx = 0; cx < w; ++cx) {
				uint16_t col[256];
				uint8_t has[256];
				memset(has, 0, sizeof(has));
				for (int k = 0; k < (skipLayer2 ? 2 : 3); ++k)
					drawBlock(col, has, dict, dictN, ids, pal, palN, vram, k == 0 ? 0 : 2, ox[k] + cx, oy[k] + cy);
				bool any = false;
				for (int y = 0; y < 8; ++y) {
					for (int x = 0; x < 8; ++x) {
						int r = 0, g = 0, b = 0, c = 0;
						for (int dy = 0; dy < 2; ++dy) {
							for (int dx = 0; dx < 2; ++dx) {
								int o = (y * 2 + dy) * 16 + x * 2 + dx;
								if (!has[o]) continue;
								r += col[o] >> 11; g += (col[o] >> 5) & 63; b += col[o] & 31;
								++c;
							}
						}
						if (!c) continue;
						uint16_t v = (uint16_t) (((r / c) << 11) | ((g / c) << 5) | (b / c));
						canvas[(cy * 8 + y) * cw + cx * 8 + x] = v ? v : 1;
						any = true;
					}
				}
				occ[cy * w + cx] = any;
			}
		}
		// keep only the group of rooms the player is in: occupied cells grown by gapR cells are connected, the player's cell picks the group
		// layer 2 (BG1) scrolls with the camera; every layer's position is camera + (its origin - layer 2's origin), so layer-local = camera - layer 2's origin
		int offX = hb[12 + 16] * 8, offY = hb[13 + 16] * 8;
		int extX = w * 16, extY = h * 16;
		int locCamX = ((camX - offX) % extX + extX) % extX, locCamY = ((camY - offY) % extY + extY) % extY;
		int plx = locCamX + psx, ply = locCamY + psy;
		int x0 = 0, y0 = 0, x1 = w - 1, y1 = h - 1;
		if (!noCrop) {
			int pcx = plx / 16, pcy = ply / 16;
			if (pcx < 0) pcx = 0;
			if (pcx >= w) pcx = w - 1;
			if (pcy < 0) pcy = 0;
			if (pcy >= h) pcy = h - 1;
			uint8_t* dil = calloc((size_t) w * h, 1);
			uint8_t* grp = calloc((size_t) w * h, 1);
			int* stack = malloc((size_t) w * h * sizeof(int));
			if (dil && grp && stack) {
				for (int y = 0; y < h; ++y)
					for (int x = 0; x < w; ++x)
						if (occ[y * w + x])
							for (int dy = -gapR; dy <= gapR; ++dy)
								for (int dx = -gapR; dx <= gapR; ++dx)
									if (x + dx >= 0 && x + dx < w && y + dy >= 0 && y + dy < h) dil[(y + dy) * w + x + dx] = 1;
				// start at the player's cell, or the nearest occupied one
				int sx = -1, sy = -1, best = 1 << 30;
				for (int y = 0; y < h; ++y)
					for (int x = 0; x < w; ++x)
						if (occ[y * w + x]) {
							int d = (x - pcx) * (x - pcx) + (y - pcy) * (y - pcy);
							if (d < best) { best = d; sx = x; sy = y; }
						}
				if (sx >= 0) {
					int sp = 0;
					stack[sp++] = sy * w + sx;
					grp[sy * w + sx] = 1;
					while (sp) {
						int c = stack[--sp], x = c % w, y = c / w;
						const int nx[4] = {1, -1, 0, 0}, ny[4] = {0, 0, 1, -1};
						for (int k = 0; k < 4; ++k) {
							int xx = x + nx[k], yy = y + ny[k];
							if (xx < 0 || xx >= w || yy < 0 || yy >= h || !dil[yy * w + xx] || grp[yy * w + xx]) continue;
							grp[yy * w + xx] = 1;
							stack[sp++] = yy * w + xx;
						}
					}
					x0 = w; y0 = h; x1 = -1; y1 = -1;
					for (int y = 0; y < h; ++y)
						for (int x = 0; x < w; ++x) {
							if (occ[y * w + x] && grp[y * w + x]) {
								if (x < x0) x0 = x;
								if (x > x1) x1 = x;
								if (y < y0) y0 = y;
								if (y > y1) y1 = y;
							} else {   // not in the player's group: blank the cell
								for (int yy = 0; yy < 8; ++yy) memset(canvas + (y * 8 + yy) * cw + x * 8, 0, 8 * 2);
							}
						}
					if (x1 < 0) { x0 = 0; y0 = 0; x1 = w - 1; y1 = h - 1; }
				}
			}
			free(dil); free(grp); free(stack);
		}
		x0 = x0 > 0 ? x0 - 1 : 0;
		y0 = y0 > 0 ? y0 - 1 : 0;
		x1 = x1 < w - 1 ? x1 + 1 : w - 1;
		y1 = y1 < h - 1 ? y1 + 1 : h - 1;
		int nw = (x1 - x0 + 1) * 8, nh = (y1 - y0 + 1) * 8;
		uint16_t* out = malloc((size_t) nw * nh * 2);
		if (!out) goto done;
		for (int y = 0; y < nh; ++y) memcpy(out + y * nw, canvas + (y0 * 8 + y) * cw + x0 * 8, (size_t) nw * 2);
		cur.mapId = mapId;
		cur.w = nw;
		cur.h = nh;
		cur.px = out;
		cur.originX = x0 * 16;
		cur.originY = y0 * 16;
		cur.offX = offX;
		cur.offY = offY;
		cur.extX = extX;
		cur.extY = extY;
		cur.skipLayer2 = skipLayer2;
		curValid = true;
		ok = true;
	}
done:
	free(blob); free(dict); free(ids); free(pal); free(vram); free(canvas); free(occ);
	return ok;
}
