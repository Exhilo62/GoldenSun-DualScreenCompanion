# Playing GoldenSun3DS in an emulator (Azahar)

The app runs in [Azahar](https://azahar-emu.org) (and other Citra-derived emulators) on Windows, Linux and Android. It needs the right emulator settings
to run at full speed with sound, and the speed you get depends on your hardware.

> **Important - Old 3DS and slow machines.** The app is built for a **New 3DS** (about 60 fps). On an **Old 3DS**, or in an emulator set to behave like one,
> it runs slower (roughly 25-35 fps), and **the automatic helpers may not be able to fix that**: the app skips drawing some frames when it cannot keep up
> (automatic frame skip) and silences the sound when the speed drops (**Mute if slow**), but if the emulated processor is simply too slow the game still
> plays in slow motion, and the sound is slow and choppy. That is a limit of the (emulated) hardware, not something a setting inside the app can remove.
> Fast forward is not offered for the same reason.

## Steps

### 1. Build the game file with the patcher
1. Run `GoldenSun3DS-Patcher.exe` (or `Install.bat`) from the download.
2. Step 1: choose **New 3DS** (even if you only use an emulator: this is the profile that targets 60 fps and uses two cores).
3. Step 2: choose **.cia**, then tick **"Also make a .3ds file for emulators"**. Pick your own ROM(s) and press **Build**.
4. You now have `GoldenSun3DS.cia` and `GoldenSun3DS.3ds` next to each other. Use the `.3ds` for the emulator (the `.cia` is for a real console).
   Your ROM(s) are inside the file, so keep it for yourself and do not share it.

### 2. Set up the emulator (these are the changes that were needed)
In Azahar, open the settings (PC: **Emulation > Configure...**, Android: the settings screen of the app; names vary a little between versions):
1. **System**: turn on **New 3DS mode** (the emulator then behaves like a New 3DS with the faster processor and the second core).
2. **System > CPU clock speed**: raise it to about **300%** (400% if the game still runs slowly). The default 100% emulates an Old 3DS and is too slow: you
   will see about 40 FPS in the Menu tab and slow, choppy sound. 300% corresponds roughly to a real New 3DS.
3. **Audio**: leave the emulation on **HLE** (the default). Nothing else is needed.
4. Open the `.3ds` (PC: **File > Open**; Android: add it to the game list).

### 3. Sound
The 3DS audio library the app uses insists on a file called `dspfirm.cdc`, which normally has to be dumped from a real console. In an emulator the app
solves this itself: if the file is missing it writes a placeholder (all zeros, no Nintendo data) into the emulator's virtual SD card (`sdmc/3ds/dspfirm.cdc`)
and starts the audio. Azahar's built-in audio never reads the contents, so **you do not need a 3DS to get sound**. (On a real console this is never done.)
- The Menu tab footer shows the speed, for example `60 FPS`. It adds **`(no audio)`** if the audio could not start and **`(muted: slow)`** if **Mute if slow**
  is silencing the sound because the game runs below about 54 fps. If you see `(muted: slow)`, raise the CPU clock (step 2) or accept the silence.
- If you turn **Mute if slow** off while the game runs slowly, you will hear the slow, choppy sound.

### 4. Where things are
- Saves, settings, save states and the map are written to the emulator's virtual SD card, folder `sdmc/3ds/GoldenSun3DS/` (PC: Azahar's user directory,
  for example `%APPDATA%/Azahar/sdmc/3ds/GoldenSun3DS/`).
- To use a save from another emulator or console, add the `.sav` in the patcher (steps 6 and 7) and copy the `.sav` it writes next to the output into that folder.

## Troubleshooting
| What you see | Cause and fix |
|---|---|
| Game runs in slow motion, footer shows 35-45 FPS | CPU clock still at 100% or New 3DS mode off: see step 2. A slow PC or phone may not reach 60 fps at all. |
| Sound slow and choppy | The game is below 60 fps: see above. |
| `(muted: slow)` in the footer | Same cause; the app muted the sound on purpose (**Mute if slow**). |
| `(no audio)` in the footer | The audio library could not start (very old emulator, or audio disabled in the emulator). Check Azahar's audio settings. |
| Black screen / crash at start on a real 3DS from a `.cia` | See the main README (known limitations) and send the crash screen. |

Android devices have not been tested by the maintainer; the steps above follow what worked in Azahar for Windows.
