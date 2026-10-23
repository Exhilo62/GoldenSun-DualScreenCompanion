#ifndef LAUNCHER_H
#define LAUNCHER_H
#include <stdbool.h>
// Themed start-up screens. launcherPick shows the game picker (last = highlighted game, 0 or 1; installed[i] = that ROM exists) and returns the chosen game
// index or -1 when the player exits. launcherMessage shows an error message and waits for START.
int launcherPick(int last, const bool installed[2]);   // a game without a ROM is shown greyed out and cannot be chosen
void launcherMessage(const char* msg);
#endif
