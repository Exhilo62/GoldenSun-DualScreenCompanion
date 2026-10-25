// What the shared app (app.c) needs from the system it runs on. Implemented by platform_3ds.c (Nintendo 3DS) and by the SDL layer in desktop/ (Windows, later Android).
#ifndef PLATFORM_H
#define PLATFORM_H
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "gs_game.h"

struct mCore;
struct Job;

typedef struct {
	uint32_t keys;   // GBA buttons held, as a bit mask of 1 << GBA_KEY_*
	bool touch;      // the touch screen is pressed
	int tx, ty;      // touch position on the 320x240 bottom screen
	bool turbo;      // the fast-forward button is held
} PlInput;

// Clock. Ticks are whatever the platform counts in; these constants say how many make a millisecond and how the loop's timing figures are scaled.
#ifdef __3DS__
#define PL_TICKS_PER_MS 268123ull
#define PL_FPS_SCALE 268123000.0     // frames * this / elapsed ticks = frames per second
#define PL_FF_CAP_TICKS 8850000.0    // fast-forward keeps about 30 loops a second: frames per loop * ticks per frame <= this
#else
#define PL_TICKS_PER_MS 1000ull
#define PL_FPS_SCALE 1000000.0
#define PL_FF_CAP_TICKS 33000.0
#endif
uint64_t plTicks(void);

bool plInit(void);                      // first call of the platform's main(); false = give up
bool plRunning(void);                   // the main loop condition (the application was not asked to quit)
void plExit(void);
void plFatal(const char* msg);          // show a message to the player (the app then stops)
bool plChooseGame(GsGameId* out);       // which game to run (a picker on the 3DS); false = nothing to run

void plPollInput(PlInput* in);

// Audio: plAudioInit connects the core's sound output (before core->reset). Muting is asked for by the loop every frame.
void plAudioInit(struct mCore* core);
void plAudioMute(bool mute);
bool plHasSound(void);
bool plAudioSkipped(void);              // sound was left off because an earlier start-up crashed (3DS only)

// Frame pipeline. The app fills appJob (and appJobFrame / appJobTop) and submits it; the platform runs appProcessJob() on it, draws the top
// screen from job.top and shows the result, on another thread when it has the cores for it.
bool plTwoCore(void);                   // jobs run on a second core
bool plJobSlotFree(void);               // a new job may be submitted (waits for the previous one when the platform runs jobs inline)
void plJobSubmit(struct Job* j);
void plJobFinish(struct Job* j);        // run the last job (the final map save) and stop the pipeline
uint64_t plPace(void);                  // sleep to the GBA's 59.73 Hz when running ahead; returns the ticks slept
int plTakeVblankWaits(void);            // real display waits since the last call (shown in the perflog line), 0 where not counted
unsigned plCpuMs(void);                 // start-up CPU benchmark (shown in the perflog line), 0 if not measured
void plPerfExtra(char* out, size_t n);  // free-memory figures for the perflog line
#endif
