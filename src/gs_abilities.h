#ifndef GS_ABILITIES_H
#define GS_ABILITIES_H
// Psynergy / ability names by ability id (the low byte of an entry in a character's ability list).
const char* gsAbilityName(unsigned id);
// Finds the ability whose in-game description equals `text` (what the battle list shows for the highlighted entry); NULL when none.
const char* gsAbilityByDescription(const char* text, unsigned* idOut);
#endif
