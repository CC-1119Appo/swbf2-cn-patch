
# -*- coding: utf-8 -*-
import os, struct, ctypes

STAGE = r"D:\Documents\DSH\swbf2_chs\installer_stage"
OUT = r"D:\Documents\DSH\swbf2_chs\installer\payload.bin"
CHUNK = 48 * 1024 * 1024      # 大文件分块压缩块大小
LIMIT = 96 * 1024 * 1024      # 单块压缩上限

ntdll = ctypes.WinDLL("ntdll")
cws = ctypes.c_ulong(); dws = ctypes.c_ulong()
ntdll.RtlGetCompressionWorkSpaceSize(2, ctypes.byref(cws), ctypes.byref(dws))
WORK = cws.value

def lz(data):
    work = ctypes.create_string_buffer(WORK)
    dst = ctypes.create_string_buffer(len(data) + 4096)
    final = ctypes.c_ulong(0)
    st = ntdll.RtlCompressBuffer(2, data, len(data), dst, len(dst), 4096, ctypes.byref(final), work)
    if st != 0: return None
    return dst.raw[:final.value]

files = []
for dp, dn, fn in os.walk(STAGE):
    for f in fn:
        p = os.path.join(dp, f)
        files.append((os.path.relpath(p, STAGE).replace("\\", "/"), p))
files.sort()

entries = []
raw_total = 0
stored_total = 0
with open(OUT, "wb") as out:
    for rel, p in files:
        data = open(p, "rb").read()
        raw_total += len(data)
        method, payload = 0, data
        if len(data) <= LIMIT:
            z = lz(data)
            if z is not None and len(z) < len(data) * 95 // 100:
                method, payload = 1, z
        else:
            # 分块压缩：method=2，格式 [u32 n][ (u32 csz, u32 osz, data) * n ]
            chunks = [data[i:i+CHUNK] for i in range(0, len(data), CHUNK)]
            parts = []
            ok = True
            for c in chunks:
                z = lz(c)
                if z is None or len(z) >= len(c):
                    parts.append((len(c), len(c), c))
                else:
                    parts.append((len(z), len(c), z))
            if ok:
                method = 2
                buf = struct.pack("<I", len(parts))
                for csz, osz, blob in parts:
                    buf += struct.pack("<II", csz, osz) + blob
                payload = buf
        nb = rel.encode("utf-8")
        out.write(struct.pack("<H", len(nb)))
        out.write(nb)
        out.write(struct.pack("<B", method))
        out.write(struct.pack("<Q", len(payload)))
        out.write(struct.pack("<Q", len(data)))
        off = out.tell()
        out.write(payload)
        entries.append((rel, off, len(payload), len(data), method))
        stored_total += len(payload)
        print("   %-46s %8.1f -> %8.1f MB  m=%d" % (rel, len(data)/1e6, len(payload)/1e6, method))
    index_off = out.tell()
    out.write(struct.pack("<I", len(entries)))
    for rel, off, sz, orig, m in entries:
        nb = rel.encode("utf-8")
        out.write(struct.pack("<H", len(nb)))
        out.write(nb)
        out.write(struct.pack("<B", m))
        out.write(struct.pack("<QQ", off, sz))
        out.write(struct.pack("<Q", orig))
    out.write(struct.pack("<Q", index_off))
    out.write(b"SWCNBLOB")

print("payload %.1f MB (raw %.1f MB, %.1f%%)  files %d" % (
    os.path.getsize(OUT)/1e6, raw_total/1e6, 100.0*stored_total/raw_total, len(entries)))
