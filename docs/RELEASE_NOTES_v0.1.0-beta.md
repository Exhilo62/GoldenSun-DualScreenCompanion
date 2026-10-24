## GoldenSun3DS v0.1.0 (beta)

First public test build. **We are looking for obvious problems**, so please try it and report anything that crashes, freezes, misbehaves or looks wrong.

### Install
1. Download **GoldenSun3DS_Patcher_v0.1.0-beta.zip** below and unzip it.
2. Run **Install.bat**, choose New 3DS or Old 3DS, choose `.cia` or SD files, pick your own Golden Sun / The Lost Age ROM(s) and press Build.
3. Install the `.cia` with FBI (or Azahar: File > Install CIA).

No ROMs are included. You need your own dumps.

### What to test
- Start-up: the pick screen (shows `v0.1.0 beta`), loading each game, autosave resume.
- Bottom-screen tabs in the field and in battle; the map filling in as you walk.
- Tap to equip (stat preview), Equip best, casting Psynergy, flipping Djinn, Djinn sets, Repeat round.
- Speed: is it smooth? Hold Y / ZR for fast forward. The **Menu** tab footer (`NNfps ... eMM ... cNN`) is the most useful thing to include in a report.

### Known limitations
- Old 3DS cannot reach full speed; fast forward cannot beat what the console can emulate.
- Equip preview / Equip best, Djinn sets and the live map are Golden Sun only; The Lost Age has a partial info UI.
- Repeat round, Djinn set Save / Apply / All Set and Equip best on real hardware are lightly tested. Android dual-screen devices are untested.

### Reporting
Open an issue with: what you did, what you expected, what happened, New or Old 3DS (or Azahar), real hardware or emulator, and the Menu footer line.

Golden Sun is (c) Nintendo / Camelot. Unofficial fan project. See `THIRD_PARTY_NOTICES.md`.
