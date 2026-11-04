# 星球大战：前线 II 简体中文包 v1.4.0

> 适用于 GOG / Steam 版《Star Wars: Battlefront II (2005)》，以及社区 **Remaster 1.5r2** 模组。

## 本版本说明

**v1.4.0 起不再附带启动器**：直接运行游戏目录下的 **BattlefrontII.exe** 即可，无需 SWBF2_CN.exe。
中文字幕已烧录进影片，中文字体直接内置在 core.lvl（Remaster 版还内置在 addon\Remaster\Fonts），
**不依赖任何运行时钩子**，原版与 Remaster 表现一致。

## 包含内容

- **界面汉化**：菜单、设置、战役列表、HUD、任务提示、武器/单位名等 5000+ 条
- **中文字体**：IBM Plex Sans SC（含加粗），覆盖原版 5 档字号 + Remaster 三套分辨率字体
- **过场动画字幕**：35 段剧情过场（内嵌，无需外挂）
- **开场动画字幕**：旧共和国的陨落 / 帝国的崛起（内嵌）
- **部署界面修复**：修正「单位数量」与「部署」按钮重叠
- **官方译名**：对齐星球大战中文网译名库（Kashyyyk → 卡希克、Mygeeto → 麦基托 等）

## 安装

1. 运行 **SWBF2_CN_Installer_v1.4.0.exe**（未签名，SmartScreen 选「更多信息 → 仍要运行」）
2. 选择游戏目录：
   - GOG：`...\Star Wars - Battlefront 2\GameData`
   - Steam：`...\steamapps\common\Star Wars Battlefront II Classic\GameData`
3. 点击「检测版本」，会显示：操作系统、平台（GOG 2018 / GOG 经典 / Steam）、Remaster 是否安装、汉化补丁是否已安装，并自动勾选建议的安装版本
4. 点击「开始安装」
5. 直接运行游戏目录下的 **BattlefrontII.exe**

安装日志：`GameData\install_cn.log`

### 静默安装

```bat
SWBF2_CN_Installer_v1.4.0.exe --silent --dir="E:\...\GameData" --edition=remaster
SWBF2_CN_Installer_v1.4.0.exe --detect --dir="E:\...\GameData"     # 只检测，写 detect.txt
```

## 关于 cn_hook.dll / cn_hook_xp.dll

安装包会按系统只放入其中一个：Vista 及以上装 cn_hook.dll，Windows XP 装 cn_hook_xp.dll。v1.4.0 不需要它们也能正常显示中文，可手动删除。

## 还原

| 按钮 | 作用 |
|---|---|
| **还原原版** | 用内置的原始 core.lvl / ingame.lvl / shell.lvl（modern / old 两族）覆盖回去 |
| **恢复先前游戏状态** | 还原安装时自动备份到 `GameData\_cn_backup\` 的文件 |
| 验证文件 | 影片（ingame.mvs / crawl.mvs）用 Steam/GOG 的「验证/修复文件」恢复 |

命令行：`--restore-orig` / `--restore`

## 版本识别规则

| 版本 | 主程序大小 | core.lvl | 原版文件族 |
|---|---|---|---|
| GOG 2018 修订 | 4,366,848 | 7,193,880 | modern |
| Steam | 4,357,632 | 7,193,880 | modern |
| GOG 经典版 | 4,608,000 | 7,192,668 | old |
| CD 版 1.0（2005 原始盘） | 7,790,592 | 7,164,480 | cd10 |

Remaster 检测：`GameData\addon\Remaster` 目录是否存在。

## 系统要求

| 项 | 要求 |
|---|---|
| 系统 | Windows XP SP3 及以上（无需额外运行库） |
| 游戏 | GOG / Steam 1.1；Remaster 版需先安装 Remaster 1.5r2 |
| 空间 | 约 1.0 GB（含影片与还原用原版文件） |

## 已知限制

- 社区 v1.3 / v1.5 Extension 的自定义菜单项（Resume / Minimize / Cheat Menu 等）为脚本硬编码，无法汉化
- 部分关卡载入页标题字号偏小（同一字符串被多处复用）

## 致谢

- 译名：星球大战中文网译名库（starwarschina.com/glossary）
- 字体：IBM Plex Sans SC（github.com/IBM/plex）
- Remaster 模组：[GT]Anakin
- 工具：RAD Video Tools、ffmpeg、SWBF2 Mod Tools

## 免责声明

非官方粉丝汉化补丁，与 Lucasfilm / Disney / EA 无关。发布包不包含游戏原始资源（仅含用于还原的官方原文件）。
STAR WARS 及相关名称、角色、标志版权归 Lucasfilm Ltd. 所有。
