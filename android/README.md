# Android app (debug APK)

The same shared core as the 3DS and desktop builds (`src/app.c`) on the SDL2 platform layer in `desktop/platform_sdl.c`, packaged by Gradle + CMake.
mGBA is compiled in from the `third_party/mgba` submodule with 16-bit colour, for arm64-v8a and armeabi-v7a.

## Build
Needs: JDK 17+, the Android SDK (platform 34, build-tools 34.0.0, NDK 26.3.11579264) and the SDL2 2.30.9 **source** (`SDL2-2.30.9.zip` from https://libsdl.org/release/, unzipped).
Put the repo in a path without spaces, then run `android\build_apk.bat` (it reads `JAVA_HOME`, `ANDROID_HOME`, `SDL2_DIR`; the defaults in the script are this project's folders).
Output: `android/app/build/outputs/apk/debug/app-debug.apk`.

## Start-up screen
The same themed pick screen as the 3DS app (banner with the logo, a card per game, and **Add ROM file**): tap a card, or use the D-pad and A / Start on a controller. A game without a ROM is dimmed; tapping it (or **Add ROM file**) opens the system file picker. Back leaves the app from this screen only.

## ROMs
You supply your own ROMs. At start-up the app shows a menu: **Play** (for each ROM it has) and **Add ROM file**, which opens Android's file picker. The chosen file's header is checked (Golden Sun = AGS*, The Lost Age = AGF*) and it is copied into the app's folder (`Android/data/com.exhilo.goldensundualscreen/files/` as `golden_sun.gba` / `lost_age.gba`); it never leaves the device. Copying the files there by hand works too.
Autosave: when the app leaves the foreground (home button, app switch, screen off) the autosave state, bestiary and map are written first, if Autosave is On.

## Screens
- Phone / tablet: one display. Portrait stacks the GBA picture over the companion UI with on-screen buttons below; landscape puts them side by side with buttons on both sides.
- Dual-screen handhelds (Retroid Pocket Duo and similar): the screen the app is started on is the touch screen with the companion UI (no on-screen buttons); the other display shows the GBA picture. To swap them, start the app on the other screen.
- **Game controllers** (USB / Bluetooth / built-in) work and map like the 3DS, by the button labels the device reports: A = A, B = B, shoulders = L / R, Start / Back = Start / Select, D-pad or left stick = move, Y or the right trigger = fast forward (the 3DS's Y / ZR). If a device reports A/B and X/Y the other way round, hold **Select and press ZL** once to flip them (remembered in `facebuttons.txt`).
- **Swap the screens** (dual-screen handhelds): press **ZL** (or the **W** key). Handhelds differ in which physical screen the app opens on; swapping puts the companion UI on the other display (touch works there) and the game on this one. The choice is remembered (`screenswap.txt` in the app folder).
- **Top screen: Fill** (Menu tab) stretches the 3:2 game picture to **16:9**: it fills a 16:9 display (up to 21:9) and is a 16:9 box across the width on taller screens; **1X** keeps the original proportions. The app runs in immersive full screen (no status or navigation bar).
- **Controls list:** Menu tab > **Controls** shows all the hotkeys above on screen (Android and desktop builds; the 3DS build does not have the entry).
- **System bars:** the status bar and the navigation bar / gesture pill are hidden on both displays and hidden again whenever Android shows them (a swipe from the edge still brings them back for a moment).
- **Focus:** the window on the second display never takes input focus, so touching it does not pull the controller away from the game.
- **Touch buttons**: a small `PAD` chip below the FF button cycles **Auto** (hidden while a controller is connected, shown again when it is unplugged), **On** and **Off**. The chip stays on screen, dimmed, when the buttons are hidden; the choice is saved in `padmode.txt` in the app folder. On a dual-screen handheld the buttons are always hidden.

Status: tested on one dual-screen handheld and on an Android 34 emulator (picker, layouts, touch, saving, dual-screen output). Other devices are unknown.
