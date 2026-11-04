
# -*- coding: utf-8 -*-
import struct, os, shutil
BASE = r"D:\Documents\DSH\swbf2_chs\installer\installer_xp.exe"
PAY = r"D:\Documents\DSH\swbf2_chs\installer\payload.bin"
OUT = os.environ.get("LINK_OUT") or r"D:\Documents\DSH\swbf2_chs\installer\SWBF2_CN_Setup_XP.exe"

base_size = os.path.getsize(BASE)
data = bytearray(open(PAY, "rb").read())
idx = struct.unpack("<Q", data[-16:-8])[0]
cnt = struct.unpack_from("<I", data, idx)[0]
pos = idx + 4
entries = []
for i in range(cnt):
    nl = struct.unpack_from("<H", data, pos)[0]; pos += 2
    name = data[pos:pos+nl].decode(); pos += nl
    method = struct.unpack_from("<B", data, pos)[0]; pos += 1
    off, sz, orig = struct.unpack_from("<QQQ", data, pos); pos += 24
    entries.append((name, method, off, sz, orig))
# 重写索引（绝对偏移）
pos = idx + 4
for name, method, off, sz, orig in entries:
    nb = name.encode()
    struct.pack_into("<H", data, pos, len(nb)); pos += 2
    data[pos:pos+len(nb)] = nb; pos += len(nb)
    struct.pack_into("<B", data, pos, method); pos += 1
    struct.pack_into("<QQQ", data, pos, off + base_size, sz, orig); pos += 24
struct.pack_into("<Q", data, len(data) - 16, idx + base_size)
with open(OUT, "wb") as f:
    with open(BASE, "rb") as b:
        shutil.copyfileobj(b, f)
    f.write(data)
print("linked:", OUT, os.path.getsize(OUT), "base", base_size, "entries", cnt)
print("   compressed entries:", sum(1 for e in entries if e[1] == 1), "/", cnt)
