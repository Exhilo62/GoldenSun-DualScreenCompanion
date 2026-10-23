// Area (map id) names. Names live in compressed ROM text, so this is a hand-built table that can be extended at run time
// from an optional text file on the SD card.
#ifndef GS_AREAS_H
#define GS_AREAS_H
#include <stdbool.h>

// Name for a map id; never NULL. Unknown ids come back as "Area 0xNNN" (or a region guess).
const char* gsAreaName(int id);
// Loads extra/overriding names from a text file: one per line, "<id> <name>", id decimal or 0x-hex, '#' starts a comment.
void gsAreasLoad(const char* path);
// The built-in names are for Golden Sun only; turn them off for other games.
void gsAreasUseBuiltin(bool on);
// Which game's built-in table to use: 0 Golden Sun (default), 1 The Lost Age.
void gsAreasSelectGame(int game);
#endif
