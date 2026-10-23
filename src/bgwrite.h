#ifndef BGWRITE_H
#define BGWRITE_H
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
// Background file writer: big saves (the stitched map, the autosave state) are handed over as a memory buffer and written to the SD card in
// small slices from the main loop, so a slow SD write costs a few milliseconds per frame instead of freezing the game for a second.
// The data goes to <path>.tmp and replaces <path> when complete. Thread-safe: start may be called from any thread, pump from the main loop.
void bgWriteStart(const char* path, uint8_t* malloced, size_t size);   // takes ownership of the buffer; a job still waiting is replaced
void bgWritePump(void);                                                  // writes one slice (about 16 KB) of the current job
void bgWriteFlush(void);                                                 // finishes everything (use at exit)
bool bgWriteBusy(void);
#endif
