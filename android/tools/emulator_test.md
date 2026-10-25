# Testing the APK on an emulator (no phone needed)

Install `emulator` and `system-images;android-34;google_apis;x86_64` with `sdkmanager`, create an AVD (`avdmanager create avd -n gs_phone -k "system-images;android-34;google_apis;x86_64" -d pixel_6`), start it with `emulator -avd gs_phone -no-window -no-audio -gpu swiftshader_indirect` (needs Windows Hypervisor Platform / HAXM / KVM), then:

    adb install -r android/app/build/outputs/apk/debug/app-debug.apk
    adb push your_rom.gba /sdcard/Download/        # on Windows Git Bash set MSYS_NO_PATHCONV=1
    adb shell am start -n com.exhilo.goldensundualscreen/.MainActivity
    adb exec-out screencap -p > shot.png

`adb shell input tap x y` works for the UI; the debug APK also contains an x86_64 library for this. Rotate with `adb shell settings put system user_rotation 1`.
