#include "utils.h"
#include "patches.h"

#include <dxgi.h>
#define PSAPI_VERSION 2
#include <psapi.h>

typedef HRESULT(WINAPI *D3DGETBLOBPART)(LPCVOID pSrcData, SIZE_T SrcDataSize, UINT Part, UINT Flags, void **ppPart);
HMODULE d3dHMOD = nullptr;
D3DGETBLOBPART fpD3DGetBlobPart = nullptr;
void SetupD3DCompilerProxy()
{
  char syspath[MAX_PATH];
  if (GetSystemDirectoryA(syspath, MAX_PATH))
  {
    strcat_s(syspath, "\\D3DCompiler_43.dll");
    if ((d3dHMOD = LoadLibraryA(syspath)))
    {
      fpD3DGetBlobPart = (D3DGETBLOBPART)GetProcAddress(d3dHMOD, "D3DGetBlobPart");
    }
  }
}
extern "C" __declspec(dllexport) HRESULT WINAPI D3DGetBlobPart(LPCVOID pSrcData, SIZE_T SrcDataSize, UINT Part, UINT Flags, void **ppPart)
{
  return fpD3DGetBlobPart(pSrcData, SrcDataSize, Part, Flags, ppPart);
}

typedef struct
{
  const char *key;
  const char *value;
  void *var;
  bool isFloat = false;
} Setting;
constexpr Setting defaultSettings[] = {
    {"EnableFpsUnlock", "1", &FPSUNLOCK_ENABLED},
    {"FpsLimit", "165", &FPS_LIMIT},
    {"RefreshRate", "0", &REFRESH_RATE},
    {"EnableVSync", "1", &VSYNC_ENABLED},
    {"EnableFOV", "0", &FOV_ENABLED},
    {"ValueFOV", "0", &FOV_VALUE},
    {"EnableAutoloot", "1", &AUTOLOOT_ENABLED},
    {"EnableIntroSkip", "1", &INTROSKIP_ENABLED},
    {"EnableBorderlessFullscreen", "1", &BORDERLESS_ENABLED},
    {"EnableWindowedHdr", "1", &WINDOWED_HDR_ENABLED},
    {"AspectRatio", "0", &ASPECT_RATIO_MODE},
    {"StretchHud", "2", &HUD_LAYOUT},
    {"DisableCamReset", "0", &DISABLE_CAMRESET_ENABLED},
    {"DisableCamAutorotate", "0", &DISABLE_CAMERA_AUTOROTATE_ENABLED},
    {"DisableDeathPenalties", "0", &DISABLE_DEATH_PENALTIES_ENABLED},
    {"DisableDragonrot", "0", &DISABLE_DRAGONROT_ENABLED},
    {"SoundVolumeMusic", "-1", &SOUND_VOLUME_MUSIC},
    {"SoundVolumeEffects", "-1", &SOUND_VOLUME_EFFECTS},
    {"SoundVolumeVoice", "-1", &SOUND_VOLUME_VOICE}};

bool WriteProtectedMemory(uintptr_t address, const void *data, size_t size, const char *msg)
{
  SIZE_T bytesWritten = 0;
  BOOL result = WriteProcessMemory(GetCurrentProcess(), (LPVOID)address, data, size, &bytesWritten);

  if (!result || bytesWritten != size)
  {
    DWORD errCode = GetLastError();
    char buf[128];
    snprintf(buf, sizeof(buf), "%s\nWriteProcessMemory failed at 0x%p (error %lu)", msg, (void *)address, errCode);

    return LogMessage(LogLevel::Error, true, buf), false;
  }

  LogMessage(LogLevel::Success, false, "%s.", msg);
  return true;
}

uintptr_t AllocateMemoryNearAddress(uintptr_t target, size_t size, bool executable)
{
  DWORD flProtect = executable ? PAGE_EXECUTE_READWRITE : PAGE_READWRITE;

  SYSTEM_INFO si;
  GetSystemInfo(&si);

  uintptr_t granularity = si.dwAllocationGranularity;
  uintptr_t minAddr = (target >= 0x70000000)
                          ? target - 0x70000000
                          : reinterpret_cast<uintptr_t>(si.lpMinimumApplicationAddress);
  uintptr_t maxAddr = target + 0x70000000;

  HANDLE process = GetCurrentProcess();
  MEMORY_BASIC_INFORMATION mbi;

  for (uintptr_t addr = target; addr >= minAddr; addr -= granularity)
  {
    if (VirtualQueryEx(process, (LPCVOID)addr, &mbi, sizeof(mbi)) != sizeof(mbi))
      continue;

    if (mbi.State == MEM_FREE && mbi.RegionSize >= size)
    {
      LPVOID allocated = VirtualAllocEx(process, (LPVOID)addr, size, MEM_COMMIT | MEM_RESERVE, flProtect);
      if (allocated)
        return reinterpret_cast<uintptr_t>(allocated);
    }
  }

  for (uintptr_t addr = target + granularity; addr < maxAddr; addr += granularity)
  {
    if (VirtualQueryEx(process, (LPCVOID)addr, &mbi, sizeof(mbi)) != sizeof(mbi))
      continue;

    if (mbi.State == MEM_FREE && mbi.RegionSize >= size)
    {
      LPVOID allocated = VirtualAllocEx(process, (LPVOID)addr, size, MEM_COMMIT | MEM_RESERVE, flProtect);
      if (allocated)
        return reinterpret_cast<uintptr_t>(allocated);
    }
  }

  LogMessage(LogLevel::Error, true, "Failed to allocate memory near target address 0x%p", (void *)target);
  return 0;
}

// sekiro.exe is decrypted by its Steam stub after the DLL is loaded, the code is executable again once it is done
bool CodeReady(LPVOID target, const uint8_t *expected, size_t size)
{
  MEMORY_BASIC_INFORMATION mbi;
  return VirtualQuery(target, &mbi, sizeof(mbi)) == sizeof(mbi) &&
         (mbi.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)) &&
         memcmp(target, expected, size) == 0;
}

uintptr_t DereferenceStaticX64Pointer(uintptr_t instructionAddress, int instructionLength)
{
  return instructionAddress + *(int32_t *)(instructionAddress + (instructionLength - 0x04)) + instructionLength;
}

bool IsValidReadPtr(uintptr_t addr, size_t size)
{
  if (addr == 0 || size == 0)
    return false;

  if (addr + size < addr)
    return false;

  HANDLE hProcess = GetCurrentProcess();

  BYTE buffer[1];
  SIZE_T bytesRead;

  const char *startAddr = reinterpret_cast<const char *>(addr);
  const char *endAddr = startAddr + size - 1;

  if (!ReadProcessMemory(hProcess, startAddr, buffer, 1, &bytesRead) || bytesRead != 1)
    return false;

  if (!ReadProcessMemory(hProcess, endAddr, buffer, 1, &bytesRead) || bytesRead != 1)
    return false;

  return true;
}

uintptr_t GetAbsoluteAddress(uintptr_t offset)
{
  HMODULE hGameExe = GetModuleHandle(nullptr);
  uintptr_t base = reinterpret_cast<uintptr_t>(hGameExe);
  uintptr_t absoluteAddress = base + offset;

  if (absoluteAddress < base)
    return 0;

  return absoluteAddress;
}

std::vector<uint8_t> RelJmpBytes(int length, int32_t relative)
{
  std::vector<uint8_t> bytes(length);
  bytes[0] = 0xE9;
  memcpy(&bytes[1], &relative, sizeof(int32_t));
  for (int i = 5; i < length; i++)
    bytes[i] = 0x90;
  return bytes;
}

uintptr_t InjectShellcodeWithJumpBack(uintptr_t targetAddr, const uint8_t *shellcode, size_t shellcodeSize, size_t patchSize, const char *msg)
{
  uintptr_t codeCave = AllocateMemoryNearAddress(targetAddr, shellcodeSize + 5, true);
  uintptr_t returnAddr = targetAddr + patchSize;

  int32_t relJumpBack = (int32_t)(returnAddr - (codeCave + shellcodeSize + 5));

  std::vector<uint8_t> detour(shellcodeSize + 5);
  memcpy(detour.data(), shellcode, shellcodeSize);
  detour[shellcodeSize] = 0xE9;
  memcpy(detour.data() + shellcodeSize + 1, &relJumpBack, sizeof(relJumpBack));

  WriteProtectedMemory(codeCave, detour.data(), detour.size(), msg);
  return codeCave;
}

// Logs where the game dies so crashes can be traced back to a patch or an offset.
static LONG WINAPI LogCrash(PEXCEPTION_POINTERS info)
{
  const EXCEPTION_RECORD *rec = info->ExceptionRecord;
  // first-chance handler: only memory faults, the game raises other exceptions on purpose
  static int logged;
  if (rec->ExceptionCode != EXCEPTION_ACCESS_VIOLATION && rec->ExceptionCode != EXCEPTION_STACK_OVERFLOW && rec->ExceptionCode != EXCEPTION_ILLEGAL_INSTRUCTION)
    return EXCEPTION_CONTINUE_SEARCH;
  if (logged++ >= 16)
    return EXCEPTION_CONTINUE_SEARCH;
  uintptr_t rip = (uintptr_t)rec->ExceptionAddress, base = (uintptr_t)GetModuleHandleA(nullptr);
  HMODULE owner = nullptr;
  char module[MAX_PATH] = "?";
  if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)rip, &owner))
    GetModuleFileNameA(owner, module, sizeof(module));
  LogMessage(LogLevel::Error, false, "Crash: code 0x%08lX at %p (%s+0x%llX), exe base %p, access %llu address %p", rec->ExceptionCode,
             (void *)rip, module, (unsigned long long)(rip - (uintptr_t)owner), (void *)base,
             rec->NumberParameters > 0 ? (unsigned long long)rec->ExceptionInformation[0] : 0ULL,
             rec->NumberParameters > 1 ? (void *)rec->ExceptionInformation[1] : nullptr);
  return EXCEPTION_CONTINUE_SEARCH;
}

void InstallCrashLogger()
{
  AddVectoredExceptionHandler(1, LogCrash);
  SetUnhandledExceptionFilter(LogCrash);
}

HWND GetGameWindow()
{
  HWND result = NULL;

  EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL
              {
    DWORD pid;
    GetWindowThreadProcessId(hwnd, &pid);
    
    if (pid != GetCurrentProcessId() || !IsWindowVisible(hwnd) || GetWindow(hwnd, GW_OWNER))
      return TRUE;
      
    char className[256];
    GetClassNameA(hwnd, className, sizeof(className));
    if (strcmp(className, "CSclass") == 0) {
      *(HWND*)lParam = hwnd;
      return FALSE;  
    }
    return TRUE; }, (LPARAM)&result);

  return result;
}

void RefreshIniValues()
{
  // Generate INI if not present
  if (!ini.read(config))
    ini.generate(config);

  // Write missing settings if INI is present
  // Retrieve setting values
  for (const Setting &setting : defaultSettings)
  {
    if (!config["Settings"].has(setting.key))
    {
      config["Settings"][setting.key] = setting.value;
      ini.write(config);
    }

    if (setting.isFloat)
      *(float *)setting.var = GetIniValue<float>(setting.key);
    else
      *(int *)setting.var = GetIniValue<int>(setting.key);
  }

}

void LogMessage(LogLevel level, bool showMessage, const char *err, ...)
{
  const char *levelStr = nullptr;
  switch (level)
  {
  case LogLevel::Info:
    levelStr = "[INFO]";
    break;
  case LogLevel::Error:
    levelStr = "[ERRO]";
    break;
  case LogLevel::Success:
    levelStr = "[SUCC]";
    break;
  }

  va_list ArgList;
  char message[1024];
  va_start(ArgList, err);
  vsnprintf(message, sizeof(message), err, ArgList);
  va_end(ArgList);

  FILE *logFile;
  fopen_s(&logFile, "SekiroFPSUnlockAndMoreNative.log", "a");
  if (logFile)
  {
    time_t now = time(NULL);
    tm timeinfo;
    localtime_s(&timeinfo, &now);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "[%Y-%m-%d %H:%M:%S]", &timeinfo);

    fprintf(logFile, "%s %s %s\n", timestamp, levelStr, message);
    fclose(logFile);
  }

  if (showMessage)
    MessageBoxA(nullptr, message, "Sekiro FPS Unlock And More Native", MB_OK | MB_ICONERROR);
}

void LogEnvironment()
{
  HMODULE ntdll = GetModuleHandleA("ntdll.dll");
  RTL_OSVERSIONINFOW os = {sizeof(os)};
  auto getVersion = (LONG(WINAPI *)(RTL_OSVERSIONINFOW *))GetProcAddress(ntdll, "RtlGetVersion");
  if (getVersion && getVersion(&os) == 0)
    LogMessage(LogLevel::Info, false, "Windows %lu.%lu build %lu%s", os.dwMajorVersion, os.dwMinorVersion, os.dwBuildNumber,
               GetProcAddress(ntdll, "wine_get_version") ? " (Wine/Proton)" : "");

  auto getDpi = (UINT(WINAPI *)())GetProcAddress(GetModuleHandleA("user32.dll"), "GetDpiForSystem");
  LogMessage(LogLevel::Info, false, "Primary screen %dx%d, %d monitor(s), %u dpi", GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN),
             GetSystemMetrics(SM_CMONITORS), getDpi ? getDpi() : 0);

  using CreateFactoryFn = HRESULT(WINAPI *)(REFIID, void **);
  IDXGIFactory1 *factory = nullptr;
  auto createFactory = (CreateFactoryFn)GetProcAddress(LoadLibraryA("dxgi.dll"), "CreateDXGIFactory1");
  if (createFactory && SUCCEEDED(createFactory(__uuidof(IDXGIFactory1), (void **)&factory)))
  {
    IDXGIAdapter1 *adapter = nullptr;
    for (UINT i = 0; factory->EnumAdapters1(i, &adapter) == S_OK; i++)
    {
      DXGI_ADAPTER_DESC1 desc;
      LARGE_INTEGER driver = {};
      adapter->GetDesc1(&desc);
      adapter->CheckInterfaceSupport(__uuidof(IDXGIDevice), &driver);
      LogMessage(LogLevel::Info, false, "GPU %u: %ls, vendor 0x%04X, driver %u.%u.%u.%u, %u MB", i, desc.Description, desc.VendorId,
                 (unsigned)HIWORD(driver.HighPart), (unsigned)LOWORD(driver.HighPart), (unsigned)HIWORD(driver.LowPart),
                 (unsigned)LOWORD(driver.LowPart), (unsigned)(desc.DedicatedVideoMemory >> 20));
      adapter->Release();
    }
    factory->Release();
  }

  std::string settings;
  for (const Setting &setting : defaultSettings)
    settings += std::string(setting.key) + "=" + (setting.isFloat ? std::to_string(*(float *)setting.var) : std::to_string(*(int *)setting.var)) + " ";
  LogMessage(LogLevel::Info, false, "Settings: %s", settings.c_str());

  // overlays and other injectors (ReShade, Discord, RTSS, ...) are the usual suspects for a vanished picture or interface
  HMODULE modules[512];
  DWORD needed = 0;
  char windir[MAX_PATH] = {};
  GetWindowsDirectoryA(windir, MAX_PATH);
  std::string others;
  if (K32EnumProcessModules(GetCurrentProcess(), modules, sizeof(modules), &needed))
    for (DWORD i = 0; i < std::min<DWORD>(needed, sizeof(modules)) / sizeof(HMODULE); i++)
    {
      char path[MAX_PATH] = {};
      if (GetModuleFileNameA(modules[i], path, MAX_PATH) && _strnicmp(path, windir, strlen(windir)) != 0)
        others += std::string(strrchr(path, '\\') ? strrchr(path, '\\') + 1 : path) + " ";
    }
  LogMessage(LogLevel::Info, false, "Modules outside Windows: %s", others.c_str());
}
