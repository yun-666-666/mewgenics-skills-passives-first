# Skills & Passives First Level-Ups

This DLL changes only late-level reward requests. At level 10 and above, the
initial reward screen uses this native layout instead of four `Single Stat`
requests:

- two skill/passive upgrade requests;
- one new active-skill request;
- one original `+1 Stat` request.

The game itself checks whether the cat has enough valid upgrade candidates. If
only one remains, its native fallback produces the requested one-upgrade layout
while the MOD's final two requests remain the original stat choices. Rerolls
are unchanged and therefore stay four original stat requests.

By default, only the first screen is remapped; rerolls return to the original
stat-only late-level pool. Leave `RerollKeepsPriority=0` to keep this behavior.

## Three rerolls for every cat

`../SkillsPassivesFirstData` is the companion Mewtator data MOD. It adds the
game's native `AddLevelUpRerolls 3` innate passive to all vanilla player
classes, including `Colorless`, so every ordinary player cat receives three
rerolls on its level-up screen. It is separate because the game needs
Mewtator's data-MOD loader for `.gon.merge` files; the DLL alone is loaded by
Mewjector.

## Quick test switch

For a disposable test save, set `TestAtOrAboveLevel=2` and `DebugLog=1` in
`SkillsPassivesFirst.ini`. The next level-up of a level-2-or-higher cat will
exercise the same remapping logic and write each replacement to
`mod_logs/chainloader.log`. This test override rewrites every reward request
at the configured level, so restore `TestAtOrAboveLevel=0` before normal play.

## Requirements

- Mewgenics build whose SHA-256 is
  `C3A41E436A93FA58CD386EC46DAD5C2A6F21A583D33C3A57A15A2604C726439E`
- Mewjector v3.4 or newer

The DLL checks the two game function signatures before installing. A mismatch
means it writes an `Unsupported Mewgenics.exe build` message and makes no game
memory changes.

## Install

1. Install Mewjector's `version.dll` and `chainloader.ini` next to
   `Mewgenics.exe`.
2. Copy `SkillsPassivesFirst.dll` and `SkillsPassivesFirst.ini` into the
   game `Mods` folder.
3. Install Mewtator 0.5.1 or newer. Put the complete
   `SkillsPassivesFirstData` folder in Mewtator's `mods` folder, enable it in
   Mewtator, and launch the game through Mewtator. This supplies the three
   rerolls per player cat.
4. Inspect `mod_logs/chainloader.log` for the `Loaded.` message from the DLL.

The game must be launched through Mewtator whenever the companion data MOD is
needed. Launching directly from Steam still loads the DLL (via Mewjector), but
does not apply the three-reroll data MOD unless you copy Mewtator's generated
launch options into Steam.

This MOD is not compatible with Solo Leveling because both alter the same
late-level reward-generation path. Do not load both together.
