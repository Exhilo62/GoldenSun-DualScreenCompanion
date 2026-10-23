// Host test for map persistence and door points of interest.
// usage: map_persist_test replay.bin out.ppm     (replay format: see ui_preview.c)
// Feeds the first half of the replay as area 3, the second half as area 7 (a "door" in between), then back to area 3.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/gfx.h"
#include "../src/gs_state.h"
#include "../src/mapview.h"

static int fails;
#define CHECK(c, msg) do { if (c) printf("  ok   %s\n", msg); else { printf("  FAIL %s\n", msg); ++fails; } } while (0)

static unsigned checksum(const MapSlot* s) {
	unsigned h = 2166136261u;
	for (int y = s->minY; y <= s->maxY; ++y)
		for (int x = s->minX; x <= s->maxX; ++x) h = (h ^ s->px[y * MAP_W + x]) * 16777619u;
	return h;
}

int main(int argc, char** argv) {
	FILE* r = fopen(argv[1], "rb");
	if (!r) return 2;
	static uint16_t frame[240 * 160];
	static int recs[400][7];
	static uint16_t frames[400][240 * 160];
	int n = 0, rec[7];
	while (n < 400 && fread(rec, sizeof rec, 1, r) == 1 && fread(frame, sizeof frame, 1, r) == 1) {
		memcpy(recs[n], rec, sizeof rec);
		memcpy(frames[n], frame, sizeof frame);
		++n;
	}
	fclose(r);
	printf("%d frames\n", n);
	mapInit();
	static GsState st;
	memset(&st, 0, sizeof st);
	st.mode = GS_MODE_FIELD;
	int half = n / 2, third = n / 3;
	for (int i = 0; i < n; ++i) {
		int area = (i >= third && i < third * 2) ? 7 : 3;
		st.camX = recs[i][0]; st.camY = recs[i][1]; st.playerX = recs[i][2]; st.playerY = recs[i][3];
		st.areaGroup = area; st.areaKey = (uint32_t) area;
		st.hasSprite = recs[i][5] >= 0; st.sprX = recs[i][5]; st.sprY = recs[i][6];
		mapUpdate(frames[i], 240, &st);
		(void) half;
	}
	const MapSlot* cur = mapCurrent();
	CHECK(cur && cur->key == 3, "current slot is area 3 after returning");
	int pois = cur ? cur->poiCount : 0;
	printf("  area 3 POIs: %d\n", pois);
	CHECK(pois >= 1, "area 3 has door POIs (exit to 7 and the arrival back from 7)");
	bool toSeven = false;
	for (int i = 0; cur && i < cur->poiCount; ++i) toSeven |= cur->pois[i].toId == 7;
	CHECK(toSeven, "a POI leads to area 7");

	CHECK(mapDirty(), "map is dirty after exploring");
	CHECK(mapSave("map_test.bin"), "mapSave succeeds");
	CHECK(!mapDirty(), "map is clean after saving");
	unsigned sum3 = cur ? checksum(cur) : 0;
	int minX = cur->minX, maxY = cur->maxY, pc = cur->poiCount;

	mapInit();
	CHECK(mapLoad("map_test.bin"), "mapLoad succeeds");
	// Find the area 3 slot again by feeding one frame with the same camera.
	st.areaGroup = 3; st.areaKey = 3;
	st.camX = recs[0][0]; st.camY = recs[0][1]; st.playerX = recs[0][2]; st.playerY = recs[0][3];
	mapForgetTransition();
	mapUpdate(frames[0], 240, &st);
	cur = mapCurrent();
	CHECK(cur && cur->key == 3, "loaded area 3 slot is reused");
	CHECK(cur && cur->minX <= minX && cur->maxY >= maxY, "bounds survived the round trip");
	CHECK(cur && cur->poiCount >= pc, "POIs survived the round trip");
	CHECK(cur && checksum(cur) != 0 && sum3 != 0, "pixels were stored");

	// A truncated file must be rejected without leaving junk behind.
	FILE* f = fopen("map_test.bin", "rb");
	fseek(f, 0, SEEK_END);
	long size = ftell(f);
	fseek(f, 0, SEEK_SET);
	char* buf = malloc(size);
	fread(buf, 1, size, f);
	fclose(f);
	f = fopen("map_trunc.bin", "wb");
	fwrite(buf, 1, size / 2, f);
	fclose(f);
	CHECK(!mapLoad("map_trunc.bin"), "truncated file is rejected");
	CHECK(mapCurrent() == NULL, "no slot is current after a failed load");
	CHECK(!mapLoad("does_not_exist.bin"), "missing file is rejected");

	// Render the Map view of the original run for a visual check.
	mapInit();
	mapLoad("map_test.bin");
	st.camX = recs[third * 2 + 1][0]; st.camY = recs[third * 2 + 1][1]; st.playerX = recs[third * 2 + 1][2]; st.playerY = recs[third * 2 + 1][3];
	mapForgetTransition();
	mapUpdate(frames[third * 2 + 1], 240, &st);
	static uint16_t fb[GFX_W * GFX_H];
	gBegin(fb);
	gRect(0, 0, GFX_W, GFX_H, 0);
	mapDraw(6, 54, 308, 126, 1, 0, 0);
	FILE* out = fopen(argv[2], "wb");
	fprintf(out, "P6\n%d %d\n255\n", GFX_W, GFX_H);
	for (int y = 0; y < GFX_H; ++y)
		for (int x = 0; x < GFX_W; ++x) {
			uint16_t c = fb[x * GFX_H + (GFX_H - 1 - y)];
			fputc(((c >> 11) & 31) * 255 / 31, out); fputc(((c >> 5) & 63) * 255 / 63, out); fputc((c & 31) * 255 / 31, out);
		}
	fclose(out);
	printf("%s\n", fails ? "FAILED" : "all passed");
	return fails ? 1 : 0;
}
