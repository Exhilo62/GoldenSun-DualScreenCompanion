# Android app (debug APK)

The same shared core as the 3DS and desktop builds (`src/app.c`) on the SDL2 platform layer in `desktop/platform_sdl.c`, packaged by Gradle + CMake.
mGBA is compiled in from the `third_party/mgba` submodule with 16-bit colour, for arm64-v8a and armeabi-v7a.

## Build
Needs: JDK 17+, the Android SDK (platform 34, build-tools 34.0.0, NDK 26.3.11579264) and the SDL2 2.30.9 **source** (`SDL2-2.30.9.zip` from https://libsdl.org/release/, unzipped).
Put the repo in a path without spaces, then run `android\build_apk.bat` (it reads `JAVA_HOME`, `ANDROID_HOME`, `SDL2_DIR`; the defaults in the script are this project's folders).
Output: `android/app/build/outputs/apk/debug/app-debug.apk`.

## ROMs
You supply your own ROMs. For now copy `golden_sun.gba` (and optionally `lost_age.gba`) into the app's folder on shared storage: `Android/data/com.exhilo.goldensundualscreen/files/`. A file picker is the next step.

## Screens
- Phone / tablet: one display. Portrait stacks the GBA picture over the companion UI with on-screen buttons below; landscape puts them side by side with buttons on both sides.
- Dual-screen handhelds (Retroid Pocket Duo and similar): the screen the app is started on is the touch screen with the companion UI (no on-screen buttons); the other display shows the GBA picture. To swap them, start the app on the other screen.
- Game controllers work (east / south button = A / B, shoulders = L / R, right trigger = fast forward).

Status: builds and packages; not yet run on a device or emulator.
