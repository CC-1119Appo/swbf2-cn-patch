
# -*- coding: utf-8 -*-
import os, struct
STAGE = r"D:\Documents\DSH\swbf2_chs\installer_stage"
OUT = r"D:\Documents\DSH\swbf2_chs\installer\payload.bin"
files = []
for dp, dn, fn in os.walk(STAGE):
    for f in fn:
        p = os.path.join(dp, f)
        files.append((os.path.relpath(p, STAGE).replace("\\", "/"), p))
files.sort()
offsets = []
with open(OUT, "wb") as out:
    for rel, p in files:
        nb = rel.encode("utf-8")
        size = os.path.getsize(p)
        out.write(struct.pack("<H", len(nb)))
        out.write(nb)
        out.write(struct.pack("<Q", size))
        data_off = out.tell()
        offsets.append((rel, data_off, size))
        with open(p, "rb") as f:
            while True:
                chunk = f.read(1 << 20)
                if not chunk: break
                out.write(chunk)
    index_off = out.tell()
    out.write(struct.pack("<I", len(offsets)))
    for rel, off, size in offsets:
        nb = rel.encode("utf-8")
        out.write(struct.pack("<H", len(nb)))
        out.write(nb)
        out.write(struct.pack("<QQ", off, size))
    out.write(struct.pack("<Q", index_off))
    out.write(b"SWCNBLOB")
print("payload", os.path.getsize(OUT), "files", len(offsets))
# 自检
with open(OUT, "rb") as f:
    for rel, off, size in offsets[:3]:
        f.seek(off)
        print("   %-40s off=%d size=%d head=%r" % (rel, off, size, f.read(4)))
