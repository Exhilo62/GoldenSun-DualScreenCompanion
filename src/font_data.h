#ifndef FONT_DATA_H
#define FONT_DATA_H
#include <stdint.h>

// Bitmap font: ASCII 32..126, 8-bit alpha, each glyph (adv + 2) wide and h tall, drawn 1px left of its origin.
typedef struct {
	int h;              // cell height
	int capTop;         // rows above the capital letters in the cell
	int capH;           // capital letter height
	const uint8_t* adv; // advance per glyph (pixels)
	const uint32_t* off;// byte offset of each glyph in data
	const uint8_t* data;
} GFont;

extern const GFont font_tiny, font_small, font_large;
#endif
