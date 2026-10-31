# 星球大战：前线 II 简体中文补丁 v1.2.0

字幕**全部烧录进影片**（过场 35 段 + 开场 2 部），GOG 与 Steam 表现一致，不依赖运行时钩子。

## 本次更新

- **彻底移除外挂字幕**：安装包不再含任何 `*.srt`，安装时自动清理旧版残留的 `cn_subs`
- **修复 Remaster 部署界面重叠**：Remaster 自带的 `ingame.lvl` 已打布局补丁（条件替换，装了 Extension 的保持原样）
- **安装器新增汉化状态检测**：检测结果里显示「汉化补丁：已安装 v1.2.0 remaster / 未安装」
- 安装器界面重排：版本检测（GOG 2018 / GOG 经典 / Steam + Remaster）、一键还原原版、还原备份
- 安装包精简：payload 由 95 项降至 17 项

## 下载哪个

| 文件 | 适用 |
|---|---|
| **SWBF2_汉化安装器_v1.2.0.exe** | 全部（推荐） |
| SWBF2_汉化_Remaster_封装动画_v1.2.0.zip | 已装 Remaster 1.5r2 |
| SWBF2_汉化_原版_封装动画_v1.2.0.zip | 纯原版 / Steam |

## 安装

运行安装器 → 选 `...\Star Wars - Battlefront 2\GameData` → 点「检测版本」→ 确认版本 → 「开始安装」→ 用 `SWBF2_CN.exe` 启动。

## 还原

- 「还原原版」：用内置原始 `core.lvl`/`ingame.lvl`/`shell.lvl`（modern / old 两族）覆盖回去
- 「还原备份」：还原安装时自动备份到 `_cn_backup` 的文件
- 影片请用 Steam/GOG 的「验证文件」恢复

## 已知限制

- 社区 v1.3/v1.5 Extension 的自定义菜单项为脚本硬编码，无法汉化
- 部分载入页标题字号偏小

## 免责声明

非官方粉丝汉化补丁，不含游戏原始资源（仅含用于还原的官方原文件）。STAR WARS 版权归 Lucasfilm Ltd.。
