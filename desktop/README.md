# Desktop build (spike for the Android port)

Runs the shared core (state reader, touch UI, ROM maps) outside the 3DS: the GBA picture on top and the companion UI below, in one SDL2 window.
Windows, built with Visual C++ (no changes to the shared sources; `msvc_compat.h` is force-included to swallow GCC-only syntax).

1. Install Visual Studio Build Tools (C++), CMake and put this repo in a path **without spaces**.
2. Download `SDL2-devel-2.30.9-VC.zip` from https://libsdl.org/release/ and unzip it to `desktop/deps/sdl2/` (so that `desktop/deps/sdl2/SDL2-2.30.9/include` exists).
3. `desktop\build_mgba.bat` builds the mGBA library (needs the `third_party/mgba` submodule), then `desktop\build_desktop.bat` builds `desktop\build\app\gs_desktop.exe`.
4. `gs_desktop.exe path\to\your\rom.gba [save.sav]`. Keys: arrows, Z = A, X = B, A = L, S = R, Enter = Start, Backspace = Select. The mouse is the touch screen.

Test helpers (environment variables): `GS_STATE=file.state` loads a save state, `GS_TAB=n` opens a tab, `GS_SHOT=out.bmp GS_FRAMES=n` saves a picture of the window and exits (no window is shown), `GS_TRACE=1` prints start-up steps.

Not done yet: touch actions that press game buttons (open menu, equip, cast, Djinn flips, ...) live in `src/main.c` and move into the shared core in the next work package; audio.
