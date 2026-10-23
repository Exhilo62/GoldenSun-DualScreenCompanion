#include "gs_field.h"

// From the menu itself (tools/psyicons.py captured exactly these in the field list) plus Restore.
bool gsAbilityFieldUsable(unsigned id) {
	return id == 12 || id == 33 || id == 78 || (id >= 90 && id <= 101) || (id >= 140 && id <= 149);
}

bool gsAbilityNeedsTarget(unsigned id) {
	return id >= 90 && id <= 101;   // Cure ... Revive
}
