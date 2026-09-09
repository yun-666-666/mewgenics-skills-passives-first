#include <windows.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <random>
#include <string>

#include "mewjector.h"

namespace {

constexpr char kOwner[] = "SkillsPassivesFirst";
constexpr int kMinimumSupportedLevel = 10;

// These signatures cover the stable function prologues and stop at the first
// relative call. Supported public and beta builds can therefore be located
// without trusting stale fixed RVAs.
constexpr unsigned char kLevelUpEntrySignature[] = {
    0x88, 0x54, 0x24, 0x10, 0x55, 0x53, 0x56, 0x57,
    0x41, 0x54, 0x41, 0x56, 0x41, 0x57, 0x48, 0x8D,
    0xAC, 0x24, 0x30, 0xFF, 0xFF, 0xFF, 0x48, 0x81,
    0xEC, 0xD0, 0x01, 0x00, 0x00, 0x48, 0x8B, 0xF1,
    0x4C, 0x8D, 0xB1, 0x60, 0x03, 0x00, 0x00, 0x49,
    0x8B, 0xCE, 0xE8,
};
constexpr unsigned char kRewardGeneratorSignature[] = {
    0x48, 0x89, 0x5C, 0x24, 0x08, 0x57, 0x48, 0x83,
    0xEC, 0x40, 0x48, 0x8B, 0xF9, 0x44, 0x89, 0x44,
    0x24, 0x60, 0x33, 0xC9, 0xC7, 0x44, 0x24, 0x2C,
    0x01, 0x00, 0x00, 0x00, 0x89, 0x4C, 0x24, 0x28,
    0x48, 0x8B, 0xDA, 0x48, 0x89, 0x4C, 0x24, 0x30,
    0xBA, 0x01, 0x00, 0x00, 0x00, 0x48, 0x8D, 0x4C,
    0x24, 0x28, 0xE8,
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

bool IsReadable(const void* pointer, size_t byteCount) {
    if (pointer == nullptr || byteCount == 0) {
        return false;
    }

    auto current = reinterpret_cast<uintptr_t>(pointer);
    if (byteCount > UINTPTR_MAX - current) {
        return false;
    }
    const uintptr_t end = current + byteCount;

    while (current < end) {
        MEMORY_BASIC_INFORMATION memory{};
        if (VirtualQuery(reinterpret_cast<const void*>(current), &memory, sizeof(memory)) == 0 ||
            memory.State != MEM_COMMIT ||
            (memory.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0) {
            return false;
        }

        const uintptr_t regionEnd =
            reinterpret_cast<uintptr_t>(memory.BaseAddress) + memory.RegionSize;
        if (regionEnd <= current) {
            return false;
        }
        current = regionEnd < end ? regionEnd : end;
    }
    return true;
}

bool FindUniqueExecutablePattern(
    UINT_PTR gameBase,
    const unsigned char* pattern,
    size_t patternSize,
    UINT_PTR& matchedRva) {
    if (gameBase == 0 || pattern == nullptr || patternSize == 0) {
        return false;
    }

    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(gameBase);
    if (!IsReadable(dos, sizeof(*dos)) || dos->e_magic != IMAGE_DOS_SIGNATURE ||
        dos->e_lfanew <= 0) {
        return false;
    }

    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(gameBase + dos->e_lfanew);
    if (!IsReadable(nt, sizeof(*nt)) || nt->Signature != IMAGE_NT_SIGNATURE ||
        nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC ||
        nt->FileHeader.NumberOfSections == 0 || nt->FileHeader.NumberOfSections > 96) {
        return false;
    }

    const size_t imageSize = nt->OptionalHeader.SizeOfImage;
    const auto* section = IMAGE_FIRST_SECTION(nt);
    if (!IsReadable(section, sizeof(*section) * nt->FileHeader.NumberOfSections)) {
        return false;
    }

    UINT_PTR foundRva = 0;
    unsigned int matchCount = 0;
    for (unsigned int sectionIndex = 0;
         sectionIndex < nt->FileHeader.NumberOfSections;
         ++sectionIndex, ++section) {
        if ((section->Characteristics & IMAGE_SCN_MEM_EXECUTE) == 0) {
            continue;
        }

        const size_t sectionStart = section->VirtualAddress;
        const size_t sectionSize = section->Misc.VirtualSize;
        if (sectionStart >= imageSize || sectionSize > imageSize - sectionStart ||
            sectionSize < patternSize) {
            continue;
        }

        const auto* bytes = reinterpret_cast<const unsigned char*>(gameBase + sectionStart);
        if (!IsReadable(bytes, sectionSize)) {
            continue;
        }
        for (size_t offset = 0; offset <= sectionSize - patternSize; ++offset) {
            if (std::memcmp(bytes + offset, pattern, patternSize) == 0) {
                foundRva = static_cast<UINT_PTR>(sectionStart + offset);
                if (++matchCount > 1) {
                    return false;
                }
            }
        }
    }

    if (matchCount != 1) {
        return false;
    }
    matchedRva = foundRva;
    return true;
}

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

int ReadCurrentLevel(void* levelUpContext) {
    if (levelUpContext == nullptr) {
        return 0;
    }
    auto* context = static_cast<unsigned char*>(levelUpContext);
    if (!IsReadable(context + 0xA0, sizeof(void*))) {
        return 0;
    }

    void* cat = *reinterpret_cast<void**>(context + 0xA0);
    if (cat == nullptr) {
        return 0;
    }
    auto* levelAddress = static_cast<unsigned char*>(cat) + 0xC30;
    if (!IsReadable(levelAddress, sizeof(int))) {
        return 0;
    }

    const int level = *reinterpret_cast<int*>(levelAddress);
    return level > 0 && level <= 1000 ? level : 0;
}

// The first two positions use the game's native mixed-upgrade request (kind 10).
// It tries both active-skill and passive-upgrade pools and therefore counts their
// remaining upgrades together. With two or more remaining upgrades, both slots
// are upgrades; with exactly one, the exhausted second mixed request uses the
// game's normal fallback, leaving the new active/passive and stat requests to make
// the requested one-upgrade / one-new-option / two-stat layout. This also covers
// passive upgrades such as Rat Style without hard-coding either category.
int NextLateLevelRewardKind(int originalRewardKind) {
    constexpr std::array<int, 4> kInitialLayout = {
        10, // Mixed active/passive upgrade.
        10, // Mixed active/passive upgrade.
        1,  // Third slot chooses between new active and passive below.
        7,  // Original +1 stat request.
    };
    const size_t slot = g_session.rewrittenRequestCount % kInitialLayout.size();
    ++g_session.rewrittenRequestCount;
    if (slot == 2) {
        thread_local std::mt19937 random{std::random_device{}()};
        // Kind 13 uses the native new-passive pool, not passive upgrades (9).
        return std::uniform_int_distribution<int>{0, 1}(random) == 0 ? 1 : 13;
    }
    if (g_session.reroll && !g_config.rerollKeepsPriority) {
        return originalRewardKind;
    }
    return kInitialLayout[slot];
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
        (isOfficialLateLevelRequest || isTestRequest);

    if (appliesToThisRequest) {
        selectedRewardKind = NextLateLevelRewardKind(rewardKind);
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
    if (gameBase == 0) {
        g_mj.Log(kOwner, "Could not resolve the Mewgenics.exe base address.");
        return 0;
    }

    UINT_PTR levelUpEntryRva = 0;
    UINT_PTR rewardGeneratorRva = 0;
    if (!FindUniqueExecutablePattern(
            gameBase,
            kLevelUpEntrySignature,
            std::size(kLevelUpEntrySignature),
            levelUpEntryRva) ||
        !FindUniqueExecutablePattern(
            gameBase,
            kRewardGeneratorSignature,
            std::size(kRewardGeneratorSignature),
            rewardGeneratorRva)) {
        g_mj.Log(kOwner,
                 "Unsupported game build: target signatures were missing or ambiguous; no hooks installed.");
        return 0;
    }

    // Install the passive reward hook first. If the entry hook cannot be
    // installed afterward, this hook remains a no-op because no session can
    // become active.
    void* rewardGeneratorTrampoline = nullptr;
    if (!g_mj.InstallHook(rewardGeneratorRva, 0,
                          reinterpret_cast<void*>(RewardGeneratorHook),
                          &rewardGeneratorTrampoline, 20, kOwner)) {
        g_mj.Log(kOwner, "Could not install the reward generator hook.");
        return 0;
    }
    g_originalRewardGenerator = reinterpret_cast<RewardGeneratorFn>(rewardGeneratorTrampoline);

    void* levelUpEntryTrampoline = nullptr;
    if (!g_mj.InstallHook(levelUpEntryRva, 0,
                          reinterpret_cast<void*>(LevelUpEntryHook),
                          &levelUpEntryTrampoline, 20, kOwner)) {
        g_mj.Log(kOwner,
                 "Could not install the level-up entry hook; reward hook remains inactive pass-through.");
        return 0;
    }
    g_originalLevelUpEntry = reinterpret_cast<LevelUpEntryFn>(levelUpEntryTrampoline);

    g_mj.Log(kOwner,
             "Loaded with signature-resolved hooks at entry=0x%llX reward=0x%llX; reroll persistence=%d.",
             static_cast<unsigned long long>(levelUpEntryRva),
             static_cast<unsigned long long>(rewardGeneratorRva),
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
