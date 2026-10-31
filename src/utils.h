#pragma once
#define MINI_CASE_SENSITIVE

#include "mini/ini.h"
#include <windows.h>
#include <algorithm>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <functional>
#include <string>
#include <type_traits>
#include <vector>

enum class LogLevel
{
  Info,
  Error,
  Success
};

HWND GetGameWindow();
bool IsValidReadPtr(uintptr_t addr, size_t size);
bool WriteProtectedMemory(uintptr_t address, const void *data, size_t size, const char *msg);
uintptr_t GetAbsoluteAddress(uintptr_t offset);
uintptr_t DereferenceStaticX64Pointer(uintptr_t instructionAddress, int instructionLength);
bool CodeReady(LPVOID target, const uint8_t *expected, size_t size);
uintptr_t AllocateMemoryNearAddress(uintptr_t target, size_t size, bool executable = false);
uintptr_t InjectShellcodeWithJumpBack(uintptr_t targetAddr, const uint8_t *shellcode, size_t shellcodeSize, size_t patchSize, const char *msg);
std::vector<uint8_t> RelJmpBytes(int length, int32_t relative);
void RefreshIniValues();
void LogMessage(LogLevel level, bool showMessage, const char *err, ...);
void SetupD3DCompilerProxy();
void InstallCrashLogger();
// Windows version, screens, GPUs, settings and the DLLs loaded next to the game, for bug reports
void LogEnvironment();

inline mINI::INIFile ini("SekiroFPSUnlockAndMoreNative.ini");
inline mINI::INIStructure config;

template <typename T>
T GetIniValue(const char *str)
{
  if constexpr (std::is_same_v<T, int>)
    return std::stoi(config["Settings"][str]);
  else if constexpr (std::is_same_v<T, float>)
    return std::stof(config["Settings"][str]);
}

template <typename T>
void SetIniValue(const char *str, T value)
{
  config["Settings"][str] = std::to_string(value);
  ini.write(config);
}

inline HWND GAME_WINDOW = nullptr;

// INI SETTINGS
inline bool FULLSCREEN_STATE;
inline int FPS_LIMIT;
inline int REFRESH_RATE; // fullscreen refresh rate in hz, 0 = left to the driver
inline int FOV_VALUE;
inline int FPSUNLOCK_ENABLED;
inline int AUTOLOOT_ENABLED;
inline int INTROSKIP_ENABLED;
inline int BORDERLESS_ENABLED;
inline int WINDOWED_HDR_ENABLED;
inline int ASPECT_RATIO_MODE;
// HUD Layout (ini StretchHud): kept at 16:9, stretched over the whole picture, or its elements at the picture edges
enum HudLayout
{
  HUD_16_9 = 0,
  HUD_STRETCH = 1,
  HUD_EDGES = 2,
};
inline int HUD_LAYOUT;
inline int FOV_ENABLED;
inline int DISABLE_CAMRESET_ENABLED;
inline int DISABLE_CAMERA_AUTOROTATE_ENABLED;
inline int VSYNC_ENABLED;
inline int DISABLE_DEATH_PENALTIES_ENABLED;
inline int DISABLE_DRAGONROT_ENABLED;
// the game's sound volumes (0-10, -1 = not known yet), remembered for the next start
inline int SOUND_VOLUME_MUSIC;
inline int SOUND_VOLUME_EFFECTS;
inline int SOUND_VOLUME_VOICE;
