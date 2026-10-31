# 开发文档 · 星球大战：前线 II（2005）汉化实现

> 本文记录逆向与实现细节，便于后续维护/移植（例如 Steam 版）。

## 1. 环境与目标

- 游戏：GOG 版 **1.1**（`BattlefrontII.exe` 4,608,000 字节，imagebase 0x38C000，运行时加载基址 **0x400000**）
- 加装：**Remaster 1.5r2**（`addon\Remaster\`）
- 关键点：`.text` 的 RVA 与文件偏移一致（VA = 0x400000 + 文件偏移）

## 2. LVL 容器

`ucfb` + u32(总长-8) + 若干 chunk：每个 chunk = 4 字节标签 + u32 长度 + 数据（4 字节对齐）。

常用标签：`font`、`Locl`、`scr_`、`tex_`、`NAME`、`HEAD`、`FBOD`、`FTEX`、`FMT_`、`FACE`、`LVL_`、`BODY`。

## 3. 字体格式

`font` chunk 结构：`NAME` + `HEAD`(6 字节) + `FTEX` + `FBOD`。

- HEAD：`u16 字形数`、`u8 页数`、`u8 fh`、`u16 保留`
- FBOD：每条 **24 字节** —— `u16 码位`、`u8 页`、`u8 字距`、`u8 box.l`、`u8 box.r`、`u8 box.t`、`u8 box.b`、`4×f32 UV`
- 纹理：`FMT_/INFO` = `u32 format`（21=A8R8G8B8，26=A4R4G4B4）、`u16 W`、`u16 H`；`LVL_/INFO` 第二个 u32 必须等于 BODY 字节数
- A4R4G4B4：每像素 2 字节，alpha 在**第二个字节的高 4 位**

### 码位映射（关键）

引擎会**丢弃 ≥0x8000 的码位**。因此构建时：

1. 统计所有译文用到的字符；
2. 对 ≥0x8000 的字符，按排序映射到代理码位 `0x0100 + i`；
3. 字体用**代理码位**写 FBOD，Locl 文本同步替换；
4. **重建字体时必须把代理码位反查回原汉字**再栅格化（否则画出来是拉丁字母）。

## 4. Locl 格式

每条：`u32 id`（字符串 key 的 FNV-1a 小写哈希，init 0x811C9DC5，prime 0x1000193）、`u16 字节数`、UTF-16LE 文本。

## 5. Remaster 字体

`addon\Remaster\Fonts\arialblack_{default,2k,4k}.lvl`，每个含 5 个字体：
`gamefont_large_rema / medium_rema / small_rema / tiny / super_tiny`。
三套按分辨率选择（1080p 用 2k）。

## 6. 运行期 Hook（cn_hook.dll）

### 6.1 字形查找内联 Hook

- 目标：`BattlefrontII.exe + 0x2B8530`（VA 0x6B8530），特征 `51 53 55 8B 6C 24 10 56`
- 方式：5 字节 jmp + 跳板（复制被覆盖字节，跳回 +5）
- 原型：`u32 __thiscall lookup(Font* self, u32 code)`，返回字形条目指针，`ret 4`

**标记机制**：在 Locl 文本首尾插入不可见字符，hook 据此识别目标文本并在返回的条目副本上做位移/缩放：

- `U+0301/U+0302`：载入页标题/描述
- `U+0305/U+0306`：部署界面「单位数量」

### 6.2 影片请求 Hook

- 目标：`ScriptCB_PlayInGameMovie` 实现 = VA **0x426A60**（Lua 注册表在 0x7F6858）
- 原型：`int f(lua_State* L)`，影片名 = Lua 参数 2
- 取字符串用的引擎函数：VA **0x70DD00**（`(L, idx, 0)`）
- 影片「是否在播放」：全局指针 `[0x1D77BF8]`（非空即播放中）

## 7. 影片容器（ingame.mvs）

- 头部 2048 字节，其中偏移 `12*4` 处是影片数量（38），随后 **每条记录 8 个 u32**：
  `[tag][0xF3C342E6][0x8D39BDE6][影片名FNV][0x83D03615][size][0x809608B6][pad]`
- 影片数据从 2048 开始，**每段起始 2048 对齐**；`pad = 2048 - (off+size) % 2048`
- 每段后跟 16 字节分隔记录：`0x809608B6`、`pad-16`、0、0，再补 0 到 pad
- 校验：`off += size + pad` 最终等于文件长度

### 替换流程（已验证稳定）

1. `ffmpeg` 把 SRT 烧进对应 mp4（字号 14、按标点断句）；
2. `RADVideo64.exe Binkc in out /D<码率>` 编码为 Bink 1；
3. 把 Bink 头 `size` 字段改成 `len-8`；
4. 写回原偏移，补分隔记录与 0 填充，**并同步更新头部记录的 size/pad**；
5. 影片总大小与后续偏移保持不变。

码率估算：`rate = block * 0.90 / duration`（block = 原 size + pad）。

## 8. 字幕叠加（可选方案）

- 分层透明窗口（`WS_EX_LAYERED|WS_EX_TRANSPARENT|WS_EX_TOPMOST`），GDI 绘制后 `UpdateLayeredWindow`
- 影片请求时按名字映射到 `cn_subs\ingame_<n>.srt`，检测到播放开始后按时间轴显示

## 9. 构建

```bat
call vcvarsall.bat x86
cl /nologo /O2 /utf-8 /LD cn_hook.cpp overlay.cpp /link /OUT:cn_hook.dll user32.lib gdi32.lib
cl /nologo /O2 /utf-8 launcher.cpp /link /OUT:SWBF2_CN.exe /SUBSYSTEM:WINDOWS user32.lib
```

字体重建：

```bat
python tools\build_gog_cjk2.py     # core.lvl
python tools\build_rema.py         # Remaster 三套字体
```

字幕封装：

```bat
python tools\_batch_embed.py       # 批量烧录+编码+替换 ingame.mvs
```

## 10. 目录说明

| 目录 | 内容 |
|---|---|
| `tools/` | 构建与处理脚本 |
| `src/` | DLL / 启动器源码 |
| `data/` | 译文库、影片名映射、旁白原文 |
| `fonts/` | IBM Plex Sans SC 字体 |
| `subtitles/` | 35 段过场 SRT（中英双语） |
