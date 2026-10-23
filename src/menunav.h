// Tap-to-select on the mirrored GBA menu: steers the game's pointing hand towards a tapped spot using D-pad presses.
#ifndef MENUNAV_H
#define MENUNAV_H
#include <stdbool.h>

#include "gs_state.h"

typedef struct {
	bool active;
	int tx, ty;      // target hand position (top-left, GBA screen pixels)
	int wait;        // frames to wait after a press before looking again
	int pressFrames; // frames left to hold the current direction
	unsigned dir;    // UK_* being held
	int lastErr;     // error on the axis being moved, before the last press (stops on overshoot)
	int stuck;       // consecutive presses that did not move the hand
	int prevX, prevY;
	bool finishing;  // the undo press after an overshoot is in flight
} MenuNav;

// A tap at GBA-screen point (fx, fy). Returns true if the hand is already there (the caller should press A).
bool navTap(MenuNav* n, const GsState* st, int fx, int fy);
// UK_* buttons to hold this frame while a move is in progress.
unsigned navStep(MenuNav* n, const GsState* st);
#endif
