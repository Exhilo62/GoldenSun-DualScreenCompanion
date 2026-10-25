# Desktop build (Windows; the base for the Android app)

Runs the same application as the 3DS build (`src/app.c`: touch actions, save states, rewind, ROM maps, battle bar) on an SDL2 platform layer: the GBA picture on top and the companion UI below, in one window, with sound.
Built with Visual C++; the shared sources are unchanged (`msvc_compat.h` is force-included to swallow GCC-only syntax and supply two builtins).

1. Install Visual Studio Build Tools (C++), CMake and put this repo in a path **without spaces**.
2. Download `SDL2-devel-2.30.9-VC.zip` from https://libsdl.org/release/ and unzip it to `desktop/deps/sdl2/` (so that `desktop/deps/sdl2/SDL2-2.30.9/include` exists).
3. `desktop\build_mgba.bat` builds the mGBA library (needs the `third_party/mgba` submodule; 16-bit colour like the 3DS build), then `desktop\build_desktop.bat` builds `desktop\build\app\gs_desktop.exe`.
4. Put your ROMs in a folder named `gsdata` next to where you run the exe: `golden_sun.gba` and optionally `lost_age.gba` (or set `GS_ROM1` / `GS_ROM2`; `GS_DATA` moves the folder, `GS_GAME=1|2` picks the game). Saves, states and maps go in the same folder.

Keys: arrows, Z = A, X = B, A = L, S = R, Enter = Start, Backspace = Select, hold Tab = fast forward. The mouse is the touch screen.

Files: `platform_sdl.c` implements `src/platform.h`; `gs_game_desktop.c` and `bgwrite_sync.c` replace the 3DS-only `gs_game.c` and `bgwrite.c`.

Test helpers (environment variables): `GS_SHOT=out.bmp GS_FRAMES=n` saves a picture of the window and exits (no window is shown); `GS_TOUCH="x,y,firstLoop,loops;..."` presses the touch screen (320x240 coordinates) without a mouse; `runshot.ps1` wraps these.
