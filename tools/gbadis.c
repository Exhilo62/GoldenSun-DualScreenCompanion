// Disassembles ROM code with mGBA's decoder. usage: gbadis rom.gba START_ADDR LEN_BYTES [arm]   (addresses like 08002F40; Thumb unless "arm")
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <mgba/internal/arm/decoder.h>
#include <mgba/internal/arm/arm.h>

int main(int argc, char** argv) {
	if (argc < 4) return 1;
	FILE* f = fopen(argv[1], "rb");
	if (!f) return 2;
	static uint8_t rom[0x2000000];
	size_t n = fread(rom, 1, sizeof(rom), f);
	(void) n;
	unsigned a = (unsigned) strtoul(argv[2], NULL, 16), len = (unsigned) strtoul(argv[3], NULL, 0);
	int arm = argc > 4 && !strcmp(argv[4], "arm");
	for (unsigned off = 0; off < len;) {
		unsigned o = (a & 0x1FFFFFF) + off;
		struct ARMInstructionInfo info; char buf[128];
		if (arm) {
			uint32_t op = rom[o] | rom[o + 1] << 8 | rom[o + 2] << 16 | (uint32_t) rom[o + 3] << 24;
			ARMDecodeARM(op, &info);
			ARMDisassemble(&info, NULL, NULL, 0x08000000 + o + 8, buf, sizeof(buf));
			printf("%08X: %08X  %s\n", 0x08000000 + o, op, buf);
			off += 4;
		} else {
			uint16_t op = rom[o] | rom[o + 1] << 8, op2 = rom[o + 2] | rom[o + 3] << 8;
			struct ARMInstructionInfo info2, comb;
			ARMDecodeThumb(op, &info);
			ARMDecodeThumb(op2, &info2);
			if (ARMDecodeThumbCombine(&info, &info2, &comb)) {
				ARMDisassemble(&comb, NULL, NULL, 0x08000000 + o + 4, buf, sizeof(buf));
				printf("%08X: %04X%04X  %s\n", 0x08000000 + o, op, op2, buf);
				off += 4;
			} else {
				ARMDisassemble(&info, NULL, NULL, 0x08000000 + o + 4, buf, sizeof(buf));
				printf("%08X: %04X  %s\n", 0x08000000 + o, op, buf);
				off += 2;
			}
		}
	}
	return 0;
}
