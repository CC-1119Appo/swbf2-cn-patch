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
- **单文件安装器**：选目录 → 选版本 → 安装，兼容 Windows XP SP3 及以上

> **v1.1.3 起只提供「封装动画」版本**：字幕直接烧在影片里，GOG 与 Steam 表现一致，不依赖运行时钩子。

## 下载

前往 [Releases](../../releases) 页面：

| 文件 | 说明 |
|---|---|
| SWBF2_汉化安装器_v1.1.3.exe | **推荐**：单文件安装器，自动检测版本 |
| SWBF2_汉化_Remaster_封装动画_v1.1.3.zip | 手动包（Remaster） |
| SWBF2_汉化_原版_封装动画_v1.1.3.zip | 手动包（原版 / Steam） |

## 快速开始

1. 运行安装器 → 选择 `...\Star Wars - Battlefront 2\GameData`
2. 选择游戏版本（自动检测 / Remaster / 原版）
3. 安装完成后用 **`SWBF2_CN.exe`** 启动

静默安装：

```bat
SWBF2_汉化安装器_v1.1.3.exe --silent --dir="E:\GOG Games\Star Wars - Battlefront 2\GameData" --edition=remaster
```

## 从源码构建

见 `BUILD.md` 与 `docs/DEV_NOTES.md`。

## 致谢

- 译名：[星球大战中文网译名库](http://starwarschina.com/glossary/)
- 字体：[IBM Plex Sans SC](https://github.com/IBM/plex)
- Remaster 模组：[GT]Anakin
- 工具：RAD Video Tools、ffmpeg、SWBF2 Mod Tools

## 免责声明

非官方粉丝汉化补丁，与 Lucasfilm / Disney / EA 无关。仓库不包含游戏原始资源。STAR WARS 及相关版权归 Lucasfilm Ltd.。代码以 MIT 开源。
