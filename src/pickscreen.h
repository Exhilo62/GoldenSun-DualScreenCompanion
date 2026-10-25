// The themed game pick screen for the SDL platforms (Android, desktop): the same cards, colours and art as the 3DS launcher (launcher.c), drawn into two
// 320x240 canvases in the 3DS framebuffer layout (column-major, flipped) so the platform can show them like the companion UI. No platform code in here.
#ifndef PICKSCREEN_H
#define PICKSCREEN_H
#include <stdbool.h>
#include <stdint.h>

#include "gfx.h"

enum { PICK_GAME1, PICK_GAME2, PICK_ADD, PICK_COUNT };   // the three things that can be chosen; PICK_NONE for a tap on nothing
#define PICK_NONE (-1)

// Top canvas: logo banner with the version; `sub` is a line under the tagline (the selected game, or a status such as "ROM added").
void pickDrawTop(uint16_t* canvas, const char* sub);
// Bottom canvas: the two game cards and the "Add ROM file" button. sel = highlighted PICK_*, last = last played game (0 / 1, or -1).
void pickDrawBottom(uint16_t* canvas, int sel, int last, const bool installed[2]);
// Which PICK_* a tap at (x, y) on the bottom canvas hits, or PICK_NONE.
int pickHit(int x, int y);
#endif
