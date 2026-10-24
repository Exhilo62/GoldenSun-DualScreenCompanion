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
    if fmt != 0:
        raise ValueError('format %d' % fmt)
    b = Bits(data, pos)
    while True:
        if b.get(1):
            out.append(b.get(8))
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
