
# -*- coding: utf-8 -*-
import struct, os, shutil
BASE = r"D:\Documents\DSH\swbf2_chs\installer\installer_xp.exe"
PAY = r"D:\Documents\DSH\swbf2_chs\installer\payload.bin"
OUT = r"D:\Documents\DSH\swbf2_chs\installer\SWBF2_CN_Setup_XP.exe"

base_size = os.path.getsize(BASE)
data = bytearray(open(PAY, "rb").read())
idx = struct.unpack("<Q", data[-16:-8])[0]
cnt = struct.unpack_from("<I", data, idx)[0]
pos = idx + 4
entries = []
for i in range(cnt):
    nl = struct.unpack_from("<H", data, pos)[0]; pos += 2
    name = data[pos:pos+nl].decode(); pos += nl
    off, size = struct.unpack_from("<QQ", data, pos); pos += 16
    entries.append((name, off, size))
# 重写索引（绝对偏移）
new_idx = len(data) - (len(data) - idx)  # 索引仍在同一位置
pos = idx + 4
for name, off, size in entries:
    nb = name.encode()
    struct.pack_into("<H", data, pos, len(nb)); pos += 2
    data[pos:pos+len(nb)] = nb; pos += len(nb)
    struct.pack_into("<QQ", data, pos, off + base_size, size); pos += 16
# 尾部索引偏移也改成绝对
struct.pack_into("<Q", data, len(data) - 16, idx + base_size)
with open(OUT, "wb") as f:
    with open(BASE, "rb") as b:
        shutil.copyfileobj(b, f)
    f.write(data)
print("linked:", OUT, os.path.getsize(OUT), "base", base_size, "entries", cnt)