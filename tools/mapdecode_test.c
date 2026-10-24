// Host test for src/mapdecode.c. usage: mapdecode_test rom.gba MAPID out.u16 [crop]   (writes w, h as two u32, then the RGB565 canvas)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../src/mapdecode.h"

int main(int argc, char** argv) {
	if (argc < 4) return 1;
	FILE* f = fopen(argv[1], "rb");
	if (!f) return 2;
	uint8_t* rom = malloc(0x2000000);
	size_t n = fread(rom, 1, 0x2000000, f);
	fclose(f);
	int game = getenv("MD_GAME") ? atoi(getenv("MD_GAME")) : 1;
	int ids = game == 2 ? 325 : 205;
	if (!mdInit(rom, n, game)) { printf("mdInit failed\n"); return 3; }
	if (getenv("MD_GAP")) mdSetGap(atoi(getenv("MD_GAP")));
	if (atoi(argv[2]) < 0) {   // every map id: report size / failures and the time
		int okN = 0;
		clock_t t0 = clock();
		for (int id = 0; id < ids; ++id) {
			bool ok = mdEnter(id, 0, 0, 120, 90, false);
			const MdMap* m = mdCurrent();
			if (ok) { ++okN; printf("%d:%dx%d ", id, m->w, m->h); } else printf("%d:FAIL ", id);
		}
		printf("\n%d of %d ok, %.0f ms per map\n", okN, ids, (clock() - t0) * 1000.0 / CLOCKS_PER_SEC / ids);
		return 0;
	}
	mdSetNoCrop(argc < 5);
	if (!mdEnter(atoi(argv[2]), 0, 0, 120, 90, false)) { printf("mdEnter failed\n"); return 4; }
	const MdMap* m = mdCurrent();
	printf("map %d canvas %dx%d origin %d,%d\n", m->mapId, m->w, m->h, m->originX, m->originY);
	f = fopen(argv[3], "wb");
	uint32_t hdr[2] = {(uint32_t) m->w, (uint32_t) m->h};
	fwrite(hdr, 4, 2, f);
	fwrite(m->px, 2, (size_t) m->w * m->h, f);
	fclose(f);
	return 0;
}
