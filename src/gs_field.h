// Field Psynergy (the abilities the game's own Psynergy menu lists outside battle) and which of them ask for a target.
#ifndef GS_FIELD_H
#define GS_FIELD_H
#include <stdbool.h>
bool gsAbilityFieldUsable(unsigned id);   // appears in the field Psynergy menu (Golden Sun ids, same in The Lost Age)
bool gsAbilityNeedsTarget(unsigned id);   // healing Psynergy: the game asks "on whom?"
#endif
