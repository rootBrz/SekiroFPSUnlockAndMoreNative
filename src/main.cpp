#include "MinHook.h"
#include "fixes.h"
#include "hdr.h"
#include "menu.h"
#include "offset.h"
#include "patches.h"
#include "sound.h"
#include "utils.h"

#include <cstring>

typedef long(WINAPI *changeDisplaySettingsExW)(LPCWSTR lpszDeviceName, DEVMODEW *lpDevMode, HWND hwnd, DWORD dwflags, LPVOID lParam);
typedef uint64_t (*presentInterval)(void *ctrl, unsigned type, void *a3, void *a4);

static changeDisplaySettingsExW fnChangeDisplaySettingsExW;
static presentInterval fnPresentInterval;

// whether the display has a mode of this size at this refresh rate
static bool HasMode(LPCWSTR device, DWORD width, DWORD height, DWORD rate)
{
  DEVMODEW dm = {};
  dm.dmSize = sizeof(dm);
  for (DWORD i = 0; EnumDisplaySettingsW(device, i, &dm); i++)
    if (dm.dmPelsWidth == width && dm.dmPelsHeight == height && dm.dmDisplayFrequency == rate)
      return true;
  return false;
}

// Removes the 60hz refresh rate lock: the chosen refresh rate when the display has it for the size, otherwise none to
// leave it to the driver
static long WINAPI DetourChangeDisplaySettingsExW(LPCWSTR lpszDeviceName, DEVMODEW *lpDevMode, HWND hwnd, DWORD dwflags, LPVOID lParam)
{
  if (lpDevMode != nullptr && REFRESH_RATE > 0 && HasMode(lpszDeviceName, lpDevMode->dmPelsWidth, lpDevMode->dmPelsHeight, REFRESH_RATE))
  {
    lpDevMode->dmDisplayFrequency = REFRESH_RATE;
    lpDevMode->dmFields |= DM_DISPLAYFREQUENCY;
  }
  else if (lpDevMode != nullptr)
    lpDevMode->dmFields &= ~DM_DISPLAYFREQUENCY;

  return fnChangeDisplaySettingsExW(lpszDeviceName, lpDevMode, hwnd, dwflags, lParam);
}

// The game's own once-per-frame swap chain call; its result is the DXGI sync interval passed to Present.
static uint64_t DetourPresentInterval(void *ctrl, unsigned type, void *a3, void *a4)
{
  static uint8_t lastLoadingStatus;
  uint64_t interval = fnPresentInterval(ctrl, type, a3, a4);

  uintptr_t window = *(uintptr_t *)GetAbsoluteAddress(window_singleton_offset);
  FULLSCREEN_STATE = IsValidReadPtr(window + 128, sizeof(int)) && *(int *)(window + 128) == 1;

  GAME_LOADING = *reinterpret_cast<uint8_t *>(is_game_in_loading_addr);
  if (lastLoadingStatus != GAME_LOADING)
  {
    lastLoadingStatus = GAME_LOADING;
    ApplyFramelockPatch();
  }

  MenuFrame();
  UpdateFixes();
  UpdateSoundVolumes();
  // The game restyles and resizes the window when screen mode or resolution changes; keep borderless.
  if (BORDERLESS_ENABLED && !FULLSCREEN_STATE)
    ApplyBorderlessPatch();

  return GAME_LOADING || !VSYNC_ENABLED ? 0 : (interval ? interval : 1);
}

static void InitHooks()
{
  InstallPresentHook();
  HMODULE hUser32 = GetModuleHandleA("user32.dll");
  LPVOID pChangeDisplaySettingsExW = (LPVOID)GetProcAddress(hUser32, "ChangeDisplaySettingsExW");

  if (MH_CreateHook(pChangeDisplaySettingsExW, (LPVOID)&DetourChangeDisplaySettingsExW, (LPVOID *)&fnChangeDisplaySettingsExW) != MH_OK ||
      MH_EnableHook(pChangeDisplaySettingsExW) != MH_OK)
    LogMessage(LogLevel::Error, false, "ChangeDisplaySettingsExW hook failed");

  constexpr uint8_t expected[] = {0x40, 0x56, 0x57, 0x48, 0x83, 0xEC, 0x38, 0x8B};
  LPVOID target = (LPVOID)GetAbsoluteAddress(present_interval_offset);
  if (memcmp(target, expected, sizeof(expected)) != 0)
    LogMessage(LogLevel::Error, false, "Present interval: unexpected bytes, VSync option disabled");
  else if (MH_CreateHook(target, (LPVOID)&DetourPresentInterval, (LPVOID *)&fnPresentInterval) != MH_OK || MH_EnableHook(target) != MH_OK)
    LogMessage(LogLevel::Error, false, "Present interval hook failed");
}

void Shutdown()
{
  INITIALIZED = false;
  MH_DisableHook(MH_ALL_HOOKS);
  MH_Uninitialize();

  LogMessage(LogLevel::Info, false, "Shutdown initiated.");
  Sleep(100);

  // Necessary because in pair with SLE game hungs on exit, don't see any cons
  ExitProcess(0);
}

// The game is ready once its window exists and the render system has built its first output.
static bool GameReady()
{
  uintptr_t window = *(uintptr_t *)GetAbsoluteAddress(window_singleton_offset);
  uintptr_t system = *(uintptr_t *)GetAbsoluteAddress(render_system_offset);
  if (!window || !system || !IsValidReadPtr(system + 160, sizeof(uintptr_t)))
    return false;

  uintptr_t output = *(uintptr_t *)(system + 160);
  return IsValidReadPtr(output + 272, 2 * sizeof(uint32_t)) && *(uint32_t *)(output + 272) && *(uint32_t *)(output + 276);
}

DWORD WINAPI InitThread(LPVOID lpParam)
{
  SetupD3DCompilerProxy();

  remove("SekiroFPSUnlockAndMoreNative.log");
  LogMessage(LogLevel::Info, false, "Sekiro FPS Unlock And More Native v%s log initialized.", PROJECT_VERSION_STRING);

  MH_Initialize();
  RefreshIniValues();

  InstallCrashLogger();
  // The game applies its default sound volumes long before it has finished starting up
  InstallSoundFix();
  // The swap chain is created while the game starts up
  InstallWindowedHdr();

  // Nothing is hooked or patched before the game finished starting up
  while (!(GAME_WINDOW = GetGameWindow()))
    Sleep(100);
  for (int waited = 0; !GameReady(); waited += 100)
  {
    if (waited >= 120000)
      return LogMessage(LogLevel::Error, false, "Game did not finish starting up, nothing was patched"), false;
    Sleep(100);
  }
  Sleep(1000);

  InstallWindowHooks();
  ApplyFullscreenHZPatch();
  is_game_in_loading_addr = GetAbsoluteAddress(is_game_in_loading_offset);
  ApplyBorderlessPatch();
  ApplyIntroPatch();
  ApplyCamResetPatch();
  ApplyCamAutorotatePatch();
  ApplyDeathPenaltiesPatch();
  ApplyDragonrotPatch();
  ApplyAutolootPatch();
  ApplyFramelockPatch();
  ApplyFovPatch();
  INITIALIZED = true;
  InstallMenuHooks();

  InitHooks();
  // the render targets were built before the hook, so a chosen ratio needs them built again
  ApplyAspectRatio(ASPECT_RATIO_MODE != 0);
  InstallFixes();
  LogEnvironment();

  // The game recreates its window when the resolution or screen mode changes, so follow it and
  // only shut down once no game window has been there for a while.
  for (int missing = 0; missing < 100;)
  {
    Sleep(100);
    if (HWND window = GetGameWindow())
    {
      if (window != GAME_WINDOW)
        LogMessage(LogLevel::Info, false, "Game window %p replaces %p.", (void *)window, (void *)GAME_WINDOW);
      GAME_WINDOW = window;
      missing = 0;
    }
    else
      missing++;
  }

  Shutdown();

  return true;
}

BOOL WINAPI DllMain(HMODULE hModule, DWORD fdwReason, LPVOID lpvReserved)
{

  if (fdwReason == DLL_PROCESS_ATTACH)
  {
    DisableThreadLibraryCalls(hModule);
    CreateThread(NULL, 0, reinterpret_cast<LPTHREAD_START_ROUTINE>(InitThread), NULL, 0, NULL);
  }

  return true;
}
