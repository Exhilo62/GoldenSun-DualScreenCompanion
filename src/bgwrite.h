#ifndef BGWRITE_H
#define BGWRITE_H
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
// Background file writer. Saves (the stitched map, the autosave state, settings, the bestiary) are handed over as a memory buffer and written to
// the SD card by a low-priority thread, so a slow card (opening, closing, deleting and renaming files can each stall for a tenth of a second) never
// freezes the game loop. Jobs run in order; a job for a path that is still waiting replaces the older one.
// By default the data goes to <path>.tmp and replaces <path> when complete. Thread-safe: start may be called from any thread.
void bgWriteInit(int core);                                              // start the thread (core -1 = the default core, 2 = the second New 3DS core)
void bgWriteStart(const char* path, uint8_t* malloced, size_t size);   // takes ownership of the buffer
// In-place variant for files whose loss or damage is harmless (the explored map): the existing file is overwritten without being deleted and
// renamed. The file may keep stale bytes after the new data if it got shorter.
void bgWriteStartInPlace(const char* path, uint8_t* malloced, size_t size);
void bgWriteAppend(const char* path, uint8_t* malloced, size_t size);   // appends to the file (log lines); never replaces a queued job
void bgWritePump(void);                                                  // kept for older callers: the thread does the work now
void bgWriteFlush(void);                                                 // waits until everything is on the card (use at exit)
bool bgWriteBusy(void);
#endif
