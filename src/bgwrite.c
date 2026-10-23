#include "bgwrite.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <3ds.h>

#define SLICE 16384

typedef struct { char path[160]; uint8_t* data; size_t size; } Pending;

static LightLock lock;
static bool lockInit;
static Pending next;                    // waiting to start (guarded by lock)
static Pending cur;                     // being written (main loop only)
static FILE* curFile;
static size_t curDone;

static void ensureLock(void) { if (!lockInit) { LightLock_Init(&lock); lockInit = true; } }

void bgWriteStart(const char* path, uint8_t* data, size_t size) {
	ensureLock();
	LightLock_Lock(&lock);
	free(next.data);   // an older request that never started is superseded
	snprintf(next.path, sizeof(next.path), "%s", path);
	next.data = data; next.size = size;
	LightLock_Unlock(&lock);
}

static void finishCur(bool ok) {
	char tmp[170];
	if (curFile) {
		ok = fclose(curFile) == 0 && ok;
		curFile = NULL;
	}
	if (ok) {
		snprintf(tmp, sizeof(tmp), "%s.tmp", cur.path);
		remove(cur.path);
		rename(tmp, cur.path);
	}
	free(cur.data);
	cur.data = NULL;
}

bool bgWriteBusy(void) {
	ensureLock();
	LightLock_Lock(&lock);
	bool b = cur.data || next.data;
	LightLock_Unlock(&lock);
	return b;
}

void bgWritePump(void) {
	ensureLock();
	if (!cur.data) {
		LightLock_Lock(&lock);
		cur = next;
		next.data = NULL;
		LightLock_Unlock(&lock);
		if (!cur.data) return;
		char tmp[170];
		snprintf(tmp, sizeof(tmp), "%s.tmp", cur.path);
		curFile = fopen(tmp, "wb");
		curDone = 0;
		if (!curFile) finishCur(false);
		return;   // opening the file was this frame's work
	}
	size_t n = cur.size - curDone < SLICE ? cur.size - curDone : SLICE;
	bool ok = n == 0 || fwrite(cur.data + curDone, 1, n, curFile) == n;
	curDone += n;
	if (!ok || curDone >= cur.size) finishCur(ok);
}

void bgWriteFlush(void) {
	while (bgWriteBusy()) bgWritePump();
}
