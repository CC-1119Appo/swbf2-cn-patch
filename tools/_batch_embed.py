
# -*- coding: utf-8 -*-
import os, subprocess, struct, shutil, json, time

G = r"E:\GOG Games\Star Wars - Battlefront 2\GameData"
P = os.path.join(G, "data", "_lvl_pc", "movies", "ingame.mvs")
BAK = P + ".original"
SUB = r"C:\Users\Kille\Desktop\SWBF2_过场动画\2_剧情"
WORK = r"D:\Documents\DSH\swbf2_chs\movies_out\enc"
os.makedirs(WORK, exist_ok=True)
LOG = os.path.join(WORK, "batch.log")

def log(s):
    line = "[%s] %s" % (time.strftime("%H:%M:%S"), s)
    print(line, flush=True)
    open(LOG, "a", encoding="utf-8").write(line + "\n")

# 解析原始容器
orig = open(BAK, "rb").read()
count = struct.unpack_from("<I", orig, 11*4)[0]
recs = []
for k in range(count):
    base = 12 + k*8
    v = struct.unpack_from("<8I", orig, base*4)
    recs.append({"base": base, "hash": v[3], "size": v[5], "pad": v[7]})
offs = []
o = 2048
for r in recs:
    offs.append(o)
    o += r["size"] + r["pad"]
log("records=%d total=%d" % (count, o))

def duration(mp4):
    out = subprocess.run(["ffprobe", "-v", "error", "-show_entries", "format=duration",
                          "-of", "csv=p=0", mp4], capture_output=True, text=True)
    try:
        return float(out.stdout.strip())
    except Exception:
        return 0.0

work = bytearray(orig)
done, skipped = [], []

for i in range(1, count + 1):
    srt = os.path.join(SUB, "ingame_%d.srt" % i)
    mp4 = os.path.join(SUB, "ingame_%d.mp4" % i)
    if not (os.path.exists(srt) and os.path.exists(mp4)):
        skipped.append(i)
        continue
    r = recs[i-1]
    off, block = offs[i-1], r["size"] + r["pad"]
    dur = duration(mp4)
    if dur <= 0:
        skipped.append(i)
        continue
    burn = os.path.join(WORK, "i%d.mp4" % i)
    bik = os.path.join(WORK, "i%d.bik" % i)
    style = "FontName=Microsoft YaHei,FontSize=14,PrimaryColour=&H00FFFFFF,OutlineColour=&H00000000,BorderStyle=1,Outline=1.4,Shadow=0.4,Alignment=2,MarginV=16,MarginL=20,MarginR=20"
    srt_esc = srt.replace("\\", "/").replace(":", "\\:")
    subprocess.run(["ffmpeg", "-y", "-hide_banner", "-loglevel", "error", "-i", mp4,
                    "-vf", "subtitles='%s':force_style='%s'" % (srt_esc, style),
                    "-c:v", "libx264", "-preset", "veryfast", "-crf", "20",
                    "-c:a", "copy", burn], check=False)
    rate = int(block * 0.90 / dur)
    ok = False
    for attempt in range(3):
        if os.path.exists(bik):
            os.remove(bik)
        subprocess.run([r"D:\RADVideo\radvideo64.exe", "Binkc", burn, bik, "/D%d" % rate], check=False)
        if not os.path.exists(bik):
            rate = int(rate * 0.8); continue
        if os.path.getsize(bik) <= block - 16:
            ok = True
            break
        rate = int(rate * 0.8)
    if not ok:
        log("#%d FAILED (block %d)" % (i, block))
        skipped.append(i)
        continue
    d = bytearray(open(bik, "rb").read())
    struct.pack_into("<I", d, 4, len(d) - 8)
    size = len(d)
    pad = block - size
    work[off:off+size] = bytes(d)
    struct.pack_into("<I", work, off + size, 0x809608B6)
    struct.pack_into("<I", work, off + size + 4, pad)
    work[off+size+8:off+block] = b"\x00" * (pad - 8)
    struct.pack_into("<I", work, r["base"]*4 + 20, size)
    struct.pack_into("<I", work, r["base"]*4 + 28, pad)
    log("#%d ok size=%d pad=%d rate=%d" % (i, size, pad, rate))
    done.append(i)

open(P, "wb").write(work)
log("DONE embedded=%s skipped=%s" % (done, skipped))
