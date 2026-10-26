// Nintendo 3DS platform layer: screens, buttons, sound, the two-core frame pipeline and the picker. The game itself is in app.c.
#include <3ds.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include <mgba/core/core.h>
#include <mgba-util/audio-buffer.h>
#include <mgba/core/interface.h>
#include <mgba/gba/interface.h>
#include <mgba/internal/gba/input.h>
#include <mgba-util/vfs.h>

#include "app.h"
#include "gs_game.h"
#include "launcher.h"
#include "platform.h"
#include "ui.h"

#define AUDIO_SAMPLES 384
#define DSP_BUFFERS 8   // ~94 ms of queued audio: rides out a slow frame without stuttering

static volatile bool audioMute;   // set by the main loop: fast-forward, or too slow with "Mute if slow" on
static ndspWaveBuf dspBuffer[DSP_BUFFERS];
static int bufferId;
static bool hasSound;

static void postAudioBuffer(struct mAVStream* stream, struct mAudioBuffer* buffer) {
	(void) stream;
	if (!hasSound) {
		return;
	}
	if (audioMute) {
		mAudioBufferClear(buffer);
		return;
	}
	for (int i = 0; i < DSP_BUFFERS; ++i) {
		int id = (bufferId + i) & (DSP_BUFFERS - 1);
		if (dspBuffer[id].status == NDSP_WBUF_QUEUED || dspBuffer[id].status == NDSP_WBUF_PLAYING) {
			continue;
		}
		bufferId = id;
		void* data = dspBuffer[id].data_pcm16;
		memset(&dspBuffer[id], 0, sizeof(dspBuffer[id]));
		dspBuffer[id].data_pcm16 = data;
		dspBuffer[id].nsamples = AUDIO_SAMPLES;
		mAudioBufferRead(buffer, data, AUDIO_SAMPLES);
		DSP_FlushDataCache(data, AUDIO_SAMPLES * 2 * sizeof(int16_t));
		ndspChnWaveBufAdd(0, &dspBuffer[id]);
		return;
	}
	mAudioBufferClear(buffer);
}

static void audioRateChanged(struct mAVStream* stream, unsigned rate) {
	(void) stream;
	if (!rate) {
		rate = 32768;
	}
	// We run 60 Hz frames; the GBA runs 59.7275 Hz.
	ndspChnSetRate(0, rate * (60.0f / 59.7275f));
}

// Some consoles fault inside libctru's ndspInit (a data abort writing to DSP memory) and take the whole app down. A flag file is created before
// the call and removed after it: if it is still there at the next start the previous attempt crashed, so sound stays off instead of crashing again.
#define AUDIO_FLAG_PATH "sdmc:/3ds/GoldenSun3DS/audio_crash.flag"
static bool audioSkipped;
#define DSPFIRM_PATH "sdmc:/3ds/dspfirm.cdc"

// Citra-derived emulators (Azahar, Citra) answer this undocumented system-info query; a real console reports an error for it.
static bool runningInEmulator(void) {
	s64 v = 0;
	return R_SUCCEEDED(svcGetSystemInfo(&v, 0x20000, 0));
}

static void initAudio(void) {
	FILE* flag = fopen(AUDIO_FLAG_PATH, "rb");
	if (flag) { fclose(flag); audioSkipped = true; return; }   // last time audio start-up crashed: delete this file to try again
	flag = fopen(AUDIO_FLAG_PATH, "wb");
	if (flag) { fputs("audio start-up was in progress", flag); fclose(flag); }
	Result rc = ndspInit();
	if (R_FAILED(rc) && runningInEmulator()) {
		// The audio library insists on a file named dspfirm.cdc, but an emulator's built-in (HLE) audio never looks inside it. So when there is no
		// such file, write a placeholder (all zeros, no Nintendo data) into the emulator's virtual SD card and try once more. Never done on a console.
		FILE* have = fopen(DSPFIRM_PATH, "rb");
		if (have) { fclose(have); }
		else {
			FILE* ph = fopen(DSPFIRM_PATH, "wb");
			if (ph) {
				static const unsigned char zeros[0x4000];
				fwrite(zeros, 1, sizeof(zeros), ph);
				fclose(ph);
				rc = ndspInit();
			}
		}
	}
	if (R_FAILED(rc)) {
		remove(AUDIO_FLAG_PATH);   // it failed cleanly (for example no DSP firmware): not a crash
		return;
	}
	ndspSetOutputMode(NDSP_OUTPUT_STEREO);
	ndspSetOutputCount(1);
	ndspChnReset(0);
	ndspChnSetFormat(0, NDSP_FORMAT_STEREO_PCM16);
	ndspChnSetInterp(0, NDSP_INTERP_NONE);
	ndspChnWaveBufClear(0);
	for (int i = 0; i < DSP_BUFFERS; ++i) {
		memset(&dspBuffer[i], 0, sizeof(dspBuffer[i]));
		dspBuffer[i].data_pcm16 = linearAlloc(AUDIO_SAMPLES * 2 * sizeof(int16_t));
		dspBuffer[i].status = NDSP_WBUF_DONE;
	}
	hasSound = true;
	remove(AUDIO_FLAG_PATH);   // start-up survived
}

static uint32_t mapKeys(u32 held) {
	uint32_t k = 0;
	if (held & KEY_A) k |= 1 << GBA_KEY_A;
	if (held & KEY_B) k |= 1 << GBA_KEY_B;
	if (held & KEY_SELECT) k |= 1 << GBA_KEY_SELECT;
	if (held & KEY_START) k |= 1 << GBA_KEY_START;
	if (held & KEY_DRIGHT) k |= 1 << GBA_KEY_RIGHT;
	if (held & KEY_DLEFT) k |= 1 << GBA_KEY_LEFT;
	if (held & KEY_DUP) k |= 1 << GBA_KEY_UP;
	if (held & KEY_DDOWN) k |= 1 << GBA_KEY_DOWN;
	if (held & KEY_R) k |= 1 << GBA_KEY_R;
	if (held & KEY_L) k |= 1 << GBA_KEY_L;
	if (held & KEY_CPAD_RIGHT) k |= 1 << GBA_KEY_RIGHT;
	if (held & KEY_CPAD_LEFT) k |= 1 << GBA_KEY_LEFT;
	if (held & KEY_CPAD_UP) k |= 1 << GBA_KEY_UP;
	if (held & KEY_CPAD_DOWN) k |= 1 << GBA_KEY_DOWN;
	return k;
}

// 240x160 RGB565 (row-major) -> top screen (400x240, column-major and flipped): 1x centred, or FILL stretch (5/3 wide x 3/2 tall, seams blended).
// The source is read in blocks of 8 columns so every cache line is used once.
#define AVG565(a, b) ((uint16_t) ((((a) & 0xF7DE) >> 1) + (((b) & 0xF7DE) >> 1) + ((a) & (b) & 0x0821)))
#define BLK 8
// Draws the top screen into the private buffer fb (400x240, column-major); presentFrame() copies it to the real framebuffer.
static void blitTop(uint16_t* fb, const uint16_t* src, size_t stride, int mode) {
	static int clearedMode = -1;
	if (mode == TOP_1X) {
		if (clearedMode != mode) { memset(fb, 0, 400 * 240 * sizeof(uint16_t)); clearedMode = mode; }   // the black border only needs painting once
		uint16_t col[BLK][160];
		for (int x0 = 0; x0 < 240; x0 += BLK) {
			for (int y = 0; y < 160; ++y) {
				const uint16_t* row = src + y * stride + x0;
				for (int k = 0; k < BLK; ++k) col[k][y] = row[k];
			}
			for (int k = 0; k < BLK; ++k) {
				uint16_t* dst = fb + (x0 + k + 80) * 240 + 240 - 40 - 160;   // dst[239 - (y + 40)] for y = 0..159 runs backwards
				for (int y = 0; y < 160; ++y) dst[159 - y] = col[k][y];
			}
		}
		return;
	}
	if (mode == TOP_FILL) {
		// "Sharp bilinear" stretch: 3 source columns -> 5 (s0, mix, s1, mix, s2) and 2 source rows -> 3 (a, mix, b). Only the seam pixels are
		// blended, so text keeps even stroke widths instead of the 1-2-2 pixel jitter that plain nearest-neighbour stretching gives.
		clearedMode = -1;
		uint16_t col[BLK][160];
		static uint16_t E[240][240];   // every source column already stretched to 240 rows (flipped)
		for (int x0 = 0; x0 < 240; x0 += BLK) {
			for (int y = 0; y < 160; ++y) {
				const uint16_t* row = src + y * stride + x0;
				for (int k = 0; k < BLK; ++k) col[k][y] = row[k];
			}
			for (int k = 0; k < BLK; ++k) {
				uint16_t* e = E[x0 + k];
				for (int j = 0; j < 80; ++j) {
					uint16_t a = col[k][2 * j], b = col[k][2 * j + 1];
					e[239 - 3 * j] = a; e[238 - 3 * j] = AVG565(a, b); e[237 - 3 * j] = b;
				}
			}
		}
		for (int g = 0; g < 80; ++g) {
			const uint16_t* s0 = E[3 * g]; const uint16_t* s1 = E[3 * g + 1]; const uint16_t* s2 = E[3 * g + 2];
			uint16_t* d = fb + 5 * g * 240;
			memcpy(d, s0, 480);
			for (int i = 0; i < 240; ++i) d[240 + i] = AVG565(s0[i], s1[i]);
			memcpy(d + 480, s1, 480);
			for (int i = 0; i < 240; ++i) d[720 + i] = AVG565(s1[i], s2[i]);
			memcpy(d + 960, s2, 480);
		}
		return;
	}
}


void plAudioInit(struct mCore* core) {
	core->setAudioBufferSize(core, AUDIO_SAMPLES);
	initAudio();
	static struct mAVStream stream;
	stream.postAudioBuffer = postAudioBuffer;
	stream.audioRateChanged = audioRateChanged;
	core->setAVStream(core, &stream);
}
void plAudioMute(bool mute) { audioMute = mute; }
bool plHasSound(void) { return hasSound; }
bool plAudioSkipped(void) { return audioSkipped; }

uint64_t plTicks(void) { return svcGetSystemTick(); }

void plSwapScreens(void) {}

void plPollInput(PlInput* in) {
	hidScanInput();
	u32 held = hidKeysHeld();
	touchPosition touch = {0, 0};
	if (held & KEY_TOUCH) hidTouchRead(&touch);
	in->keys = mapKeys(held);
	in->touch = (held & KEY_TOUCH) != 0;
	in->tx = touch.px;
	in->ty = touch.py;
	in->turbo = (held & (KEY_Y | KEY_ZR)) != 0;   // hold Y / ZR
}


void plFatal(const char* msg) {
	launcherMessage(msg);
	gfxExit();
}

// Picks the game to run. With one ROM present it is used directly; with both, the player chooses (A / B) and
// the choice is remembered. Returns false (after showing a message) if nothing can be started.
bool plChooseGame(GsGameId* out) {
	int count = 0;
	for (int i = 0; i < GS_GAME_COUNT; ++i) {
		if (gsGameRomExists((GsGameId) i)) ++count;
	}
	if (!count) {
		char a[128], b[128], msg[300];
		snprintf(msg, sizeof(msg), "No ROM found. Put your ROM at:\n%s\n(Lost Age, optional:\n%s)", gsGameRomPath(GS_GAME_1, a, sizeof(a)), gsGameRomPath(GS_GAME_2, b, sizeof(b)));
		plFatal(msg);
		return false;
	}
	char lastPath[128];
	gsGamePath(GS_GAME_1, "lastgame.txt", lastPath, sizeof(lastPath));
	int last = 0;
	FILE* f = fopen(lastPath, "r");
	if (f) { int v; if (fscanf(f, "%d", &v) == 1 && v >= 0 && v < GS_GAME_COUNT) last = v; fclose(f); }
	const bool installed[2] = {gsGameRomExists(GS_GAME_1), gsGameRomExists(GS_GAME_2)};
	int pick = launcherPick(last, installed);
	if (pick < 0) { gfxExit(); return false; }
	f = fopen(lastPath, "w");
	if (f) { fprintf(f, "%d\n", pick); fclose(f); }
	*out = (GsGameId) pick;
	return true;
}

// ---- Frame pipeline --------------------------------------------------------------------------------------------------
// On a New 3DS the job runs on the second application core while the next frame is emulated; otherwise (Old 3DS, or profile "old") it runs
// inline on the same core.
static LightSemaphore jobReady, jobDone;
static volatile bool workerQuit;
static bool workerBusy;
static bool swapPending;
static u64 swapTick;
static volatile int vbWaits;   // real vblank waits since the last statistics line
static u64 vbRef;   // system tick just after a vblank we waited for (0 = none yet)
static Thread worker;
static unsigned cpuMs;

// True when a vblank (predicted from vbRef) fell between the two ticks.
static bool vblankBetween(u64 from, u64 to) {
	if (to <= from) return false;
	const double period = 268111856.0 / 59.8261;
	double a = ((double) from - (double) vbRef) / period, b = ((double) to - (double) vbRef) / period;
	return floor(b) > floor(a);
}

// Everything is drawn into these private buffers first, so a job never has to wait for the display before it starts painting: the wait for the
// vblank happens at the end, right before the finished picture is copied into the framebuffer and swapped in.
static uint16_t privTop[400 * 240], privBot[320 * 240];
static bool botDrawn;   // the bottom buffer was repainted by the last job (it is only copied then)

static void presentFrame(void) {
	memcpy(gfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL), privTop, sizeof(privTop));
	if (botDrawn) memcpy(gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, NULL, NULL), privBot, sizeof(privBot));
}

static void processJob(Job* j) {
	u64 t0 = svcGetSystemTick();
	blitTop(privTop, j->top, 256, j->ui.topScale);
	j->tTop = svcGetSystemTick() - t0;
	botDrawn = appProcessJob(j, privBot);
}

static void workerMain(void* arg) {
	(void) arg;
	for (;;) {
		LightSemaphore_Acquire(&jobReady, 1);
		if (workerQuit) break;
		// The other core may have written the job through its own data cache: drop stale lines before reading, and push our
		// framebuffer writes out before the buffers go to the GPU.
		svcInvalidateProcessDataCache(CUR_PROCESS_HANDLE, (u32) &appJob, sizeof(appJob));
		svcInvalidateProcessDataCache(CUR_PROCESS_HANDLE, (u32) appJobFrame, sizeof(appJobFrame));
		svcInvalidateProcessDataCache(CUR_PROCESS_HANDLE, (u32) appJobTop, sizeof(appJobTop));
		processJob(&appJob);   // paints into the private buffers: no waiting for the display here
		// The previous swap only takes effect at a vblank and the framebuffer we are about to fill is the one still on screen until then.
		// Waiting for the next vblank costs up to a whole frame, so it is skipped when a vblank has certainly happened since the swap: the
		// vblanks arrive every 4.48 M ticks (59.83 Hz) from a reference taken at the last real wait, and the reference is refreshed at least
		// every two seconds. Without this the wait was taken on about half of the frames and made the job longer than a frame, so the main
		// loop found the worker busy and dropped the next picture.
		if (swapPending) {
			u64 now = svcGetSystemTick();
			bool landed = vbRef && now - vbRef < (u64) 268111 * 2000 && vblankBetween(swapTick + 268111 * 3, now - 268111 * 2);
			if (!landed) { gspWaitForVBlank(); vbRef = svcGetSystemTick(); ++vbWaits; }
		}
		presentFrame();
		gfxFlushBuffers();
		gfxSwapBuffers();
		swapPending = true;
		swapTick = svcGetSystemTick();
		LightSemaphore_Release(&jobDone, 1);
	}
}

static void waitWorker(void) {
	if (workerBusy) { LightSemaphore_Acquire(&jobDone, 1); workerBusy = false; }
}

bool plTwoCore(void) { return worker != NULL; }

bool plJobSlotFree(void) {
	if (worker) {
		if (workerBusy && LightSemaphore_TryAcquire(&jobDone, 1) == 0) workerBusy = false;
		return !workerBusy;
	}
	waitWorker();
	return true;
}

void plJobSubmit(Job* j) {
	if (worker) {
		svcFlushProcessDataCache(CUR_PROCESS_HANDLE, (u32) j, sizeof(*j));
		svcFlushProcessDataCache(CUR_PROCESS_HANDLE, (u32) appJobFrame, sizeof(appJobFrame));
		if (j->top == appJobTop) svcFlushProcessDataCache(CUR_PROCESS_HANDLE, (u32) appJobTop, sizeof(appJobTop));
		workerBusy = true;
		LightSemaphore_Release(&jobReady, 1);
	} else {
		processJob(j);
		presentFrame();
		gfxFlushBuffers();
		gfxSwapBuffers();
		gspWaitForVBlank();   // the next frame is drawn into the buffer that was on screen until this vblank
	}
}

void plJobFinish(Job* j) {
	waitWorker();
	if (worker) {
		svcFlushProcessDataCache(CUR_PROCESS_HANDLE, (u32) j, sizeof(*j));
		workerBusy = true;
		LightSemaphore_Release(&jobReady, 1);
		waitWorker();
		workerQuit = true;
		LightSemaphore_Release(&jobReady, 1);
		threadJoin(worker, U64_MAX);
		threadFree(worker);
		worker = NULL;
	}
}

uint64_t plPace(void) {
	if (!worker) return 0;
	static u64 deadline;
	u64 now = svcGetSystemTick(), slept = 0;
	if (!deadline) deadline = now;
	deadline += (u64) (268111856.0 / 59.7275);
	if (now + 4000 < deadline) { svcSleepThread((s64) ((deadline - now) * 1000 / 268)); slept = svcGetSystemTick() - now; }
	else if (now > deadline + (u64) (268111856.0 / 59.7275) * 4) deadline = now;
	return slept;
}

unsigned plCpuMs(void) { return cpuMs; }
int plTakeVblankWaits(void) { int n = vbWaits; vbWaits = 0; return n; }

void plPerfExtra(char* out, size_t n) {
	snprintf(out, n, " appFreeKB=%u linearFreeKB=%u", (unsigned) (osGetMemRegionFree(MEMREGION_APPLICATION) / 1024), (unsigned) (linearSpaceFree() / 1024));
}

bool plRunning(void) { return aptMainLoop(); }

void plExit(void) {
	if (hasSound) ndspExit();
	gfxExit();
}

// A fixed integer workload timed at start-up: about 30 ms with the New 3DS clock (804 MHz + L2 cache) running, about 90 ms without it, so the
// "c" figure in the perflog line shows whether the console really is running at full speed.
static unsigned cpuBenchMs(void) {
	volatile uint32_t x = 1;
	u64 t0 = svcGetSystemTick();
	for (uint32_t i = 0; i < 3000000u; ++i) x = x * 1664525u + 1013904223u;
	return (unsigned) ((svcGetSystemTick() - t0) / 268123u);
}

bool plInit(void) {
	gfxInit(GSP_RGB565_OES, GSP_RGB565_OES, false);
	osSetSpeedupEnable(true);
	cpuMs = cpuBenchMs();
	gsGameInitRomfs();
	gsGameEnsureDir(GS_GAME_1);   // creates sdmc:/3ds/GoldenSun3DS before anything writes there
	bool newHw = false;
	APT_CheckNew3DS(&newHw);
	int profile = 0;   // 0 auto, 1 new (two cores), 2 old (single core): sdmc:/3ds/GoldenSun3DS/profile.txt, written by the patcher
	{
		char pp[128];
		FILE* pf = fopen(gsAppFilePath("profile.txt", pp, sizeof(pp)), "r");
		char w[16] = "";
		if (pf) { if (fscanf(pf, "%15s", w) == 1) profile = !strcmp(w, "new") ? 1 : !strcmp(w, "old") ? 2 : 0; fclose(pf); }
	}
	if (profile == 1 || (profile == 0 && newHw)) {
		LightSemaphore_Init(&jobReady, 0, 1);
		LightSemaphore_Init(&jobDone, 0, 1);
		s32 prio = 0x30;
		svcGetThreadPriority(&prio, CUR_THREAD_HANDLE);
		worker = threadCreate(workerMain, NULL, 128 * 1024, prio, 2, false);   // core 2 exists on New 3DS only; NULL falls back to inline
	}
	return true;
}

int main(void) {
	if (!plInit()) return 1;
	return appRun();
}
