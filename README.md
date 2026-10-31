# 星球大战：前线 II（2005）简体中文补丁

> 为 GOG / Steam 版《Star Wars: Battlefront II (2005)》及其社区 **Remaster 1.5r2** 模组提供完整简体中文支持。

[![Version](https://img.shields.io/badge/version-v1.1-blue.svg)](#)
[![Platform](https://img.shields.io/badge/platform-Windows%20XP%2B-lightgrey.svg)](#)
[![License](https://img.shields.io/badge/license-MIT-green.svg)](LICENSE)

![封面](screenshots/cover-remaster.png)

## 特性

- **全界面汉化**：菜单、设置、战役列表、HUD、任务提示、武器/单位名等 5000+ 条文案
- **中文字体重制**：基于 IBM Plex Sans SC（含加粗），覆盖原版 5 档字号与 Remaster 三套分辨率字体
- **过场动画字幕**：35 段过场中文字幕，两种形式可选
  - *外挂*：SRT 叠加显示，包体小（约 13 MB）
  - *封装*：烧录进影片，任何显示模式可见（含 727 MB 影片）
- **开场动画字幕**：旧共和国的陨落 / 帝国的崛起 两部开场同样支持（外挂或封装）
- **部署界面修复**：修正「单位数量」与「部署」按钮重叠
- **官方译名**：地点、人物、飞船等专有名词对齐 starwarschina.com 译名库（如 Kashyyyk → 卡希克）
- **单文件安装器**：选目录 → 选版本 → 安装，兼容 Windows XP SP3 及以上

## 下载

前往 [Releases](../../releases) 页面下载：

| 文件 | 说明 |
|---|---|
| `SWBF2_汉化安装器_v1.1.exe` | **推荐**：单文件安装器，自动检测版本、可选字幕形式 |
| `SWBF2_汉化_Remaster_外挂字幕_v1.1.zip` | 手动安装包（Remaster，轻量） |
| `SWBF2_汉化_Remaster_封装动画_v1.1.zip` | 手动安装包（Remaster，字幕烧录） |
| `SWBF2_汉化_原版_外挂字幕_v1.1.zip` | 手动安装包（原版 1.1，轻量） |
| `SWBF2_汉化_原版_封装动画_v1.1.zip` | 手动安装包（原版 1.1，字幕烧录） |

## 快速开始

1. **确认游戏版本**
   - 装了 [Remaster 1.5r2](https://www.moddb.com/mods/star-wars-battlefront-ii-full-hd-interface) → 用 Remaster 版
   - 纯原版 1.1 → 用原版版
2. 运行安装器 → 选择 `...\Star Wars - Battlefront 2\GameData` → 开始安装
3. 以后用游戏目录下的 **`SWBF2_CN.exe`** 启动

### 静默安装（进阶）

```bat
SWBF2_汉化安装器_v1.1.exe --silent --dir="E:\GOG Games\Star Wars - Battlefront 2\GameData" --edition=remaster --mode=overlay
```

`--edition`：`remaster` / `vanilla`；`--mode`：`overlay` / `embed`

## 注意事项

- 安装器未签名，首次运行若提示「已保护你的电脑」→ 更多信息 → 仍要运行
- **独占全屏下叠加字幕可能不可见**，建议窗口化/无边框；或使用「封装动画」版
- 社区扩展 mod（v1.3/v1.5 Extension）的自定义菜单项为脚本硬编码字符串，**无法汉化**；走 Locl 的条目已中文化
- 还原：GOG Galaxy「验证/修复文件」，或删除 `cn_hook.dll`、`SWBF2_CN.exe` 并恢复备份的 `core.lvl`

## 从源码构建

见 [docs/INSTALL.md](docs/INSTALL.md) 与 [BUILD.md](BUILD.md)（若存在）。核心流程：

1. `src/build_hook.bat` → `cn_hook.dll` + `SWBF2_CN.exe`
2. `python tools/build_gog_cjk2.py`（Remaster）或 `build_vanilla_cjk.py`（原版）
3. `python tools/build_rema.py`（Remaster 字体）
4. `python tools/_batch_embed.py`（可选：影片封装）
5. `installer/build_xp.bat` → 打包 payload → 拼接为单文件安装器

逆向细节（容器格式、地址表、字体布局）见 [docs/DEV_NOTES.md](docs/DEV_NOTES.md)。

## 致谢

- 译名参考：[星球大战中文网译名库](http://starwarschina.com/glossary/)
- 字体：[IBM Plex Sans SC](https://github.com/IBM/plex)
- Remaster 模组：[GT]Anakin
- 工具：RAD Video Tools、ffmpeg、SWBF2 Mod Tools

## 免责声明

本项目为**非官方粉丝汉化补丁**，与 Lucasfilm / Disney / EA 无关。
仓库内**不包含任何游戏原始资源**（`core.lvl`、`*.mvs` 等均需用户自备游戏本体）。
《Star Wars》及相关名称、角色、标志的版权归 Lucasfilm Ltd. 所有。

代码部分以 [MIT](LICENSE) 协议开源。
