#include "bgwrite.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <3ds.h>

#define SLICE 4096   // bytes written per frame: small enough that even a slow SD write costs only a few milliseconds

typedef struct { char path[160]; uint8_t* data; size_t size; } Pending;

enum { ST_IDLE, ST_OPEN, ST_WRITE, ST_CLOSE, ST_REMOVE, ST_RENAME };

static LightLock lock;
static bool lockInit;
static Pending next;                    // waiting to start (guarded by lock)
static Pending cur;                     // being written (main loop only)
static FILE* curFile;
static size_t curDone;
static int state = ST_IDLE;
static bool curOk;

static void ensureLock(void) { if (!lockInit) { LightLock_Init(&lock); lockInit = true; } }

void bgWriteStart(const char* path, uint8_t* data, size_t size) {
	ensureLock();
	LightLock_Lock(&lock);
	free(next.data);   // an older request that never started is superseded
	snprintf(next.path, sizeof(next.path), "%s", path);
	next.data = data; next.size = size;
	LightLock_Unlock(&lock);
}

static void tmpName(char* out, size_t n) { snprintf(out, n, "%s.tmp", cur.path); }

static void done(void) {
	free(cur.data);
	cur.data = NULL;
	state = ST_IDLE;
}

bool bgWriteBusy(void) {
	ensureLock();
	LightLock_Lock(&lock);
	bool b = state != ST_IDLE || next.data;
	LightLock_Unlock(&lock);
	return b;
}

// One step per call. Opening, closing, deleting and renaming on a FAT card each cost far more than a small write, so each gets a frame of its own.
void bgWritePump(void) {
	ensureLock();
	char tmp[170];
	switch (state) {
	case ST_IDLE:
		LightLock_Lock(&lock);
		cur = next;
		next.data = NULL;
		LightLock_Unlock(&lock);
		if (cur.data) { state = ST_OPEN; curOk = true; }
		break;
	case ST_OPEN:
		tmpName(tmp, sizeof(tmp));
		curFile = fopen(tmp, "wb");
		curDone = 0;
		if (curFile) state = ST_WRITE; else done();
		break;
	case ST_WRITE: {
		size_t n = cur.size - curDone < SLICE ? cur.size - curDone : SLICE;
		if (n && fwrite(cur.data + curDone, 1, n, curFile) != n) curOk = false;
		curDone += n;
		if (!curOk || curDone >= cur.size) state = ST_CLOSE;
		break;
	}
	case ST_CLOSE:
		if (fclose(curFile) != 0) curOk = false;
		curFile = NULL;
		if (curOk) state = ST_REMOVE;
		else { tmpName(tmp, sizeof(tmp)); remove(tmp); done(); }
		break;
	case ST_REMOVE:
		remove(cur.path);
		state = ST_RENAME;
		break;
	case ST_RENAME:
		tmpName(tmp, sizeof(tmp));
		rename(tmp, cur.path);
		done();
		break;
	}
}

void bgWriteFlush(void) {
	while (bgWriteBusy()) bgWritePump();
}
