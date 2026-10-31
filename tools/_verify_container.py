
# -*- coding: utf-8 -*-
import struct, os
P = r"E:\GOG Games\Star Wars - Battlefront 2\GameData\data\_lvl_pc\movies\ingame.mvs"
d = open(P, "rb").read()
count = struct.unpack_from("<I", d, 11*4)[0]
off = 2048
ok = True
for k in range(count):
    base = 12 + k*8
    v = struct.unpack_from("<8I", d, base*4)
    size, pad = v[5], v[7]
    magic = d[off:off+4]
    sep_at = off + size
    sep = struct.unpack_from("<I", d, sep_at)[0] if sep_at + 4 <= len(d) else 0
    sep_pad = struct.unpack_from("<I", d, sep_at + 4)[0] if sep_at + 8 <= len(d) else 0
    good = magic == b"BIKi" and sep == 0x809608B6 and sep_pad == pad - 16
    if not good:
        ok = False
        print("#%d BAD magic=%r sep=%#x sep_pad=%d pad=%d" % (k+1, magic, sep, sep_pad, pad))
    off += size + pad
print("file size:", len(d), "computed end:", off, "all ok:", ok and off == len(d))