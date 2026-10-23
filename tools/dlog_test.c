// Host check of the message log: dlog_test dump.bin [ringAddr]. Feeds the ring from a RAM dump (IWRAM 32K + EWRAM 256K) and prints the log.
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "../src/dialoglog.h"

int main(int argc, char** argv) {
	if (argc < 2) return 1;
	uint32_t addr = argc > 2 ? (uint32_t) strtoul(argv[2], NULL, 16) : 0x0203396Cu;
	FILE* f = fopen(argv[1], "rb");
	if (!f) return 1;
	static uint8_t d[0x8000 + 0x40000];
	size_t n = fread(d, 1, sizeof(d), f);
	fclose(f);
	(void) n;
	uint16_t ring[DLOG_RING_WORDS];
	for (int i = 0; i < DLOG_RING_WORDS; ++i) {
		uint32_t o = 0x8000 + (addr - 0x02000000u) + 2u * (uint32_t) i;
		ring[i] = (uint16_t) (d[o] | (d[o + 1] << 8));
	}
	dlogPoll(ring, DLOG_RING_WORDS);
	dlogPoll(ring, DLOG_RING_WORDS);   // the same ring again: nothing new, nothing doubled
	for (int i = 0; i < dlogCount(); ++i) printf("[%d] %s\n", i, dlogText(i));
	printf("entries=%d\n", dlogCount());
	return 0;
}
