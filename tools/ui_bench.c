// Host benchmark for the bottom-UI draw cost. usage: ui_bench dump.bin [battle]
// Prints pixel writes, alpha blends, rect calls and microseconds per frame for every tab (host speed is only a relative guide).
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../src/gs_state.h"
#include "../src/mapview.h"
#include "../src/ui.h"

extern unsigned long gStatPx, gStatBlend, gStatRectCalls;
static int forceBattle;
static unsigned char ram[0x8000 + 0x40000];

static uint32_t rd(void* ctx, uint32_t addr, int size) {
	(void) ctx;
	if (addr == 0x04000000) return forceBattle ? 1 : 0;
	if (!((addr >= 0x02000000 && addr < 0x02040000) || (addr >= 0x03000000 && addr < 0x03008000))) return 0;
	uint32_t off = (addr >= 0x02000000 && addr < 0x02040000) ? 0x8000 + (addr - 0x02000000) : addr - 0x03000000;
	uint32_t v = 0;
	for (int i = 0; i < size; ++i) v |= (uint32_t) ram[off + i] << (8 * i);
	return v;
}

static double now(void) {
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	return t.tv_sec * 1e6 + t.tv_nsec / 1e3;
}

int main(int argc, char** argv) {
	FILE* f = fopen(argv[1], "rb");
	if (!f || fread(ram, 1, sizeof ram, f) != sizeof ram) return 2;
	fclose(f);
	forceBattle = argc > 2;
	static uint16_t fb[UI_W * UI_H];
	GsState st;
	gsReadState(&st, rd, NULL);
	mapInit();
	UiView v;
	uiInit(&v);
	const char* names[] = {"party", "psynergy", "djinn", "map", "items", "menu", "battle"};
	uiDraw(fb, &st, &v);  // warm up (generates art)
	double tg = now();
	for (int i = 0; i < 200; ++i) gsReadState(&st, rd, NULL);
	printf("gsReadState: %.1f us/frame\n", (now() - tg) / 200);
	for (int tab = 0; tab < 7; ++tab) {
		v.tab = tab;
		gStatPx = gStatBlend = gStatRectCalls = 0;
		uiDraw(fb, &st, &v);
		unsigned long px = gStatPx, bl = gStatBlend, rc = gStatRectCalls;
		double t0 = now();
		for (int i = 0; i < 200; ++i) uiDraw(fb, &st, &v);
		printf("%-9s px=%7lu blends=%7lu rects=%6lu  %7.1f us/frame\n", names[tab], px, bl, rc, (now() - t0) / 200);
	}
	return 0;
}
