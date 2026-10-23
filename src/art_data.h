#ifndef ART_DATA_H
#define ART_DATA_H
#include <stdint.h>

// Pre-rendered art baked from the reference mockups by tools/genart.ps1.
typedef struct {
	int w, h;
	const uint16_t* col;  // RGB565
	const uint8_t* alpha; // 0..255, or NULL when opaque
} ArtImage;

extern const ArtImage artBakedIconBig[6];    // tab icons, 46x32, index = ICON_* order
extern const ArtImage artBakedIconSmall[6];  // 29x20
extern const ArtImage artBakedPortrait[8];   // 68x46: Isaac, Garet, Ivan, Mia (mockup), Felix, Jenna, Sheba, Piers (game status-screen art)
extern const char* const artBakedPortraitNames[8];
extern const ArtImage artBakedCorner;        // 22x22 ornate frame corner (bottom-left; flip for the others)
extern const ArtImage artBakedLogo;          // 144x72 Golden Sun logo for the launcher (tools/genlogo.ps1)
extern const ArtImage artBakedCompass;       // 64x64 compass rose watermark
extern const uint16_t* const artBakedBackground;  // 320x240 row-major cloud texture

// The game's own menu art (tools/genicons.py -> icon_data.c): item icons keyed by item id (Golden Sun and The Lost Age have different tables), and the four Djinn element sprites.
extern const int artItemIconCount, artItemIconCountTla;
extern const uint16_t artItemIconIds[], artItemIconIdsTla[];      // sorted
extern const ArtImage artItemIconImages[], artItemIconImagesTla[];   // 16x16, index-aligned with the ids
extern const int artPsyIconCount, artPsyIconCountTla;
extern const uint16_t artPsyIconIds[], artPsyIconIdsTla[];
extern const ArtImage artPsyIconImages[], artPsyIconImagesTla[];   // 16x16 Psynergy-menu icons (field-usable abilities only), with their own backdrops
extern const ArtImage artDjinnImages[4];     // 24x30, GS_VENUS, GS_MARS, GS_JUPITER, GS_MERCURY order
#endif
