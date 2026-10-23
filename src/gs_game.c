#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include <3ds.h>
#include "gs_game.h"

#define APP_DIR "sdmc:/3ds/GoldenSun3DS/"

static const GsGameInfo games[GS_GAME_COUNT] = {
	{"Golden Sun", "golden_sun.gba", "", "AGS", true},
	// The Lost Age: party, items, Djinn(unchecked), coins, area id, menu flag and basic battle data are mapped; map position and
	// battle menu details are not (see gsRamTla in gs_state.c and docs/LOST_AGE.md).
	{"Golden Sun: The Lost Age", "lost_age.gba", "lost_age/", "AGF", true},
};

const GsGameInfo* gsGameInfo(GsGameId id) { return &games[id]; }

char* gsGamePath(GsGameId id, const char* file, char* out, size_t n) {
	snprintf(out, n, "%s%s%s", APP_DIR, games[id].dataDir, file ? file : "");
	return out;
}

static bool romfsMounted;

void gsGameInitRomfs(void) { romfsMounted = R_SUCCEEDED(romfsInit()); }

static bool fileExists(const char* p) {
	FILE* f = fopen(p, "rb");
	if (!f) return false;
	fclose(f);
	return true;
}

char* gsGameRomPath(GsGameId id, char* out, size_t n) {
	if (romfsMounted) {
		snprintf(out, n, "romfs:/%s", games[id].romFile);
		if (fileExists(out)) return out;
	}
	snprintf(out, n, "%s%s", APP_DIR, games[id].romFile);
	return out;
}

char* gsGameSavePath(GsGameId id, char* out, size_t n) {
	snprintf(out, n, "%s%s", APP_DIR, games[id].romFile);
	char* dot = strrchr(out, '.');
	if (dot) snprintf(dot, n - (size_t) (dot - out), ".sav");
	return out;
}

char* gsAppFilePath(const char* file, char* out, size_t n) {
	if (romfsMounted) {
		snprintf(out, n, "romfs:/%s", file);
		if (fileExists(out)) return out;
	}
	snprintf(out, n, "%s%s", APP_DIR, file);
	return out;
}

bool gsGameRomExists(GsGameId id) {
	char p[128];
	FILE* f = fopen(gsGameRomPath(id, p, sizeof(p)), "rb");
	if (!f) return false;
	fclose(f);
	return true;
}

bool gsGameRomCode(GsGameId id, char code[5]) {
	char p[128];
	FILE* f = fopen(gsGameRomPath(id, p, sizeof(p)), "rb");
	if (!f) return false;
	bool ok = fseek(f, 0xAC, SEEK_SET) == 0 && fread(code, 1, 4, f) == 4;
	fclose(f);
	code[4] = 0;
	return ok;
}

bool gsGameRomMatches(GsGameId id) {
	char code[5];
	return gsGameRomCode(id, code) && strncmp(code, games[id].codePrefix, 3) == 0;
}

void gsGameEnsureDir(GsGameId id) {
	mkdir("sdmc:/3ds", 0777);   // a .cia install has no app folder yet: settings, saves and states live here
	mkdir("sdmc:/3ds/GoldenSun3DS", 0777);
	if (!games[id].dataDir[0]) return;
	char p[128];
	gsGamePath(id, NULL, p, sizeof(p));
	size_t n = strlen(p);
	if (n && p[n - 1] == '/') p[n - 1] = 0;
	mkdir(p, 0777);
}
