# Skills & Passives First - 3 Rerolls

一个用于 **Mewgenics** 的双组件 MOD：

- `SkillsPassivesFirst`（DLL）：改变 **10 级及以上**首次升级时的四个奖励候选；
- `SkillsPassivesFirstData`（Mewtator 数据 MOD）：让每只普通玩家猫在升级界面获得 **3 次重掷**。

## 功能

原版在 10 级以后通常只给属性奖励。本 MOD 只改变该等级段的**首次**升级候选：

| 猫咪当前可升级的主动技能与被动总数 | 首次升级界面 |
| --- | --- |
| 大于等于 2 | 2 个升级选项 + 1 个新主动技能 + 1 个属性 |
| 等于 1 | 1 个升级选项 + 1 个新主动技能 + 2 个属性 |
| 没有可升级项 | 使用游戏的原生回退奖励 |

“升级选项”使用游戏内置的混合升级池，因此可来自主动技能或被动技能；例如 `老鼠流` 仍处于 1 级时也会被纳入升级候选。两项都来自主动、两项都来自被动，或一主动一被动都可以，取决于该猫实际还剩哪些可升级项。

点击骰子重掷后，四个候选会恢复为原版 10 级以上的普通属性池；不会持续强制技能/被动奖励。

`SkillsPassivesFirstData` 会给 Fighter、Hunter、Mage、Medic、Tank、Thief、Colorless 以及全部 7 个进阶职业添加原生 `AddLevelUpRerolls 3`，因此每只普通玩家猫都有 3 次升级重掷机会。

## 安装

### 前置条件

1. 已安装 **Mewjector 3.4 或更新版本**：游戏根目录（与 `Mewgenics.exe` 同级）需要有 Mewjector 的 `version.dll` 和 `chainloader.ini`。
2. 已安装 **Mewtator 0.5.1 或更新版本**：用于加载数据 MOD。
3. 当前 DLL 针对的游戏版本 SHA-256：
   `C3A41E436A93FA58CD386EC46DAD5C2A6F21A583D33C3A57A15A2604C726439E`。

### 步骤

1. 将 `SkillsPassivesFirst/dist/SkillsPassivesFirst.dll` 和
   `SkillsPassivesFirst/dist/SkillsPassivesFirst.ini` 复制到：

   ```text
   <Mewgenics 游戏目录>/Mods/
   ```

2. 将完整的 `SkillsPassivesFirstData` 文件夹复制到：

   ```text
   <Mewgenics 游戏目录>/Mewtator/mods/SkillsPassivesFirstData/
   ```

3. 打开 Mewtator，启用 `SkillsPassivesFirstData`，然后用 Mewtator 的 **Launch Game** 启动游戏。
4. 首次启动后检查：

   ```text
   <Mewgenics 游戏目录>/mod_logs/chainloader.log
   ```

   其中应有 `SkillsPassivesFirst` 的 `Loaded.` 记录。

DLL 可由 Steam 直接启动时的 Mewjector 加载；但 **3 次骰子**由 Mewtator 数据 MOD 提供，因此需要通过 Mewtator 启动（或自行正确配置其启动参数）。

## 配置

文件：`Mods/SkillsPassivesFirst.ini`

```ini
[General]
Enabled=1
RerollKeepsPriority=0
DebugLog=0
TestAtOrAboveLevel=0
```

- `Enabled=1`：启用 DLL；设为 `0` 可临时关闭。
- `RerollKeepsPriority=0`：推荐默认值。重掷后回到原版属性池。
- `DebugLog=1`：把奖励替换记录写入 `mod_logs/chainloader.log`，排查问题时使用。
- `TestAtOrAboveLevel=2`：仅用于快速测试。会从 2 级开始模拟 10 级以上的候选逻辑；测试后务必恢复为 `0`。

编辑 INI 前请先完全关闭游戏，然后重新启动。

## 快速验证

1. 在测试存档把 `DebugLog=1`、`TestAtOrAboveLevel=2`。
2. 使用任意新猫升到 2 级；界面应显示骰子 `x3`。
3. 检查日志：首次候选会显示两次 `7 -> 10`，随后是 `7 -> 1` 与 `7 -> 7`。
4. 点击一次骰子，确认新候选回到普通属性奖励。
5. 测试结束后把 `TestAtOrAboveLevel=0`。

## 兼容性与限制

- 不要与 **Solo Leveling** 同时启用：两者都会修改同一条后期升级奖励生成路径。
- DLL 启动时会校验两个目标函数的机器码签名。游戏更新后如果不匹配，它会写入 `Unsupported Mewgenics.exe build` 日志并自动不安装钩子，不会继续改写游戏内存。
- 这是面向单机 MOD 使用的项目；请在游戏更新后重新验证。

## 从源码构建

需要 Visual Studio 2022（含 x64 C++ 工具）。仓库已包含构建所需的 Mewjector API 头文件及其 MIT 许可证。

```powershell
powershell -ExecutionPolicy Bypass -File .\SkillsPassivesFirst\build.ps1
```

构建产物在 `SkillsPassivesFirst/dist/`。

## 致谢

- 感谢 **GPT / OpenAI Codex** 协助进行需求梳理、原版行为分析、实现与文档整理。
- 感谢 Nexus Mods 上的 [Solo Leveling MOD](https://www.nexusmods.com/mewgenics/mods/448)。本项目参考了其公开可观察到的后期升级 MOD 思路，用于理解兼容边界与测试方向；本项目不包含、复制或分发该 MOD 的文件或代码。
- 感谢 **Mewjector** 与 **Mewtator** 提供的 Mewgenics MOD 加载与数据合并能力。
