
# -*- coding: utf-8 -*-
"""cjkfont.py - 把 SWBF2 字体块重建为含中文的版本（支持额外大字形槽位）"""
import struct
from PIL import Image, ImageDraw, ImageFont

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
        if t == k:
            return (o, sz)
    return None

def build_font(D, cps, size, W, H, wanted, inv=None, y_off=0, fh_override=0, extra=None, log=None, name_override=None):
    """extra: list of (code, char, size_multiplier) —— 额外槽位，用更大字号渲染。"""
    inv = inv or {}
    extra = extra or []
    ho = get(D, 0, len(D), b"HEAD"); to = get(D, 0, len(D), b"FTEX")
    HEAD = D[ho[0]:ho[0]+ho[1]]
    tex = get(D, to[0], to[0]+to[1], b"tex_")
    fmt = get(D, tex[0], tex[0]+tex[1], b"FMT_")
    info = get(D, fmt[0], fmt[0]+fmt[1], b"INFO")
    INFO = bytearray(D[info[0]:info[0]+info[1]])
    struct.pack_into("<H", INFO, 4, W); struct.pack_into("<H", INFO, 6, H)

    fbo = get(D, 0, len(D), b"FBOD")
    fbd = D[fbo[0]:fbo[0]+fbo[1]]
    native_base = None
    for i in range(len(fbd)//24):
        e = fbd[i*24:(i+1)*24]
        if struct.unpack_from("<H", e, 0)[0] == 0x41:
            native_base = e[7]; break

    # 组织所有条目：code -> (char, px_size)
    items = {}
    for cp in cps:
        if cp not in wanted or cp < 0x20:
            continue
        items[cp] = (chr(inv.get(cp, cp)), size)
    for code, ch, mult in extra:
        items[code] = (ch, int(round(size * mult)))

    fonts = {}
    def font_for(px):
        if px not in fonts:
            fonts[px] = ImageFont.truetype(FONT_PATH, px, index=0)
        return fonts[px]

    BG = bytes([0xFF, 0x0F])
    buf = bytearray(BG * (W*H))
    def pack(a):
        a = max(0, min(255, int(a)))
        return bytes([0xFF, ((a >> 4) << 4) | 0x0F])

    place = {}
    x = 1; y = 1; rowh = 0
    for code in sorted(items):
        ch, px = items[code]
        f = font_for(px)
        asc, desc = f.getmetrics()
        try:
            bbox = f.getbbox(ch)
        except Exception:
            continue
        x0, y0, x1, y1 = bbox
        w = x1 - x0; h = y1 - y0
        if w <= 0 or h <= 0:
            continue
        if x + w + 1 > W:
            x = 1; y += rowh + 1; rowh = 0
        if y + h + 1 > H:
            continue
        img = Image.new("L", (w, h), 0)
        dd = ImageDraw.Draw(img)
        dd.text((-x0, -y0), ch, font=f, fill=255, anchor="la")
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
                    if xx > 0 and alpha[b0 + xx - 1] > v:
                        v = alpha[b0 + xx - 1]
                    a2[yy * w2 + xx] = v
            alpha = bytes(a2); w = w2
        for dy in range(h):
            for dx in range(w):
                a = alpha[dy*w + dx]
                i = ((y+dy)*W + (x+dx))*2
                buf[i:i+2] = pack(a)
        base = native_base if native_base is not None else asc
        shift = base - asc
        place[code] = (x, y, w, h, y0 + shift + y_off, y1 + shift + y_off, px)
        x += w + 1
        rowh = max(rowh, h)

    all_codes = sorted(set(cps) | set(it[0] for it in extra))
    fbod = bytearray()
    for code in all_codes:
        if code in items:
            ch, px = items[code]
            f = font_for(px)
            try:
                adv = int(round(f.getlength(ch)))
            except Exception:
                adv = px
        else:
            adv = size
        adv = max(0, min(255, adv))
        p = place.get(code)
        if not p:
            fbod += struct.pack("<H", code) + bytes([0, adv, 0, 0, 0, 0]) + struct.pack("<4f", 0, 0, 0, 0)
            continue
        px_, py_, w_, h_, t_, b_, _ = p
        u1 = (px_+0.5)/W; u2 = (px_+w_+0.5)/W; v1 = (py_+0.5)/H; v2 = (py_+h_+0.5)/H
        fbod += struct.pack("<H", code) + bytes([0, adv, 0, min(w_,255), min(max(t_,0),255), min(max(b_,0),255)]) + struct.pack("<4f", u1, u2, v1, v2)

    fh_byte = HEAD[3] if fh_override == 0 else fh_override
    head = struct.pack("<H", len(all_codes)) + bytes([1, fh_byte]) + HEAD[4:6]

    out = bytearray()
    tex_seen = [0]
    name_seen = [0]
    for t, o, sz in parts(D, 0, len(D)):
        data = D[o:o+sz]
        if t == b"NAME" and name_override:
            nm = name_override.encode("ascii") + b"\x00"
            data = struct.pack("<I", len(nm)) + nm
        elif t == b"HEAD":
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
    return bytes(out), len(place)

def pick_atlas(n, size, ss=1.0):
    need = n * (size*ss + 4) * (size*ss + 4) * 1.20
    for s in (512, 1024, 2048, 4096):
        if s * s >= need:
            return s, s
    return 4096, 4096