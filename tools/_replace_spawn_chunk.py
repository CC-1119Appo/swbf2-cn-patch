
# -*- coding: utf-8 -*-
import sys, struct, os, shutil
sys.path.insert(0, r"D:\Documents\DSH\swbf2_chs\tools")
import lvl
P = r"E:\GOG Games\Star Wars - Battlefront 2\GameData\data\_lvl_pc\ingame.lvl"
BAK = P + ".original"
shutil.copy2(BAK, P)
L = lvl.load(P)
newbody = open(r"D:\Documents\DSH\swbf2_chs\build\spawn_strip.out", "rb").read()

def chunk(tag, payload):
    return tag + struct.pack("<I", len(payload)) + payload + b"\x00" * ((4 - len(payload) % 4) % 4)

for i, ch in enumerate(L.chunks):
    if ch.type == b"scr_" and b"ifs_pc_spawnselect" in ch.data:
        d = ch.data
        # 解析原 NAME / INFO
        nl = struct.unpack_from("<I", d, 4)[0]
        name = d[8:8+nl]
        q = 8 + ((nl + 3) & ~3)
        il = struct.unpack_from("<I", d, q+4)[0]
        info = d[q+8:q+8+il]
        print("name", name, "info", info.hex())
        data = chunk(b"NAME", name) + chunk(b"INFO", info) + chunk(b"BODY", newbody)
        L.chunks[i] = lvl.Chunk(b"scr_", len(data), data, b"")
        print("old chunk len", len(d), "new", len(data))
        break
raw = L.serialize()
open(P, "wb").write(raw)
print("written", os.path.getsize(P), "(orig", os.path.getsize(BAK), ")")
