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
	int offX, offY;      // game pixels to subtract from the camera to get layer-local coordinates (layer 2's origin: layer 2 scrolls with the camera)
	int extX, extY;      // size of a layer in game pixels: the layer-local coordinate wraps at this size
	bool skipLayer2;     // the glow layer (alpha blended by the game) was left out
} MdMap;

// rom/size: the cartridge image of game 1 (Golden Sun) or 2 (The Lost Age). Returns true when the tables of that game's USA/Europe release were
// recognised; pass NULL for any other ROM.
bool mdInit(const uint8_t* rom, size_t size, int game);
bool mdAvailable(void);
// Decodes and draws map `mapId`. camX/camY: the game's camera (world position of the screen's top-left); psx/psy: the player's feet on screen. Only the
// connected group of rooms around the player is kept. Returns false (and leaves no map) if the data does not look right.
bool mdEnter(int mapId, int camX, int camY, int psx, int psy, bool skipLayer2);
const MdMap* mdCurrent(void);
void mdForget(void);
// For tests: keep the whole layer area instead of cropping to the player's rooms.
void mdSetNoCrop(bool on);
// For tests: how many empty cells (x2) may separate two rooms of one group (default 1: gaps of up to two cells join).
void mdSetGap(int r);
// Decompresses one game blob (format byte first); returns the output length or 0 on error. Exposed for tests.
size_t mdDecompress(const uint8_t* in, size_t inSize, uint8_t* out, size_t cap);
#endif
