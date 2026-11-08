#pragma once
#include <atomic>
#include <cstdint>

void ApplyFramelockPatch();
void ApplyAutolootPatch();
void ApplyIntroPatch();
void ApplyPressStartPatch();
void ApplyBorderlessPatch();
void InstallWindowHooks();
void ApplyFovPatch();
void ApplyAspectRatio(bool rebuild);
void ApplyCamResetPatch();
void ApplyCamAutorotatePatch();
void ApplyFullscreenHZPatch();
// Writes the chosen fullscreen refresh rate into the patches of ApplyFullscreenHZPatch.
void ApplyRefreshRate(bool switchMode = false);
void InstallPresentHook();
void ApplyDeathPenaltiesPatch();
void ApplyDragonrotPatch();

inline std::atomic_bool INITIALIZED = false;
inline uint8_t GAME_LOADING;
