// The shared application: emulation loop, touch actions, save states, settings and the render job. Platform-neutral; see platform.h.
#ifndef APP_H
#define APP_H
#include <stdbool.h>
#include <stdint.h>

#include "dialoglog.h"
#include "gs_state.h"
#include "ui.h"

// Everything that is not emulation (top-screen copy, map stitching, bottom-screen drawing) is one Job.
typedef struct Job {
	GsState gs;
	UiView ui;
	const uint16_t* frame;   // snapshot of the live GBA frame
	const uint16_t* top;     // frame shown on the top screen (differs from `frame` while a game menu is mirrored below)
	bool doMap, clearMap, forgetMap, saveMap, finalSave;
	bool reloadSlots;        // re-read the save-slot thumbnails
	bool haveRing;           // ring holds a fresh copy of the text ring for the message log
	uint16_t ring[DLOG_RING_WORDS];
	uint64_t tTop, tMap, tUi;   // outputs: ticks spent (tTop is set by the platform's top-screen copy)
} Job;

extern Job appJob;
extern uint16_t appJobFrame[256 * 160], appJobTop[256 * 160];

int appRun(void);   // the game loop; the platform's main() calls plInit() and then this
// The platform-neutral part of a job: map stitching, slot thumbnails, message log, change detection and the bottom-screen drawing into botFb
// (320x240, the 3DS framebuffer layout). Returns true when botFb was repainted.
bool appProcessJob(Job* j, uint16_t* botFb);
#endif
