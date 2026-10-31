# 星球大战：前线 II（2005）简体中文补丁

> 为 GOG / Steam 版《Star Wars: Battlefront II (2005)》及其社区 **Remaster 1.5r2** 模组提供完整简体中文支持。

![封面](screenshots/cover-remaster.png)

## 特性

- **全界面汉化**：菜单、设置、战役列表、HUD、任务提示、武器/单位名等 5000+ 条
- **中文字体重制**：IBM Plex Sans SC（含加粗），覆盖原版 5 档字号 + Remaster 三套分辨率字体
- **过场动画字幕**：35 段剧情过场，**字幕烧录进影片**
- **开场动画字幕**：旧共和国的陨落 / 帝国的崛起，同样内嵌
- **部署界面修复**：修正「单位数量」与「部署」按钮重叠
- **官方译名**：对齐星球大战中文网译名库（Kashyyyk → 卡希克、Mygeeto → 麦基托 等）
- **单文件安装器**：版本检测 + 可选安装 + 一键还原，兼容 Windows XP SP3 及以上

> **v1.2.0 起只提供「封装动画」版本**：字幕直接烧在影片里，GOG 与 Steam 表现一致，不依赖运行时钩子。

## 下载

前往 [Releases](../../releases) 页面：

| 文件 | 说明 |
|---|---|
| SWBF2_汉化安装器_v1.2.0.exe | **推荐**：单文件安装器（含版本检测与一键还原） |
| SWBF2_汉化_Remaster_封装动画_v1.2.0.zip | 手动包（Remaster） |
| SWBF2_汉化_原版_封装动画_v1.2.0.zip | 手动包（原版 / Steam） |

## 安装

1. 运行安装器 → 选择 `...\Star Wars - Battlefront 2\GameData`（Steam 为 `...\Star Wars Battlefront II Classic\GameData`）
2. 点击 **「检测版本」**，会显示平台、主程序大小、**Remaster 是否已安装**，并自动勾选建议的安装版本（可手动改）
3. 点击「开始安装」，完成后用 **`SWBF2_CN.exe`** 启动

### 版本识别规则

| 版本 | 主程序大小 | core.lvl | 原版文件族 |
|---|---|---|---|
| GOG 2018 修订 | 4,366,848 | 7,193,880 | modern |
| Steam | 4,357,632 | 7,193,880 | modern |
| GOG 经典版 | 4,608,000 | 7,192,668 | old |

Remaster 检测：`GameData\addon\Remaster` 目录是否存在。

### 静默安装

```bat
SWBF2_汉化安装器_v1.2.0.exe --silent --dir="E:\...\GameData" --edition=remaster
SWBF2_汉化安装器_v1.2.0.exe --detect --dir="E:\...\GameData"        # 只检测，写 detect.txt
```

## 还原

| 方式 | 作用 |
|---|---|
| **还原原版**（安装器按钮 / `--restore-orig`） | 用内置的原始 `core.lvl`/`ingame.lvl`/`shell.lvl`（modern / old 两族）覆盖回去，并删除 `cn_hook.dll`、`SWBF2_CN.exe` |
| **还原备份**（安装器按钮 / `--restore`） | 还原安装时自动备份到 `GameData\_cn_backup\` 的文件 |
| Steam/GOG 验证文件 | 影片（`ingame.mvs` / `crawl.mvs`）用平台自带功能恢复 |

## 从源码构建

见 `BUILD.md` 与 `docs/DEV_NOTES.md`。

## 致谢

- 译名：[星球大战中文网译名库](http://starwarschina.com/glossary/)
- 字体：[IBM Plex Sans SC](https://github.com/IBM/plex)
- Remaster 模组：[GT]Anakin
- 工具：RAD Video Tools、ffmpeg、SWBF2 Mod Tools

## 免责声明

非官方粉丝汉化补丁，与 Lucasfilm / Disney / EA 无关。仓库不包含游戏原始资源。STAR WARS 及相关版权归 Lucasfilm Ltd.。代码以 MIT 开源。
