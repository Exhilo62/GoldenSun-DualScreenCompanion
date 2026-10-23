#include <stdio.h>
#include <string.h>

#include "bestiary.h"

static BestEntry list[BEST_MAX];
static int count;
static bool dirty, saveWanted, wasBattle;

#define MAGIC 0x45425347u   // "GSBE"

void bestReset(void) {
	count = 0;
	dirty = saveWanted = wasBattle = false;
}

bool bestDirty(void) { return dirty; }
int bestCount(void) { return count; }
const BestEntry* bestAt(int i) { return i >= 0 && i < count ? &list[i] : NULL; }

bool bestTakeSave(void) {
	bool w = saveWanted;
	saveWanted = false;
	return w;
}

static void put16(unsigned char* p, unsigned v) { p[0] = (unsigned char) v; p[1] = (unsigned char) (v >> 8); }
static unsigned get16(const unsigned char* p) { return p[0] | (unsigned) p[1] << 8; }

// One record: 16 name bytes, level, pad, maxHp, atk, def, agi, seen (little endian, 28 bytes).
#define REC 28

bool bestSave(const char* path) {
	FILE* f = fopen(path, "wb");
	if (!f) return false;
	unsigned char head[8] = {MAGIC & 255, (MAGIC >> 8) & 255, (MAGIC >> 16) & 255, MAGIC >> 24, 0, 0, 0, 0};
	put16(head + 4, (unsigned) count);
	bool ok = fwrite(head, 1, 8, f) == 8;
	for (int i = 0; ok && i < count; ++i) {
		unsigned char r[REC] = {0};
		memcpy(r, list[i].name, 16);
		r[16] = list[i].level;
		put16(r + 18, list[i].maxHp); put16(r + 20, list[i].atk); put16(r + 22, list[i].def); put16(r + 24, list[i].agi); put16(r + 26, list[i].seen);
		ok = fwrite(r, 1, REC, f) == REC;
	}
	fclose(f);
	if (ok) dirty = false;
	return ok;
}

bool bestLoad(const char* path) {
	bestReset();
	FILE* f = fopen(path, "rb");
	if (!f) return false;
	unsigned char head[8];
	bool ok = fread(head, 1, 8, f) == 8 && head[0] == (MAGIC & 255) && head[1] == ((MAGIC >> 8) & 255) && head[2] == ((MAGIC >> 16) & 255) && head[3] == (MAGIC >> 24);
	unsigned n = ok ? get16(head + 4) : 0;
	if (n > BEST_MAX) ok = false;
	for (unsigned i = 0; ok && i < n; ++i) {
		unsigned char r[REC];
		if (fread(r, 1, REC, f) != REC) { ok = false; break; }
		BestEntry* e = &list[count];
		memcpy(e->name, r, 16);
		e->name[15] = 0;
		e->level = r[16];
		e->maxHp = (uint16_t) get16(r + 18); e->atk = (uint16_t) get16(r + 20); e->def = (uint16_t) get16(r + 22); e->agi = (uint16_t) get16(r + 24); e->seen = (uint16_t) get16(r + 26);
		e->counted = false;
		if (!e->name[0]) { ok = false; break; }
		++count;
	}
	fclose(f);
	if (!ok) count = 0;
	return ok;
}

static int findOrInsert(const char* name) {
	int lo = 0, hi = count;   // lower bound by name (case-sensitive; names are fixed ASCII)
	while (lo < hi) {
		int mid = (lo + hi) / 2;
		if (strcmp(list[mid].name, name) < 0) lo = mid + 1; else hi = mid;
	}
	if (lo < count && !strcmp(list[lo].name, name)) return lo;
	if (count >= BEST_MAX) return -1;
	memmove(&list[lo + 1], &list[lo], sizeof(BestEntry) * (size_t) (count - lo));
	memset(&list[lo], 0, sizeof(list[lo]));
	snprintf(list[lo].name, sizeof(list[lo].name), "%s", name);
	++count;
	return lo;
}

void bestObserve(const GsState* st) {
	bool battle = st->mode == GS_MODE_BATTLE && st->enemyCount > 0;
	if (battle && !wasBattle) for (int i = 0; i < count; ++i) list[i].counted = false;
	if (!battle && wasBattle && dirty) saveWanted = true;
	wasBattle = battle;
	if (!battle) return;
	for (int i = 0; i < st->enemyCount; ++i) {
		const GsMember* m = &st->enemies[i];
		if (!m->present || strlen(m->name) < 2) continue;
		int k = findOrInsert(m->name);
		if (k < 0) continue;
		BestEntry* e = &list[k];
		if (!e->counted) {
			e->counted = true;
			if (e->seen < 65535) ++e->seen;
			dirty = true;
		}
		if (e->level != m->level || e->maxHp != m->maxHp || e->atk != m->atk || e->def != m->def || e->agi != m->agi) {
			e->level = m->level; e->maxHp = m->maxHp; e->atk = m->atk; e->def = m->def; e->agi = m->agi;
			dirty = true;
		}
	}
}
