# Map data in the ROM (research notes, Golden Sun USA/Europe)

Goal: draw complete area maps from the player's own ROM at patch time (no art is shipped), instead of building the map while exploring.
Tools: `tools/gsmap.py` (file table + decompressor), `tools/gbadis.c` (Thumb/ARM disassembler built on mGBA's decoder; `build/gbadis.exe rom START LEN [arm]`).
Resource used: the Golden Sun Data Wiki "ROM Map" page (gsdata.fandom.com). Its addresses are for another ROM version; here they differ.

## Verified
- **Master File Table** (MFT): ROM offset `0x320000`, 975 u32 ROM pointers (`0x08xxxxxx`); file `n` = `ROM[(MFT[n] & 0x1FFFFFF)]`, its size is the gap to the next higher pointer. `get_mft_file(n)` is the Thumb function at `0x08002F40`.
- **Compression**: the first byte of a compressed blob is the format. Format 1 (flag bytes, MSB first; 12-bit distance, 4-bit length; end marker distance 0) and format 0 (LSB-first bit stream: literal = 1 + 8 bits; lengths 2..7 as 0 / 10 / 110 / 1110 / 11110x, long forms 11111 + 2 bits (8..10) or + 7 bits (+10, 0 = end); distance = 1 + 5 bits, or a variable width of bit_length(written-33) bits + 32 (12 bits once 2048+ bytes are written)). The decoder is at `0x08002544` (copied to RAM and run). `gsmap.decompress` reproduces the game's output byte for byte (checked against EWRAM after a map load). Format 2 (tile images) is a different routine, not decoded yet.
- **A map is a block of consecutive MFT files** (6 for map 7: files 326-331). The first is an uncompressed container: bytes 0-7 layer/scroll settings, `+0x0C` three 8-byte layer records, `+0x24 / 0x28 / 0x2C / 0x30 / 0x34 / 0x38` offsets (relative to the file) of six blobs. Blob 0 = a small byte map (decompressed, post-processed), blob 1 -> EWRAM `0x0202C000`, blob 2 (64 KB = 128x128 u32 per tile: the collision / event table) -> `0x02010000`, blob 3 -> `0x0202D000`, blob 4 -> `0x0202DE00`. The loader is at `0x0800FB60`. Files 327-331 (palette-like data and four format-2 images) are loaded by `0x0800F9F4` / `0x0800FAC8`.
- The current map id is the u16 at `0x02000400`; it is written by the event/warp handler at `0x0808A6B4`.

## Still to do
1. Decode format 2 (tile images) and the palette file; find which blob holds the BG tile entries for each layer and their format.
2. Find how a map id (`0x02000400`) selects its first MFT file (the loader adds a constant to a halfword read from the area table).
3. Composite the three BG layers (priorities, flips, palettes) into one image per map; the world map uses affine layers and needs its own handling.
4. Write the result into `maps.bin` (existing format) so the app shows a fully explored map.
