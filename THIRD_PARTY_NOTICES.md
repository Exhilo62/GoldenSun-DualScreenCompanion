# Third-party notices

| Component | Use | License | Source |
|---|---|---|---|
| mGBA (commit `9660a668f2093cb4aba5983f52bebf8878dc5b8b`) | GBA emulation core, linked into the app | MPL-2.0 | https://github.com/mgba-emu/mgba (submodule `third_party/mgba`, unmodified apart from the build-time zlib `zconf.h` handling done by the 3DS CMake build) |
| libctru / devkitPro | 3DS homebrew library and toolchain | zlib / various (see devkitPro) | https://devkitpro.org |
| makerom | builds the `.cia` (shipped in `GoldenSun3DS_Patcher.zip`, not in this repository) | MIT | https://github.com/3DSGuy/Project_CTR |
| bannertool | builds the banner / icon (shipped in `GoldenSun3DS_Patcher.zip`, not in this repository) | MIT | https://github.com/Steveice10/bannertool |
| Ubuntu Font Family | bitmap fonts rasterised into `src/font_data.c` | Ubuntu Font Licence 1.0, (c) Canonical Ltd | https://design.ubuntu.com/font |

## mGBA (MPL-2.0)
mGBA's source is available at the repository above, at the commit listed. Binaries built from this project contain mGBA code under the Mozilla Public License 2.0; its full text is in `third_party/mgba/LICENSE` once the submodule is checked out, and at https://www.mozilla.org/MPL/2.0/. Changes to mGBA files, if any are ever made, will be published here.

## Game content
Golden Sun and Golden Sun: The Lost Age are (c) Nintendo / Camelot Software Planning. This project contains **no game code and no ROMs**; you supply your own. The following are derived from the games' artwork and are included for the user interface only:
- character portraits (`assets/reference/*_game.png`), item icons (`assets/reference/gs1_icons`, `tla_icons`), Psynergy icons (`gs1_psy`, `tla_psy`) and Djinn sprites, captured from the games' own menus and baked into `src/art_data.c` / `src/icon_data.c`;
- the title logo (`assets/reference/logo.png`) and app icon (`assets/reference/icon_source.png`), fan-made renditions of the official artwork.

This is an unofficial fan project. If a rights holder objects, these assets will be removed on request.
