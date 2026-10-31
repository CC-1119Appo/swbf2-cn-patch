# -*- coding: utf-8 -*-
"""GOG 汉化 v3：代理码位（>=0x8000 的字映射到 0x0100+）+ 5 字体重建 + Locl 改写。"""
import os, sys, struct, json
sys.path.insert(0, r"D:\Documents\DSH\swbf2_chs\tools")
import lvl, locl, cjkfont
from PIL import Image, ImageDraw, ImageFont

BASE = r"D:\Documents\DSH\swbf2_chs\backup\gog\core.lvl.rema_original"
OUT = r"D:\Documents\DSH\swbf2_chs\build\gog_cjk_v3.lvl"
WB = r"D:\Documents\DSH\swbf2_chs\workbook\workbook.zh.json"
FONT_PATH = r"D:\Documents\DSH\swbf2_chs\fonts\IBMPlexSansSC-Regular.ttf"
BOLD = True

def parts(d, s, e):
    o = []; p = s
    while p + 8 <= e:
        t = d[p:p+4]; sz = struct.unpack_from("<I", d, p+4)[0]
        o.append((t, p+8, sz)); p += (8+sz+3) & ~3
    return o
def get(d, s, e, k):
    for t, o, sz in parts(d, s, e):
        if t == k: return (o, sz)
    return None

wb = json.load(open(WB, encoding="utf-8"))
zh_map = {}
chars = set()
for e in wb:
    zh = (e.get("zh") or "").strip()
    if not zh: continue
    zh_map[int(e["id"])] = zh
    for c in zh:
        o = ord(c)
        if 0x4E00 <= o <= 0x9FFF or 0x3000 <= o <= 0x303F or 0xFF01 <= o <= 0xFF5E or o in (0x2018,0x2019,0x201C,0x201D,0x2026,0x2014):
            chars.add(c)
print("translations:", len(zh_map), "chars:", len(chars))

# ---- 代理码位：把 >=0x8000 的字映射到 0x0100+ ----
high = sorted(o for o in (ord(c) for c in chars) if o >= 0x8000)
sur = {cp: 0x0100 + i for i, cp in enumerate(high)}
inv = {v: k for k, v in sur.items()}
print("high-codepoint chars:", len(high), "-> surrogate base 0x0100")

latin = list(range(0x1E, 0x100))
RANGES = [(0x0100, 0x0FFF), (0x2000, 0x206F), (0x3000, 0x303F), (0x4E00, 0x9FFF), (0xFF00, 0xFF5F)]
all_cps = sorted(set(latin) | set().union(*[set(range(a, b+1)) for a, b in RANGES]))
# 需要位图的 id：拉丁 + 中文（<0x8000 用原码，>=0x8000 用代理）
wanted = set(latin) | set(o for o in (ord(c) for c in chars) if o < 0x8000) | set(sur.values())
print("entries:", len(all_cps), "with bitmap:", len(wanted))

def build_font(D, cps, size, W, H, y_off=0, fh_override=0, ss=1.0):
    rsize = max(6, int(round(size * ss)))
    ho = get(D,0,len(D),b"HEAD"); to = get(D,0,len(D),b"FTEX")
    HEAD = D[ho[0]:ho[0]+ho[1]]
    tex = get(D,to[0],to[0]+to[1],b"tex_"); fmt = get(D,tex[0],tex[0]+tex[1],b"FMT_")
    info = get(D,fmt[0],fmt[0]+fmt[1],b"INFO")
    INFO = bytearray(D[info[0]:info[0]+info[1]])
    struct.pack_into("<H", INFO, 4, W); struct.pack_into("<H", INFO, 6, H)
    font = ImageFont.truetype(FONT_PATH, rsize, index=0)
    asc, desc = font.getmetrics()
    # 用模板中 'A' 的 box 底边作为原生基线，做垂直对齐补偿
    fbo = get(D, 0, len(D), b"FBOD")
    fbd = D[fbo[0]:fbo[0]+fbo[1]]
    native_base = None
    for i in range(len(fbd)//24):
        e = fbd[i*24:(i+1)*24]
        if struct.unpack_from("<H", e, 0)[0] == 0x41:
            native_base = e[7]
            break
    if native_base is None:
        native_base = asc
    shift = native_base - int(round(asc / ss))
    BG = bytes([0xFF, 0x0F])
    buf = bytearray(BG * (W*H))
    def pack(a):
        a = max(0, min(255, int(a)))
        return bytes([0xFF, ((a >> 4) << 4) | 0x0F])
    place = {}
    x = 1; y = 1; rowh = 0
    rendered = 0
    for cp in cps:
        if cp not in wanted or cp < 0x20:
            place[cp] = None; continue
        src = inv.get(cp, cp)
        ch = chr(src)
        try:
            bbox = font.getbbox(ch)
        except Exception:
            place[cp] = None; continue
        x0, y0, x1, y1 = bbox
        w = x1 - x0; h = y1 - y0
        if w <= 0 or h <= 0:
            place[cp] = None; continue
        if x + w + 1 > W:
            x = 1; y += rowh + 1; rowh = 0
        if y + h + 1 > H:
            place[cp] = None; continue
        img = Image.new("L", (w, h), 0)
        dd = ImageDraw.Draw(img)
        dd.text((-x0, -y0), ch, font=font, fill=255, anchor="la")
        alpha = img.tobytes()
        if BOLD:
            w2 = w + 1
            a2 = bytearray(w2 * h)
            for yy in range(h):
                b0 = yy * w
                for xx in range(w2):
                    v = 0
                    if xx < w:
                        v = alpha[b0 + xx]
                    if xx > 0:
                        v2 = alpha[b0 + xx - 1]
                        if v2 > v:
                            v = v2
                    a2[yy * w2 + xx] = v
            alpha = bytes(a2)
            w = w2
        for dy in range(h):
            for dx in range(w):
                a = alpha[dy*w + dx]
                i = ((y+dy)*W + (x+dx))*2
                buf[i:i+2] = pack(a)
        place[cp] = (x, y, w, h, int(round(y0 / ss)) + shift + y_off, int(round(y1 / ss)) + shift + y_off)
        rendered += 1
        x += w + 1
        rowh = max(rowh, h)
    fbod = bytearray()
    for cp in cps:
        src = inv.get(cp, cp)
        try:
            adv = int(round(font.getlength(chr(src)) / ss)) if cp >= 0x20 else 5
        except Exception:
            adv = 17
        adv = max(0, min(255, adv))
        p = place.get(cp)
        if not p:
            fbod += struct.pack("<H", cp) + bytes([0, adv, 0,0,0,0]) + struct.pack("<4f",0,0,0,0)
            continue
        px, py, w, h, t, b = p
        u1=(px+0.5)/W; u2=(px+w+0.5)/W; v1=(py+0.5)/H; v2=(py+h+0.5)/H
        wq = max(1, int(round(w / ss)))
        fbod += struct.pack("<H", cp) + bytes([0, adv, 0, min(wq,255), min(max(t,0),255), min(max(b,0),255)]) + struct.pack("<4f",u1,u2,v1,v2)
    fh_byte = HEAD[3] if fh_override == 0 else fh_override
    head = struct.pack("<H", len(cps)) + bytes([1, fh_byte]) + HEAD[4:6]
    out = bytearray()
    tex_seen = [0]
    for t, o, sz in parts(D, 0, len(D)):
        data = D[o:o+sz]
        if t == b"HEAD":
            data = head
        elif t == b"FBOD":
            data = bytes(fbod)
        elif t == b"FTEX":
            ft = bytearray()
            for t2, o2, sz2 in parts(D, o, o+sz):
                d2 = D[o2:o2+sz2]
                if t2 == b"tex_":
                    tex_seen[0] += 1
                    if tex_seen[0] > 1:
                        continue
                    tx = bytearray()
                    for t3, o3, sz3 in parts(D, o2, o2+sz2):
                        d3 = D[o3:o3+sz3]
                        if t3 == b"FMT_":
                            fm = bytearray()
                            for t4, o4, sz4 in parts(D, o3, o3+sz3):
                                d4 = D[o4:o4+sz4]
                                if t4 == b"INFO":
                                    d4 = bytes(INFO)
                                elif t4 == b"FACE":
                                    fc = bytearray()
                                    for t5, o5, sz5 in parts(D, o4, o4+sz4):
                                        d5 = D[o5:o5+sz5]
                                        if t5 == b"LVL_":
                                            lv = bytearray()
                                            for t6, o6, sz6 in parts(D, o5, o5+sz5):
                                                d6 = D[o6:o6+sz6]
                                                if t6 == b"INFO":
                                                    i2 = bytearray(d6)
                                                    if len(i2) >= 8:
                                                        struct.pack_into("<I", i2, 4, len(buf))
                                                    d6 = bytes(i2); sz6 = len(d6)
                                                elif t6 == b"BODY":
                                                    d6 = bytes(buf); sz6 = len(buf)
                                                lv += t6 + struct.pack("<I", sz6) + d6
                                                lv += b"\x00" * ((4 - (8+sz6) % 4) % 4)
                                            d5 = bytes(lv); sz5 = len(lv)
                                        fc += t5 + struct.pack("<I", sz5) + d5
                                        fc += b"\x00" * ((4 - (8+sz5) % 4) % 4)
                                    d4 = bytes(fc); sz4 = len(fc)
                                fm += t4 + struct.pack("<I", sz4) + d4
                                fm += b"\x00" * ((4 - (8+sz4) % 4) % 4)
                            d3 = bytes(fm); sz3 = len(fm)
                        tx += t3 + struct.pack("<I", sz3) + d3
                        tx += b"\x00" * ((4 - (8+sz3) % 4) % 4)
                    d2 = bytes(tx); sz2 = len(tx)
                ft += t2 + struct.pack("<I", sz2) + d2
                ft += b"\x00" * ((4 - (8+sz2) % 4) % 4)
            data = bytes(ft)
        sz = len(data)
        out += t + struct.pack("<I", sz) + data
        out += b"\x00" * ((4 - (8+sz) % 4) % 4)
    return out, rendered

LOAD_IDS = (2478589025, 1694151954)
LOAD_CHARS = []
for _id in LOAD_IDS:
    for c in (zh_map.get(_id) or ""):
        if c not in LOAD_CHARS:
            LOAD_CHARS.append(c)
LOAD_SLOT = {c: 0x0E00 + i for i, c in enumerate(LOAD_CHARS)}
EXTRA = []
print("load big glyphs:", len(EXTRA))

PLAN = {
    "gamefont_large":      (17, 1024, 1024, 0),
    "gamefont_medium":     (18, 1024, 1024, 0),
    "gamefont_small":      (21, 2048, 2048, 0),
    "gamefont_tiny":       (13, 1024, 1024, 0),
    "gamefont_super_tiny": (18, 1024, 1024, 0, 17),
}

L = lvl.load(BASE)
for i, ch in enumerate(L.chunks):
    if ch.type != b"font": continue
    nm = ch.data[8:8+struct.unpack_from("<I", ch.data, 4)[0]].rstrip(b"\x00").decode("ascii","replace")
    if nm not in PLAN: continue
    plan = PLAN[nm]
    size, W, H, yoff = plan[0], plan[1], plan[2], plan[3]
    fhov = plan[4] if len(plan) > 4 else 0
    print("building %-20s size=%d atlas=%dx%d yoff=%d fh=%s ..." % (nm, size, W, H, yoff, fhov or "native"), end=" ")
    data, rendered = cjkfont.build_font(ch.data, all_cps, size, W, H, wanted, inv, yoff, fhov, extra=EXTRA)
    print("rendered=%d chunk=%d" % (rendered, len(data)))
    L.chunks[i] = lvl.Chunk(b"font", len(data), bytes(data), b"")

# ---- Locl：写入改写后的文本（>=0x8000 换成代理码）----
total = 0
for ch in locl.find_locl_chunks(L):
    loc = locl.parse_chunk(ch)
    n = 0
    for e in loc.entries:
        zh = zh_map.get(int(e["id"]))
        if not zh: continue
        # 术语覆盖（必须在代理转换之前，否则 >=0x8000 的字会被引擎丢弃）
        conv = zh
        for k, v in (("出生点", "部署点"), ("出生", "部署")):
            if conv == k:
                conv = v
                break
        if int(e["id"]) in LOAD_IDS:
            conv = "".join(chr(sur.get(ord(c), ord(c))) for c in conv)
            conv = "\u0301" + conv + "\u0302"
        else:
            conv = "".join(chr(sur.get(ord(c), ord(c))) for c in conv)
            if int(e["id"]) in (3975694703, 3408044191):
                conv = "\u0305" + conv + "\u0306"
        if e["text"] != conv:
            e["text"] = conv
            e.pop("raw", None)
            n += 1
    if n:
        L.chunks[L.chunks.index(ch)] = locl.build_chunk(loc)
        total += n
print("locl entries rewritten:", total)

raw = L.serialize()
open(OUT, "wb").write(raw)
print("saved", OUT, len(raw))