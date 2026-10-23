// Minimal software drawing for the 320x240 RGB565 bottom framebuffer (3DS layout: fb[x * 240 + (239 - y)]).
#ifndef GFX_HEADER_H
#define GFX_HEADER_H
#include <stdbool.h>
#include <stdint.h>

#define GFX_W 320
#define GFX_H 240

#define RGB(r, g, b) ((uint16_t) ((((r) >> 3) << 11) | (((g) >> 2) << 5) | ((b) >> 3)))

typedef struct {
	int w, h;
	uint16_t* col;  // RGB565
	uint8_t* alpha; // 0..255
} Sprite;

void gBegin(uint16_t* fb);
// Direct pointer to pixel (x, y); moving down the screen decrements the pointer. No bounds checks.
uint16_t* gPixelPtr(int x, int y);
// Whole-screen gradient cached after the first call.
void gBackground(uint16_t top, uint16_t bottom);
// Copy a screen region out to / back from a buffer of w*h pixels (column-major), used to cache static parts of the UI.
void gSaveRegion(int x, int y, int w, int h, uint16_t* buf);
void gRestoreRegion(int x, int y, int w, int h, const uint16_t* buf);
uint16_t gMix(uint16_t a, uint16_t b, int t256);
void gPx(int x, int y, uint16_t c);
void gPxA(int x, int y, uint16_t c, int a256);
void gBlit(int x, int y, const uint16_t* src, int stride, int w, int h);
uint16_t gGet(int x, int y);
void gRect(int x, int y, int w, int h, uint16_t c);
void gRectA(int x, int y, int w, int h, uint16_t c, int a256);
void gVGrad(int x, int y, int w, int h, uint16_t top, uint16_t bottom);
// Rounded panel: gold outline, bronze inner line, vertical gradient fill.
void gPanel(int x, int y, int w, int h, uint16_t top, uint16_t bottom, uint16_t outline);
// Pill with pointed (diamond) ends, used for headers.
void gPill(int x, int y, int w, int h, uint16_t top, uint16_t bottom, uint16_t edge);
void gSprite(int x, int y, const Sprite* s);
// Sprite with optional horizontal / vertical flips (used for the four frame corners).
void gSpriteFlip(int x, int y, const Sprite* s, bool flipX, bool flipY);
// Overlay the ornate corner flourishes on a panel's four corners.
void gOrnateCorners(int x, int y, int w, int h, const Sprite* corner);
// Use a 320x240 row-major RGB565 image for gBackground instead of the plain gradient (NULL = gradient).
// Set once at startup so gPanel can draw ornate corners.
extern const Sprite* gCornerSprite;
void gSetBackgroundImage(const uint16_t* image);
void gSpriteClip(int x, int y, const Sprite* s, int sx, int sy, int sw, int sh);
void gSpriteTint(int x, int y, const Sprite* s, uint16_t tint, int t256);

// Bitmap text (Ubuntu fonts, see src/font_data.c). scale 0 = tiny (10px bold), 1 = body (11px), 2 = heading (16px bold).
// y is the top of capital letters. Widths are proportional.
int gTextW(const char* s, int scale);
void gText(int x, int y, const char* s, int scale, uint16_t c);
void gTextShadow(int x, int y, const char* s, int scale, uint16_t c);
void gTextCentered(int cx, int y, const char* s, int scale, uint16_t c);
void gTextRight(int rx, int y, const char* s, int scale, uint16_t c);
#endif
