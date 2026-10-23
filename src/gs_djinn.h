#ifndef GS_DJINN_H
#define GS_DJINN_H
// Djinn: element 0..3 (Venus, Mars, Jupiter, Mercury), index 0..6.
const char* gsDjinnName(int element, int index);
const char* gsDjinnByDescription(const char* text);
// The game's description of one Djinn (element 0..3 in the app order Venus, Mars, Jupiter, Mercury; index = bit in the owned mask), or "".
const char* gsDjinnDescription(int element, int index);
#endif
