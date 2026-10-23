// Host preview of the GBA-menu mirror layout. usage: mirror_preview frame.ppm(240x160) out.ppm [pressedKeysMask]
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/gs_state.h"
#include "../src/mapview.h"
#include "../src/ui.h"
int main(int argc, char** argv) {
	if (argc < 3) return 1;
	static uint16_t frame[240 * 160], fb[UI_W * UI_H];
	FILE* f = fopen(argv[1], "rb");
	char hdr[64]; int w, h, mx;
	if (!f || fscanf(f, "%2s %d %d %d", hdr, &w, &h, &mx) != 4) return 2;
	fgetc(f);
	for (int i = 0; i < 240 * 160; ++i) { int r = fgetc(f), g = fgetc(f), b = fgetc(f); frame[i] = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3); }
	fclose(f);
	static GsState st; static UiView v;
	uiInit(&v); mapInit();
	strcpy(st.party[0].name, "Isaac"); st.party[0].level = 54; st.areaGroup = 0xBA; st.coins = 999999;
	v.mirror = true; v.hideMenus = true; v.touchKeys = argc > 3 ? (unsigned) atoi(argv[3]) : 0;
	uiSetGbaFrame(frame, 240);
	uiDraw(fb, &st, &v);
	FILE* o = fopen(argv[2], "wb");
	fprintf(o, "P6\n%d %d\n255\n", UI_W, UI_H);
	for (int y = 0; y < UI_H; ++y) for (int x = 0; x < UI_W; ++x) {
		uint16_t c = fb[x * UI_H + (UI_H - 1 - y)];
		fputc(((c >> 11) & 31) << 3, o); fputc(((c >> 5) & 63) << 2, o); fputc((c & 31) << 3, o);
	}
	fclose(o);
	return 0;
}
