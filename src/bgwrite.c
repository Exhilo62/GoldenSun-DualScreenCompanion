#include "bgwrite.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <3ds.h>

#define QUEUE_N 8
#define CHUNK 16384

typedef struct { char path[160]; uint8_t* data; size_t size; bool inPlace; bool append; } Job;

static LightLock lock;
static LightSemaphore jobs;
static Job queue[QUEUE_N];
static int qHead, qCount;
static int active;              // queued plus running
static Thread thread;
static bool started;

static bool writeJob(const Job* j) {
	char tmp[170];
	FILE* f;
	if (j->append) {
		f = fopen(j->path, "ab");
	} else if (j->inPlace) {
		f = fopen(j->path, "r+b");
		if (!f) f = fopen(j->path, "wb");
	} else {
		snprintf(tmp, sizeof(tmp), "%s.tmp", j->path);
		f = fopen(tmp, "wb");
	}
	if (!f) return false;
	bool ok = true;
	for (size_t done = 0; ok && done < j->size; ) {
		size_t n = j->size - done < CHUNK ? j->size - done : CHUNK;
		ok = fwrite(j->data + done, 1, n, f) == n;
		done += n;
	}
	if (fclose(f) != 0) ok = false;
	if (!j->inPlace && !j->append) {
		if (ok) { remove(j->path); rename(tmp, j->path); }
		else remove(tmp);
	}
	return ok;
}

static void threadMain(void* arg) {
	(void) arg;
	for (;;) {
		LightSemaphore_Acquire(&jobs, 1);
		LightLock_Lock(&lock);
		if (qCount == 0) { LightLock_Unlock(&lock); continue; }
		Job j = queue[qHead];
		qHead = (qHead + 1) % QUEUE_N;
		--qCount;
		LightLock_Unlock(&lock);
		writeJob(&j);
		free(j.data);
		LightLock_Lock(&lock);
		--active;
		LightLock_Unlock(&lock);
	}
}

void bgWriteInit(int core) {
	if (started) return;
	LightLock_Init(&lock);
	LightSemaphore_Init(&jobs, 0, QUEUE_N);
	thread = threadCreate(threadMain, NULL, 64 * 1024, 0x3F, core, false);   // lowest priority: it only runs when the game threads wait
	if (!thread && core != -1) thread = threadCreate(threadMain, NULL, 64 * 1024, 0x3F, -1, false);
	started = thread != NULL;
	if (!started) LightLock_Init(&lock);
}

static void enqueue(const char* path, uint8_t* data, size_t size, bool inPlace, bool append) {
	if (!started) {   // no thread (creation failed): write right here
		Job j; snprintf(j.path, sizeof(j.path), "%s", path); j.data = data; j.size = size; j.inPlace = inPlace; j.append = append;
		writeJob(&j); free(data);
		return;
	}
	LightLock_Lock(&lock);
	for (int i = 0; i < qCount; ++i) {   // a newer copy of the same file replaces one that has not started yet
		Job* q = &queue[(qHead + i) % QUEUE_N];
		if (!append && !q->append && !strcmp(q->path, path)) { free(q->data); q->data = data; q->size = size; q->inPlace = inPlace; LightLock_Unlock(&lock); return; }
	}
	if (qCount == QUEUE_N) { LightLock_Unlock(&lock); free(data); return; }   // far more than ever expected: drop rather than block
	Job* q = &queue[(qHead + qCount) % QUEUE_N];
	snprintf(q->path, sizeof(q->path), "%s", path);
	q->data = data; q->size = size; q->inPlace = inPlace; q->append = append;
	++qCount; ++active;
	LightLock_Unlock(&lock);
	LightSemaphore_Release(&jobs, 1);
}

void bgWriteStart(const char* path, uint8_t* data, size_t size) { enqueue(path, data, size, false, false); }
void bgWriteAppend(const char* path, uint8_t* data, size_t size) { enqueue(path, data, size, false, true); }
void bgWriteStartInPlace(const char* path, uint8_t* data, size_t size) { enqueue(path, data, size, true, false); }
void bgWritePump(void) {}

bool bgWriteBusy(void) {
	if (!started) return false;
	LightLock_Lock(&lock);
	bool b = active > 0;
	LightLock_Unlock(&lock);
	return b;
}

void bgWriteFlush(void) {
	while (bgWriteBusy()) svcSleepThread(2000000);   // 2 ms
}
