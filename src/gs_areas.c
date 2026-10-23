#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gs_areas.h"

typedef struct {
	int id;
	char name[28];
} AreaName;

// Identified by walking the game (see docs/RAM_MAP.md, "Map ids identified so far").
static const AreaName builtin[] = {
	{0x002, "World Map"},
	{0x003, "Vale"},
	{0x007, "Vale - House 1"},
	{0x008, "Vale - House 2"},
	{0x009, "Vale Sanctum"},
	{0x013, "Sol Sanctum Entrance"},
	{0x0A7, "Suhalla"},
	{0x0A8, "Suhalla - House"},
	{0x0B1, "Road to Babi Lighthouse"},
	{0x0B2, "Lalivero"},
	{0x0B3, "Lalivero Sanctum"},
	{0x0A9, "Suhalla Gate"},
	{0x0AA, "Suhalla Gate"},
	{0x0B0, "Babi's Lighthouse"},
	{0x0B4, "Lalivero - House"},
	{0x069, "Lunpa - House"},
	{0x0B5, "Venus Lighthouse Grounds"},
	{0x0B6, "Venus Lighthouse Hall"},
	{0x0BA, "Venus Lighthouse Upper"},
	{0x0BB, "Venus Lighthouse Tower"},
};

// The Lost Age (gen: tools/gen_ta_areas.py): map id -> location name, or the internal map label where the location is not certain.
static const AreaName builtinTla[] = {
#include "gs_areas_tla.inc"
};

#define MAX_EXTRA 64
static AreaName extra[MAX_EXTRA];
static int extraCount;
static bool useBuiltin = true;
static int areaGame;   // 0 Golden Sun, 1 The Lost Age

void gsAreasUseBuiltin(bool on) { useBuiltin = on; }
void gsAreasSelectGame(int game) { areaGame = game; }

const char* gsAreaName(int id) {
	for (int i = 0; i < extraCount; ++i) if (extra[i].id == id) return extra[i].name;
	if (areaGame == 1) {
		for (unsigned i = 0; i < sizeof(builtinTla) / sizeof(builtinTla[0]); ++i) if (builtinTla[i].id == id) return builtinTla[i].name;
	} else {
		for (unsigned i = 0; useBuiltin && i < sizeof(builtin) / sizeof(builtin[0]); ++i) if (builtin[i].id == id) return builtin[i].name;
	}
	static char unknown[32];
	if (useBuiltin && areaGame == 0 && id >= 0x0B5 && id < 0x0BC) snprintf(unknown, sizeof(unknown), "Venus Lighthouse (0x%03X)", id);
	else snprintf(unknown, sizeof(unknown), "Area 0x%03X", id);
	return unknown;
}

void gsAreasLoad(const char* path) {
	FILE* f = fopen(path, "r");
	if (!f) return;
	char line[96];
	while (fgets(line, sizeof(line), f) && extraCount < MAX_EXTRA) {
		char* p = line;
		while (*p == ' ' || *p == '\t') ++p;
		if (*p == '#' || *p == '\n' || *p == 0) continue;
		char* end;
		long id = strtol(p, &end, 0);
		if (end == p) continue;
		while (*end == ' ' || *end == '\t') ++end;
		size_t n = strcspn(end, "\r\n");
		if (!n) continue;
		AreaName* a = &extra[extraCount++];
		a->id = (int) id;
		snprintf(a->name, sizeof(a->name), "%.*s", (int) n, end);
	}
	fclose(f);
}
