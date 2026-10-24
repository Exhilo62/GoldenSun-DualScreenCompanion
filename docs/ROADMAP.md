# Roadmap

Golden Sun - Dual Screen Companion is built for the **New 3DS / New 3DS XL**. That is the only console it is developed and tested on.

## Other devices: try it today
Support for other devices is being worked on (see below). Until then you are welcome to try the existing `.3ds` / `.cia` on anything else that can run them:
- **Old 3DS / 2DS:** runs, but slower (roughly 25-35 fps).
- **PC, Android, Linux (Azahar and similar emulators):** use the `.3ds` the patcher can make; see [EMULATORS.md](EMULATORS.md) for the settings that matter.

Results outside the New 3DS XL are untested, so reports (what device, what speed, what broke) are very welcome.

## In progress / planned
1. **Shared core, thin platform layer.** Separate the game logic and companion UI (RAM reading, party, Djinn, map, bestiary, touch UI) from the 3DS-specific parts (screens, audio, input, threads, SD card). The 3DS build keeps working throughout.
2. **Desktop build (Windows / Linux).** The first non-3DS target, because it is quick to test. Native speed, native audio, no 3DS firmware files.
3. **Android app (.apk).** Runs the same mGBA core natively with Android audio and touch, the game on top and the companion UI below (portrait) or side by side (landscape). You pick your own ROM with a file picker.
4. **Features that need more speed** (such as real fast forward) return on the faster targets.

No dates yet. Progress will be listed in the [changelog](../CHANGELOG.md).
