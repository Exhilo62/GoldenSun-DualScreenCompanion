# Building GoldenSun3DS

Build from a path **without spaces** (the Makefile and devkitPro tools do not like them).

## Tools
- [devkitPro](https://devkitpro.org/wiki/Getting_Started) with the `3ds-dev` group (devkitARM, libctru, 3dsxtool, smdhtool). The Makefile assumes `C:/devkitPro` under MSYS2 / Git Bash; override with `DEVKITPRO=...`.
- CMake and make (for the mGBA core), Python 3, PowerShell 5+ (Windows) for the packaging scripts.

## 1. Get the sources
```
git clone --recurse-submodules <this repo> GoldenSun3DS
cd GoldenSun3DS
```
`third_party/mgba` is a submodule pinned to the commit listed in `THIRD_PARTY_NOTICES.md`.

## 2. Build the mGBA core for the 3DS (once)
```
mkdir -p build/mgba-3ds && cd build/mgba-3ds
cmake ../../third_party/mgba   -DCMAKE_TOOLCHAIN_FILE=$PWD/../../third_party/mgba/src/platform/3ds/CMakeToolchain.txt   -DCMAKE_BUILD_TYPE=Release -DBUILD_STATIC=ON -DBUILD_LTO=ON   -DM_CORE_GB=OFF -DUSE_DEBUGGERS=OFF -DUSE_PNG=OFF -DUSE_LIBZIP=OFF -DUSE_SQLITE3=OFF -DUSE_FFMPEG=OFF   -DUSE_EDITLINE=OFF -DUSE_EPOXY=OFF -DBUILD_QT=OFF -DBUILD_SDL=OFF   -DCMAKE_POLICY_VERSION_MINIMUM=3.5
make mgba zlibstatic
```
These are the options the maintainer's build directory was configured with (taken from its `CMakeCache.txt`); a from-scratch clone build has not been repeated yet, so if CMake complains about a missing option, please open an issue.
The app links `build/mgba-3ds/libmgba.a` and `build/mgba-3ds/zlib/libzlibstatic.a`.

## 3. Build the app
```
make        # -> build/app/GoldenSun3DS.3dsx (+ .smdh icon, .elf)
```

## 4. Package a release
```
powershell -ExecutionPolicy Bypass -File tools/make_dist.ps1
```
Needs `makerom.exe` and `bannertool.exe` in `tools/patcher/bin/` (not stored in git; get them from [Project_CTR](https://github.com/3DSGuy/Project_CTR/releases) and [bannertool](https://github.com/Steveice10/bannertool/releases)). Output: `dist/GoldenSun3DS-Setup/` and `dist/releases/v<version>-<stage>/GoldenSun3DS_Patcher_v<version>-<stage>.zip`. The version lives in `src/version.h`.

## Artwork scripts
`tools/genlogo.ps1` (launcher logo + HOME banner from `assets/reference/logo.png`), `tools/genicon.ps1` (icon from `assets/reference/icon_source.png`), `tools/genart.ps1` (tab icons, portraits), `tools/genicons.py` (item/Djinn/Psynergy icons), `tools/genfont.ps1` (needs `UBUNTU_FONT_DIR`).

## Host-side tests (maintainers)
`tools/selftest.py` builds the host tools with gcc against a host build of mGBA (`build/mgba-host`, same CMake options with the host toolchain) and checks them against saved emulator states from a Golden Sun run (`build/*.state`). Those states and the ROMs are not distributed, so the script only runs on a machine that has them. `tools/probe.c` (a headless emulator driven from `tools/gs.py`) is the research tool used to find the RAM layout documented in `docs/RAM_MAP.md`.

## Testing without a console
[Azahar](https://azahar-emu.org) runs the `.3dsx`; put your ROM at `sdmc:/3ds/GoldenSun3DS/golden_sun.gba` on its virtual SD. `tools/azahar_smoke.ps1` is a guarded GUI driver (it refuses to click unless Azahar is the foreground window). Azahar at its default 100% CPU clock behaves like an Old 3DS.
