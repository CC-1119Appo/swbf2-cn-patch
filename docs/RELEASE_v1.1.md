# 星球大战：前线 II 简体中文补丁 v1.1

首个完整版。为《Star Wars: Battlefront II (2005)》GOG / Steam 版及其社区 **Remaster 1.5r2** 模组提供完整简体中文支持。

## 本次包含

- **界面汉化**：菜单、设置、战役列表、HUD、任务提示、武器/单位名等 5000+ 条文案
- **中文字体重制**：基于 IBM Plex Sans SC（含加粗），覆盖原版 5 档字号 + Remaster 三套分辨率字体
- **过场动画字幕**：35 段剧情过场，两种形式可选（外挂 / 封装）
- **开场动画字幕**：旧共和国的陨落 / 帝国的崛起 两部开场同样支持
- **部署界面修复**：修正「单位数量」与「部署」按钮重叠
- **官方译名**：地点、人物、飞船等专有名词对齐星球大战中文网译名库（如 Kashyyyk → 卡希克、Mygeeto → 麦基托）
- **单文件安装器**：选目录 → 选版本 → 安装，兼容 Windows XP SP3 及以上

## 下载哪个

| 文件 | 适用 | 说明 |
|---|---|---|
| **SWBF2_汉化安装器_v1.1.exe** | 全部 | **推荐**，978 MB，自动检测版本、可选字幕形式 |
| SWBF2_汉化_Remaster_外挂字幕_v1.1.zip | 装了 Remaster 1.5r2 | 轻量，12.8 MB |
| SWBF2_汉化_Remaster_封装动画_v1.1.zip | 装了 Remaster 1.5r2 | 字幕烧录进影片，685.7 MB |
| SWBF2_汉化_原版_外挂字幕_v1.1.zip | 纯原版 1.1 | 轻量，7.7 MB |
| SWBF2_汉化_原版_封装动画_v1.1.zip | 纯原版 1.1 | 字幕烧录进影片，678.4 MB |
| SWBF2_汉化_完整源码_v1.1.zip | 开发者 | 源码 + 脚本 + 译文库 + 文档，11.4 MB |

> 外挂 / 封装的差别：外挂不修改影片，占用小，但**独占全屏下叠加层可能不可见**；封装把字幕烧进视频，任何显示模式都能看到。

## 安装

1. 运行 `SWBF2_汉化安装器_v1.1.exe`（未签名，SmartScreen 提示时选「更多信息 → 仍要运行」）
2. 选择游戏目录：`...\Star Wars - Battlefront 2\GameData`
3. 选择游戏版本（默认自动检测）与字幕形式
4. 安装完成后用游戏目录下的 **`SWBF2_CN.exe`** 启动

静默安装：

```bat
SWBF2_汉化安装器_v1.1.exe --silent --dir="E:\GOG Games\Star Wars - Battlefront 2\GameData" --edition=remaster --mode=overlay
```

`--edition`：`remaster` / `vanilla`；`--mode`：`overlay` / `embed`

## 已知限制

- 社区 **v1.3 / v1.5 Extension 扩展 mod** 的自定义菜单项（暂停菜单 Resume/Options/Minimize、Cheat Menu 等）仍是英文：这些是扩展脚本里的硬编码字符串，8 位编码放不下中文，无法通过翻译文件汉化；走 Locl 的条目（如「退出 Windows」）已中文化。
- 部分关卡载入页标题字号偏小（同一字符串被多处复用，无法单独放大）。
- 独占全屏下外挂字幕可能不可见，建议窗口化/无边框或使用封装版。

## 还原

- GOG Galaxy：右键游戏 → 管理 → **验证/修复文件**
- 或手动：恢复包内 `backup\` 的 `core.lvl` 与字体，删除 `cn_hook.dll`、`SWBF2_CN.exe`、`cn_subs\`

## 系统要求

| 项 | 要求 |
|---|---|
| 系统 | Windows XP SP3 及以上（安装器为 32 位，子系统 5.01，无 CRT 依赖） |
| 游戏 | GOG / Steam 版 1.1；Remaster 版需先安装 Remaster 1.5r2 |
| 空间 | 外挂约 130 MB；封装约 1 GB |

## 校验

各发布资产的 SHA-256 见仓库内 `docs/RELEASE_ASSETS.md`。

## 致谢

- 译名参考：[星球大战中文网译名库](http://starwarschina.com/glossary/)
- 字体：[IBM Plex Sans SC](https://github.com/IBM/plex)
- Remaster 模组：[GT]Anakin
- 工具：RAD Video Tools、ffmpeg、SWBF2 Mod Tools

## 免责声明

非官方粉丝汉化补丁，与 Lucasfilm / Disney / EA 无关。发布包**不包含任何游戏原始资源**，需自备游戏本体。STAR WARS 及相关名称、角色、标志版权归 Lucasfilm Ltd. 所有；本项目原创代码以 MIT 协议开源。
