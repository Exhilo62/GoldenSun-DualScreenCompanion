# Map data in the ROM (research notes, Golden Sun USA/Europe)

Goal: draw complete area maps from the player's own ROM at patch time (no art is shipped), instead of building the map while exploring.
Tools: `tools/gsmap.py` (file table + decompressor), `tools/gbadis.c` (Thumb/ARM disassembler built on mGBA's decoder; `build/gbadis.exe rom START LEN [arm]`).
Resource used: the Golden Sun Data Wiki "ROM Map" page (gsdata.fandom.com). Its addresses are for another ROM version; here they differ.

## Verified
- **Master File Table** (MFT): ROM offset `0x320000`, 975 u32 ROM pointers (`0x08xxxxxx`); file `n` = `ROM[(MFT[n] & 0x1FFFFFF)]`, its size is the gap to the next higher pointer. `get_mft_file(n)` is the Thumb function at `0x08002F40`.
- **Compression**: the first byte of a compressed blob is the format. Format 1 (flag bytes, MSB first; 12-bit distance, 4-bit length; end marker distance 0) and format 0 (LSB-first bit stream: literal = 1 + 8 bits; lengths 2..7 as 0 / 10 / 110 / 1110 / 11110x, long forms 11111 + 2 bits (8..10) or + 7 bits (+10, 0 = end); distance = 1 + 5 bits, or a variable width of bit_length(written-33) bits + 32 (12 bits once 2048+ bytes are written)). The decoder is at `0x08002544` (copied to RAM and run). `gsmap.decompress` reproduces the game's output byte for byte (checked against EWRAM after a map load). Format 2 (tile images) is a different routine, not decoded yet.
- **A map is a block of consecutive MFT files** (6 for map 7: files 326-331). The first is an uncompressed container: bytes 0-7 layer/scroll settings, `+0x0C` three 8-byte layer records, `+0x24 / 0x28 / 0x2C / 0x30 / 0x34 / 0x38` offsets (relative to the file) of six blobs. Blob 0 = a small byte map (decompressed, post-processed), blob 1 -> EWRAM `0x0202C000`, blob 2 (64 KB = 128x128 u32 per tile: the collision / event table) -> `0x02010000`, blob 3 -> `0x0202D000`, blob 4 -> `0x0202DE00`. The loader is at `0x0800FB60`. Files 327-331 (palette-like data and four format-2 images) are loaded by `0x0800F9F4` / `0x0800FAC8`.
- The current map id is the u16 at `0x02000400`; it is written by the event/warp handler at `0x0808A6B4`.

## How a map is drawn (all verified; `tools/gsmap.py` renders Vale pixel-for-pixel against the emulator)
1. **Map id -> files**: ROM `0x9F1AC`, 8 bytes per map id (0..204): `u16 graphicsSet, 0, mapCodeFile, flags`. `0x13784` has 12 bytes per graphics set: six u16 MFT offsets (+296): container, palette, three tile images (the 6th file is a pool of animated tiles, not needed). Several maps can share a set.
2. **Container** (first file, uncompressed): `[2]` width and `[3]` height in 8px tiles; `+12 + 8*k` layer records (k = 0..2): `[0]`,`[1]` = the layer's x / y origin in tiles (cell = tile / 2) inside the shared 128x128 cell table; `+0x24..0x38` six u16.. u32 offsets of compressed blobs.
3. **Blob 0 = block dictionary** (format 0 decompression): byte 0 = mode, then `N = (len-1)/2` u16 tile entries. Mode 1 stores them as a high-byte plane then a low-byte plane, and the entries are a running XOR (`entry[i] = raw[i] ^ entry[i-1]`). Each block (id) is 4 entries = 16x16 px: top-left, top-right, bottom-left, bottom-right. Entry bits: 0-9 tile number, 10 h-flip, 11 v-flip, 12-15 palette bank.
4. **Blob 2 = 64 KB cell table**: bytes 0..0x3FFF and 0x4000..0x7FFF are the low / high byte planes of each cell's high half; bytes 0x8000.. are 16384 u16 low halves, where a low 12 bits value of 0xFFF means "the next unused block id" (counter 0, 1, 2 ...). `cell & 0xFFF` = block id. The result is the 128x128 u32 table the game keeps at EWRAM `0x02010000` (identical to `gsmap.load_map()`'s `cells`).
5. **Tile images** (format 2 = format 0 with move-to-front nibble literals): three images of 16 KB at VRAM `0x4000`, `0x8000`, `0xC000`; tile number t of a layer with charblock c is at `c*0x4000 + t*32` (4 bits per pixel, low nibble first). Layer 0 (BG3) uses charblock 0 and layers 1 / 2 (BG2 / BG1) charblock 2 -- tile numbers >= 0x200 are what the maps use (the images). Palette = file 2 of the set, 224 BGR555 colours, 16 per bank, colour 0 transparent.
6. **Drawing**: layer 0 at the bottom, then 1, then 2 (BG priorities 3, 2, 1). Layer 2 (BG1) is alpha blended by the game (BLDCNT/BLDALPHA, 6/16 over 16/16) on some maps (it is a light-glow layer in houses), plain on others.
7. **Camera**: the game's BG scroll shadow is at IWRAM `0x03001AD0` (BG0 hofs, vofs, BG1 ..., 16 bytes); camera = BG1 = `0x03001AD4/6`. Table pixel of a layer's top-left screen pixel = camera + layer origin (in px) - 960 on x (checked for Vale and a house). For a map, layer-local pixel = (cam x - 960, cam y).
8. Probe additions: `vdump`, `iotrace` (changes of write-only I/O registers), `bp`, `trace`, `steps`, and `build/gbadis.exe` (Thumb/ARM disassembler).

## The Lost Age (BAGFE) -- same engine, different tables (verified against RAM in six maps, and in the app)
- File table at ROM `0x680000` (1723 entries), map table `0xF17AC` (8 bytes per id, 325 ids, id 0x0E.. etc.), graphics-set table `0x2F380` (12 bytes per set), file base **620** (the loader `0x0802A6B8` adds `0x26C` to each u16). A set row is `[container, palette, image x4]`; rows often share files (e.g. rows 5 and 6 share the images).
- Dictionary / cell table / palette / tile image formats and RAM addresses (`0x02020000`, `0x02010000`) are identical to Golden Sun. The camera is at `0x0300112C/2E`.
- **Layer-local coordinates** (both games): layer 2 (BG1) scrolls with the camera, so a layer's position is camera + (its origin - layer 2's origin); the layer-local pixel is `(camera - layer 2 origin * 8) mod (layer size in pixels)` (the mod matters in The Lost Age where layer 2's origin is below the map).
- Rooms: several maps share one container, so the app keeps only the connected group of rooms (occupied 16x16 cells grown by 1 cell) around the player.

## Status
Implemented in `src/mapdecode.c` (both games), used by `src/mapview.c` (Map tab) and tested by `tools/mapdecode_test.c` / `tools/selftest.py`
(all 205 / 324 of 325 map ids decode; Vale matches the Python reference pixel for pixel).

## Still to do
- The world maps (affine layers) still use the stitched map; the Lost Age world map id is not known yet.
- Layer 2 is skipped where the game alpha-blends it (a glow layer in Golden Sun houses); maps that blend it differently may look slightly different from the game.
- Maps with no data in the layers the app draws (some cutscene / battle-only ids) show nothing.
- Other regional releases (Japanese, German...) have different table addresses; they fall back to the stitched map.
