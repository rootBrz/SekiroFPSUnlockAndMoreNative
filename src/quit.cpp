#include "quit.h"
#include "MinHook.h"
#include "lang.h"
#include "strings.h"
#include "utils.h"

#include <cstring>

// The confirmation of Quit Game is an AppMenu::CommandSelectDialog built by sub_140DE9D40 (called from the two builders of
// the Quit Game confirmation, the dialog texts 110500 and 110501): it adds Yes (sub_140DED860), then No (sub_140DEC770), and
// selects No. Each choice is an item with its index at +0 and its command at +8; activating one (sub_140DE8100) stores
// index + 1 as the result, so Yes is 1 and No is 2. The box only shows two items, so No is replaced by Quit Game to
// Desktop, tagged in the unused fourth int of its command. Its index (1) is also the result No has (2): it is Yes (and asks
// for the game to be closed at the title screen) only when it was picked with A, and B and Esc, which the game answers by
// activating the last item, leave it as No.
namespace
{
constexpr uintptr_t EMPTY_JOB = 0xDDC5E0;                    // sub_140DDC5E0(out): a job that is done at once
constexpr uintptr_t CRASH_NOTICE = 0xDF4C40;                  // (job out, state): shows System_Message 400003, the notice after the game was not closed with Quit Game
constexpr uintptr_t DIALOG = 0xDE9D40, ADD_ITEM = 0xDED860, ADD_CANCEL = 0xDEC770, SELECT_ITEM = 0xDEAAB0, ACTIVATE = 0xDE8100;
constexpr uintptr_t QUIT_CALLERS[] = {0xCC3A2A, 0xDD6D0F}; // return addresses into the two builders of the Quit Game confirmation
constexpr uintptr_t YES_COMMAND_TEXT = 0x2AAFE90;           // the text pointer of the Yes command
constexpr int MARK = 0x54495551;                            // 'QUIT'
constexpr int YES = 0;
constexpr WORD XINPUT_B = 0x2000;
constexpr unsigned YES_TEXT_ID = 3; // the Yes text of the dialog texts, which our item shows instead

struct Command
{
  int id, type, value, mark;
  const void *text;
};

using DialogFn = void *(*)(void *out, void *holder, void *text, void *command);
using EmptyJobFn = void *(*)(void *out);
using NoticeFn = void *(*)(void *state, void *out);
using AddItemFn = void *(*)(void *dialog, const Command *command);
using AddCancelFn = void *(*)(void *dialog);
using SelectItemFn = void *(*)(void *dialog, void *out, char flag);
using ActivateFn = void *(*)(int *item, void *out, void *holder);
using XInputGetStateFn = DWORD(WINAPI *)(DWORD pad, void *state);
struct XInputState
{
  DWORD packet;
  WORD buttons;
  BYTE triggers[2];
  SHORT sticks[4];
};

DialogFn fnDialog;
EmptyJobFn fnEmptyJob;
NoticeFn fnNotice;
AddItemFn fnAddItem;
AddCancelFn fnAddCancel;
SelectItemFn fnSelectItem;
ActivateFn fnActivate;

bool building;       // inside the Quit Game confirmation
bool labelWanted;    // the next Yes text is ours
bool quitRequested;  // Quit Game to Desktop was chosen
bool inGame;         // the in-game HUD had a viewport since the title screen was last seen

bool IsTitle(const char *name) { return strstr(name, "05_000_title.gfx") || strstr(name, "05_002_title_goty.gfx"); }

void *DetourDialog(void *out, void *holder, void *text, void *command)
{
  uintptr_t caller = (uintptr_t)__builtin_return_address(0) - GetAbsoluteAddress(0);
  building = caller == QUIT_CALLERS[0] || caller == QUIT_CALLERS[1];
  void *r = fnDialog(out, holder, text, command);
  building = false;
  return r;
}

// the notice at the start that the game was not closed with Quit Game (e.g. with Alt+F4); without it the job is done at once
void *DetourNotice(void *state, void *out)
{
  if (!PRESSSTART_SKIP_ENABLED)
    return fnNotice(state, out);
  return fnEmptyJob(out);
}

// B or Esc down: the game answers its box by activating the item at its "go back" index, the same way as A on that item
bool BackPressed()
{
  if (GetAsyncKeyState(VK_ESCAPE) & 0x8000)
    return true;
  static XInputGetStateFn getState = [] {
    for (const char *dll : {"xinput1_4.dll", "xinput1_3.dll", "xinput9_1_0.dll"})
      if (HMODULE m = LoadLibraryA(dll))
        if (auto f = (XInputGetStateFn)GetProcAddress(m, "XInputGetState"))
          return f;
    return (XInputGetStateFn) nullptr;
  }();
  XInputState state;
  for (DWORD pad = 0; getState && pad < 4; pad++)
    if (getState(pad, &state) == ERROR_SUCCESS && (state.buttons & XINPUT_B))
      return true;
  return false;
}

// Yes and Quit Game to Desktop, and no No: with only these two items B and Esc activate the last one (the box's "go back"
// index), which then answers No (the result of the second item, 2) instead of quitting.
void *DetourAddCancel(void *dialog)
{
  if (!building)
    return fnAddCancel(dialog);
  Command command = {100, 2, 41, MARK, (const void *)GetAbsoluteAddress(YES_COMMAND_TEXT)};
  labelWanted = true;
  fnAddItem(dialog, &command);
  labelWanted = false;
  return dialog;
}

void *DetourSelectItem(void *dialog, void *out, char flag)
{
  if (building)
  {
    *(int *)((uint8_t *)dialog + 32) = YES; // Yes is the selected item
    flag = 1;                               // B and Esc answer, as in the game's other Yes / No boxes
  }
  return fnSelectItem(dialog, out, flag);
}

void *DetourActivate(int *item, void *out, void *holder)
{
  if (item[5] == MARK)
  {
    if (BackPressed())
      LogMessage(LogLevel::Info, false, "Quit Game box answered No.");
    else
    {
      quitRequested = true;
      item[0] = YES;
      LogMessage(LogLevel::Info, false, "Quit Game to Desktop chosen.");
    }
  }
  return fnActivate(item, out, holder);
}

bool Hook(uintptr_t rva, const uint8_t *expected, size_t size, void *detour, void **original, const char *name)
{
  uintptr_t address = GetAbsoluteAddress(rva);
  if (memcmp((void *)address, expected, size) != 0)
    return LogMessage(LogLevel::Error, false, "%s: unexpected bytes, Quit Game to Desktop disabled", name), false;
  if (!original)
    return true;
  if (MH_CreateHook((LPVOID)address, detour, original) != MH_OK || MH_EnableHook((LPVOID)address) != MH_OK)
    return LogMessage(LogLevel::Error, false, "%s: hook failed", name), false;
  return true;
}
} // namespace

void InstallQuitHooks()
{
  static const uint8_t dialogBytes[] = {0x40, 0x53, 0x57, 0xB8, 0xB8, 0x10, 0x00, 0x00};
  static const uint8_t addItemBytes[] = {0x4C, 0x8B, 0xDC, 0x57, 0x48, 0x81, 0xEC, 0x80, 0x00, 0x00};
  static const uint8_t emptyJobBytes[] = {0x48, 0x89, 0x4C, 0x24, 0x08, 0x53, 0x48, 0x83, 0xEC, 0x30};
  static const uint8_t noticeBytes[] = {0x40, 0x55, 0x56, 0x57, 0x48, 0x8D, 0x6C, 0x24, 0xB9, 0x48, 0x81, 0xEC};
  static const uint8_t addCancelBytes[] = {0x40, 0x57, 0x48, 0x81, 0xEC, 0x90, 0x00, 0x00, 0x00};
  static const uint8_t selectBytes[] = {0x4C, 0x8B, 0xDC, 0x56, 0x57, 0x41, 0x56, 0x48, 0x81, 0xEC};
  static const uint8_t activateBytes[] = {0x48, 0x89, 0x54, 0x24, 0x10, 0x53, 0x56, 0x57, 0x48, 0x83};

  fnAddItem = (AddItemFn)GetAbsoluteAddress(ADD_ITEM);
  fnEmptyJob = (EmptyJobFn)GetAbsoluteAddress(EMPTY_JOB);
  if (Hook(ADD_ITEM, addItemBytes, sizeof(addItemBytes), nullptr, nullptr, "dialog item") &&
      Hook(EMPTY_JOB, emptyJobBytes, sizeof(emptyJobBytes), nullptr, nullptr, "empty job") &&
      Hook(DIALOG, dialogBytes, sizeof(dialogBytes), (void *)DetourDialog, (void **)&fnDialog, "dialog") &&
      Hook(ADD_CANCEL, addCancelBytes, sizeof(addCancelBytes), (void *)DetourAddCancel, (void **)&fnAddCancel, "dialog cancel item") &&
      Hook(CRASH_NOTICE, noticeBytes, sizeof(noticeBytes), (void *)DetourNotice, (void **)&fnNotice, "start-up notice") &&
      Hook(SELECT_ITEM, selectBytes, sizeof(selectBytes), (void *)DetourSelectItem, (void **)&fnSelectItem, "dialog selection") &&
      Hook(ACTIVATE, activateBytes, sizeof(activateBytes), (void *)DetourActivate, (void **)&fnActivate, "dialog item activation"))
    LogMessage(LogLevel::Success, false, "Quit Game to Desktop.");
}

bool QuitDialogLabel(unsigned id, const wchar_t *name, std::wstring &text)
{
  if (!labelWanted || id != YES_TEXT_ID || !name || wcscmp(name, L"FDP_Dialogues"))
    return false;
  const std::wstring &s = str::QuitToDesktop[GameLanguage()];
  text = s.empty() ? str::QuitToDesktop[EN] : s;
  return true;
}

void QuitToDesktopViewport(const char *name, bool isHud)
{
  if (isHud)
  {
    inGame = true;
    return;
  }
  if (!inGame || !IsTitle(name))
    return;

  inGame = false;
  if (!quitRequested)
    return;
  quitRequested = false;

  HWND window = GAME_WINDOW ? GAME_WINDOW : GetGameWindow();
  LogMessage(LogLevel::Info, false, "Quit Game to Desktop: back at the title screen, closing the game.");
  if (!window || !PostMessageW(window, WM_CLOSE, 0, 0))
    LogMessage(LogLevel::Error, false, "Quit Game to Desktop: could not close the game window (error %lu).", GetLastError());
}
