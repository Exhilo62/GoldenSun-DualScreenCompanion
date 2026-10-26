// The shared application: emulation loop, touch actions, save states, settings and the render job. See app.h and platform.h.
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <mgba/core/core.h>
#include <mgba/core/interface.h>
#include <mgba/core/serialize.h>
#include <mgba/internal/gba/gba.h>
#include <mgba/internal/gba/input.h>
#include <mgba-util/vfs.h>

#include "app.h"
#include "art.h"
#include "bestiary.h"
#include "bgwrite.h"
#include "dialoglog.h"
#include "gs_abilities.h"
#include "gs_areas.h"
#include "gs_djinn.h"
#include "gs_field.h"
#include "gs_game.h"
#include "gs_items.h"
#include "gs_ram.h"
#include "gs_state.h"
#include "mapdecode.h"
#include "mapview.h"
#include "menunav.h"
#include "platform.h"
#include "ui.h"

typedef uint64_t u64;
typedef uint32_t u32;

// Per-game files (maps, saved states, autosave, area names) live in gsGamePath(); filled in once the game is chosen.
static char mapsPath[128], areasPath[128], autoPath[128], stateDir[128], bestPath[128];
static char settingsPath[128], perflogPath[128];

// Scripted button presses (opening a game menu from a tap), one key mask per emulated frame.
static uint16_t macroQ[2400];
static int macroN, macroI;

static void macroPress(unsigned gbaKeyBit, int hold, int gap) {
	for (int i = 0; i < hold && macroN < 2400; ++i) macroQ[macroN++] = (uint16_t) (1u << gbaKeyBit);
	for (int i = 0; i < gap && macroN < 2400; ++i) macroQ[macroN++] = 0;
}

static void macroOpenMenu(int which, int member, bool menuAlreadyOpen) {
	macroN = macroI = 0;
	if (menuAlreadyOpen) for (int i = 0; i < 5; ++i) macroPress(GBA_KEY_B, 3, 8);   // back out to the field first
	macroPress(GBA_KEY_A, 3, 14);                                                    // quick bar; the cursor starts on Psynergy
	if (which == MENU_BAR) return;
	for (int i = 0; i < which; ++i) macroPress(GBA_KEY_RIGHT, 3, 6);
	macroPress(GBA_KEY_A, 3, 10);
	if (member >= 0 && which != MENU_DJINN) {            // Psynergy / Items / Status: pick the first member, then L/R to the wanted one
		macroPress(GBA_KEY_A, 3, 14);
		for (int i = 0; i < member; ++i) macroPress(GBA_KEY_R, 3, 12);
	}
}

static void macroClose(void) {
	macroN = macroI = 0;
	for (int i = 0; i < 6; ++i) macroPress(GBA_KEY_B, 3, 8);
}

static uint32_t touchKeysToGba(unsigned k) {
	uint32_t m = 0;
	if (k & UK_A) m |= 1 << GBA_KEY_A;
	if (k & UK_B) m |= 1 << GBA_KEY_B;
	if (k & UK_L) m |= 1 << GBA_KEY_L;
	if (k & UK_R) m |= 1 << GBA_KEY_R;
	if (k & UK_UP) m |= 1 << GBA_KEY_UP;
	if (k & UK_DOWN) m |= 1 << GBA_KEY_DOWN;
	if (k & UK_LEFT) m |= 1 << GBA_KEY_LEFT;
	if (k & UK_RIGHT) m |= 1 << GBA_KEY_RIGHT;
	return m;
}

static uint32_t fnv(uint32_t h, const void* data, size_t n) {
	const unsigned char* p = data;
	while (n--) h = (h ^ *p++) * 16777619u;
	return h;
}

static uint32_t memRead(void* ctx, uint32_t addr, int size) {
	struct mCore* core = ctx;
	return size == 1 ? core->busRead8(core, addr) : size == 2 ? core->busRead16(core, addr) : core->busRead32(core, addr);
}

// Keeps the last BACKUPS distinct copies of the battery save (<save>.1 newest ... <save>.5 oldest) so a bad overwrite can be undone.
#define BACKUPS 5
static bool sameFile(const char* a, const char* b) {
	FILE* fa = fopen(a, "rb"); FILE* fb = fopen(b, "rb");
	bool same = fa && fb;
	while (same) {
		unsigned char x[512], y[512];
		size_t na = fread(x, 1, sizeof(x), fa), nb = fread(y, 1, sizeof(y), fb);
		if (na != nb || memcmp(x, y, na)) same = false;
		if (na < sizeof(x)) break;
	}
	if (fa) fclose(fa);
	if (fb) fclose(fb);
	return same;
}

static void backupSave(const char* savePath) {
	FILE* f = fopen(savePath, "rb");
	if (!f) return;
	char cur[160], next[160];
	snprintf(cur, sizeof(cur), "%s.1", savePath);
	if (sameFile(savePath, cur)) { fclose(f); return; }
	for (int i = BACKUPS - 1; i >= 1; --i) {
		snprintf(cur, sizeof(cur), "%s.%d", savePath, i);
		snprintf(next, sizeof(next), "%s.%d", savePath, i + 1);
		remove(next);
		rename(cur, next);
	}
	snprintf(cur, sizeof(cur), "%s.1", savePath);
	FILE* o = fopen(cur, "wb");
	if (o) {
		unsigned char buf[4096];
		size_t n;
		while ((n = fread(buf, 1, sizeof(buf), f)) > 0) fwrite(buf, 1, n, o);
		fclose(o);
	}
	fclose(f);
}

static char djinnPath[128];
static void loadDjinnSets(UiView* ui) {   // 3 slots of {valid byte, 4 x u32 little-endian Set masks}
	FILE* f = fopen(djinnPath, "rb");
	if (!f) return;
	for (int i = 0; i < 3; ++i) {
		unsigned char b[17];
		if (fread(b, 1, sizeof(b), f) != sizeof(b)) break;
		ui->loadValid[i] = b[0] == 1;
		for (int e = 0; e < 4; ++e) ui->loadSet[i][e] = (uint32_t) b[1 + 4 * e] | (uint32_t) b[2 + 4 * e] << 8 | (uint32_t) b[3 + 4 * e] << 16 | (uint32_t) b[4 + 4 * e] << 24;
	}
	fclose(f);
}
static void saveDjinnSets(const UiView* ui) {
	uint8_t* all = malloc(3 * 17);
	if (!all) return;
	for (int i = 0; i < 3; ++i) {
		uint8_t* b = all + 17 * i;
		b[0] = ui->loadValid[i] ? 1 : 0;
		for (int e = 0; e < 4; ++e) for (int k = 0; k < 4; ++k) b[1 + 4 * e + k] = (uint8_t) (ui->loadSet[i][e] >> (8 * k));
	}
	bgWriteStart(djinnPath, all, 3 * 17);
}

static void loadSettings(UiView* ui) {
	FILE* f = fopen(settingsPath, "rb");
	if (!f) return;
	unsigned char b[6] = {0, 0, 0, 0, 0, 0};
	size_t n = fread(b, 1, 6, f);
	if (n >= 2 && b[0] < TOP_MODES) {
		ui->topScale = b[0];
		ui->hideMenus = b[1] == 1;
		if (n >= 4) {
			ui->stateSlot = b[2] < 3 ? b[2] : 0;
			ui->autoSave = b[3] == 1;
		}
		if (n >= 6) {
			ui->speedMul = b[4] >= 2 && b[4] <= uiMaxSpeed ? b[4] : 3;
			ui->muteSlow = b[5] == 1;
		}
		ui->fastForward = false;
	}
	fclose(f);
}

static void saveSettings(const UiView* ui) {
	uint8_t* b = malloc(6);
	if (!b) return;
	b[0] = (uint8_t) ui->topScale; b[1] = (uint8_t) ui->hideMenus; b[2] = (uint8_t) ui->stateSlot; b[3] = (uint8_t) ui->autoSave; b[4] = (uint8_t) ui->speedMul; b[5] = (uint8_t) ui->muteSlow;
	bgWriteStart(settingsPath, b, 6);
}

// Slot 1 keeps the original file name so older saved states still load.
static void statePath(char* out, size_t n, int slot) {
	if (slot <= 0) snprintf(out, n, "%sstate.ss", stateDir);
	else snprintf(out, n, "%sstate%d.ss", stateDir, slot + 1);
}

static void writeStateAsync(struct mCore* core, const char* path) {   // snapshot in memory, write to the card in slices
	struct VFile* vf = VFileMemChunk(NULL, 0);
	if (!vf) return;
	if (mCoreSaveStateNamed(core, vf, SAVESTATE_SAVEDATA)) {
		size_t n = (size_t) vf->size(vf);
		void* m = n ? vf->map(vf, n, MAP_READ) : NULL;
		uint8_t* buf = m ? malloc(n) : NULL;
		if (buf) { memcpy(buf, m, n); bgWriteStart(path, buf, n); }
		if (m) vf->unmap(vf, m, n);
	}
	vf->close(vf);
}

static bool writeState(struct mCore* core, const char* path) {
	struct VFile* vf = VFileOpen(path, O_CREAT | O_TRUNC | O_RDWR);
	bool ok = vf && mCoreSaveStateNamed(core, vf, SAVESTATE_SAVEDATA);
	if (vf) vf->close(vf);
	return ok;
}

static volatile bool pendingForget, pendingClear;
// Touch actions that drive the game's own menus (equip / use an item, cast field Psynergy, flip a Djinn). One at a time.
enum { ACT_ITEM = 1, ACT_PSY, ACT_DJINN, ACT_LOAD };
// The equip stat preview (ATK, DEF, AGI words). Golden Sun: a fixed address. The Lost Age: a heap object found through a pointer.
static uint32_t previewBase(struct mCore* core) {
	const GsRam* r = gsRam();
	if (!r->eqPreview) return 0;
	if (!r->eqPreviewIndirect) return r->eqPreview;
	uint32_t p = core->busRead32(core, r->eqPreview);
	return p >= 0x02000000u && p < 0x0203FFE0u ? p + 8 : 0;
}
static uint32_t previewRead(struct mCore* core, int i) {
	uint32_t b = previewBase(core);
	return b ? core->busRead32(core, b + 4u * (unsigned) i) : 0xFFFFFFFFu;
}
static void previewClear(struct mCore* core) {   // only where the preview has a fixed address: a moved pointer must never be written through
	uint32_t b = gsRam()->eqPreviewIndirect ? 0 : previewBase(core);
	if (b) for (int i = 0; i < 3; ++i) core->busWrite32(core, b + 4u * (unsigned) i, 0xFFFFFFFFu);
}
#define PREVIEW_CLEAR() previewClear(core)
static struct {
	int kind;
	int phase;            // 0 idle; 1 walking to an item; 2 waiting for a target tap; 3 final presses; 4 closing the menus; 5 target prompt is up
	int member, index, slot, id, elem, row, bit, target;
	bool equipping;       // item path taken: Equip (vs Use)
	char name[24];
	int ppBefore, qtyBefore, idBefore, closes;
	bool equippedBefore, setBefore;
	bool best, bestDone;   // "Equip best": scan every bag item's stat preview, equip the best gain, repeat, then the next member
	int scanIdx, scanN, bestIdx, bestScore, equipsDone, totalEquips, lastIdx;
	uint32_t mask, newCand;   // bag positions still worth a look / improvements found in this pass
	uint32_t tmask[4];        // Djinn sets: the Set mask wanted per element
	int togN, togI, togE[80], togRow[80], curE, curRow;
	int tries;                // preview reads that came back empty
	bool single;              // Equip best for one member only
	bool scanned;             // this member has had its first full pass
} act;
// ---- repeat last round: the key presses of the last command round are recorded (from the round menu until the fight starts) and can be replayed ----
static uint16_t recBuf[2400], lastRound[2400];
static int recN, lastN, recIdle;
static bool recActive, recSaw, repeating;
static double frameTicksEma;   // smoothed emulation cost of one GBA frame in system ticks (268 MHz)
static bool ffActive;          // more than one frame per loop is running right now
static bool perflogOn;     // perflog.txt exists (checked at start-up)
static int spikeLogged;
static u64 spPump, spRewind, spSave, spSleep;   // this loop: SD write step / rewind snapshot / autosave snapshot, in ticks
static bool autoSkip;    // the console is too slow for full speed: draw every other frame
static unsigned skipPhase;

static void roundFinalize(UiView* ui) {   // keep the recorded presses, dropping leading/trailing idle time and capping long waits
	int n = 0, zeros = 0, i = 0;
	while (i < recN && recBuf[i] == 0) ++i;
	for (; i < recN; ++i) {
		if (recBuf[i] == 0) { if (++zeros > 36) continue; } else zeros = 0;
		lastRound[n++] = recBuf[i];
	}
	while (n > 0 && lastRound[n - 1] == 0) --n;
	if (n > 0) { lastN = n; ui->roundSaved = true; }
	recActive = recSaw = false;
}

static int slotOfShown(const GsMember* m, int shown) {   // the n-th listed bag item -> its position in the record
	int n = 0;
	for (int k = 0; k < 15; ++k) if (m->items[k].id && n++ == shown) return k;
	return -1;
}
static int targetSlotFromHand(int x) {   // the target cursor sits at x about -1, 23, 47, 71 over the four party members
	static const int xs[4] = {-1, 23, 47, 71};
	int best = 0;
	for (int i = 1; i < 4; ++i) if (abs(x - xs[i]) < abs(x - xs[best])) best = i;
	return best;
}

static bool readState(struct mCore* core, const char* path) {
	struct VFile* vf = VFileOpen(path, O_RDONLY);
	bool ok = vf && mCoreLoadStateNamed(core, vf, SAVESTATE_SAVEDATA);
	if (vf) vf->close(vf);
	if (ok) pendingForget = true;   // a state jump is not a door (applied by the map owner, see processJob)
	return ok;
}

static volatile bool reloadSlots = true;   // the slot cards re-read their thumbnails (done by the worker)

static void saveState(struct mCore* core, UiView* ui, const uint16_t* frame, const char* note) {
	char path[96], msg[24];
	statePath(path, sizeof(path), ui->stateSlot);
	bool ok = writeState(core, path);
	if (ok) { uiWriteSlotThumb(stateDir, ui->stateSlot, frame, 256, note); reloadSlots = true; }
	snprintf(msg, sizeof(msg), ok ? "STATE %d SAVED" : "SAVE FAILED", ui->stateSlot + 1);
	uiToast(ui, msg);
}

// ---- rewind: one in-memory snapshot per second, the last few kept ----
#define REWIND_N 6
static struct VFile* rewindSnap[REWIND_N];
static int rewindCount;

// Snapshots live in fixed buffers (a state is 0x61020 bytes) that are reused, so taking one every second costs a memcpy and never an allocation.
#define REWIND_SZ 0x62000
static struct VFile* rewindPool[REWIND_N];
static int rewindPoolN, rewindMade;

static struct VFile* rewindGet(void) {
	if (rewindPoolN) return rewindPool[--rewindPoolN];
	if (rewindMade >= REWIND_N + 1) return NULL;
	uint8_t* mem = malloc(REWIND_SZ);
	struct VFile* vf = mem ? VFileFromMemory(mem, REWIND_SZ) : NULL;
	if (vf) ++rewindMade; else free(mem);
	return vf;
}

static void rewindRelease(struct VFile* vf) {
	if (rewindPoolN < REWIND_N) rewindPool[rewindPoolN++] = vf;
}

static void rewindPush(struct mCore* core) {
	if (rewindCount == REWIND_N) {   // recycle the oldest buffer
		rewindRelease(rewindSnap[0]);
		memmove(rewindSnap, rewindSnap + 1, sizeof(rewindSnap[0]) * (REWIND_N - 1));
		--rewindCount;
	}
	struct VFile* vf = rewindGet();
	if (!vf) return;
	vf->seek(vf, 0, SEEK_SET);
	if (!mCoreSaveStateNamed(core, vf, 0)) { rewindRelease(vf); return; }
	rewindSnap[rewindCount++] = vf;
}

// Goes back up to five snapshots (about five seconds); further presses keep going back until the history runs out.
static void rewindBack(struct mCore* core, UiView* ui) {
	if (rewindCount == 0) { uiToast(ui, "Nothing to rewind"); return; }
	int idx = rewindCount > 5 ? rewindCount - 5 : 0;
	struct VFile* vf = rewindSnap[idx];
	vf->seek(vf, 0, SEEK_SET);
	bool ok = mCoreLoadStateNamed(core, vf, 0);
	int back = rewindCount - idx;
	for (int i = idx; i < rewindCount; ++i) { rewindRelease(rewindSnap[i]); rewindSnap[i] = NULL; }
	rewindCount = idx;
	if (ok) pendingForget = true;
	char msg[24];
	snprintf(msg, sizeof(msg), ok ? "Rewound %d s" : "Rewind failed", back);
	uiToast(ui, msg);
}

static void loadState(struct mCore* core, UiView* ui) {
	char path[96], msg[24];
	statePath(path, sizeof(path), ui->stateSlot);
	snprintf(msg, sizeof(msg), readState(core, path) ? "STATE %d LOADED" : "NO STATE IN SLOT %d", ui->stateSlot + 1);
	uiToast(ui, msg);
}


// ---- Frame pipeline (shared part) ---------------------------------------------------------------------------------------
Job appJob;
uint16_t appJobFrame[256 * 160], appJobTop[256 * 160];
static uint32_t jobLastHash;
static int jobDirty = 2;

bool appProcessJob(Job* j, uint16_t* botFb) {
	bool botDrawn = false;
	u64 t1 = plTicks();
	if (j->forgetMap) mapForgetTransition();
	if (j->clearMap) mapClearCurrent();
	uiSetGbaFrame(j->frame, 256);
	if (j->doMap) mapUpdate(j->frame, 256, &j->gs);
	u64 t2 = plTicks();
	if (j->reloadSlots) uiReloadSlots(stateDir);
	if (j->haveRing) dlogPoll(j->ring, DLOG_RING_WORDS);
	if (j->ui.mirror) jobDirty = 2;   // the bottom screen shows the live game menu
	uint32_t h = fnv(2166136261u, &j->gs, sizeof(j->gs));
	h = fnv(h, &j->ui, sizeof(j->ui));
	if (uiShowsMap(&j->ui)) {
		unsigned mv = mapVersion() >> 1;   // redraw the map at most every other painted frame
		h = fnv(h, &mv, sizeof(mv));
	}
	if (j->ui.showLog) {
		unsigned lv = dlogVersion();
		h = fnv(h, &lv, sizeof(lv));
	}
	if (j->ui.showSlots) {
		unsigned sv = uiSlotsVersion();
		h = fnv(h, &sv, sizeof(sv));
	}
	if (h != jobLastHash) { jobLastHash = h; jobDirty = 2; }
	if (jobDirty > 0) {
		--jobDirty;
		uiDraw(botFb, &j->gs, &j->ui);
		botDrawn = true;
	}
	if ((j->saveMap || j->finalSave) && mapDirty()) {   // serialise now (fast, in memory); the SD write happens in slices from the main loop
		size_t n = 0;
		uint8_t* buf = mapSerialize(&n);
		if (buf) bgWriteStartInPlace(mapsPath, buf, n);   // the map is expendable: overwrite in place, no delete / rename
	}
	j->tMap = t2 - t1; j->tUi = plTicks() - t2;
	return botDrawn;
}

// Leaving the foreground (Android): the process may be killed without notice, so everything is written now. Called by the platform from the
// emulation thread, between frames (from inside the event poll).
static bool skipSaves;   // after importing saves: nothing may be written over them on the way out
static struct mCore* saveCore;
static UiView* saveUi;
void appSaveNow(void) {
	if (!saveCore || !saveUi || skipSaves) return;
	if (saveUi->autoSave) writeState(saveCore, autoPath);
	if (bestDirty()) bestSave(bestPath);
	if (!plTwoCore() && mapDirty()) mapSave(mapsPath);
}

int appRun(void) {
	gsGamePath(GS_GAME_1, "settings.bin", settingsPath, sizeof(settingsPath));   // shared by both games
	gsGamePath(GS_GAME_1, "perflog.txt", perflogPath, sizeof(perflogPath));   // opt-in: create an empty file with this name and a line is appended every ten seconds
	GsGameId game;
	if (!plChooseGame(&game)) return 1;
	const GsGameInfo* gi = gsGameInfo(game);
	if (!gsGameRomMatches(game)) {
		char code[5] = "????", msg[200];
		gsGameRomCode(game, code);
		snprintf(msg, sizeof(msg), "%s is not %s\n(ROM game code %s, expected %s*).", gi->romFile, gi->name, code, gi->codePrefix);
		plFatal(msg);
		return 1;
	}
	gsGameEnsureDir(game);
	gsGamePath(game, NULL, stateDir, sizeof(stateDir));
	gsGamePath(game, "maps.bin", mapsPath, sizeof(mapsPath));
	gsGamePath(game, "bestiary.bin", bestPath, sizeof(bestPath));
	gsGamePath(game, "areas.txt", areasPath, sizeof(areasPath));
	gsGamePath(game, "auto.ss", autoPath, sizeof(autoPath));
	gsGamePath(game, "djinn.bin", djinnPath, sizeof(djinnPath));
	{ FILE* pf = fopen(perflogPath, "r"); if (pf) { fclose(pf); perflogOn = true; } }
	gsSetGame(gi->name, gi->mapped);
	gsSetRam(game == GS_GAME_2 ? &gsRamTla : &gsRamGs1);
	gsAreasUseBuiltin(true);
	gsAreasSelectGame(game == GS_GAME_2 ? 1 : 0);
	char romPath[128];
	gsGameRomPath(game, romPath, sizeof(romPath));

	struct mCore* core = mCoreFind(romPath);
	if (!core) {
		plFatal("Could not open ROM.");
		return 1;
	}

	core->init(core);
	mCoreInitConfig(core, NULL);   // without this the config hash table is uninitialised: harmless on an emulator that zeroes memory, a data abort in HashTableLookup on real hardware
	static uint16_t video[256 * 160];
	core->setVideoBuffer(core, video, 256);
	if (!mCoreLoadFile(core, romPath)) {
		plFatal("Could not load ROM.");
		return 1;
	}
	{
		char savePath[128];
		gsGameSavePath(game, savePath, sizeof(savePath));
		backupSave(savePath);   // before the game can write to it
		mCoreLoadSaveFile(core, savePath, false);   // the ROM may sit in the read-only RomFS of a .cia, so the save path is explicit
	}

	plAudioInit(core);
	core->reset(core);

	static UiView ui;
	static MenuNav nav;
	static GsState gs;
	uiInit(&ui);
	loadDjinnSets(&ui);
	saveCore = core; saveUi = &ui;
	if (plAudioSkipped()) { uiToast(&ui, "Sound off (see notes)"); ui.toastFrames = 400; }
	bgWriteInit(plTwoCore() ? 2 : -1);   // SD writes run on their own low-priority thread
	mapInit();
	{ struct GBA* gb = core->board; mdInit((const uint8_t*) gb->memory.rom, gb->memory.romSize, game == GS_GAME_1 ? 1 : 2); }   // draw maps from the ROM when its tables are recognised
	gsAreasLoad(areasPath);
	mapLoad(mapsPath);
	bestLoad(bestPath);
	unsigned mapSaveTimer = 0;
	for (int i = 0; i < ICON_COUNT; ++i) artIcon(i);
	for (const char* const* n = (const char* const[]) {"Isaac", "Garet", "Ivan", "Mia", "Felix", "Jenna", "Sheba", "Piers", NULL}; *n; ++n) artPortrait(*n);
	loadSettings(&ui);
	int lastScale = ui.topScale;
	bool lastHide = ui.hideMenus;
	int lastSlot = ui.stateSlot;
	bool lastAuto = ui.autoSave;
	int lastMul = ui.speedMul;
	bool lastMute = ui.muteSlow;
	bool slowNow = false;
	unsigned autoTimer = 0;
	if (ui.autoSave && readState(core, autoPath)) uiToast(&ui, "AUTOSAVE LOADED");
	static uint16_t clean[3][256 * 160];  // most recent frames without a menu on screen: a ring, newest at cleanHead
	int cleanHead = 0;
	unsigned ringTick = 0, rewindTick = 0;
	int equipTail = 0;   // tap-to-equip: the macro is running / frames still to hide after it ended
	bool haveClean = false;
	int menuHold = 0;
	u64 acc[5] = {0}, wallStart = plTicks();
	int droppedDraws = 0;
	bool pendingSave = false;
	int accFrames = 0, accGame = 0;
	while (plRunning()) {
		u64 t0 = plTicks();
		PlInput in;
		plPollInput(&in);
		gsReadState(&gs, memRead, core);
		gs.bldcnt = ((struct GBA*) core->board)->memory.io[0x50 >> 1];
		bestObserve(&gs);
		if (bestTakeSave()) { size_t bn = 0; uint8_t* bb = bestSerialize(&bn); if (bb) bgWriteStart(bestPath, bb, bn); }   // a few KB, written once when a battle that added something ends
		{   // The depth byte reads 0 for a single frame every ~24 frames while a command bar is up: only believe a list after 3 frames in a row.
			static GsState prevGs;
			static int zeroRun;
			static bool prevBattle;
			if (gs.mode == GS_MODE_BATTLE && gs.battleDepth == 0) ++zeroRun; else zeroRun = 0;
			if (gs.mode == GS_MODE_BATTLE && gs.battleDepth == 0 && zeroRun < 3 && prevBattle && prevGs.battlePhase == GS_PH_COMMAND) {
				gs.battlePhase = prevGs.battlePhase; gs.battleCmd = prevGs.battleCmd; gs.battleTop = prevGs.battleTop;
				gs.battleTarget = prevGs.battleTarget; gs.battleTargetAlly = prevGs.battleTargetAlly;
				memcpy(gs.battleText, prevGs.battleText, sizeof(gs.battleText));
			}
			prevGs = gs;
			prevBattle = gs.mode == GS_MODE_BATTLE;
		}
		UiOutput uo;
		if (ui.topScale != lastScale || ui.hideMenus != lastHide || ui.stateSlot != lastSlot || ui.autoSave != lastAuto || ui.speedMul != lastMul || ui.muteSlow != lastMute) {
			lastScale = ui.topScale; lastHide = ui.hideMenus; lastSlot = ui.stateSlot; lastAuto = ui.autoSave; lastMul = ui.speedMul; lastMute = ui.muteSlow;
			saveSettings(&ui);
		}
		uiUpdate(&ui, &gs, in.touch, in.tx, in.ty, &uo);
		if (uo.saveState) {
			char note[40];   // shown on the slot card: party level and where you are
			if (gs.partySize > 0 && gs.areaGroup != 0) snprintf(note, sizeof(note), "Lv%u %s", (unsigned) gs.party[0].level, gsAreaName(gs.areaGroup));
			else note[0] = 0;
			saveState(core, &ui, haveClean ? clean[cleanHead] : video, note);
		}
		if (uo.rewind) { rewindBack(core, &ui); macroN = macroI = 0; act.phase = 0; ui.pickActive = false; ui.eqActive = false; }
		if (uo.loadState) loadState(core, &ui);
		if (uo.swapScreens) plSwapScreens();
		if (uo.backup) { if (uo.backup == 1) appSaveNow(); plBackup(uo.backup); }
		{
			char bm[96];
			bool restart = false;
			if (plBackupPoll(bm, sizeof(bm), &restart)) {
				uiToast(&ui, bm);
				ui.toastFrames = 600;
				if (restart) { skipSaves = true; plRequestQuit(); }
			}
		}
		if (uo.clearMap) { pendingClear = true; uiToast(&ui, "Map cleared"); }

		// ---- touch actions through the game's own menus ----
		// Each one walks the game's menus with button presses, checks where the pointing hand ended up (so a mismatch backs out instead
		// of doing the wrong thing), and judges the result from the game's own numbers afterwards.
		if (uo.loadSave >= 0 && gs.mode == GS_MODE_FIELD) {   // remember the current Set / Standby layout
			for (int e = 0; e < 4; ++e) ui.loadSet[uo.loadSave][e] = gs.djinnSet[e];
			ui.loadValid[uo.loadSave] = true;
			saveDjinnSets(&ui);
			char tb[24]; snprintf(tb, sizeof(tb), "Saved set %d", uo.loadSave + 1); uiToast(&ui, tb);
		}
		if (act.phase == 0 && gs.mode == GS_MODE_FIELD && macroI >= macroN && uo.loadApply >= 0) {
			memset(&act, 0, sizeof(act));
			act.kind = ACT_LOAD;
			for (int e = 0; e < 4; ++e) act.tmask[e] = (uo.loadApply == 0 ? gs.djinnOwned[e] : uo.loadApply == 1 ? 0 : ui.loadSet[uo.loadApply - 2][e]) & gs.djinnOwned[e];
			if (gsRam()->djinnCount > 7) {   // The Lost Age: one page, one column per holder; walk columns left to right, rows top to bottom
				for (int col = 0; col < gs.partySize; ++col) {
					int row = 0;
					for (int e = 0; e < 4; ++e) {
						for (int k = 0; k < gsRam()->djinnCount; ++k) {
							if (!((gs.party[col].djinnOwned[e] >> k) & 1)) continue;
							if (((gs.djinnSet[e] ^ act.tmask[e]) >> k) & 1) { act.togE[act.togN] = col; act.togRow[act.togN] = row; ++act.togN; }
							++row;
						}
					}
				}
			} else
			for (int e = 0; e < 4; ++e) {
				int row = 0;
				for (int k = 0; k < 7; ++k) {
					if (!((gs.djinnOwned[e] >> k) & 1)) continue;
					if (((gs.djinnSet[e] ^ act.tmask[e]) >> k) & 1) { act.togE[act.togN] = e; act.togRow[act.togN] = row; ++act.togN; }
					++row;
				}
			}
			if (act.togN == 0) { uiToast(&ui, "Already set like that"); act.kind = 0; }
			else {
				macroOpenMenu(MENU_DJINN, -1, gs.menuOpen);
				macroPress(0, 0, gsRam()->djinnCount > 7 ? 36 : 10);
				act.phase = 30;
				uiToast(&ui, "Working...");
				ui.toastFrames = 1000;
			}
		}
		if (act.phase == 0 && gs.mode == GS_MODE_FIELD && macroI >= macroN && uo.equipBest >= 0 && gsRam()->eqPreview && gs.partySize > 0) {
			memset(&act, 0, sizeof(act));
			act.kind = ACT_ITEM; act.best = true; act.single = true; act.member = uo.equipBest; act.phase = 20;
			uiToast(&ui, "Working...");
			ui.toastFrames = 1000;
		}
		if (act.phase == 0 && gs.mode == GS_MODE_FIELD && macroI >= macroN && (uo.equipIndex >= 0 || uo.psyIndex >= 0 || uo.djinnElem >= 0)) {
			memset(&act, 0, sizeof(act));
			if (uo.equipIndex >= 0) {   // tap a bag item: Equip if it is gear, otherwise Use (asks for a target)
				act.kind = ACT_ITEM; act.member = uo.equipChar; act.index = uo.equipIndex;
				int shown = 0;
				for (int k = 0; k < 15; ++k) {
					const GsItem* it = &gs.party[act.member].items[k];
					if (!it->id) continue;
					if (shown++ == act.index) {
						act.slot = k; act.idBefore = it->id; act.qtyBefore = it->qty; act.equippedBefore = it->equipped;
						snprintf(act.name, sizeof(act.name), "%s", gsItemName(it->id));
						break;
					}
				}
				macroOpenMenu(MENU_ITEMS, act.member, gs.menuOpen);
				const bool preview = gsRam()->eqPreview && !act.equippedBefore;
				for (int i = 0; i < act.index / 5; ++i) macroPress(GBA_KEY_RIGHT, preview ? 3 : 4, preview ? 16 : 10);   // five items per page, pages flip with Right
				for (int i = 0; i < act.index % 5; ++i) macroPress(GBA_KEY_DOWN, preview ? 3 : 4, preview ? 5 : 10);   // faster than hold 3 / gap 5 and the game drops presses
				if (preview) {   // gear shows its stat change in the list: read it and let the player confirm
					if (gsRam()->djinnCount <= 7) previewClear(core);   // the Lost Age only rewrites it when the cursor lands, so its own value is read as is
					macroPress(0, 0, gsRam()->djinnCount > 7 ? 90 : act.index >= 5 ? 20 : act.index ? 8 : 22);   // the game refreshes the preview about 4 frames after the cursor moves
					act.phase = 8;
				} else {
					macroPress(GBA_KEY_A, 4, 20);
					act.phase = 1;
				}
			} else if (uo.psyIndex >= 0) {   // tap a castable Psynergy
				act.kind = ACT_PSY; act.member = uo.psyChar; act.index = uo.psyIndex; act.id = uo.psyId;
				snprintf(act.name, sizeof(act.name), "%s", gsAbilityName((unsigned) act.id));
				act.ppBefore = gs.party[act.member].pp;
				if (gsAbilityNeedsTarget((unsigned) act.id)) {
					ui.pickActive = true;
					snprintf(ui.pickLabel, sizeof(ui.pickLabel), "%s", act.name);
					act.phase = 2;
				} else {
					macroOpenMenu(MENU_PSYNERGY, act.member, gs.menuOpen);
					for (int i = 0; i < act.index / 5; ++i) macroPress(GBA_KEY_RIGHT, 4, 10);
					for (int i = 0; i < act.index % 5; ++i) macroPress(GBA_KEY_DOWN, 4, 10);
					macroPress(GBA_KEY_A, 4, 40);
					act.phase = 7;
				}
			} else {   // tap a Djinn: R flips Set / Standby on the highlighted one in the Djinn page
				act.kind = ACT_DJINN; act.elem = uo.djinnElem; act.row = uo.djinnRow; act.bit = uo.djinnBit;
				act.setBefore = (gs.djinnSet[act.elem] >> act.bit) & 1;
				snprintf(act.name, sizeof(act.name), "%s", gsDjinnName(act.elem, act.bit));
				macroOpenMenu(MENU_DJINN, -1, gs.menuOpen);
				if (gsRam()->djinnCount > 7) {   // The Lost Age: the page has one column per party member listing the Djinn they hold (ascending element, then bit)
					macroPress(0, 0, 36);   // its Djinn page takes longer to come up than Golden Sun's: presses sent earlier are dropped
					int col = -1, row = 0;
					for (int p = 0; p < gs.partySize && col < 0; ++p) if ((gs.party[p].djinnOwned[act.elem] >> act.bit) & 1) col = p;
					if (col >= 0) {
						for (int e2 = 0; e2 < GS_ELEMENTS; ++e2) {
							uint32_t held = gs.party[col].djinnOwned[e2];
							if (e2 > act.elem) break;
							if (e2 == act.elem) held &= (1u << act.bit) - 1u;
							row += __builtin_popcount(held);
						}
						for (int i = 0; i < col; ++i) macroPress(GBA_KEY_RIGHT, 4, 10);
						for (int i = 0; i < row; ++i) macroPress(GBA_KEY_DOWN, 4, 10);
					}
				} else {
					for (int i = 0; i < act.elem; ++i) macroPress(GBA_KEY_RIGHT, 4, 10);
					for (int i = 0; i < act.row; ++i) macroPress(GBA_KEY_DOWN, 4, 10);
				}
				macroPress(GBA_KEY_R, 4, 24);
				act.phase = 7;
			}
			if (act.phase) {
				uiToast(&ui, act.phase == 2 ? "Choose a target" : "Working...");
				ui.toastFrames = 1000;   // stays up until the action finishes
			}
		} else if (act.phase == 30 && macroI >= macroN) {   // Djinn sets: one toggle per pass, walking the Djinn pages relative to the cursor
			if (act.togI >= act.togN) { act.closes = 0; act.phase = 7; macroN = macroI = 0; }
			else {
				int e = act.togE[act.togI], row = act.togRow[act.togI];
				macroN = macroI = 0;
				char tb[24]; snprintf(tb, sizeof(tb), "Djinn %d/%d", act.togI + 1, act.togN); uiToast(&ui, tb); ui.toastFrames = 1000;
				if (e != act.curE) {
					for (int i = 0; i < act.curRow; ++i) macroPress(GBA_KEY_UP, 3, 6);   // back to the top of the page first: pages keep the cursor row
					act.curRow = 0;
					for (int i = 0; i < e - act.curE; ++i) macroPress(GBA_KEY_RIGHT, 3, 10);
					act.curE = e;
				}
				for (int i = act.curRow; i < row; ++i) macroPress(GBA_KEY_DOWN, 3, 6);
				act.curRow = row;
				macroPress(GBA_KEY_R, 4, 12);
				++act.togI;
			}
		} else if (act.phase == 20 && macroI >= macroN) {   // equip best: open this member's item list and start scanning
			const GsMember* pm = &gs.party[act.member];
			act.scanN = 0; act.scanIdx = 0; act.bestIdx = -1; act.bestScore = 0; act.newCand = 0;
			uint32_t gear = 0;
			for (int k = 0; k < 15; ++k) {
				if (!pm->items[k].id) continue;
				if (gsRam()->itemTable && core->busRead8(core, gsRam()->itemTable + 44u * pm->items[k].id + 2) != 0) gear |= 1u << act.scanN;   // item type 0 = consumable
				++act.scanN;
			}
			if (!act.scanned) act.mask = gear; else act.mask &= gear;   // first pass looks at all gear, later ones only at what was an improvement
			act.scanned = true;
			if (act.mask == 0) { act.phase = 4; act.equipping = false; }
			else {
				char tb[24]; snprintf(tb, sizeof(tb), "Gear: %s", pm->name); uiToast(&ui, tb); ui.toastFrames = 1000;
				act.lastIdx = 31 - __builtin_clz(act.mask);
				macroOpenMenu(MENU_ITEMS, act.member, gs.menuOpen);
				PREVIEW_CLEAR();
				macroPress(0, 0, gsRam()->djinnCount > 7 ? 90 : 22);   // The Lost Age takes about 60-90 frames to compute the preview for the highlighted item (Golden Sun: 4)
				act.phase = 21;
			}
		} else if (act.phase == 21 && macroI >= macroN) {   // the highlighted bag item's stat preview: remember the best gain
			const GsMember* pm = &gs.party[act.member];
			if ((act.mask >> act.scanIdx) & 1) {
				int slot = slotOfShown(pm, act.scanIdx), nw[3];
				bool ok = slot >= 0 && !pm->items[slot].equipped;
				for (int i = 0; i < 3; ++i) { nw[i] = (int) (int32_t) previewRead(core, i); if (nw[i] < 0 || nw[i] > 9999) ok = false; }
				if (ok) {
					int score = (nw[0] - pm->atk) + (nw[1] - pm->def) + (nw[2] - pm->agi);
					if (score > 0) act.newCand |= 1u << act.scanIdx;
					if (score > act.bestScore) { act.bestScore = score; act.bestIdx = act.scanIdx; }
				}
			}
			{ char tb[32]; snprintf(tb, sizeof(tb), "%s: checking %d/%d", pm->name, act.scanIdx + 1, act.scanN); uiToast(&ui, tb); ui.toastFrames = 1000; }
			macroN = macroI = 0;
			if (act.scanIdx < act.lastIdx) {
				PREVIEW_CLEAR();
				macroPress(GBA_KEY_DOWN, 3, 5);
				if (act.scanIdx % 5 == 4) { macroPress(GBA_KEY_RIGHT, 3, 12); }   // Down wrapped to the top of the page; Right flips to the next
				++act.scanIdx;
				if ((act.mask >> act.scanIdx) & 1) macroPress(0, 0, gsRam()->djinnCount > 7 ? 90 : 3);   // the game needs about 4 frames to refresh the preview (The Lost Age: 60-90)
			} else {
				act.mask = act.newCand;   // the improvements found now are the only candidates worth another look
				act.phase = 22;
			}
		} else if (act.phase == 22 && macroI >= macroN) {   // scan done: equip the winner (the normal item path), or move on
			if (act.bestIdx < 0) { act.phase = 4; act.equipping = false; }
			else {
				const GsItem* it = &gs.party[act.member].items[slotOfShown(&gs.party[act.member], act.bestIdx)];
				act.slot = slotOfShown(&gs.party[act.member], act.bestIdx); act.index = act.bestIdx;
				snprintf(act.name, sizeof(act.name), "%s", gsItemName(it->id));
				act.mask &= ~(1u << act.bestIdx);
				// still in the list with the cursor on the last scanned item: walk to the winner (pages keep the row, clamped to the page's last row)
				macroN = macroI = 0;
				int cp = act.lastIdx / 5, cr = act.lastIdx % 5, tp = act.bestIdx / 5, tr = act.bestIdx % 5;
				int tsz = act.scanN - 5 * tp < 5 ? act.scanN - 5 * tp : 5;
				for (int i = 0; i < (tp > cp ? tp - cp : cp - tp); ++i) macroPress(tp > cp ? GBA_KEY_RIGHT : GBA_KEY_LEFT, 3, 8);
				int row = cr < tsz ? cr : tsz - 1;
				for (int i = 0; i < (tr - row + tsz) % tsz; ++i) macroPress(GBA_KEY_DOWN, 3, 8);
				macroPress(GBA_KEY_A, 4, 20);
				act.phase = 1;
			}
		} else if (act.phase == 8 && macroI >= macroN) {   // the item is highlighted: did the game compute a stat change for it?
			int nw[3];
			bool ok = true;
			for (int i = 0; i < 3; ++i) { nw[i] = (int) (int32_t) previewRead(core, i); if (nw[i] < 0 || nw[i] > 9999) ok = false; }
			const GsMember* pm = &gs.party[act.member];
			if (ok && gsRam()->djinnCount > 7 && nw[0] == pm->atk && nw[1] == pm->def && nw[2] == pm->agi) ok = false;   // the Lost Age rewrites the current stats for anything that is not new gear
			if (ok) {
				ui.eqActive = true;
				for (int i = 0; i < 3; ++i) { ui.eqNew[i] = nw[i]; ui.eqCur[i] = i == 0 ? pm->atk : i == 1 ? pm->def : pm->agi; }
				snprintf(ui.eqLabel, sizeof(ui.eqLabel), "%s: %s", pm->name, act.name);
				act.phase = 9;
				ui.toastFrames = 0;
			} else if (act.tries++ < (gsRam()->djinnCount > 7 ? 2 : 1)) {
				macroN = macroI = 0;
				macroPress(0, 0, gsRam()->djinnCount > 7 ? 60 : 24);
			} else {   // not equippable gear (or Use items): carry on as before
				macroN = macroI = 0;
				macroPress(GBA_KEY_A, 4, 20);
				act.phase = 1;
			}
		} else if (act.phase == 9 && uo.eqAnswer != 0) {   // Equip / Cancel on the preview
			ui.eqActive = false;
			macroN = macroI = 0;
			if (uo.eqAnswer > 0) { macroPress(GBA_KEY_A, 4, 20); act.phase = 1; uiToast(&ui, "Working..."); ui.toastFrames = 1000; }
			else { macroClose(); uiToast(&ui, "Cancelled"); act.kind = 0; act.phase = 4; }
		} else if (act.phase == 1 && macroI >= macroN) {   // an item's menu is open: where is the hand?
			if (gs.handFound && abs(gs.handX - (gsRam()->djinnCount > 7 ? 134 : 125)) <= 8 && abs(gs.handY - 38) <= 6) {   // on "Equip" (the Lost Age's item menu sits a little further right)
				act.equipping = true;
				macroN = macroI = 0;
				macroPress(GBA_KEY_A, 4, 34);
				act.phase = 7;
			} else if (gs.handFound && abs(gs.handX - 95) <= 8 && abs(gs.handY - 37) <= 6) {   // on "Use": needs someone to use it on
				ui.pickActive = true;
				snprintf(ui.pickLabel, sizeof(ui.pickLabel), "%s", act.name);
				uiToast(&ui, "Choose a target");
				ui.toastFrames = 1000;
				act.phase = 2;
			} else {
				macroClose();
				uiToast(&ui, "Cannot use that");
				act.kind = 0;
				act.phase = 4;
			}
		} else if (act.phase == 2 && uo.pickTarget != -1) {   // the target tap (or Cancel) arrived
			ui.pickActive = false;
			if (uo.pickTarget < 0) {
				macroClose();
				uiToast(&ui, "Cancelled");
				act.kind = 0;
				act.phase = 4;
			} else {
				act.target = uo.pickTarget;
				if (act.kind == ACT_PSY) {   // not in the game's menus yet: walk there now
					macroOpenMenu(MENU_PSYNERGY, act.member, gs.menuOpen);
					for (int i = 0; i < act.index / 5; ++i) macroPress(GBA_KEY_RIGHT, 4, 10);
					for (int i = 0; i < act.index % 5; ++i) macroPress(GBA_KEY_DOWN, 4, 10);
				} else {
					macroN = macroI = 0;
				}
				macroPress(GBA_KEY_A, 4, 36);   // select the ability / "Use": the target prompt opens
				act.phase = 5;
				uiToast(&ui, "Working...");
				ui.toastFrames = 1000;
			}
		} else if (act.phase == 5 && macroI >= macroN) {   // the target prompt is up: steer its cursor to the chosen member
			int cur = gs.handFound ? targetSlotFromHand(gs.handX) : 0;
			macroN = macroI = 0;
			for (int i = 0; i < (act.target - cur + 4) % 4; ++i) macroPress(GBA_KEY_RIGHT, 4, 14);
			macroPress(GBA_KEY_A, 4, 40);
			act.phase = 7;
		} else if (act.phase == 7 && macroI >= macroN) {   // closing: B until the game's menu flag is clear, then a short settle
			macroN = macroI = 0;
			if (gs.menuOpen && ++act.closes <= 8) macroPress(GBA_KEY_B, 3, 6);
			else { macroPress(0, 0, 10); act.phase = 4; }
		} else if (act.phase == 4 && macroI >= macroN && act.best) {   // equip best: loop on this member, then the next
			const GsMember* pm = &gs.party[act.member];
			bool did = act.equipping && act.slot >= 0 && pm->items[act.slot].equipped;
			act.equipping = false;
			if (did && ++act.equipsDone < 8) { ++act.totalEquips; act.phase = 20; }
			else {
				if (did) ++act.totalEquips;
				act.equipsDone = 0; act.scanned = false;
				if (act.single || ++act.member >= gs.partySize) { act.best = false; act.bestDone = true; act.closes = 0; act.phase = 7; macroN = macroI = 0; }
				else act.phase = 20;
			}
		} else if (act.phase == 4 && macroI >= macroN) {   // everything pressed: judge the result from the game's numbers
			char msg[24] = "";
			if (act.bestDone) snprintf(msg, sizeof(msg), act.totalEquips ? "Best gear equipped" : "Gear is already best");
			if (act.kind == ACT_LOAD) {
				int wrong = 0;
				for (int e = 0; e < 4; ++e) {
					uint32_t reach = gs.djinnOwned[e];   // the Lost Age's page only lists the Djinn the four heroes hold
					if (gsRam()->djinnCount > 7) { reach = 0; for (int p = 0; p < gs.partySize; ++p) reach |= gs.party[p].djinnOwned[e]; }
					wrong += __builtin_popcount((gs.djinnSet[e] ^ act.tmask[e]) & reach);
				}
				snprintf(msg, sizeof(msg), wrong ? "Djinn: %d not changed" : "Djinn set applied", wrong);
			}
			if (act.bestDone || act.kind == ACT_LOAD) { /* message set above */ }
			else if (act.kind == ACT_ITEM) {
				const GsItem* it = &gs.party[act.member].items[act.slot];
				if (act.equipping) snprintf(msg, sizeof(msg), it->equipped ? "Equipped %s" : "Could not equip", act.name);
				else snprintf(msg, sizeof(msg), (it->id != act.idBefore || it->qty != act.qtyBefore) ? "Used %s" : "No effect", act.name);
			} else if (act.kind == ACT_PSY) {
				snprintf(msg, sizeof(msg), gs.party[act.member].pp < act.ppBefore ? "Cast %s" : (gsAbilityNeedsTarget((unsigned) act.id) ? "No effect" : "Could not cast"), act.name);
			} else if (act.kind == ACT_DJINN) {
				bool nowSet = (gs.djinnSet[act.elem] >> act.bit) & 1;
				if (nowSet != act.setBefore) snprintf(msg, sizeof(msg), "%s: %s", act.name, nowSet ? "Set" : "Standby");
				else snprintf(msg, sizeof(msg), "Djinn unchanged");
			}
			if (msg[0]) uiToast(&ui, msg);
			act.phase = 0;
			equipTail = 20;   // the menus fade out a few frames after the last B
		} else if (equipTail > 0) {
			--equipTail;
		}
		// ---- touch keyboard for the Lost Age password screen: steer the game's pointing hand to the key, then press A ----
		static struct { bool active; int tx, ty, steps; } kb;
		if (uo.kbKey >= 0 && !kb.active && act.phase == 0 && macroI >= macroN) {
			if (uo.kbKey == UKEY_DEL) { macroN = macroI = 0; macroPress(GBA_KEY_B, 4, 12); }
			else if (uiKeyTarget(uo.kbKey, &kb.tx, &kb.ty)) { kb.active = true; kb.steps = 0; }
		}
		if (kb.active && macroI >= macroN) {
			macroN = macroI = 0;
			int dx = kb.tx - gs.handX, dy = kb.ty - gs.handY;
			if (!gs.handFound || ++kb.steps > 70) { kb.active = false; uiToast(&ui, "Open the password screen first"); }
			else if (abs(dy) >= 4) macroPress(dy > 0 ? GBA_KEY_DOWN : GBA_KEY_UP, 3, 7);
			else if (abs(dx) >= 4) macroPress(dx > 0 ? GBA_KEY_RIGHT : GBA_KEY_LEFT, 3, 7);
			else { macroPress(GBA_KEY_A, 4, 14); kb.active = false; }
		}
		if (uo.openMenu != MENU_NONE) macroOpenMenu(uo.openMenu, uo.openChar, gs.menuOpen);
		if (uo.closeMenu) macroClose();
		if (uo.menuTap && navTap(&nav, &gs, uo.tapX, uo.tapY)) { macroN = macroI = 0; macroPress(GBA_KEY_A, 3, 6); }   // second tap on the same spot confirms
		if (uo.keys || uo.closeMenu || uo.openMenu != MENU_NONE || !ui.mirror) nav.active = false;                      // buttons override tap-navigation
		const uint32_t keysBase = in.keys | touchKeysToGba(uo.keys | navStep(&nav, &gs));
		const bool turbo = ui.fastForward || in.turbo;   // menu toggle, or hold Y / ZR
		// While a touch action (equip, cast, Djinn, keyboard) is pressing buttons in the hidden game menus, run four frames per loop:
		// the presses are fed one per emulated frame, so the sequence finishes about four times sooner and the sound is muted.
		if (uo.repeatRound && gs.mode == GS_MODE_BATTLE && gs.battleTop && lastN > 0 && macroI >= macroN && act.phase == 0 && !repeating) {
			memcpy(macroQ, lastRound, (size_t) lastN * sizeof(lastRound[0]));
			macroN = lastN; macroI = 0;
			repeating = true; ui.roundRepeating = true;
			uiToast(&ui, "Repeating round");
		}
		if (repeating && (macroI >= macroN || gs.mode != GS_MODE_BATTLE)) { if (gs.mode != GS_MODE_BATTLE) macroN = macroI = 0; repeating = false; ui.roundRepeating = false; }
		if (gs.mode != GS_MODE_BATTLE) recActive = recSaw = false;
		else if (!recActive && gs.battleTop) { recActive = true; recN = 0; recSaw = false; recIdle = 0; }
		const bool actFast = ((act.phase != 0 || kb.active) && macroI < macroN) || (repeating && macroI < macroN);
		int frames = turbo ? ui.speedMul : (actFast ? 8 : 1);   // scripted menu presses are fed one per emulated frame, so 8 per loop finishes them sooner (sound muted)
		// Fast-forward cannot beat the console's real emulation speed: asking for 4 frames per loop when one takes 30 ms only makes the picture
		// update 8 times a second. Keep at least ~30 loops a second by capping the frames per loop to what the console can actually do.
		if (frames > 1 && frameTicksEma > 0) {
			int cap = (int) (PL_FF_CAP_TICKS / frameTicksEma);
			if (cap < 1) cap = 1;
			if (frames > cap) frames = cap;
		}
		ffActive = frames > 1;
		plAudioMute(ffActive || (ui.muteSlow && slowNow));
		u64 t1 = plTicks();
		struct GBA* gbaBoard = core->board;
		int camPreX = 0, camPreY = 0;
		for (int i = 0; i < frames; ++i) {
			uint32_t k = keysBase;
			if (macroI < macroN) k |= macroQ[macroI++];
			// Drawing the picture is the biggest part of a GBA frame. When several frames run in one loop only the last one is drawn, and when the
			// console cannot hold full speed every other frame is skipped (game logic and sound keep running at full rate).
			const bool drawIt = frames > 1 ? i == frames - 1 : (!autoSkip || (++skipPhase & 1) == 0);
			gbaBoard->video.frameskipCounter = drawIt ? 0 : 1;
			core->setKeys(core, k);
			if (i == frames - 1) { camPreX = (int) memRead(core, gsRam()->camX, 2); camPreY = (int) memRead(core, gsRam()->camY, 2); }   // the camera for the picture this frame will produce (see GsRam.camLag)
			core->runFrame(core);
			if (recActive && recN < 2400) recBuf[recN++] = (uint16_t) (k & 0x3FF);
		}
		if (recActive) {   // the round is over once the party's input has been seen and the game has gone back to executing
			if (gs.battlePhase != GS_PH_NONE) recSaw = true;
			if (recSaw && !gs.battleTop && gs.battlePhase == GS_PH_NONE) recIdle += frames; else recIdle = 0;
			if (recIdle >= 40 || recN >= 2400) roundFinalize(&ui);
		}
		if (gsRam()->camLag && gi->mapped && gsRam()->hasMap) {   // pair the picture with the camera it was drawn with (the RAM value is a frame ahead)
			gs.playerX += camPreX - gs.camX; gs.playerY += camPreY - gs.camY;
			gs.camX = camPreX; gs.camY = camPreY;
		}
		{ u64 tp = plTicks(); bgWritePump(); spPump = plTicks() - tp; }   // one step of any pending map / autosave write
		accGame += frames;
		if (gi->mapped && (rewindTick += (unsigned) frames) >= 60) { rewindTick = 0; if (act.phase == 0) { u64 tr = plTicks(); rewindPush(core); spRewind = plTicks() - tr; } }
		u64 t2 = plTicks();
		{ double perFrame = (double) (t2 - t1) / frames; frameTicksEma = frameTicksEma > 0 ? frameTicksEma * 0.9 + perFrame * 0.1 : perFrame; }
		bool menuBit = gi->mapped && (memRead(core, gsRam()->menuFlags, 1) & GS_MENU_BIT) != 0 && (memRead(core, GS_REG_DISPCNT, 2) & 7) != 1;
		if (menuBit) menuHold = 2; else if (menuHold > 0) --menuHold;  // the panels vanish a frame after the bit clears
		bool menuNow = menuBit || menuHold > 0;
		if (ui.autoSave && ++autoTimer >= 60 * 60 * 5 && !menuNow && gs.mode == GS_MODE_FIELD) { autoTimer = 0; u64 ta = plTicks(); writeStateAsync(core, autoPath); spSave = plTicks() - ta; }   // every ~5 minutes in the field
		bool saveMap = (++mapSaveTimer >= 10800 && !gs.menuOpen && gs.mode == GS_MODE_FIELD) || pendingSave;   // every ~3 minutes in the field (and on exit), only if the map changed
		if (saveMap) { mapSaveTimer = 0; pendingSave = false; }

		// Two-core mode: emulation never waits for the display. If the worker is still painting or presenting the previous frame this
		// frame is simply not drawn (it is still emulated), so a slow stretch costs a few frames on screen instead of halving the speed.
		u64 tw0 = plTicks();
		bool submit = plJobSlotFree();
		u64 tw1 = plTicks();
		// Keep the menu-free frame history current even for frames that are not drawn.
		if (!menuNow) {
			cleanHead = (cleanHead + 2) % 3;           // newest goes into the oldest slot
			memcpy(clean[cleanHead], video, sizeof(clean[0]));
			haveClean = true;
		}
		const bool equipHide = (act.phase != 0 || equipTail > 0) && haveClean;   // tap-to-equip hides every menu: the top shows the last clean frame, the bottom shows only the toast
		ui.mirror = menuNow && ui.hideMenus && haveClean && !equipHide;
		if (submit) {
			const uint16_t* topFrame = video;
			if (((menuNow && ui.hideMenus) || equipHide) && haveClean) topFrame = clean[(cleanHead + 2) % 3];   // the menu bit rises a frame or two before the panels draw: show the oldest kept frame
			memcpy(appJobFrame, video, sizeof(appJobFrame));
			appJob.gs = gs;
			appJob.ui = ui;
			appJob.frame = appJobFrame;
			if (topFrame == video) appJob.top = appJobFrame;
			else { memcpy(appJobTop, topFrame, sizeof(appJobTop)); appJob.top = appJobTop; }
			appJob.doMap = gi->mapped && gsRam()->hasMap && !gs.menuOpen && !menuNow;
			appJob.clearMap = pendingClear; pendingClear = false;
			appJob.forgetMap = pendingForget; pendingForget = false;
			appJob.saveMap = saveMap;
			appJob.reloadSlots = reloadSlots; reloadSlots = false;
			appJob.haveRing = false;
			if (gi->mapped && gsRam()->textBuf && !gs.menuOpen && (++ringTick & 3) == 0) {   // the message log reads the text ring a few times a second
				for (int i = 0; i < DLOG_RING_WORDS; ++i) appJob.ring[i] = (uint16_t) core->busRead16(core, gsRam()->textBuf + 2u * (uint32_t) i);
				appJob.haveRing = true;
			}
			plJobSubmit(&appJob);
		} else {
			++droppedDraws;
			pendingSave |= saveMap;   // do not lose a periodic map save on a dropped frame
		}
		spSleep = plPace();   // pace the emulation to the GBA's 59.73 Hz: sleeps only when ahead of schedule (two-core mode)
		u64 t5 = plTicks();
		if (perflogOn && t5 - t0 > PL_TICKS_PER_MS * 45 && spikeLogged < 300) {   // a loop longer than 45 ms: note where the time went
			char* sl = malloc(200);   // written by the background thread: the log itself must not stall the loop
			if (sl) {
				int n = snprintf(sl, 200, "SPIKE total=%ums pre=%ums emu=%ums mid=%ums submit=%ums sleep=%ums rewind=%ums autosave=%ums frames=%d mode=%d\n", (unsigned) ((t5 - t0) / PL_TICKS_PER_MS),
					(unsigned) ((t1 - t0) / PL_TICKS_PER_MS), (unsigned) ((t2 - t1) / PL_TICKS_PER_MS), (unsigned) ((tw0 - t2) / PL_TICKS_PER_MS), (unsigned) ((tw1 - tw0) / PL_TICKS_PER_MS), (unsigned) (spSleep / PL_TICKS_PER_MS),
					(unsigned) (spRewind / PL_TICKS_PER_MS), (unsigned) (spSave / PL_TICKS_PER_MS), frames, (int) gs.mode);
				bgWriteAppend(perflogPath, (uint8_t*) sl, (size_t) (n > 0 && n < 200 ? n : 0));
				++spikeLogged;
			}
		}
		spPump = spRewind = spSave = spSleep = 0;
		acc[0] += t1 - t0; acc[1] += t2 - t1; acc[2] += tw1 - tw0; acc[3] += appJob.tTop + appJob.tMap; acc[4] += appJob.tUi;
		if (++accFrames >= 60) {   // PL_TICKS_PER_MS ticks per millisecond
			u64 wall = t5 - wallStart;
			const double tpm = (double) PL_TICKS_PER_MS * accFrames;
			char perfFull[96];   // the detailed line goes to perflog.txt (when it exists); the Menu footer shows only the frame rate
			snprintf(perfFull, sizeof(perfFull), "%.0ffps%s game%.0f e%.1f d%d t%.1f u%.1f w%d c%u%s", accFrames * PL_FPS_SCALE / (double) (wall ? wall : 1), plTwoCore() ? " 2c" : "",
				accGame * PL_FPS_SCALE / (double) (wall ? wall : 1), (double) acc[1] / tpm, droppedDraws, (double) acc[3] / tpm, (double) acc[4] / tpm, plTakeVblankWaits(), plCpuMs(), autoSkip ? " sk" : "");
			snprintf(ui.perf, sizeof(ui.perf), "%.0f FPS", accGame * PL_FPS_SCALE / (double) (wall ? wall : 1));
			static int perfReports;
			if (perflogOn && ++perfReports % 10 == 0) {   // only when the player created perflog.txt: one line per ten seconds for real-console timing reports
				char* pl = malloc(200);
				if (pl) {
					char extra[64];
						plPerfExtra(extra, sizeof(extra));
						int n = snprintf(pl, 200, "%s mode=%d%s\n", perfFull, (int) gs.mode, extra);
					bgWriteAppend(perflogPath, (uint8_t*) pl, (size_t) (n > 0 && n < 200 ? n : 0));
				}
			}
			const bool slow = accGame * PL_FPS_SCALE / (double) (wall ? wall : 1) < 54.0 && !ffActive;
			static int skipOk;
			if (slow) { autoSkip = true; skipOk = 0; }
			else if (autoSkip && ++skipOk >= 30) { autoSkip = false; skipOk = 0; }   // after half a minute of full speed try full drawing again
			slowNow = slow;
			{   // say why there is no sound, if there is none: no audio device, or "Mute if slow" silencing a console that cannot hold full speed
				const char* why = !plHasSound() ? "  (no audio)" : (ui.muteSlow && slow) ? "  (muted: slow)" : "";
				snprintf(ui.perf, sizeof(ui.perf), "%.0f FPS%s", accGame * PL_FPS_SCALE / (double) (wall ? wall : 1), why);
			}
			accGame = 0; droppedDraws = 0;
			for (int i = 0; i < 5; ++i) acc[i] = 0;
			accFrames = 0; wallStart = t5;
		}
	}

	appJob.doMap = appJob.clearMap = appJob.forgetMap = appJob.saveMap = false;
	saveCore = NULL;
	appJob.finalSave = !skipSaves;   // the worker owns the map: it writes the final copy
	plJobFinish(&appJob);
	bgWriteFlush();
	if (!skipSaves) {
		if (bestDirty()) bestSave(bestPath);
		if (!plTwoCore()) { if (mapDirty()) mapSave(mapsPath); }
		if (ui.autoSave) writeState(core, autoPath);
	}
	core->deinit(core);
	plExit();
	return 0;
}
