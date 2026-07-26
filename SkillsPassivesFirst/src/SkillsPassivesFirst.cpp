#include <windows.h>

#include <array>
#include <cstring>
#include <string>

#include "mewjector.h"

namespace {

constexpr char kOwner[] = "SkillsPassivesFirst";
constexpr UINT_PTR kLevelUpEntryRva = 0x383880;
constexpr UINT_PTR kRewardGeneratorRva = 0x37F7E0;
constexpr int kMinimumSupportedLevel = 10;

// These signatures are for Mewgenics.exe SHA-256
// C3A41E436A93FA58CD386EC46DAD5C2A6F21A583D33C3A57A15A2604C726439E.
constexpr std::array<unsigned char, 16> kLevelUpEntrySignature = {
    0x88, 0x54, 0x24, 0x10, 0x55, 0x53, 0x56, 0x57,
    0x41, 0x54, 0x41, 0x56, 0x41, 0x57, 0x48, 0x8D,
};
constexpr std::array<unsigned char, 16> kRewardGeneratorSignature = {
    0x48, 0x89, 0x5C, 0x24, 0x08, 0x57, 0x48, 0x83,
    0xEC, 0x40, 0x48, 0x8B, 0xF9, 0x44, 0x89, 0x44,
};

struct Config {
    bool enabled = true;
    bool rerollKeepsPriority = false;
    bool debugLog = false;
    int testAtOrAboveLevel = 0;
};

struct LevelUpSession {
    bool active = false;
    bool reroll = false;
    int level = 0;
    unsigned int rewrittenRequestCount = 0;
};

using LevelUpEntryFn = void(__fastcall*)(void* levelUpContext, unsigned char reroll);
using RewardGeneratorFn = void*(__fastcall*)(void* levelUpContext, void* output, int rewardKind);

HMODULE g_module = nullptr;
MewjectorAPI g_mj{};
Config g_config{};
LevelUpEntryFn g_originalLevelUpEntry = nullptr;
RewardGeneratorFn g_originalRewardGenerator = nullptr;
thread_local LevelUpSession g_session{};

std::string GetModuleDirectory() {
    char buffer[MAX_PATH]{};
    const DWORD length = GetModuleFileNameA(g_module, buffer, static_cast<DWORD>(sizeof(buffer)));
    if (length == 0 || length >= sizeof(buffer)) {
        return {};
    }

    std::string path(buffer, length);
    const size_t separator = path.find_last_of("\\/");
    return separator == std::string::npos ? std::string{} : path.substr(0, separator + 1);
}

void LoadConfig() {
    const std::string iniPath = GetModuleDirectory() + "SkillsPassivesFirst.ini";
    g_config.enabled = GetPrivateProfileIntA("General", "Enabled", 1, iniPath.c_str()) != 0;
    g_config.rerollKeepsPriority =
        GetPrivateProfileIntA("General", "RerollKeepsPriority", 0, iniPath.c_str()) != 0;
    g_config.debugLog = GetPrivateProfileIntA("General", "DebugLog", 0, iniPath.c_str()) != 0;
    g_config.testAtOrAboveLevel = static_cast<int>(
        GetPrivateProfileIntA("General", "TestAtOrAboveLevel", 0, iniPath.c_str()));
}

bool MatchesSignature(UINT_PTR gameBase, UINT_PTR rva, const unsigned char* expected, size_t length) {
    return std::memcmp(reinterpret_cast<const void*>(gameBase + rva), expected, length) == 0;
}

int ReadCurrentLevel(void* levelUpContext) {
    if (levelUpContext == nullptr) {
        return 0;
    }

    auto* context = static_cast<unsigned char*>(levelUpContext);
    void* cat = *reinterpret_cast<void**>(context + 0xA0);
    if (cat == nullptr) {
        return 0;
    }

    return *reinterpret_cast<int*>(static_cast<unsigned char*>(cat) + 0xC30);
}

// The first two positions use the game's native mixed-upgrade request (kind 10).
// It tries both active-skill and passive-upgrade pools and therefore counts their
// remaining upgrades together. With two or more remaining upgrades, both slots
// are upgrades; with exactly one, the exhausted second mixed request uses the
// game's normal fallback, leaving the fixed new-skill and stat requests to make
// the requested one-upgrade / one-skill / two-stat layout. This also covers
// passive upgrades such as Rat Style without hard-coding either category.
int NextInitialLateLevelRewardKind() {
    constexpr std::array<int, 4> kInitialLayout = {
        10, // Mixed active/passive upgrade.
        10, // Mixed active/passive upgrade.
        1,  // New active skill (vanilla level-5 template).
        7,  // Original +1 stat request.
    };
    const int kind = kInitialLayout[g_session.rewrittenRequestCount % kInitialLayout.size()];
    ++g_session.rewrittenRequestCount;
    return kind;
}

void __fastcall LevelUpEntryHook(void* levelUpContext, unsigned char reroll) {
    if (g_originalLevelUpEntry == nullptr) {
        return;
    }

    const LevelUpSession previousSession = g_session;
    g_session.active = true;
    g_session.reroll = reroll != 0;
    g_session.level = ReadCurrentLevel(levelUpContext);
    g_session.rewrittenRequestCount = 0;

    const bool isTestLevel = g_config.testAtOrAboveLevel > 0 &&
        g_session.level >= g_config.testAtOrAboveLevel;
    if (g_config.debugLog && (g_session.level >= kMinimumSupportedLevel || isTestLevel)) {
        g_mj.Log(kOwner, "Level %d %s: processing %s reward requests.",
                 g_session.level, g_session.reroll ? "reroll" : "initial",
                 isTestLevel ? "TEST" : "late-level");
    }

    g_originalLevelUpEntry(levelUpContext, reroll);
    g_session = previousSession;
}

void* __fastcall RewardGeneratorHook(void* levelUpContext, void* output, int rewardKind) {
    int selectedRewardKind = rewardKind;
    const bool isOfficialLateLevelRequest =
        g_session.level >= kMinimumSupportedLevel && rewardKind == 7;
    // This opt-in test mode intentionally rewrites every reward request so a
    // low-level cat can exercise the exact remapping path without a long run.
    const bool isTestRequest =
        g_config.testAtOrAboveLevel > 0 &&
        g_session.level >= g_config.testAtOrAboveLevel;
    const bool appliesToThisRequest =
        g_config.enabled &&
        g_session.active &&
        (isOfficialLateLevelRequest || isTestRequest) &&
        (g_config.rerollKeepsPriority || !g_session.reroll);

    if (appliesToThisRequest) {
        selectedRewardKind = NextInitialLateLevelRewardKind();
        if (g_config.debugLog) {
            g_mj.Log(kOwner, "Level %d %s%s: reward kind %d -> %d.",
                     g_session.level, g_session.reroll ? "reroll" : "initial",
                     isTestRequest ? " TEST" : "",
                     rewardKind, selectedRewardKind);
        }
    }

    return g_originalRewardGenerator != nullptr
        ? g_originalRewardGenerator(levelUpContext, output, selectedRewardKind)
        : nullptr;
}

DWORD WINAPI Initialize(void*) {
    if (!MJ_Require(kOwner) || !MJ_Resolve(&g_mj)) {
        return 0;
    }

    LoadConfig();
    const UINT_PTR gameBase = g_mj.GetGameBase();
    if (gameBase == 0 ||
        !MatchesSignature(gameBase, kLevelUpEntryRva,
                          kLevelUpEntrySignature.data(), kLevelUpEntrySignature.size()) ||
        !MatchesSignature(gameBase, kRewardGeneratorRva,
                          kRewardGeneratorSignature.data(), kRewardGeneratorSignature.size())) {
        g_mj.Log(kOwner, "Unsupported Mewgenics.exe build: hooks were not installed.");
        return 0;
    }

    void* levelUpEntryTrampoline = nullptr;
    if (!g_mj.InstallHook(kLevelUpEntryRva, 0,
                          reinterpret_cast<void*>(LevelUpEntryHook),
                          &levelUpEntryTrampoline, 20, kOwner)) {
        g_mj.Log(kOwner, "Could not install the level-up entry hook.");
        return 0;
    }
    g_originalLevelUpEntry = reinterpret_cast<LevelUpEntryFn>(levelUpEntryTrampoline);

    void* rewardGeneratorTrampoline = nullptr;
    if (!g_mj.InstallHook(kRewardGeneratorRva, 0,
                          reinterpret_cast<void*>(RewardGeneratorHook),
                          &rewardGeneratorTrampoline, 20, kOwner)) {
        g_mj.Log(kOwner, "Could not install the reward generator hook.");
        return 0;
    }
    g_originalRewardGenerator = reinterpret_cast<RewardGeneratorFn>(rewardGeneratorTrampoline);

    g_mj.Log(kOwner, "Loaded. Late levels use native mixed upgrades; reroll persistence=%d.",
             g_config.rerollKeepsPriority ? 1 : 0);
    return 0;
}

} // namespace

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_module = module;
        DisableThreadLibraryCalls(module);
        HANDLE thread = CreateThread(nullptr, 0, Initialize, nullptr, 0, nullptr);
        if (thread != nullptr) {
            CloseHandle(thread);
        }
    }
    return TRUE;
}
