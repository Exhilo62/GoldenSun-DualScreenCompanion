// Host test of the live map: boots the real state reader + mosaic code on the emulated game.
// usage: maptest rom.gba save.sav|- start.state seed steps out.ppm
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mgba/core/core.h>
#include <mgba/core/serialize.h>
#include <mgba/internal/gba/input.h>
#include <mgba-util/vfs.h>

#include "../src/gs_ram.h"
#include "../src/gs_state.h"
#include "../src/mapview.h"

static uint32_t memRead(void* ctx, uint32_t addr, int size) {
	struct mCore* core = ctx;
	return size == 1 ? core->busRead8(core, addr) : size == 2 ? core->busRead16(core, addr) : core->busRead32(core, addr);
}

int main(int argc, char** argv) {
	if (argc < 7) return 1;
	struct mCore* core = mCoreFind(argv[1]);
	core->init(core);
	mCoreInitConfig(core, NULL);
	static uint32_t video[256 * 160];
	core->setVideoBuffer(core, (mColor*) video, 256);
	mCoreLoadFile(core, argv[1]);
	if (strcmp(argv[2], "-")) mCoreLoadSaveFile(core, argv[2], false);
	core->reset(core);
	if (getenv("GS_TLA")) gsSetRam(&gsRamTla);   // The Lost Age address table
	struct VFile* vf = VFileOpen(argv[3], O_RDONLY);
	mCoreLoadStateNamed(core, vf, SAVESTATE_SAVEDATA | SAVESTATE_RTC);
	vf->close(vf);
	srand(atoi(argv[4]));
	int steps = atoi(argv[5]);
	static const int dirs[8] = {1 << GBA_KEY_UP, 1 << GBA_KEY_DOWN, 1 << GBA_KEY_LEFT, 1 << GBA_KEY_RIGHT,
		(1 << GBA_KEY_UP) | (1 << GBA_KEY_RIGHT), (1 << GBA_KEY_DOWN) | (1 << GBA_KEY_LEFT), (1 << GBA_KEY_UP) | (1 << GBA_KEY_LEFT), (1 << GBA_KEY_DOWN) | (1 << GBA_KEY_RIGHT)};
	static uint16_t frame[256 * 160];
	const char* lagEnv = getenv("GS_CAMLAG");   // use the camera from N emulated frames ago (tests camera/picture alignment)
	int lag = lagEnv ? atoi(lagEnv) : gsRam()->camLag;
	int batch = getenv("GS_BATCH") ? atoi(getenv("GS_BATCH")) : 1;
	static int ringX[16], ringY[16]; static unsigned ringN;
	const char* lagYEnv = getenv("GS_CAMLAGY");   // separate lag for Y (default: same as GS_CAMLAG)
	int lagY = lagYEnv ? atoi(lagYEnv) : lag;
	static uint16_t alignCanvas[4096 * 4096 / 4]; static uint8_t alignSeen[4096 * 4096 / 4];   // 2048x2048 world canvas for the alignment metric
	long misN = 0, misBad = 0;
	mapInit();
	GsState st;
	uint32_t lastKey = 0xFFFFFFFF; int slotsSeen = 0; unsigned lastVer = 0;
	for (int i = 0; i < steps; ++i) {
		int d = dirs[rand() % 8], n = 10 + rand() % 30;
		core->setKeys(core, d);
		for (int f = 0; f < n; ++f) {
			core->runFrame(core);
			{ GsState t; gsReadState(&t, memRead, core); ringX[ringN & 15] = t.camX; ringY[ringN & 15] = t.camY; ++ringN; }
			if (batch > 1 ? (f % batch) != batch - 1 : (f % 2)) continue;   // GS_BATCH=4 mimics fast-forward: only the last frame of each batch is stitched
			for (int k = 0; k < 256 * 160; ++k) { uint32_t c = video[k]; frame[k] = ((c & 0xFF) >> 3) << 11 | (((c >> 8) & 0xFF) >> 2) << 5 | ((c >> 16) & 0xFF) >> 3; }
			gsReadState(&st, memRead, core);
			if (lag > 0 && ringN > (unsigned) lag) st.camX = ringX[(ringN - 1 - lag) & 15];
			if (lagY > 0 && ringN > (unsigned) lagY) st.camY = ringY[(ringN - 1 - lagY) & 15];
			if (getenv("GS_MISMATCH") && st.areaKey == 236) {   // alignment metric: paint each frame at its camera and count disagreements with earlier paint
				long fN = 0, fBad = 0;
				for (int y = 0; y < 160; ++y) for (int x = 0; x < 240; ++x) {
					int wx = st.camX + x, wy = st.camY + y;
					if (wx < 0 || wy < 0 || wx >= 2048 || wy >= 2048) continue;
					int idx = wy * 2048 + wx; uint16_t c = frame[y * 256 + x];
					if (alignSeen[idx]) { ++misN; ++fN; uint16_t o = alignCanvas[idx]; if (((o >> 11) ^ (c >> 11)) > 3 || (((o >> 5) & 63) ^ ((c >> 5) & 63)) > 6) { ++misBad; ++fBad; } }
					alignCanvas[idx] = c; alignSeen[idx] = 1;
				}
				if (getenv("GS_MISLOG") && fN > 4000) { static int pX, pY; printf("frame mismatch %5.1f%% cam %d,%d d=%d,%d%c", 100.0 * fBad / fN, st.camX, st.camY, st.camX - pX, st.camY - pY, 10); pX = st.camX; pY = st.camY; }
			}
			if (st.areaKey != lastKey) { printf("step %d map %u (0x%X) player %d,%d cam %d,%d\n", i, st.areaKey, st.areaKey, st.playerX, st.playerY, st.camX, st.camY); lastKey = st.areaKey; }
			mapUpdate(frame, 256, &st);
		}
	}
	if (getenv("GS_MISMATCH")) printf("alignment: %ld of %ld overlapping pixels disagree (%.2f%%)%c", misBad, misN, misN ? 100.0 * misBad / misN : 0.0, 10);
	core->setKeys(core, 0);
	printf("final player %d,%d cam %d,%d sprite %d (%d,%d)%c", st.playerX, st.playerY, st.camX, st.camY, st.hasSprite, st.sprX, st.sprY, 10);
	(void) slotsSeen; (void) lastVer;
	const MapSlot* s = mapCurrent();
	if (!s || s->maxX < 0) { printf("empty map\n"); return 2; }
	int w = s->maxX - s->minX + 1, h = s->maxY - s->minY + 1;
	printf("map key %u bounds %d x %d (canvas px), player %d,%d\n", s->key, w, h, s->playerX, s->playerY);
	FILE* f = fopen(argv[6], "wb");
	fprintf(f, "P6\n%d %d\n255\n", w, h);
	for (int y = s->minY; y <= s->maxY; ++y) for (int x = s->minX; x <= s->maxX; ++x) {
		uint16_t c = s->px[y * MAP_W + x];
		int mark = abs(x - s->playerX) < 4 && abs(y - s->playerY) < 4;
		if (mark) { fputc(255, f); fputc(0, f); fputc(255, f); continue; }
		fputc(((c >> 11) & 31) << 3, f); fputc(((c >> 5) & 63) << 2, f); fputc((c & 31) << 3, f);
	}
	fclose(f);
	return 0;
}
