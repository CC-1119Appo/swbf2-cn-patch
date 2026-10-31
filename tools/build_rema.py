
# -*- coding: utf-8 -*-
"""build_rema.py - 重建 Remaster 的三套字体为中文版，并翻译其本地化文件。"""
import os, sys, struct, json, shutil
sys.path.insert(0, r"D:\Documents\DSH\swbf2_chs\tools")
import lvl, locl, cjkfont

G = r"E:\GOG Games\Star Wars - Battlefront 2\GameData"
REMA = os.path.join(G, "addon", "Remaster")
WB = r"D:\Documents\DSH\swbf2_chs\workbook\workbook.zh.json"
OUT = r"D:\Documents\DSH\swbf2_chs\build\rema"

os.makedirs(OUT, exist_ok=True)

wb = json.load(open(WB, encoding="utf-8"))
chars = set()
for e in wb:
    for c in (e.get("zh") or ""):
        o = ord(c)
        if 0x4E00 <= o <= 0x9FFF or 0x3000 <= o <= 0x303F or 0xFF01 <= o <= 0xFF5E or o in (0x2018,0x2019,0x201C,0x201D,0x2026,0x2014):
            chars.add(c)
high = sorted(o for o in (ord(c) for c in chars) if o >= 0x8000)
sur = {cp: 0x0100 + i for i, cp in enumerate(high)}
RANGES = [(0x0100, 0x0FFF), (0x2000, 0x206F), (0x3000, 0x303F), (0x4E00, 0x9FFF), (0xFF00, 0xFF5F)]
latin = list(range(0x1E, 0x100))
all_cps = sorted(set(latin) | set().union(*[set(range(a, b+1)) for a, b in RANGES]))
wanted = set(latin) | set(o for o in (ord(c) for c in chars) if o < 0x8000) | set(sur.values())
LOAD_IDS = (2478589025, 1694151954)
LOAD_CHARS = []
for _e in wb:
    if int(_e.get("id", 0)) in LOAD_IDS:
        for c in (_e.get("zh") or ""):
            if c not in LOAD_CHARS:
                LOAD_CHARS.append(c)
EXTRA = []
print("load big glyphs:", len(EXTRA))
print("entries per font:", len(all_cps), "with bitmap:", len(wanted), "high chars:", len(high))

def head_fh(D):
    ho = cjkfont.get(D, 0, len(D), b"HEAD")
    return D[ho[0] + 3]

for variant in ["default", "2k", "4k"]:
    src = os.path.join(REMA, "Fonts", "arialblack_%s.lvl" % variant)
    if not os.path.exists(src):
        print("MISS", src); continue
    bak = src + ".original"
    if not os.path.exists(bak):
        shutil.copy2(src, bak)
    L = lvl.load(src)
    for i, ch in enumerate(L.chunks):
        if ch.type != b"font":
            continue
        D = ch.data
        nm = D[8:8+struct.unpack_from("<I", D, 4)[0]].rstrip(b"\x00").decode("ascii", "replace")
        fh = head_fh(D)
        size = max(8, int(round(fh * 0.78)))
        if "tiny" in nm:
            size = int(round(size * 1.7))
        elif "medium" in nm:
            size = int(round(size * 1.2))
        elif "small" in nm:
            size = int(round(size * 1.45))
        W, H = cjkfont.pick_atlas(len(wanted), size, 1.0)
        inv = {v: k for k, v in sur.items()}
        out, _rendered = cjkfont.build_font(D, all_cps, size, W, H, wanted, inv, extra=EXTRA)
        print("  %-8s %-22s fh=%2d size=%2d atlas=%dx%d chunk=%d" % (variant, nm, fh, size, W, H, len(out)))
        L.chunks[i] = lvl.Chunk(b"font", len(out), out, b"")
    raw = L.serialize()
    dst = os.path.join(OUT, "arialblack_%s.lvl" % variant)
    open(dst, "wb").write(raw)
    shutil.copy2(dst, src)
    print("  -> installed %s (%d)" % (variant, len(raw)))

# 翻译 Remaster 自带文案
REMA_LOC = {
    1135589274: "{OptionL} 更改 {OptionR}",
    149100241: "时期：%s",
    2070004821: "AI 英雄",
    3213034778: "奖励与增益效果",
    1200855905: "奖励武器",
    1899422059: "自定义地图颜色",
    1261137152: "英雄语音",
    16204945: "保存比赛选项",
    2603874995: "安装",
    3595513015: "游戏脚本：",
    3594527277: "界面脚本：",
    3325791183: "设置脚本：",
    1581296749: "正在搜索可用脚本，此过程游戏会暂时卡顿。",
    3147918531: "共和国突击队",
    605452416: "复刻版",
    2153774755: "设置",
    2170552374: "脚本管理器",
}