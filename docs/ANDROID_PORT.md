# Android port: what it takes

Goal: an installable `.apk` built from the same source as the 3DS app, so one change reaches both. Survey done Oct 24 on the v0.1.0 code.

## What is already portable
About 9,000 of the 10,400 lines of C have nothing to do with the 3DS: the game-state reader (`gs_state.c`), the touch UI (`ui.c`, `gfx.c`, `art*.c`),
the map decoder (`mapdecode.c`, `mapview.c`), the tables (`gs_items.c` ...), the bestiary, dialogue log and menu navigation. They already build and run
on a PC (the host tools `ui_preview`, `stateprobe` and `selftest.py` use them). mGBA itself builds for Android and desktop.

## What is 3DS-specific (the whole surface)
Only four files include `<3ds.h>`: `app.c`, `platform_3ds.c`, `launcher.c`, `bgwrite.c`, `gs_game.c`. The calls are:
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
2. **Split `main.c` (2-3).** Extract `app.c` + `actions.c` behind `platform.h`; port the 3DS side onto it; regression: `selftest.py` (110 checks) plus Azahar smoke runs plus a console check.
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
- Not done: touch actions that press game buttons (they live in `app.c` now; the desktop loop still has to be moved onto `appRun()`), audio, a ROM picker.

## Status (Oct 24): work package 2 done
`main.c` is split in three (see `src/`):
- `app.c` / `app.h`: the shared core: touch actions, save states, rewind, settings, the frame loop (`appRun`) and the per-frame job (`appProcessJob`: map stitching, UI drawing).
- `platform.h`: the contract a platform implements (input, timing, audio, jobs/threads, picker, fatal errors) plus tick constants.
- `platform_3ds.c`: everything libctru: ndsp audio, the top-screen blit, the worker thread on core 2, vblank pacing, the game picker. `main()` is just `plInit()` then `appRun()`.
The 3DS build is unchanged in behaviour: `selftest.py` (110 checks) passes and Azahar smoke runs (ROM map, Lost Age cast flow, menu at 60 FPS) match before the split. Not re-run: a console perf check.
Next: `platform_sdl.c` implementing `platform.h` (mGBA built with `COLOR_16_BIT` + `COLOR_5_6_5` like the 3DS), so the desktop build runs `appRun()` and the touch actions work.

## Status (Oct 24): desktop build on the shared core
`desktop/platform_sdl.c` implements `platform.h` (SDL2 window, keyboard / mouse-as-touch, audio queue, inline jobs, pacing to 59.73 Hz) and the desktop build now runs `appRun()`, replacing the old spike loop. Two small desktop replacements stand in for the 3DS-only files: `gs_game_desktop.c` (paths under `gsdata/`, ROMs from there or `GS_ROM1/2`) and `bgwrite_sync.c` (writes on the calling thread). mGBA is built with `COLOR_16_BIT` + `COLOR_5_6_5`, the same RGB565 video as the 3DS.
Checked on Windows with real ROMs: Golden Sun autosave loads in Vale and the Map tab draws the ROM map with the player marker; Lost Age Psynergy tap opens "Use Cure on whom?" through the game's own menus (the touch-action state machine works off the 3DS).
Next: Android shell (work package 3). That needs the Android SDK + NDK (several GB) and a Gradle project built around the same `platform_sdl.c`.

## Status (Oct 24): work package 3 started, debug APK builds
`android/` holds the Gradle + CMake project (see `android/README.md`): it compiles mGBA, the shared core and `desktop/platform_sdl.c` into `libmain.so` for arm64-v8a and armeabi-v7a, with SDL2's Java glue; `android\build_apk.bat` produces `app-debug.apk` (about 3.5 MB). The SDL layer lays itself out for the window (portrait stack with on-screen buttons below, landscape side by side), reads several fingers at once, and supports game controllers.
**Dual-screen handhelds** (Retroid Pocket Duo and similar) are a requirement: `MainActivity` finds the other display, shows a `Presentation` on it and passes its Surface to native code, which draws the GBA picture there; the display the app runs on becomes the touch screen alone (no on-screen buttons). Swapping the screens means starting the app on the other display; an in-app swap option is still to do.
Not done / untested: nothing has run on a device or emulator yet (no emulator installed); ROM file picker (ROMs go in `Android/data/com.exhilo.goldensundualscreen/files/` for now); launcher icon (SDL's placeholder); autosave on pause; release signing; CI.

## Status (Oct 24): ROM picker and save on background
Start-up menu (Play each ROM present / Add ROM file); the picker is the system Storage Access Framework (`ACTION_OPEN_DOCUMENT`), the file is validated by its header code and copied to the app folder by `MainActivity.copyRom`. `appSaveNow()` (new, in `app.c`) writes the autosave, bestiary and map at once; the SDL layer calls it from an event watcher when Android reports `SDL_APP_WILLENTERBACKGROUND` (SDL raises that on the emulation thread from inside the event poll, i.e. between frames). 3DS build and selftest (110) unaffected. Still untested on hardware.

## Status (Oct 24): tested on an Android 34 x86_64 emulator
Run on a Pixel 6 AVD (see `android/tools/emulator_test.md`): the start-up menu appears; **Add ROM file** opens the system picker, a non-ROM file is rejected with a message, a real Golden Sun ROM is accepted and copied to the app folder, and **Play Golden Sun** starts the game with the companion UI; portrait and landscape layouts and the on-screen buttons draw correctly; a held touch and an instant tap both reach the UI (very short taps are latched for four frames); pressing Home writes `auto.ss` (checked in the app folder). The APK now also carries an x86_64 library for emulators.
Not tested: the dual-screen path (the emulator had one display), sound, frame rate on real hardware, Lost Age, a game controller, any physical device.

## Status (Oct 24): dual-screen path tested on a simulated second display
On the emulator, `settings put global overlay_display_devices "1280x720/240"` adds a second display (id 2, flagged as a presentation). With the app running on the main display: the GBA picture appears on the second display (drawn by `drawTopSurface` onto the Presentation's Surface, letterboxed), the main display shows the companion UI alone, as large as it fits, with no on-screen buttons, and touching it works (Menu tab opened). Removing the second display (`settings delete global overlay_display_devices`) puts the single-screen layout and buttons back without restarting.
Still unverified on a real dual-screen handheld: which physical screen the app starts on, the second display's real resolution / refresh, touch on the second screen, and the frame-rate cost of the software blit to the Surface. If the app starts on the wrong screen, launch it on the other one (an in-app swap option is still to do).

## Status (Oct 24): controller mapping and touch-button visibility
Controller mapping follows the 3DS (see `android/README.md`); `SDL_HINT_GAMECONTROLLER_USE_BUTTON_LABELS` is off so east is A on every pad. The on-screen buttons have a three-way `PAD` chip (Auto / On / Off, persisted in `padmode.txt`); Auto hides them while `SDL_GameController` devices are attached. Checked on the desktop build and the emulator: the chip cycles and the buttons hide / show, the setting is written to the app folder. Not checked: an actual controller (the emulator has none), so the Auto hide / re-show on connect and the button mapping are untested on hardware.

## Status (Oct 24): signed release APK, icon, version, CI
- **Version:** one source, `src/version.h` (`APP_VERSION` / `APP_STAGE`); `android/app/build.gradle` reads it (versionName `0.1.0-beta`, versionCode `0.1.0` -> 100).
- **Launcher icon:** `tools/genandroidicon.ps1` makes the `mipmap-*` icons from the same art as the 3DS icon.
- **Signing:** `build_apk.bat release` builds `app-release.apk` signed with the key named in `android/keystore.properties` (not in git) or the `ANDROID_KEYSTORE_*` environment variables (CI). The key for this project lives outside the repo (`D:\android-sdk\keys\`); **back it up and keep it**: an APK signed with a different key cannot update an installed one, users would have to uninstall first. Verified with `apksigner verify` (v1 + v2 signatures) and installed and run on the emulator.
- **CI:** `.github/workflows/android.yml` (builds the APK on every push, signed when the repository secrets `ANDROID_KEYSTORE_B64`, `ANDROID_KEYSTORE_PASSWORD`, `ANDROID_KEY_ALIAS`, `ANDROID_KEY_PASSWORD` exist, attaches it to a release on `v*` tags) and `.github/workflows/3ds.yml` (devkitARM container, follows docs/BUILDING.md). **Neither workflow has been run yet** (nothing was pushed from here), so expect to fix small things on their first run.

## Status (Oct 25): screen swap and label-based controller buttons
- **Swap screens (ZL / W):** handhelds differ in which display the app opens on. Swapped, this window shows the GBA picture alone and the other display (Presentation Surface) shows the companion UI, drawn by `drawSurface()`; touches on it are forwarded by `MainActivity` (`nativeSurfaceTouch`, mapped through the same letterbox) and used as the touch screen. Remembered in `screenswap.txt`. Checked on the emulator's simulated second display: key W swaps, the UI appears on the second display, a tap there opens the Menu tab, and the swap persists across a restart.
- **Controller buttons:** a user report (A/B and X/Y swapped) showed the positional mapping was wrong on their handheld, which reports by label. Now A -> A, B -> B, Y -> fast forward; Select + ZL flips them for devices that report by position (`facebuttons.txt`). Not tested with a physical controller from here.

## Status (Oct 25, later): focus, Fill, full screen, themed pick screen
- **Focus bug:** touching the second display made its window (the Presentation) take input focus, and Android then routed controller buttons to that window instead of the game. The Presentation window now has `FLAG_NOT_FOCUSABLE` (touches still arrive); checked with `dumpsys window`: the game window keeps focus after a touch on the second display. Back is trapped (`SDL_HINT_ANDROID_TRAP_BACK_BUTTON`) so it cannot close the app mid-game.
- **Fill:** the Menu tab's "Top screen" setting is now honoured on Android / desktop: Fill = the picture stretched to 16:9 (`fillBox()`: whole display for 16:9 up to 21:9, a 16:9 or 21:9 box otherwise); 1X = original proportions. Single-display landscape layouts still fit the picture.
- **Full screen:** the window is created `SDL_WINDOW_FULLSCREEN` on Android (immersive); the system shows its one-time "Viewing full screen" hint.
- **Pick screen:** `src/pickscreen.c` draws the 3DS launcher's look (banner, cards, "Add ROM file") into two 320x240 canvases; `pickerLoop()` in `platform_sdl.c` shows them like the game (banner top, cards bottom; on dual-screen devices the cards are the touch screen, and ZL swaps them like in the game). It replaces the plain dialog; on desktop it appears with `GS_PICKER=1`. Checked on the emulator (cards on the second display, game started by a touch there) and in desktop screenshots.

## Status (Oct 25, later still): Controls list, system bars hidden
- **Menu > Controls:** `ui.c` has a new Menu-tab button and a `hotkeysView()` panel listing the hotkeys. It is shown only when the platform sets `uiShowHotkeys` (`platform_sdl.c` does; the 3DS build does not, so its Menu tab is unchanged; selftest 110/110). Checked in a desktop screenshot.
- **System bars:** a user photo showed the gesture pill over the picture. `MainActivity.hideSystemBars()` hides the status and navigation bars through the window insets controller (API 30+) or the immersive flags (older), covers the display cutout, and re-applies on every focus change and whenever the system reports the bars visible (after a short delay); the second-display Presentation window gets the same call after it is shown. On the emulator nothing regressed and no bar was visible; whether it removes the pill on the handheld still has to be confirmed there.

## Status (Oct 25, later): way back to the Battle panel (all platforms)
Shared UI change (`ui.c`), so it is on the 3DS build too: while a battle is on and another tab (or an overlay such as the log) is showing, the footer's left panel is highlighted with "Tap here: back to the battle" and a tap on it sets the tab back to the Battle panel. Checked in desktop screenshots from a battle state (tab tap shows the Party tab and the hint; footer tap returns); 3DS selftest 110/110. Not run on the console or in Azahar yet.

## Status (Oct 25, v0.1.1 polish): launcher icon, pick-screen Fill
- **Icon:** `tools/genandroidicon.ps1` now draws the artwork on the same navy gradient tile as the 3DS HOME icon (`tools/genicon.ps1`): legacy PNGs (Android 7 and older) plus an adaptive icon (Android 8+: navy gradient `ic_launcher_background.xml`, artwork in the safe zone).
- **Pick screen Fill:** the banner reads the saved "Top screen" setting (first byte of `settings.bin`, Fill when absent) and is stretched with `fillBox()` like the game picture. Checked on the emulator in both screen arrangements (ZL / W swap).

## Status (Oct 25, later): sound crackle fix
User report: slight crackling in both games. Cause: the first version queued the core's samples straight into `SDL_QueueAudio` against the wall-clock pacing; the two clocks drift, so every few seconds the queue either ran dry or hit the 120 ms cap (samples were dropped), and the core's rate (read from `audioRateChanged`, 65536 Hz in Golden Sun) was ignored. Now: `mAudioResampler` (sinc) converts to the device rate into a ring, the SDL callback drains it, and `plPace()` waits while more than 50 ms is queued (audio-clock pacing), falling back to the wall clock while muted. Desktop measurement over 15 s: 1 underrun in 1503 callbacks (the first, before any data), loop rate 59.86 per second. On the emulator the app opens an AAudio player and the system reports no mixer underruns. By ear on a real device: not tested from here.

## Status (Oct 26): Android polish
- **Touch-button options:** `uiPadMode / uiPadSize / uiPadOpacity / uiPadHaptic` (ui.h) edited in a new Menu panel that shares the slot of "Swap screens" (one display vs two); saved in `padlook.txt` (the 0.1.1 `padmode.txt` is still read). Size scales the button unit (0.122 / 0.15 / 0.178 x the free height in portrait; the largest value is the most that keeps the D-pad clear of L and Start); opacity 95 / 150 / 215; vibration = `MainActivity.haptic()` (window haptic feedback, no permission) on each newly pressed button.
- **Layouts:** landscape button columns are 1/8 of the width (was 1/6); with the buttons hidden landscape uses 98.5% of the width and portrait 93% of the height, and the PAD chip moves to a free strip at the bottom.
- **ROM management:** the pick screen acts on lift, and a hold of about 0.7 s on an installed card asks to remove its ROM (system confirmation; saves kept). Checked on the emulator: the dialog appears, Cancel keeps the ROM and does not start the game.
- **CI:** `tools/android_ci_secrets.ps1` prepares the four signing secrets from the local keystore (written to a gitignored file, never printed). The workflow itself was already in place; signed builds start once the secrets exist (not done from here).
