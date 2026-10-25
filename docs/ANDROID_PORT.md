# Android port: what it takes

Goal: an installable `.apk` built from the same source as the 3DS app, so one change reaches both. Survey done Oct 24 on the v0.1.0 code.

## What is already portable
About 9,000 of the 10,400 lines of C have nothing to do with the 3DS: the game-state reader (`gs_state.c`), the touch UI (`ui.c`, `gfx.c`, `art*.c`),
the map decoder (`mapdecode.c`, `mapview.c`), the tables (`gs_items.c` ...), the bestiary, dialogue log and menu navigation. They already build and run
on a PC (the host tools `ui_preview`, `stateprobe` and `selftest.py` use them). mGBA itself builds for Android and desktop.

## What is 3DS-specific (the whole surface)
Only four files include `<3ds.h>`: `main.c`, `launcher.c`, `bgwrite.c`, `gs_game.c`. The calls are:
| Area | libctru calls | Android replacement |
|---|---|---|
| Video | `gfxInit/GetFramebuffer/FlushBuffers/SwapBuffers` (two rotated 400x240 / 320x240 RGB565 framebuffers) | upload the same two buffers as textures (SDL2 + OpenGL ES), scaled into a stacked or side-by-side layout |
| Input | `hidScanInput/KeysHeld/TouchRead` | SDL touch (mapped to the bottom-screen rectangle), on-screen/Bluetooth gamepad, keyboard |
| Audio | `ndspInit` ... (about 10 calls) | SDL audio callback fed from mGBA's audio buffer (no `dspfirm` file, no "(no audio)" case) |
| Time / threads | `svcGetSystemTick`, `svcSleepThread`, `threadCreate`, LightLock / LightSemaphore | SDL / pthread equivalents (the 3DS two-core pipeline is not needed on a phone) |
| Files | `romfs:/` for ROMs, `sdmc:/3ds/GoldenSun3DS/` for saves | the app's private files folder; ROMs copied in with a file picker |
| App loop | `aptMainLoop`, `romfsInit` | the SDL main loop and Android lifecycle (pause saves, resume restores) |

## The real work: `main.c`
`main()` is one 670-line function that mixes the platform glue with the shared logic: touch actions (open menu, equip best, Djinn flips, Repeat round, macros),
save states, rewind, settings, frame pacing and the render job. To keep two builds maintainable, split it:
- `src/core/`  platform-neutral: everything above, plus the actions and the frame loop (new `app.c`) written against `platform.h`.
- `src/platform/3ds/`  the current libctru code, including the two-core pipeline, vblank prediction and `ndsp`. Behaviour must not change (re-check with `perflog.txt` on a console).
- `src/platform/sdl/`  Android and desktop (Windows/Linux) from one SDL2 layer.

`platform.h` (sketch): `plNowUs()`, `plSleepUs()`, `plPoll(PlInput*)` (GBA keys, touch point, quit), `plPresent(top, bottom)`, `plAudioPush(frames)`,
`plDataDir()`, `plRomPath(game)`, `plThreadStart()`, `plLock()`. The 3DS layer keeps `presentFrame()` and the worker; the SDL layer presents inline.

## Work packages (rough sessions)
1. **Desktop spike (1-2).** `platform/sdl` plus a small loop: GBA picture + the existing UI in a window, mouse as touch. Proves the core outside the 3DS and gives a Windows/Linux build for free. Needs SDL2 (MinGW development package).
2. **Split `main.c` (2-3).** Extract `app.c` + `actions.c` behind `platform.h`; port the 3DS side onto it; regression: `selftest.py` (109 checks) plus Azahar smoke runs plus a console check.
3. **Android shell (3-4).** Gradle project from SDL2's `android-project`, CMake for mGBA + core + SDL2 (arm64-v8a and armeabi-v7a), a Java file-picker (Storage Access Framework) that copies the user's ROMs into the app, immersive full screen, layouts (portrait stack, landscape side by side, dual-screen devices), lifecycle (autosave on pause), touch + gamepad mapping.
4. **Audio and timing (1-2).** Ring buffer, vsync pacing, real fast-forward (phones have the headroom), low-latency audio path.
5. **CI and releases (1).** GitHub Actions: devkitPro container for the 3DS build, Gradle for the APK, both attached to one release with one version number. Debug-signed APK first; a release keystore kept out of the repo.
6. **Polish.** Per-device layouts, haptics, controller remap, settings screen, adaptive icon.

Estimate: roughly 8-12 working sessions to a usable beta APK, after which maintenance is the shared core plus two thin layers.

## Keeping both versions in step
- One `core`, one version number (`src/version.h`), one changelog.
- `selftest.py` runs against the core on the host for every change; the platform layers stay small enough to review by eye.
- Every feature lands in the core first; a platform only adds input/output.
- Release checklist: build 3DS, build APK, run the host tests, smoke-test both.

## Needs and risks
- **Tooling to install** (none of it is on this PC yet): Android command-line tools, SDK platform 34 and build-tools, NDK r26 (about 1 GB), the Gradle wrapper (downloaded by the project), SDL2 source and the MinGW dev package. JDK 21 is already installed (Eclipse Adoptium) and works for current Android Gradle.
- **Testing:** an Android phone or tablet with USB debugging (or sideloading the APK). The Android Studio emulator is slow without hardware virtualisation.
- **ROM access:** Android 11+ blocks apps from reading other apps' folders, so ROMs go through the file picker and are copied once into private storage.
- **Licensing:** SDL2 (zlib) and mGBA (MPL-2.0) are fine for an APK; keep the source link and notices (`THIRD_PARTY_NOTICES.md`). Do not bundle ROMs or game art beyond what the 3DS build already ships. The APK is for sideloading from GitHub releases; an app-store listing of a ROM-based emulator is a separate legal and policy question.
- **Performance:** phones run mGBA far above full speed; the unknown is audio latency and vsync on varied devices.

## Suggested first step
Work package 1, the desktop spike: it needs only SDL2 and proves most of the design with no phone involved.

## Status (Oct 24): work package 1 done
The desktop spike runs: `desktop/main_sdl.c` is about 150 lines of SDL2 and shows the GBA picture plus the companion UI (tabs, party, Psynergy, Djinn, items, and the ROM-drawn maps with the player marker) in one window, with the mouse as the touch screen.
- The shared core compiled with Visual C++ **without any source changes**: a force-included header (`desktop/msvc_compat.h`) only swallows `__attribute__`. That confirms the core is portable; Android (clang, NDK) is the more forgiving case.
- mGBA builds with MSVC as a static library (`LIBMGBA_ONLY`, no zlib/png/ffmpeg); the result runs a real Golden Sun state at full speed.
- Pitfall found: the application must be compiled with the same `-D` flags as the mGBA library (`ENABLE_DEBUGGERS` and others change struct layouts); a mismatch crashed `core->init`. The Android CMake build must use one shared flag list.
- Not done: touch actions that press game buttons (they live in `main.c`, work package 2), audio, a ROM picker.
