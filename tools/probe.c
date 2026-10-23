// Headless mGBA probe for RAM research. usage: probe rom.gba [save.sav]. Commands on stdin, one per line:
//   run N [KEYS]   run N frames holding KEYS (A B S=select T=start U D L R l=Lshoulder r=Rshoulder)
//   shot FILE      write PPM screenshot      dump FILE   write IWRAM(32K)+EWRAM(256K)
//   save FILE      write savestate           load FILE   restore savestate
//   r8/r16/r32 ADDR   print bus read         quit
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>
#include <mgba/core/core.h>
#include <mgba/core/serialize.h>
#include <mgba/internal/gba/input.h>
#include <mgba-util/vfs.h>
#include <mgba/internal/arm/arm.h>
#include <mgba/internal/arm/isa-inlines.h>
#include <mgba/internal/gba/gba.h>

static uint32_t keymask(const char* k) {
	uint32_t m = 0;
	for (; *k; ++k) switch (*k) {
		case 'A': m |= 1 << GBA_KEY_A; break;
		case 'B': m |= 1 << GBA_KEY_B; break;
		case 'S': m |= 1 << GBA_KEY_SELECT; break;
		case 'T': m |= 1 << GBA_KEY_START; break;
		case 'U': m |= 1 << GBA_KEY_UP; break;
		case 'D': m |= 1 << GBA_KEY_DOWN; break;
		case 'L': m |= 1 << GBA_KEY_LEFT; break;
		case 'R': m |= 1 << GBA_KEY_RIGHT; break;
		case 'l': m |= 1 << GBA_KEY_L; break;
		case 'r': m |= 1 << GBA_KEY_R; break;
	}
	return m;
}

int main(int argc, char** argv) {
	if (argc < 2) return 1;
	struct mCore* core = mCoreFind(argv[1]);
	if (!core) {
		fprintf(stderr, "mCoreFind failed for %s\n", argv[1]);
		return 1;
	}
	fprintf(stderr, "found\n");
	core->init(core);
	mCoreInitConfig(core, NULL);
	core->setAudioBufferSize(core, 1024);
	fprintf(stderr, "init\n");
	static uint32_t video[256 * 160];
	core->setVideoBuffer(core, (mColor*) video, 256);
	fprintf(stderr, "vbuf\n");
	bool ok = mCoreLoadFile(core, argv[1]);
	fprintf(stderr, "load=%d\n", ok);
	// optional argv[2]: raw .sav (battery save), e.g. a converted SharkPort save
	if (argc > 2) fprintf(stderr, "sav=%d\n", mCoreLoadSaveFile(core, argv[2], false));
	fprintf(stderr, "loaded\n");
	core->reset(core);
	char line[512];
	while (fgets(line, sizeof line, stdin)) {
		char cmd[32] = {0}, arg[400] = {0};
		sscanf(line, "%31s %399[^\n]", cmd, arg);
		if (!strcmp(cmd, "run")) {
			int n = 0; char keys[32] = {0};
			sscanf(arg, "%d %31s", &n, keys);
			core->setKeys(core, keymask(keys));
			for (int i = 0; i < n; ++i) core->runFrame(core);
			core->setKeys(core, 0);
		} else if (!strcmp(cmd, "shot")) {
			FILE* f = fopen(arg, "wb");
			fprintf(f, "P6\n240 160\n255\n");
			for (int y = 0; y < 160; ++y) for (int x = 0; x < 240; ++x) {
				uint32_t c = video[y * 256 + x];
				fputc(c & 0xFF, f); fputc((c >> 8) & 0xFF, f); fputc((c >> 16) & 0xFF, f);
			}
			fclose(f);
		} else if (!strcmp(cmd, "dump")) {
			FILE* f = fopen(arg, "wb");
			for (uint32_t a = 0x03000000; a < 0x03008000; ++a) fputc(core->busRead8(core, a), f);
			for (uint32_t a = 0x02000000; a < 0x02040000; ++a) fputc(core->busRead8(core, a), f);
			fclose(f);
		} else if (!strcmp(cmd, "save")) {
			struct VFile* vf = VFileOpen(arg, O_CREAT | O_TRUNC | O_RDWR);
			mCoreSaveStateNamed(core, vf, SAVESTATE_SAVEDATA | SAVESTATE_RTC);
			vf->close(vf);
		} else if (!strcmp(cmd, "load")) {
			struct VFile* vf = VFileOpen(arg, O_RDONLY);
			mCoreLoadStateNamed(core, vf, SAVESTATE_SAVEDATA | SAVESTATE_RTC);
			vf->close(vf);
		} else if (!strcmp(cmd, "r8") || !strcmp(cmd, "r16") || !strcmp(cmd, "r32")) {
			uint32_t a = strtoul(arg, NULL, 16);
			uint32_t v = cmd[1] == '8' ? core->busRead8(core, a) : cmd[1] == '1' ? core->busRead16(core, a) : core->busRead32(core, a);
			printf("%08X\n", v);
		} else if (!strcmp(cmd, "w8") || !strcmp(cmd, "w16") || !strcmp(cmd, "w32")) {
			unsigned a = 0, v = 0; sscanf(arg, "%x %x", &a, &v);
			if (cmd[1] == '8') core->busWrite8(core, a, v); else if (cmd[1] == '1') core->busWrite16(core, a, v); else core->busWrite32(core, a, v);
		} else if (!strcmp(cmd, "trace")) {
			// trace ADDR LEN FRAMES [KEYS]: single-step and report the instruction after which the bytes at ADDR..ADDR+LEN change
			unsigned a = 0, len = 0, frames = 0; char keys[32] = {0};
			sscanf(arg, "%x %u %u %31s", &a, &len, &frames, keys);
			core->setKeys(core, keymask(keys));
			struct ARMCore* cpu = core->cpu;
			uint8_t prev[256]; if (len > 256) len = 256;
			for (unsigned i = 0; i < len; ++i) prev[i] = core->busRead8(core, a + i);
			uint32_t start = core->frameCounter(core); int prints = 0;
			while (core->frameCounter(core) - start < frames && prints < 60) {
				core->step(core);
				for (unsigned i = 0; i < len; ++i) {
					uint8_t b = core->busRead8(core, a + i);
					if (b != prev[i]) {
						printf("chg %08X pc=%08X lr=%08X r0=%08X r1=%08X r2=%08X r3=%08X sp=%08X\n", a + i, cpu->gprs[15], cpu->gprs[14], cpu->gprs[0], cpu->gprs[1], cpu->gprs[2], cpu->gprs[3], cpu->gprs[13]);
						prints++;
						for (unsigned k = 0; k < len; ++k) prev[k] = core->busRead8(core, a + k);
						break;
					}
				}
			}
			core->setKeys(core, 0);
		} else if (!strcmp(cmd, "bp")) {
			// bp PC FRAMES [KEYS]: print registers and the first stack words each time execution reaches PC (Thumb or ARM)
			unsigned pc = 0, frames = 0; char keys[32] = {0};
			sscanf(arg, "%x %u %31s", &pc, &frames, keys);
			core->setKeys(core, keymask(keys));
			struct ARMCore* cpu = core->cpu;
			uint32_t start = core->frameCounter(core); int prints = 0;
			while (core->frameCounter(core) - start < frames && prints < 200) {
				core->step(core);
				uint32_t cur = cpu->gprs[15];
				if (cur == pc || cur == pc + 2 || cur == pc + 4) {
					printf("bp r8=%08X r9=%08X r10=%08X r4=%08X r5=%08X r6=%08X r7=%08X r0=%08X r1=%08X r2=%08X r3=%08X lr=%08X sp=%08X s0=%08X s1=%08X s2=%08X s3=%08X\n", cpu->gprs[8], cpu->gprs[9], cpu->gprs[10], cpu->gprs[4], cpu->gprs[5], cpu->gprs[6], cpu->gprs[7], cpu->gprs[0], cpu->gprs[1], cpu->gprs[2], cpu->gprs[3], cpu->gprs[14], cpu->gprs[13],
						core->busRead32(core, cpu->gprs[13]), core->busRead32(core, cpu->gprs[13] + 4), core->busRead32(core, cpu->gprs[13] + 8), core->busRead32(core, cpu->gprs[13] + 12));
					printf("bq r10:");
					for (int k = 0; k < 8; ++k) printf(" %08X", core->busRead32(core, cpu->gprs[10] + 4 * k));
					printf("\n");
					prints++;
				}
			}
			core->setKeys(core, 0);
		} else if (!strcmp(cmd, "steps")) {
			// steps PC COUNT FRAMES [KEYS]: after the first time execution reaches PC, print COUNT executed instruction addresses with r0-r3
			unsigned pc = 0, count = 0, frames = 0; char keys[32] = {0};
			sscanf(arg, "%x %u %u %31s", &pc, &count, &frames, keys);
			core->setKeys(core, keymask(keys));
			struct ARMCore* cpu = core->cpu;
			uint32_t start = core->frameCounter(core); unsigned printed = 0; int armed = 0;
			while (core->frameCounter(core) - start < frames && printed < count) {
				core->step(core);
				if (!armed && (cpu->gprs[15] == pc || cpu->gprs[15] == pc + 2 || cpu->gprs[15] == pc + 4)) armed = 1;
				if (armed) {
					printf("st pc=%08X r0=%08X r1=%08X r2=%08X r3=%08X r4=%08X r5=%08X r6=%08X r7=%08X t=%d\n", cpu->gprs[15], cpu->gprs[0], cpu->gprs[1], cpu->gprs[2], cpu->gprs[3], cpu->gprs[4], cpu->gprs[5], cpu->gprs[6], cpu->gprs[7], cpu->cpsr.t);
					printed++;
				}
			}
			core->setKeys(core, 0);
		} else if (!strcmp(cmd, "bpdump")) {
			// bpdump PC ADDR LEN FILE FRAMES [KEYS]: write LEN bytes at ADDR to FILE the first time execution reaches PC
			unsigned pc = 0, a = 0, len = 0, frames = 0; char file[300] = {0}, keys[32] = {0};
			sscanf(arg, "%x %x %u %299s %u %31s", &pc, &a, &len, file, &frames, keys);
			core->setKeys(core, keymask(keys));
			struct ARMCore* cpu = core->cpu;
			uint32_t start = core->frameCounter(core); int done = 0;
			while (!done && core->frameCounter(core) - start < frames) {
				core->step(core);
				if (cpu->gprs[15] == pc || cpu->gprs[15] == pc + 2 || cpu->gprs[15] == pc + 4) {
					FILE* f = fopen(file, "wb");
					for (unsigned i = 0; i < len; ++i) fputc(core->busRead8(core, a + i), f);
					fclose(f); done = 1;
				}
			}
			printf("bpdump %d\n", done);
			core->setKeys(core, 0);
		} else if (!strcmp(cmd, "loadbin")) {
			// loadbin FILE ADDR: write a file's bytes into the bus at ADDR
			char file[300] = {0}; unsigned a = 0; sscanf(arg, "%299s %x", file, &a);
			FILE* f = fopen(file, "rb"); int c; unsigned n = 0;
			while (f && (c = fgetc(f)) != EOF) core->busWrite8(core, a + n++, c);
			if (f) fclose(f);
			printf("loadbin %u\n", n);
		} else if (!strcmp(cmd, "callarm")) {
			// callarm ADDR R0 R1 R2 R3: run an ARM-state function until it returns; prints r0-r3 afterwards. Leaves the CPU in a modified state.
			unsigned a = 0, r0 = 0, r1 = 0, r2 = 0, r3 = 0; sscanf(arg, "%x %x %x %x %x", &a, &r0, &r1, &r2, &r3);
			struct ARMCore* cpu = core->cpu;
			const uint32_t sentinel = 0x08000100;
			cpu->gprs[0] = r0; cpu->gprs[1] = r1; cpu->gprs[2] = r2; cpu->gprs[3] = r3;
			cpu->gprs[14] = sentinel; cpu->gprs[15] = a; cpu->cpsr.t = 0; cpu->executionMode = MODE_ARM;
			ARMWritePC(cpu);
			int steps = 0;
			while (steps < 200000 && (cpu->gprs[15] & ~3u) != sentinel + 0 && (cpu->gprs[15] & ~3u) != sentinel + 4 && (cpu->gprs[15] & ~3u) != sentinel + 8) { core->step(core); steps++; }
			printf("callarm r0=%08X r1=%08X r2=%08X r3=%08X steps=%d\n", cpu->gprs[0], cpu->gprs[1], cpu->gprs[2], cpu->gprs[3], steps);
		} else if (!strcmp(cmd, "bench")) {
			// bench FRAMES FRAMESKIP: run frames with the given mGBA frameskip and print the average host milliseconds per frame
			int n = 0, fs = 0; sscanf(arg, "%d %d", &n, &fs);
			((struct GBA*) core->board)->video.frameskip = fs;
			clock_t c0 = clock();
			for (int i = 0; i < n; ++i) core->runFrame(core);
			printf("bench %.3f ms/frame\n", 1000.0 * (clock() - c0) / CLOCKS_PER_SEC / n);
		} else if (!strcmp(cmd, "idle")) {
			// idle MODE: 0 ignore, 1 remove (known loops only), 2 detect
			int m = 0; sscanf(arg, "%d", &m);
			((struct GBA*) core->board)->idleOptimization = m;
		} else if (!strcmp(cmd, "layer")) {
			unsigned lid = 0, on = 1; sscanf(arg, "%u %u", &lid, &on); size_t id = lid;   // 0-3 BG0-3, 4 OBJ, 5-7 windows
			fputs("layer-call ", stderr); core->enableVideoLayer(core, id, on); fputs("layer-done ", stderr);
		} else if (!strcmp(cmd, "quit")) break;
		printf("ok\n"); fflush(stdout);
	}
	core->deinit(core);
	return 0;
}
