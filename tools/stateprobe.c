// Prints what the app's state reader sees for a saved emulator state. usage: stateprobe rom.gba save.sav|- start.state
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mgba/core/core.h>
#include <mgba/core/serialize.h>
#include <mgba-util/vfs.h>

#include "../src/gs_ram.h"
#include "../src/gs_state.h"

static uint32_t memRead(void* ctx, uint32_t addr, int size) {
	struct mCore* core = ctx;
	return size == 1 ? core->busRead8(core, addr) : size == 2 ? core->busRead16(core, addr) : core->busRead32(core, addr);
}

int main(int argc, char** argv) {
	if (argc < 4) return 1;
	struct mCore* core = mCoreFind(argv[1]);
	core->init(core);
	mCoreInitConfig(core, NULL);
	static uint32_t video[256 * 160];
	core->setVideoBuffer(core, (mColor*) video, 256);
	mCoreLoadFile(core, argv[1]);
	if (strcmp(argv[2], "-")) mCoreLoadSaveFile(core, argv[2], false);
	core->reset(core);
	struct VFile* vf = VFileOpen(argv[3], O_RDONLY);
	if (!mCoreLoadStateNamed(core, vf, SAVESTATE_SAVEDATA | SAVESTATE_RTC)) {
		printf("loadstate failed\n");
		return 2;
	}
	vf->close(vf);
	core->runFrame(core);
	if (getenv("GS_TLA")) gsSetRam(&gsRamTla);   // The Lost Age address table
	static GsState st;
	gsReadState(&st, memRead, core);
	printf("mode=%s map=%d menu=%d party=%d coins=%u hand=%d,%d,%d enemies=%d\n", st.mode == GS_MODE_BATTLE ? "battle" : "field", st.areaGroup,
		st.menuOpen, st.partySize, (unsigned) st.coins, st.handFound, st.handX, st.handY, st.enemyCount);
	if (st.mode == GS_MODE_BATTLE) printf("battle sel=%d phase=%d top=%d cmd=%d target=%d ally=%d depth=%d text=%s enemies=%d\n", st.battleSel, (int) st.battlePhase, st.battleTop, st.battleCmd, st.battleTarget, st.battleTargetAlly, st.battleDepth, st.battleText, st.enemyCount);
	if (getenv("GS_DUMP")) {
		printf("order:"); for (int i = 0; i < 8; ++i) printf(" %02x", core->busRead8(core, 0x0200001Cu + i));
		printf("  mask: %02x\n", core->busRead8(core, 0x02000040u));
		for (int r = 0; r < 8; ++r) { printf("rec %d:", r); for (int i = 0; i < 16; ++i) printf(" %02x", core->busRead8(core, 0x02000500u + r * 0x14Cu + i)); printf("\n"); }
	}
	for (int i = 0; i < st.partySize; ++i) {
		printf("member %d %s lv%d hp %d/%d pp %d/%d cursed=%d\n", i, st.party[i].name, st.party[i].level, st.party[i].hp, st.party[i].maxHp, st.party[i].pp,
			st.party[i].maxPp, st.party[i].cursed);
	}
	printf("djinn owned %05x %05x %05x %05x set %05x %05x %05x %05x\n", st.djinnOwned[0], st.djinnOwned[1], st.djinnOwned[2], st.djinnOwned[3],
		st.djinnSet[0], st.djinnSet[1], st.djinnSet[2], st.djinnSet[3]);
	for (int i = 0; i < st.enemyCount; ++i) {
		printf("enemy %d %s lv%d hp %d/%d\n", i, st.enemies[i].name, st.enemies[i].level, st.enemies[i].hp, st.enemies[i].maxHp);
	}
	return 0;
}
