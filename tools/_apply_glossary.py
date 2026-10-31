
# -*- coding: utf-8 -*-
import json, shutil, os
P = r"D:\Documents\DSH\swbf2_chs\workbook\workbook.zh.json"
shutil.copy2(P, P + ".bak_before_glossary")
wb = json.load(open(P, encoding="utf-8"))
REPL = [
    ("卡西克", "卡希克"),
    ("迈吉托", "麦基托"),
    ("奥德兰", "奥德朗"),
    ("太空港", "星港"),
    ("观测室", "观测站"),
    ("熔接器", "聚变切割器"),
    ("全息盘", "全息碟"),
    ("磁力守卫", "磁卫机器人"),
    ("双头光剑", "双剑"),
    ("蹲点者", "野营者"),
    ("欧米伽", "欧米加"),
    ("节流阀", "推进器"),
    ("气闸舱", "气闸"),
    ("雪地部队", "雪地士兵"),
    ("加莫雷安人", "加莫人"),
]
counts = {k: 0 for k, _ in REPL}
for e in wb:
    z = e.get("zh") or ""
    if not z: continue
    nz = z
    for a, b in REPL:
        if a in nz:
            counts[a] += nz.count(a)
            nz = nz.replace(a, b)
    e["zh"] = nz
json.dump(wb, open(P, "w", encoding="utf-8"), ensure_ascii=False, indent=1)
print("替换统计：")
for k, n in counts.items():
    print("   %-8s -> %-10s %d 处" % (k, dict(REPL)[k], n))
