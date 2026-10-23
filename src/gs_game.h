// Supported games: Golden Sun and Golden Sun: The Lost Age. The user supplies each ROM; nothing is bundled.
// Each game keeps its own save states, autosave, saved maps and area names so the two never mix.
#ifndef GS_GAME_H
#define GS_GAME_H
#include <stdbool.h>
#include <stddef.h>

typedef enum { GS_GAME_1, GS_GAME_2, GS_GAME_COUNT } GsGameId;

typedef struct {
	const char* name;        // shown in the UI
	const char* romFile;     // in the app folder on the SD card
	const char* dataDir;     // sub-folder for per-game files ("" = the app folder itself, which keeps old Golden Sun files valid)
	const char* codePrefix;  // first 3 characters of the cartridge game code in the ROM header (any region)
	bool mapped;             // true once the RAM map (src/gs_ram.h) has been verified for this game
} GsGameInfo;

const GsGameInfo* gsGameInfo(GsGameId id);
// Builds "sdmc:/3ds/GoldenSun3DS/<dataDir><file>" (file may be NULL for the directory itself). Returns out.
char* gsGamePath(GsGameId id, const char* file, char* out, size_t n);
// Full path of the ROM file for a game.
// Mounts the RomFS of an installed .cia (the patcher can embed the ROMs there); harmless when the app runs as a plain .3dsx.
void gsGameInitRomfs(void);
// Full path of the ROM file: romfs:/<file> when the .cia carries it, otherwise the SD card app folder.
char* gsGameRomPath(GsGameId id, char* out, size_t n);
// Cartridge save next to the ROM on the SD card (romfs is read-only): sdmc:/3ds/GoldenSun3DS/<rom name>.sav
char* gsGameSavePath(GsGameId id, char* out, size_t n);
// Path of a small settings file: romfs:/<file> if the .cia carries one, otherwise the app folder.
char* gsAppFilePath(const char* file, char* out, size_t n);
bool gsGameRomExists(GsGameId id);
// Reads the 4-char game code from the ROM header (offset 0xAC); false if unreadable.
bool gsGameRomCode(GsGameId id, char code[5]);
// True if the ROM in this game's slot carries this game's code (so a mis-named file is caught early).
bool gsGameRomMatches(GsGameId id);
// Creates the per-game data folder if needed.
void gsGameEnsureDir(GsGameId id);
#endif
