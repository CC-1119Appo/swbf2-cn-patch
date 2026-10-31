
# -*- coding: utf-8 -*-
import os, shutil
WS = r"D:\Documents\DSH\swbf2_chs"
STAGE = os.path.join(WS, "installer_stage")
if os.path.exists(STAGE):
    shutil.rmtree(STAGE)
os.makedirs(STAGE)

def copy_tree(src, dst_prefix):
    n = 0
    for dp, dn, fn in os.walk(src):
        for f in fn:
            sp = os.path.join(dp, f)
            rel = os.path.relpath(sp, src).replace("\\", "/")
            dp2 = os.path.join(STAGE, dst_prefix, rel)
            os.makedirs(os.path.dirname(dp2), exist_ok=True)
            shutil.copy2(sp, dp2)
            n += 1
    return n

# 公共文件（两版共用）
os.makedirs(os.path.join(STAGE, "S"), exist_ok=True)
shutil.copy2(os.path.join(WS, "tools", "cn_hook.dll"), os.path.join(STAGE, "S", "cn_hook.dll"))
shutil.copy2(os.path.join(WS, "tools", "SWBF2_CN.exe"), os.path.join(STAGE, "S", "SWBF2_CN.exe"))
print("shared dll/exe ok")

# Remaster 版专用
r = copy_tree(os.path.join(WS, "pkg_v10_remaster", "files"), "R")
print("remaster files:", r)
# 原版专用
v = copy_tree(os.path.join(WS, "pkg_v10_vanilla", "files"), "V")
print("vanilla files:", v)
# 影片（封装模式用）
os.makedirs(os.path.join(STAGE, "M"), exist_ok=True)
G = r"E:\GOG Games\Star Wars - Battlefront 2\GameData"
for name in ("crawl.mvs", "ingame.mvs"):
    shutil.copy2(os.path.join(G, "data", "_lvl_pc", "movies", name), os.path.join(STAGE, "M", name))
    print("movie:", name, os.path.getsize(os.path.join(G, "data", "_lvl_pc", "movies", name)))
total = 0
for dp, dn, fn in os.walk(STAGE):
    for f in fn:
        total += os.path.getsize(os.path.join(dp, f))
print("stage total: %.1f MB" % (total/1024/1024))
