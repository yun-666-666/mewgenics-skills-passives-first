# Skills & Passives First - 3 Rerolls

当前版本：**1.0.3**

一个用于 **Mewgenics** 的双组件 MOD：

- `SkillsPassivesFirst`（Mewjector DLL）改变 10 级及以上首次升级时的奖励候选；
- `SkillsPassivesFirstData`（Mewtator 数据 MOD）为普通玩家猫提供 3 次升级重掷。

## 当前状态

核心功能已经完成玩家实际验收：首次升级候选、三次重掷、重掷后恢复原版奖励池，以及基础职业、`Colorless` 和进阶职业覆盖均正常。

## 功能

原版在 10 级以后通常只提供属性奖励。本 MOD 只改变该等级段的**首次**升级候选：

| 猫咪当前可升级的主动技能与被动总数 | 首次升级界面 |
| --- | --- |
| 大于等于 2 | 2 个升级选项 + 1 个新主动技能 + 1 个属性 |
| 等于 1 | 1 个升级选项 + 1 个新主动技能 + 2 个属性 |
| 没有可升级项 | 使用游戏的原生回退奖励 |

“升级选项”使用游戏内置的主动技能/被动技能混合升级池，因此可能是两个主动、两个被动或各一个，取决于该猫仍可升级的内容。点击骰子重掷后，默认恢复原版 10 级以上属性奖励池。

数据 MOD 为 Fighter、Hunter、Mage、Medic、Tank、Thief、`Colorless` 和全部 7 个进阶职业添加原生 `AddLevelUpRerolls 3`。

## 安装

前置条件：

- Mewjector 3.4 或更新版本；
- Mewtator 0.5.1 或更新版本。

安装步骤：

1. 将 `SkillsPassivesFirst.dll` 和 `SkillsPassivesFirst.ini` 复制到 `<Mewgenics 游戏目录>/Mods/`。
2. 将完整的 `SkillsPassivesFirstData` 文件夹复制到 `<Mewgenics 游戏目录>/Mewtator/mods/SkillsPassivesFirstData/`。
3. 在 Mewtator 中启用 `SkillsPassivesFirstData`，并通过 Mewtator 启动游戏。
4. 首次启动后检查 `mod_logs/chainloader.log`，其中应有 `SkillsPassivesFirst` 的 `Loaded.` 记录。

直接从 Steam 启动仍可由 Mewjector 加载 DLL，但三次重掷依赖 Mewtator 数据 MOD。

## 配置

配置文件：`Mods/SkillsPassivesFirst.ini`

```ini
[General]
Enabled=1
RerollKeepsPriority=0
DebugLog=0
TestAtOrAboveLevel=0
```

- `Enabled=1`：启用 DLL。
- `RerollKeepsPriority=0`：重掷后恢复原版属性奖励池；设为 `1` 时继续使用优先布局。
- `DebugLog=1`：把奖励替换记录写入 Mewjector 日志。
- `TestAtOrAboveLevel=2`：仅用于快速测试；正常游玩前应恢复为 `0`。

编辑 INI 前请完全关闭游戏，修改后重新启动。

## 兼容性

支持当前 Steam 正式版；beta 使用同一组受验证的唯一机器码签名动态定位目标函数，不依赖容易失效的固定 RVA。

游戏更新后若目标签名不再唯一匹配，DLL 会拒绝安装原生钩子并在 `mod_logs/chainloader.log` 记录 `Unsupported game build`，不会猜测偏移继续运行。

不要与 **Solo Leveling** 同时启用：两者会修改同一条后期升级奖励生成路径。

## 从源码构建

需要安装 Visual Studio 2022 或 Build Tools 2022，并包含 x64 C++ 工具。脚本通过 `vswhere` 自动查找可用安装，不限定 Community 版本。

```powershell
powershell -ExecutionPolicy Bypass -File .\SkillsPassivesFirst\build.ps1
```

默认输出到 `build/Release/`。可通过 `-Configuration Debug` 或 `-OutputDirectory <目录>` 更改。

## 生成发布包

```powershell
powershell -ExecutionPolicy Bypass -File .\package.ps1
```

脚本会重新构建 DLL，并在 `dist/` 生成 `SkillsPassivesFirst-v<版本>-win-x64.zip`。压缩包已经包含与游戏目录对应的 `Mods/` 和 `Mewtator/mods/` 结构。

根目录的 `VERSION` 是版本号真源；打包时会校验数据 MOD 元数据与其一致。更新版本时同时维护 [CHANGELOG.md](CHANGELOG.md)。

## 许可证与致谢

本项目使用 [MIT License](LICENSE)。第三方 Mewjector API 头文件保留其自己的 MIT 许可证。

- 感谢 **GPT / OpenAI Codex** 协助需求梳理、原版行为分析、实现与文档整理。
- 感谢 **Mewjector** 与 **Mewtator** 提供 MOD 加载和数据合并能力。
- 本项目参考了 Nexus Mods 上 **Solo Leveling MOD** 公开可观察到的后期升级思路，但不包含或分发其文件与代码。
