# 星球大战：前线 II 简体中文补丁 v1.3.0

**不再需要启动器**：直接运行 `BattlefrontII.exe` 即可，原版与 Remaster 都能正常显示中文。

## 本次更新

- **移除 `SWBF2_CN.exe` / `SWBF2_CN_XP.exe`**：安装后无需任何启动器，直接双击游戏主程序
- **Remaster 版脱离钩子可用**：`core.lvl` 不再插入定位标记字符，中文字体与文本直接生效
- 安装包仍保留 `cn_hook.dll`、`cn_hook_xp.dll`（可选；不加载也不影响显示）
- 安装器：去掉「启动器」选择，界面标注“本版不含启动器”
- 还原原版时顺带清理旧启动器文件

## 下载

**SWBF2_CN_Installer_v1.3.0.exe**（约 1.0 GB，单文件安装器）

## 安装

1. 运行安装器 → 选择 `...\Star Wars - Battlefront 2\GameData`
2. 点「检测版本」→ 确认版本（Remaster / 原版）
3. 点「开始安装」
4. 直接运行 `BattlefrontII.exe`

## 还原

- 「还原原版」：内置原始 core/ingame/shell 覆盖回去
- 「还原备份」：还原安装时备份到 `_cn_backup` 的文件
- 影片用 Steam/GOG 的「验证文件」恢复

## 兼容性

- 原版（GOG 2018 / GOG 经典 / Steam）
- Remaster 1.5r2（GOG / Steam 通用）
- Windows XP SP3 及以上（安装器本身即为 XP 兼容构建）

## 已知限制

- 社区 v1.3/v1.5 Extension 的自定义菜单项为脚本硬编码，无法汉化
- 部分载入页标题字号偏小

## 免责声明

非官方粉丝汉化补丁，不含游戏原始资源（仅含用于还原的官方原文件）。STAR WARS 版权归 Lucasfilm Ltd.。
