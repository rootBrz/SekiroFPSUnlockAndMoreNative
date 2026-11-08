#include "menu.h"
#include "fixes.h"
#include "lang.h"
#include "MinHook.h"
#include "offset.h"
#include "patches.h"
#include "strings.h"
#include "utils.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

// The options pages are Scaleform dialogs (AppMenu::CSPcGraphicsSettingDialog). Every row is built from four
// std::function objects (getter, setter, choice list, reset), a text definition (title + help message ids) and a
// command-list tag. We append our own rows right before the dialog finalizes itself and serve their texts by
// answering message lookups for ids in our own range.

namespace
{
using AddRowFn = void (*)(void *page, void *textDef, int *type, void *cmdList, void *getter, void *setter, void *list, void *reset);
using HolderFn = void *(*)(void *holder, int id);
using DtorFn = void (*)(void *obj);
using ListPushFn = void *(*)(void *list, void *item);
using StrCopyFn = void *(*)(void *dst, void *src);
using StrAssignFn = void *(*)(void *dst, const wchar_t *src, void *heap);
using ModeGetFn = uint8_t (*)(void *self);
using ModeSetFn = void *(*)(void *self, uint8_t *value);
using ModeListFn = void *(*)(void *self, void *ret);
using ArrowRowFn = void *(*)(void *page, void *textDef, void *value, void *list, void *defaultValue, char flag);
using BoolListFn = void *(*)(void *list);
using TextFn = void *(*)(void *holder, void *msg, unsigned id, const wchar_t *name);
using FormatFn = void *(*)(void *holder, const wchar_t *fmt, ...);

TextFn fnText;
ArrowRowFn fnArrowRow;
BoolListFn fnBoolList;
DtorFn fnBoolItemDtor;
ModeGetFn fnModeGet;
ModeSetFn fnModeSet;
ModeListFn fnModeList;
int borderlessTextId;
AddRowFn fnAddRow;
HolderFn fnTitleHolder;
HolderFn fnHelpHolder;
DtorFn fnTextDefDtor;
DtorFn fnItemDtor;
ListPushFn fnListPush;
StrCopyFn fnStrCopy;
StrAssignFn fnStrAssign;
FormatFn fnFormat;
uintptr_t emptyWString;
uintptr_t gameHeapPtr;

constexpr int TEXT_BASE = 900000000;
constexpr size_t COMBO_MAX = 32; // DLFixedVector capacity of a ComboItemList

// every text in each of the game's languages, shown in the language the game is set to
std::vector<Text> texts;

int AddText(const Text &s)
{
  texts.push_back(s);
  return TEXT_BASE + (int)texts.size() - 1;
}

struct Option
{
  int page, titleId, helpId, choiceBase, def = 0;
  std::vector<int> values;
  std::function<int()> get;  // current choice index
  std::function<void(int)> set;
};
std::vector<Option> options;

// Two-choice options use the game's inline arrow row, which edits a value in place; polled for changes.
constexpr size_t MAX_OPTIONS = 32;
uint32_t arrowCell[MAX_OPTIONS];
int arrowApplied[MAX_OPTIONS];
int buildPage = -1;

// MSVC std::function with a small-buffer _Func_impl: space[56] + pointer to the impl living in it.
struct GameFunc
{
  alignas(8) uint8_t space[56];
  void *ptr;
};
struct Impl
{
  void **vtbl;
  int opt;
};

void *ImplCopy(Impl *self, void *where)
{
  if (!where)
    where = malloc(sizeof(Impl));
  memcpy(where, self, sizeof(Impl));
  return where;
}
void ImplDelete(Impl *self, bool dealloc)
{
  if (dealloc)
    free(self);
}
void *ImplNull(Impl *) { return nullptr; }

void FreeHolderString(uint8_t *holder)
{
  if (*(uint64_t *)(holder + 40) >= 8)
  {
    void *heap = *(void **)(holder + 8);
    (*(void (**)(void *, void *))(*(uintptr_t *)heap + 104))(heap, *(void **)(holder + 16));
  }
}

void BuildItem(void *list, uint8_t value, int textId)
{
  alignas(8) uint8_t item[120] = {0};
  alignas(8) uint8_t holder[64] = {0};
  fnTitleHolder(holder, textId);
  item[0] = value;
  *(uint64_t *)(item + 8) = *(uint64_t *)holder;
  fnStrCopy(item + 16, holder + 8);
  fnStrAssign(item + 72, (const wchar_t *)emptyWString, *(void **)gameHeapPtr);
  fnListPush(list, item);
  fnItemDtor(item);
  FreeHolderString(holder);
}

uint8_t GetterCall(Impl *self)
{
  const Option &o = options[self->opt];
  return (uint8_t)std::clamp(o.get(), 0, (int)o.values.size() - 1);
}
void *SetterCall(Impl *self, uint8_t *v)
{
  Option &o = options[self->opt];
  if (*v < o.values.size())
    o.set(*v);
  return nullptr;
}
void *ListCall(Impl *self, void *ret)
{
  const Option &o = options[self->opt];
  *(uint64_t *)((uint8_t *)ret + 3848) = 0;
  for (size_t i = 0; i < o.values.size(); i++)
    BuildItem(ret, (uint8_t)i, o.choiceBase + (int)i);
  return ret;
}
void *ResetCall(Impl *) { return nullptr; }

#define VTBL(call) {(void *)ImplCopy, (void *)ImplCopy, (void *)call, (void *)ImplNull, (void *)ImplDelete, (void *)ImplNull}
void *vtGetter[6] = VTBL(GetterCall);
void *vtSetter[6] = VTBL(SetterCall);
void *vtList[6] = VTBL(ListCall);
void *vtReset[6] = VTBL(ResetCall);

void MakeFunc(GameFunc &f, void **vtbl, int opt)
{
  memset(&f, 0, sizeof(f));
  Impl *i = (Impl *)f.space;
  i->vtbl = vtbl;
  i->opt = opt;
  f.ptr = i;
}

void AddArrowRow(void *page, int idx)
{
  Option &o = options[idx];
  alignas(16) uint8_t textDef[176] = {0};
  alignas(16) uint8_t list[512] = {0};
  fnTitleHolder(textDef + 64, o.titleId);
  fnHelpHolder(textDef + 120, o.helpId);
  fnBoolList(list); // 1 = On, 0 = Off

  arrowCell[idx] = o.get();
  arrowApplied[idx] = arrowCell[idx];
  int defaultValue = o.def;
  static int defaults[MAX_OPTIONS];
  defaults[idx] = defaultValue;
  fnArrowRow(page, textDef, &arrowCell[idx], list, &defaults[idx], 1);

  for (uint64_t i = 0, n = *(uint64_t *)(list + 248); i < n; i++)
    fnBoolItemDtor(list + 120 * i);
  fnTextDefDtor(textDef);
}

void AddRow(void *page, int idx)
{
  const Option &o = options[idx];
  if (o.values.size() == 2)
    return AddArrowRow(page, idx);
  alignas(8) uint8_t textDef[176] = {0};
  fnTitleHolder(textDef + 64, o.titleId);
  fnHelpHolder(textDef + 120, o.helpId);

  int type = 11;
  struct
  {
    int len;
    uint8_t flag;
    const wchar_t *name;
    uint64_t pad;
  } cmd = {7, 0, L"02_201_CommandList", 0};

  GameFunc getter, setter, list, reset;
  MakeFunc(getter, vtGetter, idx);
  MakeFunc(setter, vtSetter, idx);
  MakeFunc(list, vtList, idx);
  MakeFunc(reset, vtReset, idx);

  fnAddRow(page, textDef, &type, &cmd, &getter, &setter, &list, &reset);
  fnTextDefDtor(textDef);
}

// ---- option definitions ----

void Commit(const char *key, int value, void (*patch)())
{
  SetIniValue(key, value);
  RefreshIniValues();
  if (patch)
    patch();
}

void NewOption(const Text &title, const Text &help, const std::vector<Text> &labels, std::vector<int> values,
               std::function<int()> get, std::function<void(int)> set)
{
  Option o;
  o.page = buildPage;
  o.titleId = AddText(title);
  o.helpId = AddText(help);
  o.choiceBase = TEXT_BASE + (int)texts.size();
  for (const Text &l : labels)
    AddText(l);
  o.values = std::move(values);
  o.get = std::move(get);
  o.set = std::move(set);
  options.push_back(std::move(o));
}

void AddToggle(const Text &title, const Text &help, int &var, const char *key, void (*patch)() = nullptr, int def = 0)
{
  NewOption(
      title, help, {str::Off, str::On}, {0, 1}, [&var]
      { return var ? 1 : 0; },
      [&var, key, patch](int v)
      { var = v; Commit(key, v, patch); });
  options.back().def = def;
}

int NearestIndex(const std::vector<int> &values, int v)
{
  int best = 0;
  for (size_t i = 1; i < values.size(); i++)
    if (abs(values[i] - v) < abs(values[best] - v))
      best = (int)i;
  return best;
}

// 60 means locked (FPS Unlock off), 0 unlimited.
void AddFpsLimit()
{
  std::vector<int> values = {60, 90, 120, 144, 165, 180, 200, 240, 300, 0};
  std::vector<Text> labels;
  for (int v : values)
    labels.push_back(v ? Same(std::to_wstring(v) + L" FPS") : str::Unlimited);
  NewOption(
      str::FpsLimit, str::FpsLimitHelp, labels, values,
      [values]
      {
        if (!FPSUNLOCK_ENABLED)
          return 0;
        if (!FPS_LIMIT)
          return (int)values.size() - 1;
        return NearestIndex(std::vector<int>(values.begin(), values.end() - 1), FPS_LIMIT);
      },
      [values](int i)
      {
        FPSUNLOCK_ENABLED = values[i] != 60;
        FPS_LIMIT = values[i];
        SetIniValue("EnableFpsUnlock", FPSUNLOCK_ENABLED);
        Commit("FpsLimit", FPS_LIMIT, ApplyFramelockPatch);
      });
}

// The refresh rates the display offers; a change switches a fullscreen display right away.
void AddRefreshRate()
{
  std::vector<int> values = {0};
  DEVMODEW dm = {};
  dm.dmSize = sizeof(dm);
  for (DWORD i = 0; EnumDisplaySettingsW(nullptr, i, &dm); i++)
    if (dm.dmDisplayFrequency >= 24 && std::find(values.begin(), values.end(), (int)dm.dmDisplayFrequency) == values.end())
      values.push_back((int)dm.dmDisplayFrequency);
  std::sort(values.begin() + 1, values.end());
  if (values.size() > COMBO_MAX)
    values.erase(values.begin() + 1, values.end() - (COMBO_MAX - 1));
  std::vector<Text> labels = {str::Auto};
  for (size_t i = 1; i < values.size(); i++)
    labels.push_back(Same(std::to_wstring(values[i]) + L" Hz"));
  NewOption(
      str::RefreshRate, str::RefreshRateHelp, labels, values,
      [values]
      { return REFRESH_RATE > 0 ? NearestIndex(values, REFRESH_RATE) : 0; },
      [values](int i)
      { REFRESH_RATE = values[i]; Commit("RefreshRate", REFRESH_RATE, [] { ApplyRefreshRate(true); }); });
}

void AddAspectRatio()
{
  NewOption(
      str::AspectRatio, str::AspectRatioHelp, {str::FillScreen, Same(L"4:3"), Same(L"16:10"), Same(L"16:9"), Same(L"21:9")}, {0, 1, 2, 3, 4},
      []
      { return ASPECT_RATIO_MODE; },
      [](int v)
      { ASPECT_RATIO_MODE = v; Commit("AspectRatio", v, [] { ApplyAspectRatio(true); }); });
}

void AddFov()
{
  std::vector<int> values;
  std::vector<Text> labels;
  for (int v = -90; v <= 90; v += 10)
  {
    values.push_back(v);
    labels.push_back(v == 0 ? str::Default : Same((v > 0 ? L"+" : L"") + std::to_wstring(v)));
  }
  NewOption(
      str::Fov, str::FovHelp, labels, values,
      [values]
      { return NearestIndex(values, FOV_ENABLED ? FOV_VALUE : 0); },
      [values](int i)
      {
        FOV_VALUE = values[i];
        FOV_ENABLED = values[i] != 0;
        SetIniValue("ValueFOV", FOV_VALUE);
        Commit("EnableFOV", FOV_ENABLED, ApplyFovPatch);
      });
}

// The movies are given their viewport again, which places the HUD for the new layout.
void AddHudLayout()
{
  NewOption(
      str::HudLayout, str::HudLayoutHelp, {Same(L"16:9"), str::Stretch, str::ScreenEdges}, {HUD_16_9, HUD_STRETCH, HUD_EDGES},
      []
      { return HUD_LAYOUT; },
      [](int v)
      { HUD_LAYOUT = v; Commit("StretchHud", v, RelayoutHud); });
}

// Page ids of the game's settings pages (CSCommonOptionUnit page factory).
enum Page
{
  PAGE_GAME = 0,
  PAGE_CAMERA = 1,
  PAGE_GRAPHICS = 9,
};

void BuildOptions()
{
  buildPage = PAGE_GAME;
  AddToggle(str::Autoloot, str::AutolootHelp, AUTOLOOT_ENABLED, "EnableAutoloot", ApplyAutolootPatch, 1);
  AddToggle(str::SkipLogos, str::SkipLogosHelp, INTROSKIP_ENABLED, "EnableIntroSkip", nullptr, 1);
  AddToggle(str::DeathPenalties, str::DeathPenaltiesHelp, DISABLE_DEATH_PENALTIES_ENABLED, "DisableDeathPenalties", ApplyDeathPenaltiesPatch);
  AddToggle(str::Dragonrot, str::DragonrotHelp, DISABLE_DRAGONROT_ENABLED, "DisableDragonrot", ApplyDragonrotPatch);
  AddToggle(str::QuitToDesktop, str::QuitToDesktopHelp, QUIT_TO_DESKTOP_ENABLED, "QuitToDesktop");

  buildPage = PAGE_CAMERA;
  AddToggle(str::CamReset, str::CamResetHelp, DISABLE_CAMRESET_ENABLED, "DisableCamReset", ApplyCamResetPatch);
  AddToggle(str::CamAutorotate, str::CamAutorotateHelp, DISABLE_CAMERA_AUTOROTATE_ENABLED, "DisableCamAutorotate", ApplyCamAutorotatePatch);
  AddFov();
  AddToggle(str::LockOnDot, str::LockOnDotHelp, HIDE_LOCKON_ENABLED, "HideLockOnDot", RelayoutHud);

  buildPage = PAGE_GRAPHICS;
  AddFpsLimit();
  AddRefreshRate();
  AddToggle(str::VSync, str::VSyncHelp, VSYNC_ENABLED, "EnableVSync", nullptr, 1);
  AddAspectRatio();
  AddHudLayout();
}

// Applies changes made through the arrow rows; called every frame.
void PollArrowRows()
{
  for (size_t i = 0; i < options.size() && i < MAX_OPTIONS; i++)
    if (options[i].values.size() == 2 && arrowApplied[i] >= 0 && arrowCell[i] != (uint32_t)arrowApplied[i])
    {
      arrowApplied[i] = (int)arrowCell[i];
      options[i].set(arrowApplied[i] ? 1 : 0);
    }
}

// ---- hooks ----

int currentPage = -1;
bool rowsAdded;
using FactoryFn = void *(*)(int id, void *a2, void *a3, void *a4);
using ButtonFn = void *(*)(void *page, void *textDef, void *a3, void *a4, void *a5);
FactoryFn fnFactory;
ButtonFn fnButton;

void *DetourFactory(int id, void *a2, void *a3, void *a4)
{
  int prevPage = currentPage;
  bool prevAdded = rowsAdded;
  currentPage = id;
  rowsAdded = false;
  void *r = fnFactory(id, a2, a3, a4);
  currentPage = prevPage;
  rowsAdded = prevAdded;
  return r;
}

// Every settings page adds its buttons (Defaults, Advanced Settings) after its rows; ours go right before the first.
void *DetourButton(void *page, void *textDef, void *a3, void *a4, void *a5)
{
  if (!rowsAdded && currentPage >= 0)
  {
    rowsAdded = true;
    for (size_t i = 0; i < options.size(); i++)
      if (options[i].page == currentPage)
        AddRow(page, (int)i);
  }
  return fnButton(page, textDef, a3, a4, a5);
}

void *DetourText(void *holder, void *msg, unsigned id, const wchar_t *name)
{
  if (!msg && id >= (unsigned)TEXT_BASE && id < (unsigned)TEXT_BASE + texts.size())
  {
    const Text &text = texts[id - TEXT_BASE];
    const std::wstring &s = text[GameLanguage()];
    return fnFormat(holder, L"%s", (s.empty() ? text[EN] : s).c_str());
  }
  return fnText(holder, msg, id, name);
}

// Screen Mode row (Windowed = 0, Fullscreen = 1): a third choice, 2, is windowed plus the mod's borderless flag.
constexpr uint8_t MODE_BORDERLESS = 2;

uint8_t DetourModeGet(void *self)
{
  uint8_t v = fnModeGet(self);
  return !v && BORDERLESS_ENABLED ? MODE_BORDERLESS : v;
}

void *DetourModeSet(void *self, uint8_t *value)
{
  int borderless = *value == MODE_BORDERLESS;
  uint8_t native = borderless ? 0 : *value;
  if (BORDERLESS_ENABLED != borderless)
  {
    BORDERLESS_ENABLED = borderless;
    Commit("EnableBorderlessFullscreen", borderless, ApplyBorderlessPatch);
  }
  return fnModeSet(self, &native);
}

void *DetourModeList(void *self, void *ret)
{
  fnModeList(self, ret);
  BuildItem(ret, MODE_BORDERLESS, borderlessTextId);
  return ret;
}

bool Hook(uintptr_t rva, const uint8_t *expected, size_t size, void *detour, void **original, const char *name)
{
  uintptr_t addr = GetAbsoluteAddress(rva);
  if (memcmp((void *)addr, expected, size) != 0)
    return LogMessage(LogLevel::Error, false, "%s: unexpected bytes, menu rows disabled", name), false;
  if (MH_CreateHook((LPVOID)addr, detour, original) != MH_OK || MH_EnableHook((LPVOID)addr) != MH_OK)
    return LogMessage(LogLevel::Error, false, "%s: hook failed", name), false;
  return true;
}

using ResListFn = int64_t *(*)(int64_t *out, int64_t a2, char fullscreen, unsigned monitor);
using PushFn = void (*)(int64_t *vec, void *pair);
ResListFn fnResList;
PushFn fnResPush;

// The windowed resolution list is a fixed 16:9 table; replace it with every mode the display supports.
int64_t *DetourResList(int64_t *out, int64_t a2, char fullscreen, unsigned monitor)
{
  fnResList(out, a2, fullscreen, monitor);
  if (fullscreen)
    return out;

  std::vector<std::pair<int, int>> modes;
  DEVMODEW dm = {};
  dm.dmSize = sizeof(dm);
  for (DWORD i = 0; EnumDisplaySettingsW(nullptr, i, &dm); i++)
  {
    std::pair<int, int> m(dm.dmPelsWidth, dm.dmPelsHeight);
    if (m.first >= 800 && m.second >= 450 && std::find(modes.begin(), modes.end(), m) == modes.end())
      modes.push_back(m);
  }
  if (modes.empty())
    return out;
  std::sort(modes.begin(), modes.end());
  if (modes.size() > COMBO_MAX - 2) // list rows are a 32 entry fixed vector; keep the largest modes
    modes.erase(modes.begin(), modes.end() - (COMBO_MAX - 2));

  out[2] = out[1]; // clear
  for (const auto &m : modes)
  {
    int32_t pair[2] = {m.first, m.second};
    fnResPush(out, pair);
  }
  return out;
}

} // namespace

void MenuFrame()
{
  PollArrowRows();
}

void InstallMenuHooks()
{
  std::fill(std::begin(arrowApplied), std::end(arrowApplied), -1);
  constexpr uint8_t textBytes[] = {0x48, 0x89, 0x4C, 0x24, 0x08, 0x57, 0x48, 0x83};
  constexpr uint8_t factoryBytes[] = {0x48, 0x8B, 0xC4, 0x4C, 0x89, 0x48, 0x20, 0x57};
  constexpr uint8_t buttonBytes[] = {0x40, 0x55, 0x53, 0x56, 0x57, 0x41, 0x56, 0x41};
  constexpr uint8_t resListBytes[] = {0x40, 0x55, 0x56, 0x57, 0x41, 0x54, 0x41, 0x55};

  fnAddRow = (AddRowFn)GetAbsoluteAddress(0xCADD90);
  fnTitleHolder = (HolderFn)GetAbsoluteAddress(0xC7B910);
  fnHelpHolder = (HolderFn)GetAbsoluteAddress(0xC7B110);
  fnTextDefDtor = (DtorFn)GetAbsoluteAddress(0xC609A0);
  fnItemDtor = (DtorFn)GetAbsoluteAddress(0xCBBC30);
  fnListPush = (ListPushFn)GetAbsoluteAddress(0xCFF3C0);
  fnStrCopy = (StrCopyFn)GetAbsoluteAddress(0x1C7130);
  fnStrAssign = (StrAssignFn)GetAbsoluteAddress(0x1C8720);
  fnFormat = (FormatFn)GetAbsoluteAddress(0xC7A520);
  fnArrowRow = (ArrowRowFn)GetAbsoluteAddress(0xCADA70);
  fnBoolList = (BoolListFn)GetAbsoluteAddress(0xCC5FC0);
  fnBoolItemDtor = (DtorFn)GetAbsoluteAddress(0xCBBCF0);
  fnResPush = (PushFn)GetAbsoluteAddress(0x11BA840);
  emptyWString = GetAbsoluteAddress(0x3239970);
  gameHeapPtr = GetAbsoluteAddress(0x3D9CE28);

  BuildOptions();
  borderlessTextId = AddText(str::Borderless);

  bool ok = Hook(0xC7DAA0, textBytes, sizeof(textBytes), (void *)DetourText, (void **)&fnText, "menu text") &&
            Hook(0xCC4FE0, factoryBytes, sizeof(factoryBytes), (void *)DetourFactory, (void **)&fnFactory, "settings page factory") &&
            Hook(0xC98F80, buttonBytes, sizeof(buttonBytes), (void *)DetourButton, (void **)&fnButton, "page button");
  constexpr uint8_t modeGetBytes[] = {0x48, 0x8B, 0x41, 0x08, 0x80, 0xB8, 0x56, 0x12};
  constexpr uint8_t modeSetBytes[] = {0x80, 0x3A, 0x01, 0x0F, 0x94, 0xC1, 0xE9};
  constexpr uint8_t modeListBytes[] = {0x48, 0x89, 0x54, 0x24, 0x10, 0x53, 0x48, 0x83};
  if (ok)
    Hook(0xCD2A10, modeGetBytes, sizeof(modeGetBytes), (void *)DetourModeGet, (void **)&fnModeGet, "screen mode getter") &&
        Hook(0xCD3D60, modeSetBytes, sizeof(modeSetBytes), (void *)DetourModeSet, (void **)&fnModeSet, "screen mode setter") &&
        Hook(0xCD2F30, modeListBytes, sizeof(modeListBytes), (void *)DetourModeList, (void **)&fnModeList, "screen mode list");
  Hook(0x11B9CF0, resListBytes, sizeof(resListBytes), (void *)DetourResList, (void **)&fnResList, "resolution list");
  if (ok)
    LogMessage(LogLevel::Success, false, "Menu rows installed (%zu options).", options.size());
}
