// Host test of tap-to-select on a mirrored GBA menu. usage: navtest rom sav menu.state tapX tapY [tapX tapY ...] out.ppm
// Each tap runs navigation until it stops; a repeated tap on the same spot sends A.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mgba/core/core.h>
#include <mgba/core/serialize.h>
#include <mgba/internal/gba/input.h>
#include <mgba-util/vfs.h>

#include "../src/gs_state.h"
#include "../src/menunav.h"
#include "../src/ui.h"

static uint32_t memRead(void* ctx, uint32_t addr, int size) {
	struct mCore* core = ctx;
	return size == 1 ? core->busRead8(core, addr) : size == 2 ? core->busRead16(core, addr) : core->busRead32(core, addr);
}
static uint32_t toGba(unsigned k) {
	uint32_t m = 0;
	if (k & UK_A) m |= 1 << GBA_KEY_A;
	if (k & UK_UP) m |= 1 << GBA_KEY_UP;
	if (k & UK_DOWN) m |= 1 << GBA_KEY_DOWN;
	if (k & UK_LEFT) m |= 1 << GBA_KEY_LEFT;
	if (k & UK_RIGHT) m |= 1 << GBA_KEY_RIGHT;
	return m;
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
	struct VFile* vf = VFileOpen(argv[3], O_RDONLY);
	mCoreLoadStateNamed(core, vf, SAVESTATE_SAVEDATA | SAVESTATE_RTC);
	vf->close(vf);
	static GsState st; static MenuNav nav;
	core->runFrame(core);
	for (int a = 4; a + 2 < argc; a += 2) {
		int tx = atoi(argv[a]), ty = atoi(argv[a + 1]);
		gsReadState(&st, memRead, core);
		printf("tap (%d,%d): hand at (%d,%d) found=%d\n", tx, ty, st.handX, st.handY, st.handFound);
		bool sendA = navTap(&nav, &st, tx, ty);
		int frames = 0;
		if (sendA) { printf("  already there -> A\n"); core->setKeys(core, 1 << GBA_KEY_A); for (int i = 0; i < 3; ++i) core->runFrame(core); core->setKeys(core, 0); }
		while (nav.active && frames < 400) {
			gsReadState(&st, memRead, core);
			unsigned k = navStep(&nav, &st);
			if (k && getenv("NAVDBG")) printf("    f%d hand (%d,%d) press %u target (%d,%d)%c", frames, st.handX, st.handY, k, nav.tx, nav.ty, 10);
			core->setKeys(core, toGba(k));
			core->runFrame(core);
			++frames;
		}
		core->setKeys(core, 0);
		for (int i = 0; i < 20; ++i) core->runFrame(core);
		gsReadState(&st, memRead, core);
		printf("  done after %d frames: hand at (%d,%d)\n", frames, st.handX, st.handY);
	}
	FILE* f = fopen(argv[argc - 1], "wb");
	fprintf(f, "P6\n240 160\n255\n");
	for (int y = 0; y < 160; ++y) for (int x = 0; x < 240; ++x) { uint32_t c = video[y * 256 + x]; fputc(c & 255, f); fputc((c >> 8) & 255, f); fputc((c >> 16) & 255, f); }
	fclose(f);
	return 0;
}
