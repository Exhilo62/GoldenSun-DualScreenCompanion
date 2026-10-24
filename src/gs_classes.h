#ifndef GS_CLASSES_H
#define GS_CLASSES_H
// Class (Lord, Champion, ...) for the class id at record +0x129; "" when unknown.
const char* gsClassName(unsigned id);
// The same for The Lost Age (game 2), whose class table is larger (Felix's / Jenna's / Sheba's / Piers' classes).
const char* gsClassNameFor(int game, unsigned id);
#endif
