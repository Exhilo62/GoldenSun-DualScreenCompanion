// Bestiary: every enemy met in battle, with the stats the game shows for it and how many fights it appeared in. One file per game
// (<game dir>/bestiary.bin), so Golden Sun and The Lost Age keep separate lists.
#ifndef BESTIARY_H
#define BESTIARY_H
#include <stdbool.h>
#include <stdint.h>

#include "gs_state.h"

#define BEST_MAX 256

typedef struct {
	char name[16];
	uint8_t level;
	uint16_t maxHp, atk, def, agi;
	uint16_t seen;       // battles it appeared in
	bool counted;        // runtime only: already counted in the current battle
} BestEntry;

void bestReset(void);
bool bestLoad(const char* path);   // false if the file is missing or damaged (the list is then empty)
bool bestSave(const char* path);
// Call once per loop with the current state: records every enemy of a battle (once per battle) and notes when a battle ends.
void bestObserve(const GsState* st);
// True once after a battle in which something changed: the caller should save.
bool bestTakeSave(void);
bool bestDirty(void);
int bestCount(void);
const BestEntry* bestAt(int index);   // sorted by name
#endif
