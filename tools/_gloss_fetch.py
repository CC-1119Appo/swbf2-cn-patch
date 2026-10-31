
# -*- coding: utf-8 -*-
import json, re, urllib.request, urllib.parse, os, time, threading
from concurrent.futures import ThreadPoolExecutor

wb = json.load(open(r"D:\Documents\DSH\swbf2_chs\workbook\workbook.zh.json", encoding="utf-8"))
cands = set()
for e in wb:
    en = e.get("en") or ""
    for m in re.finditer(r"\b([A-Z][a-zA-Z'\-]{2,})\b", en):
        cands.add(m.group(1))
cands = sorted(cands)
print("candidates:", len(cands))

URL = "http://starwarschina.com/glossary/"
ROW = re.compile(r"<a[^>]*>([^<]+)</a></td><td>([^<]*)</td><td>([^<]*)</td>")
lock = threading.Lock()
out = {}

def fetch(word):
    data = urllib.parse.urlencode({"keyword": word, "category": "", "pgno": "0", "tijiao": "搜索"}).encode()
    try:
        req = urllib.request.Request(URL, data=data, headers={"User-Agent": "Mozilla/5.0"})
        html = urllib.request.urlopen(req, timeout=25).read().decode("utf-8", "replace")
    except Exception as e:
        return word, []
    rows = ROW.findall(html)
    res = [(en.strip(), zh.strip(), cat.strip()) for en, zh, cat in rows if en.strip()]
    return word, res

t0 = time.time()
with ThreadPoolExecutor(max_workers=6) as ex:
    for i, (w, res) in enumerate(ex.map(fetch, cands), 1):
        if res:
            out[w] = res
        if i % 200 == 0:
            print("  %d/%d  hits=%d  %.0fs" % (i, len(cands), len(out), time.time()-t0), flush=True)

json.dump(out, open(r"D:\Documents\DSH\swbf2_chs\glossary_hits.json", "w", encoding="utf-8"), ensure_ascii=False, indent=1)
print("done. words with hits:", len(out), "elapsed %.0fs" % (time.time()-t0))
