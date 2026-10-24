// Draws a complete area map straight from the Golden Sun ROM (the user's own cartridge image): the game's map graphics are decoded and the
// three background layers composited, so the Map tab can show the whole area without exploring it. See docs/MAP_DATA.md for the formats.
#ifndef MAPDECODE_H
#define MAPDECODE_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
	int mapId;
	int w, h;            // canvas size in pixels (half resolution: 1 canvas pixel = 2 game pixels)
	uint16_t* px;        // w*h RGB565, 0 = nothing here
	int originX, originY;  // layer-local game pixel of canvas (0,0)
	int l0x, l0y;        // game pixels to subtract from the BG3 scroll to get layer-local coordinates (layer 0's origin)
	bool skipLayer2;     // the glow layer (alpha blended by the game) was left out
} MdMap;

// rom/size: the cartridge image. Returns true when the tables of the USA/Europe Golden Sun were recognised; pass NULL for any other game.
bool mdInit(const uint8_t* rom, size_t size);
bool mdAvailable(void);
// Decodes and draws map `mapId`. bg3x/bg3y: the game's BG3 scroll (layer 0); psx/psy: the player's feet on screen. Only the connected group of rooms
// around the player is kept. Returns false (and leaves no map) if the data does not look right.
bool mdEnter(int mapId, int bg3x, int bg3y, int psx, int psy, bool skipLayer2);
const MdMap* mdCurrent(void);
void mdForget(void);
// For tests: keep the whole layer area instead of cropping to the player's rooms.
void mdSetNoCrop(bool on);
// For tests: how many empty cells (x2) may separate two rooms of one group (default 1: gaps of up to two cells join).
void mdSetGap(int r);
// Decompresses one game blob (format byte first); returns the output length or 0 on error. Exposed for tests.
size_t mdDecompress(const uint8_t* in, size_t inSize, uint8_t* out, size_t cap);
#endif
