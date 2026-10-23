// Procedurally generated UI art (icons, portraits), rendered once with 3x3 supersampling and cached.
#ifndef ART_H
#define ART_H
#include "gfx.h"

enum { ICON_PARTY, ICON_PSYNERGY, ICON_DJINN, ICON_MAP, ICON_ITEMS, ICON_MENU, ICON_COUNT };

#define ICON_SIZE 32     // height of the tab icons (they are 46 wide)
#define ICON_SMALL 20    // height of the small icons (29 wide)
#define PORTRAIT_W 68
#define PORTRAIT_H 46

const Sprite* artIcon(int id);       // 46 x 32, baked from the reference mockup
const Sprite* artIconSmall(int id);  // 29 x 20
// Portrait for a character name ("Isaac", "Garet", ...); unknown names get a neutral one.
const Sprite* artPortrait(const char* name);
// Portrait by character record index (0..7); falls back to the name for anything else (e.g. enemies).
const Sprite* artPortraitRec(int rec, const char* name);
// Item icon (16x16) for an item id in the active game (Golden Sun or The Lost Age), or NULL if there is none.
const Sprite* artItemIcon(unsigned id);
// Psynergy list icon for an ability id in the active game, reduced to 8x8 for the dense list; NULL if the game's menu has no icon for it
// (only field-usable Psynergy have one).
const Sprite* artPsyIconSmall(unsigned id);
// Djinn element mascot (24x30) for GS_VENUS / GS_MARS / GS_JUPITER / GS_MERCURY, (the same sprites in both games).
const Sprite* artDjinn(int element);
// Ornate panel-frame corner (22x22, bottom-left), faint compass rose (64x64) and a 320x240 row-major cloud texture.
const Sprite* artCorner(void);
const Sprite* artCompass(void);
const Sprite* artLogo(void);         // 144 x 72 Golden Sun logo with alpha
const uint16_t* artBackground(void);
#endif
