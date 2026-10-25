// Desktop / Android counterpart of src/bgwrite.c: same interface, but every write happens immediately on the calling thread (a phone's storage
// and a PC disk do not stall the way a 3DS SD card does).
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/bgwrite.h"

static void writeFile(const char* path, uint8_t* data, size_t size, bool inPlace, bool append) {
	char tmp[170];
	FILE* f;
	if (append) f = fopen(path, "ab");
	else if (inPlace) { f = fopen(path, "r+b"); if (!f) f = fopen(path, "wb"); }
	else { snprintf(tmp, sizeof(tmp), "%s.tmp", path); f = fopen(tmp, "wb"); }
	bool ok = f != NULL;
	if (f) {
		ok = fwrite(data, 1, size, f) == size;
		if (fclose(f) != 0) ok = false;
		if (!inPlace && !append) {
			if (ok) { remove(path); rename(tmp, path); }
			else remove(tmp);
		}
	}
	free(data);
}

void bgWriteInit(int core) { (void) core; }
void bgWriteStart(const char* path, uint8_t* malloced, size_t size) { writeFile(path, malloced, size, false, false); }
void bgWriteStartInPlace(const char* path, uint8_t* malloced, size_t size) { writeFile(path, malloced, size, true, false); }
void bgWriteAppend(const char* path, uint8_t* malloced, size_t size) { writeFile(path, malloced, size, false, true); }
void bgWritePump(void) {}
void bgWriteFlush(void) {}
bool bgWriteBusy(void) { return false; }
