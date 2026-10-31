
# -*- coding: utf-8 -*-
import os, subprocess, struct
G = r"E:\GOG Games\Star Wars - Battlefront 2\GameData"
P = os.path.join(G, "data", "_lvl_pc", "movies", "crawl.mvs")
SUB = r"C:\Users\Kille\Desktop\SWBF2_过场动画\1_开场"
WORK = r"D:\Documents\DSH\swbf2_chs\crawl_out"
os.makedirs(WORK, exist_ok=True)
OFF, BLOCK = 2048, 57449540
mp4 = os.path.join(SUB, "crawl_1.mp4")
srt = os.path.join(SUB, "crawl_1.srt")
burn = os.path.join(WORK, "crawl_1_zh.mp4")
bik = os.path.join(WORK, "crawl_1_zh.bik")
style = ("FontName=Microsoft YaHei,FontSize=16,PrimaryColour=&H0000D7FF,OutlineColour=&H00000000,"
         "BorderStyle=1,Outline=1.6,Shadow=0.6,Alignment=2,MarginV=28,MarginL=40,MarginR=40")
srt_esc = srt.replace("\\", "/").replace(":", "\\:")
subprocess.run(["ffmpeg", "-y", "-hide_banner", "-loglevel", "error", "-i", mp4,
                "-vf", "subtitles='%s':force_style='%s'" % (srt_esc, style),
                "-c:v", "libx264", "-preset", "veryfast", "-crf", "20",
                "-c:a", "copy", burn], check=False)
print("burned", os.path.getsize(burn))
rate = int(BLOCK * 0.90 / 98.0)
ok = False
for _ in range(3):
    if os.path.exists(bik): os.remove(bik)
    subprocess.run([r"D:\RADVideo\radvideo64.exe", "Binkc", burn, bik, "/D%d" % rate], check=False)
    if os.path.exists(bik) and os.path.getsize(bik) <= BLOCK - 16:
        ok = True; break
    rate = int(rate * 0.8)
print("encoded", os.path.getsize(bik) if os.path.exists(bik) else "FAIL", "rate", rate, "ok", ok)
assert ok
data = bytearray(open(bik, "rb").read())
struct.pack_into("<I", data, 4, len(data) - 8)
d = bytearray(open(P, "rb").read())
d[OFF:OFF+len(data)] = bytes(data)
d[OFF+len(data):OFF+BLOCK] = b"\x00" * (BLOCK - len(data))
open(P, "wb").write(d)
print("patched crawl_1; file size", os.path.getsize(P))
