// Desktop / Android counterpart of src/gs_game.c: the same game table and path rules, with the data folder chosen by the platform
// (GS_DATA environment variable, default "gsdata/") instead of the 3DS SD card. ROMs are golden_sun.gba / lost_age.gba in that folder,
// or whatever GS_ROM1 / GS_ROM2 point at.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#define MKDIR(p) _mkdir(p)
#else
#define MKDIR(p) mkdir(p, 0777)
#endif
#include "../src/gs_game.h"

static const GsGameInfo games[GS_GAME_COUNT] = {
	{"Golden Sun", "golden_sun.gba", "", "AGS", true},
	{"Golden Sun: The Lost Age", "lost_age.gba", "lost_age/", "AGF", true},
};

const GsGameInfo* gsGameInfo(GsGameId id) { return &games[id]; }

static const char* appDir(void) {
	static char dir[96];
	if (!dir[0]) {
		const char* e = getenv("GS_DATA");
		snprintf(dir, sizeof(dir), "%s", e && *e ? e : "gsdata");
		size_t n = strlen(dir);
		if (dir[n - 1] != '/' && dir[n - 1] != '\\' && n + 1 < sizeof(dir)) { dir[n] = '/'; dir[n + 1] = 0; }
	}
	return dir;
}

char* gsGamePath(GsGameId id, const char* file, char* out, size_t n) {
	snprintf(out, n, "%s%s%s", appDir(), games[id].dataDir, file ? file : "");
	return out;
}

void gsGameInitRomfs(void) {}

char* gsGameRomPath(GsGameId id, char* out, size_t n) {
	const char* e = getenv(id == GS_GAME_1 ? "GS_ROM1" : "GS_ROM2");
	if (e && *e) snprintf(out, n, "%s", e);
	else snprintf(out, n, "%s%s", appDir(), games[id].romFile);
	return out;
}

char* gsGameSavePath(GsGameId id, char* out, size_t n) {
	gsGameRomPath(id, out, n);
	char* dot = strrchr(out, '.');
	if (dot) snprintf(dot, n - (size_t) (dot - out), ".sav");
	return out;
}

char* gsAppFilePath(const char* file, char* out, size_t n) {
	snprintf(out, n, "%s%s", appDir(), file);
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
	char p[128];
	snprintf(p, sizeof(p), "%s", appDir());
	size_t n = strlen(p);
	if (n) p[n - 1] = 0;
	MKDIR(p);
	if (!games[id].dataDir[0]) return;
	gsGamePath(id, NULL, p, sizeof(p));
	n = strlen(p);
	if (n && (p[n - 1] == '/' || p[n - 1] == '\\')) p[n - 1] = 0;
	MKDIR(p);
}
