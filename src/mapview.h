// Live exploration map: stitches the game's own frames into a per-area mosaic as the player walks.
// World mapping (verified in Vale): the visible frame's top-left corner is (camX, camY) in world pixels.
#ifndef MAPVIEW_H
#define MAPVIEW_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "gs_state.h"

#define MAP_W 800  // half-resolution canvas (1600 world px wide)
#define MAP_H 800  // (1600 world px tall)
#define MAP_SLOTS 3
#define MAP_POIS 24

// A door-like point of interest: where the player left this area for toId, or arrived from it (canvas coordinates).
typedef struct {
	int x, y;
	int toId;
} MapPoi;

typedef struct {
	bool used;
	uint32_t key;
	int originX, originY;     // world coordinates of canvas (0,0)
	uint16_t* px;             // MAP_W*MAP_H RGB565, 0 = unexplored
	int minX, minY, maxX, maxY;  // explored bounds in canvas coordinates
	int playerX, playerY;     // canvas coordinates of the player marker
	bool hasPlayer;
	unsigned lastUse;
	MapPoi pois[MAP_POIS];
	int poiCount;
} MapSlot;

void mapInit(void);
void mapUpdate(const uint16_t* video, size_t stride, const GsState* st);
const MapSlot* mapCurrent(void);
bool mapRomActive(void);   // the current area is drawn from the cartridge's own map data (mapdecode.c) instead of the stitched mosaic
unsigned mapVersion(void);  // increments whenever the mosaic changed
void mapClearCurrent(void);
// Forget the previous area so a jump (loading a state) is not recorded as a door.
void mapForgetTransition(void);
// Persistence: the explored part of every area (bounding box only) plus its points of interest.
bool mapDirty(void);            // changed since the last save
bool mapSave(const char* path);
uint8_t* mapSerialize(size_t* outSize);   // the same data as one malloc'd block (caller frees); marks the map as saved
bool mapLoad(const char* path);
// Draw the current map into a viewport of the bottom framebuffer (call gBegin first).
// zoom 1 = half-res 1:1, zoom 2 = overview (1/4 res). pan is in canvas pixels from the player.
void mapDraw(int vx, int vy, int vw, int vh, int zoom, int panX, int panY);
#endif
