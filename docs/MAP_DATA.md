# Map data in the ROM (research notes, Golden Sun USA/Europe)

Goal: draw complete area maps from the player's own ROM at patch time (no art is shipped), instead of building the map while exploring.
Tools: `tools/gsmap.py` (file table + decompressor), `tools/gbadis.c` (Thumb/ARM disassembler built on mGBA's decoder; `build/gbadis.exe rom START LEN [arm]`).
Resource used: the Golden Sun Data Wiki "ROM Map" page (gsdata.fandom.com). Its addresses are for another ROM version; here they differ.

## Verified
- **Master File Table** (MFT): ROM offset `0x320000`, 975 u32 ROM pointers (`0x08xxxxxx`); file `n` = `ROM[(MFT[n] & 0x1FFFFFF)]`, its size is the gap to the next higher pointer. `get_mft_file(n)` is the Thumb function at `0x08002F40`.
- **Compression**: the first byte of a compressed blob is the format. Format 1 (flag bytes, MSB first; 12-bit distance, 4-bit length; end marker distance 0) and format 0 (LSB-first bit stream: literal = 1 + 8 bits; lengths 2..7 as 0 / 10 / 110 / 1110 / 11110x, long forms 11111 + 2 bits (8..10) or + 7 bits (+10, 0 = end); distance = 1 + 5 bits, or a variable width of bit_length(written-33) bits + 32 (12 bits once 2048+ bytes are written)). The decoder is at `0x08002544` (copied to RAM and run). `gsmap.decompress` reproduces the game's output byte for byte (checked against EWRAM after a map load). Format 2 (tile images) is a different routine, not decoded yet.
- **A map is a block of consecutive MFT files** (6 for map 7: files 326-331). The first is an uncompressed container: bytes 0-7 layer/scroll settings, `+0x0C` three 8-byte layer records, `+0x24 / 0x28 / 0x2C / 0x30 / 0x34 / 0x38` offsets (relative to the file) of six blobs. Blob 0 = a small byte map (decompressed, post-processed), blob 1 -> EWRAM `0x0202C000`, blob 2 (64 KB = 128x128 u32 per tile: the collision / event table) -> `0x02010000`, blob 3 -> `0x0202D000`, blob 4 -> `0x0202DE00`. The loader is at `0x0800FB60`. Files 327-331 (palette-like data and four format-2 images) are loaded by `0x0800F9F4` / `0x0800FAC8`.
- The current map id is the u16 at `0x02000400`; it is written by the event/warp handler at `0x0808A6B4`.

## More verified facts
- **Map id -> files**: the map table is at ROM `0x9F1AC`, 8 bytes per map id (ids 0..204): `u16 graphicsSet, u16 0, u16 mapCodeFile(0x370+id area), u16 flags`. `graphicsSet` is also copied to RAM `0x0200041A` when the map loads. The loader `0x0800FB38(r0 = graphicsSet)` reads six file offsets (u16) from the table at ROM `0x13784` (12 bytes per set, added to MFT index 296): set 6 (map 7) = files 326..331. Sets can be shared (maps 3 and 9 of that table use the same files).
- **Palette**: file 2 of the set (327 for set 6) decompresses (format 0) to 448 bytes = 224 BGR555 colours; the tail matches palette RAM exactly (the rest is changed at runtime by the time-of-day / weather colour filters).
- **Tile images**: files 3..5+ of a set start with format byte 2 (a nibble/Huffman style variant of format 0, decoder `0x08002818` in ARM, table-driven with a 16-entry nibble permutation at `0x08002848`). Not ported yet.
- **Layers in RAM after a load**: three 128x128 u16 buffers at EWRAM `0x02020000`, `0x02028000`, `0x02030000` (entries use bits 0-9 index, 0x400 / 0x800 flips). They are produced from blob 0 of the container (halfwords stored as a high-byte plane then a low-byte plane, each XOR-delta coded: `0x0800F9F4`, mode byte = first byte of blob 0, mode 1 here). They are NOT the final 8x8 BG tile entries (VRAM screenblocks hold different numbers), so there is another indirection (probably 16x16 blocks built from tiles) still to be found.
- Probe commands added: `vdump FILE` (palette, VRAM, OAM, I/O), plus the existing `bp`, `trace`, `steps`.

## Still to do
1. Port the format-2 decoder and find which decoded image lands in which VRAM charblock.
2. Find the block (metatile) table that turns the EWRAM layer entries into BG tile entries (compare against the VRAM screenblocks, which are the ground truth).
3. Composite the three BG layers (priorities, flips, palettes) into one image per map and check it against emulator screenshots with sprites hidden; the world map (affine) needs its own handling.
4. Write the result into `maps.bin` (existing format) so the app shows a fully explored map. Verify on a few maps, then run it for all 205.
