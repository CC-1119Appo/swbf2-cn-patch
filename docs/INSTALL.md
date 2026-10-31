# 安装说明

## 方式一：安装器（推荐）

1. 从 [Releases](../../releases) 下载 `SWBF2_汉化安装器_v1.1.3.3.exe`
2. 双击运行（未签名，SmartScreen 提示时选「更多信息 → 仍要运行」）
3. 选择游戏目录：`...\Star Wars - Battlefront 2\GameData`
   - 安装器会自动探测 GOG / Steam 常见路径
4. 选择游戏版本：
   - **自动检测（推荐）**：检测到 `GameData\addon\Remaster` 即按 Remaster 处理
   - 也可手动指定 `Remaster 1.5r2` 或 `原版 1.1`
5. 字幕已烧录进影片（过场 35 段 + 开场 2 部），无需额外设置
6. 点击「开始安装」，完成后用 `SWBF2_CN.exe` 启动

安装日志：`GameData\install_cn.log`

## 方式二：手动解压包

1. 下载对应版本 + 字幕形式的 zip
2. 解压后把 `files` 目录内容覆盖到 `GameData\`
3. 可选的 `optional\ingame.lvl`：复制到 `GameData\data\_lvl_pc\` 可修复部署界面重叠
   （已装 v1.3/v1.5 Extension 扩展 mod 的玩家**不要**覆盖）

## 还原

- GOG Galaxy：右键游戏 → 管理 → 验证/修复文件
- 或手动：恢复安装包 `backup\` 内的 `core.lvl` 与字体，删除 `cn_hook.dll`、`SWBF2_CN.exe`、`cn_subs\`

## 系统要求

| 项 | 要求 |
|---|---|
| 系统 | Windows XP SP3 及以上（安装器为 32 位，子系统 5.01） |
| 游戏 | GOG / Steam 版 1.1；Remaster 版需先装 Remaster 1.5r2 |
| 空间 | 约 1 GB（含影片） |