"""Golden Sun (USA/Europe) ROM map data: Master File Table, decompression (formats 0 and 1), map file containers."""
import struct

ROM = None
MFT = 0x320000


def load(path='/d/gs3ds/build/gs.gba'):
    global ROM
    ROM = open(path, 'rb').read()
    return ROM


def mft_addr(n):
    return struct.unpack_from('<I', ROM, MFT + 4 * n)[0] - 0x08000000


def mft_count():
    n = 0
    while 0x08000000 <= struct.unpack_from('<I', ROM, MFT + 4 * n)[0] < 0x0A000000:
        n += 1
    return n


def file_size(n):
    a = mft_addr(n)
    nxt = min((mft_addr(i) for i in range(mft_count()) if mft_addr(i) > a), default=len(ROM))
    return nxt - a


class Bits:
    def __init__(self, data, pos):
        self.d, self.p, self.buf, self.n = data, pos, 0, 0

    def get(self, k):
        while self.n < k:
            self.buf |= (self.d[self.p] if self.p < len(self.d) else 0) << self.n
            self.p += 1
            self.n += 8
        v = self.buf & ((1 << k) - 1)
        self.buf >>= k
        self.n -= k
        return v


def decompress(data, pos):
    """data[pos] is the format byte. Returns (bytes, input position after the stream)."""
    fmt = data[pos]
    pos += 1
    out = bytearray()
    if fmt == 1:
        while True:
            flags = data[pos]
            pos += 1
            for bit in range(7, -1, -1):
                if not (flags >> bit) & 1:
                    out.append(data[pos])
                    pos += 1
                    continue
                b0, b1 = data[pos], data[pos + 1]
                pos += 2
                dist = ((b0 & 0xF0) << 4) | b1
                n = b0 & 15
                if n == 0:
                    if dist == 0:
                        return bytes(out), pos
                    n = data[pos] + 16
                    pos += 1
                n += 1
                for _ in range(n):
                    out.append(out[-dist])
    if fmt not in (0, 2):
        raise ValueError('format %d' % fmt)
    b = Bits(data, pos)
    mtf = list(range(16))   # format 2: literals are two nibbles coded as move-to-front indices

    def nib(width):
        v = mtf.pop(b.get(width))
        mtf.insert(0, v)
        return v
    while True:
        if b.get(1):
            if fmt == 0:
                out.append(b.get(8))
            else:
                w = 2 if b.get(1) else (4 if not b.get(1) else 3)
                lo = nib(w)
                out.append(lo | nib(w) << 4)
            continue
        # length code: 0 -> 2, 10 -> 3, 110 -> 4, 1110 -> 5, 11110x -> 6/7, 11111 -> long
        n = 2
        while n < 6 and b.get(1):
            n += 1
        if n == 6:
            if b.get(1):
                k = b.get(2)
                if k:
                    n = k + 7
                else:
                    k = b.get(7)
                    if k == 0:
                        return bytes(out), b.p - (b.n >> 3)
                    n = k + 10
            else:
                n = 6 + b.get(1)
        if b.get(1):
            d = b.get(5)
        else:
            r6 = len(out) - 33
            if r6 < 0 or r6 >= 2048:
                d = b.get(12) + 32
            else:
                d = b.get(r6.bit_length()) + 32
        for _ in range(n):
            out.append(out[-(d + 1)])


# ---- map rendering ----------------------------------------------------------------------------------------------------------------------------

MAP_TABLE = 0x9F1AC       # 8 bytes per map id: u16 graphicsSet, 0, mapCode file, flags
SET_TABLE = 0x13784       # 12 bytes per graphicsSet: six u16 MFT offsets (+296)
LAYER_ORIGIN = ((0, 0), (0, 60), (60, 0))   # cell origin of each of the three layers inside the 128x128 cell table
LAYER_CELLS = 60


def map_files(map_id):
    gs = struct.unpack_from('<H', ROM, MAP_TABLE + 8 * map_id)[0]
    return gs, [296 + x for x in struct.unpack_from('<6H', ROM, SET_TABLE + 12 * gs)]


def bgr555(c):
    return ((c & 31) * 255 // 31, ((c >> 5) & 31) * 255 // 31, ((c >> 10) & 31) * 255 // 31)


def load_map(map_id):
    """Decodes everything needed to draw a map: block dictionary, 128x128 cell table, tile images (VRAM layout) and palette."""
    gs, files = map_files(map_id)
    base = mft_addr(files[0])
    h = struct.unpack_from('<15I', ROM, base)
    offs = h[9:15]
    blob0 = decompress(ROM, base + offs[0])[0]
    mode = blob0[0]
    n = (len(blob0) - 1) >> 1
    if mode == 1:
        raw = [(blob0[1 + i] << 8) | blob0[1 + n + i] for i in range(n)]
    else:
        raw = list(struct.unpack_from('<%dH' % n, blob0, 1))
    if mode == 0:
        dic = raw
    else:
        dic, prev = [], 0
        for v in raw:
            prev ^= v
            dic.append(prev)
    raw = decompress(ROM, base + offs[2])[0]   # 64 KB: two byte planes of the cells' high halves, then the low halves (0xFFF = the next unique block id)
    lo = list(struct.unpack_from('<16384H', raw, 0x8000))
    nxt = 0
    for i, v in enumerate(lo):
        if (v & 0xFFF) == 0xFFF:
            lo[i] = (v + 0xF001 + nxt) & 0xFFFF
            nxt += 1
    cells = tuple(((raw[i] | raw[0x4000 + i] << 8) << 16) | lo[i] for i in range(16384))
    pal_bytes = decompress(ROM, mft_addr(files[1]))[0]
    pal = struct.unpack('<%dH' % (len(pal_bytes) // 2), pal_bytes)
    vram = bytearray(0x10000)
    for k, f in enumerate(files[2:5]):
        img = decompress(ROM, mft_addr(f))[0]
        vram[0x4000 + 0x4000 * k:0x4000 + 0x4000 * k + len(img)] = img[:0x4000]
    hb = ROM[base:base + 0x24]
    layers = [(hb[12 + 8 * k] // 2, hb[13 + 8 * k] // 2) for k in range(3)]   # table cell origin of each layer
    return dict(gs=gs, files=files, mode=mode, dic=dic, cells=cells, pal=pal, vram=bytes(vram), header=h,
                width=hb[2] // 2, height=hb[3] // 2, origins=layers)


def render_layer(mp, k, cb, ox=None, oy=None, cells_w=LAYER_CELLS, cells_h=LAYER_CELLS):
    """RGBA rows (list of bytearray) of layer k: cells_w x cells_h cells of 16x16 px; cb = charblock (0 or 2) the layer's tile numbers are relative to."""
    if ox is None:
        ox, oy = LAYER_ORIGIN[k]
    W, H = cells_w * 16, cells_h * 16
    img = [bytearray(W * 4) for _ in range(H)]
    dic, cells, pal, vram = mp['dic'], mp['cells'], mp['pal'], mp['vram']
    for cy in range(cells_h):
        for cx in range(cells_w):
            bid = cells[((oy + cy) & 127) * 128 + ((ox + cx) & 127)] & 0xFFF
            if bid * 4 + 4 > len(dic):
                continue
            for q in range(4):
                e = dic[bid * 4 + q]
                tile, hf, vf, pb = e & 0x3FF, e & 0x400, e & 0x800, e >> 12
                addr = cb * 0x4000 + tile * 32
                if addr + 32 > len(vram):
                    continue
                tx, ty = (q & 1) * 8, (q >> 1) * 8
                for py in range(8):
                    sy = 7 - py if vf else py
                    row = img[cy * 16 + ty + py]
                    for px in range(8):
                        sx = 7 - px if hf else px
                        b = vram[addr + sy * 4 + (sx >> 1)]
                        ci = (b >> 4) if sx & 1 else (b & 15)
                        if not ci:
                            continue
                        idx = pb * 16 + ci
                        if idx >= len(pal):
                            continue
                        r, g, bl = bgr555(pal[idx])
                        o = (cx * 16 + tx + px) * 4
                        row[o:o + 4] = bytes((r, g, bl, 255))
    return img


def composite(layers):
    """Draws layers[0] first (bottom) ... layers[-1] last (top)."""
    H, W = len(layers[0]), len(layers[0][0]) // 4
    out = [bytearray(W * 4) for _ in range(H)]
    for L in layers:
        for y in range(H):
            row, src = out[y], L[y]
            for x in range(W):
                if src[x * 4 + 3]:
                    row[x * 4:x * 4 + 4] = src[x * 4:x * 4 + 4]
    return out


def write_png(path, rows):
    import zlib
    H, W = len(rows), len(rows[0]) // 4
    raw = b''.join(b'\x00' + bytes(r) for r in rows)

    def ch(t, c):
        return struct.pack('>I', len(c)) + t + c + struct.pack('>I', zlib.crc32(t + c))
    open(path, 'wb').write(b'\x89PNG\r\n\x1a\n' + ch(b'IHDR', struct.pack('>IIBBBBB', W, H, 8, 6, 0, 0, 0)) + ch(b'IDAT', zlib.compress(raw)) + ch(b'IEND', b''))
