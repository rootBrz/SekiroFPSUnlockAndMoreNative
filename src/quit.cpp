#include "quit.h"
#include "utils.h"

#include <cstring>

namespace
{
// the in-game HUD had a viewport since the title screen was last seen
bool inGame;

bool IsTitle(const char *name) { return strstr(name, "05_000_title.gfx") || strstr(name, "05_002_title_goty.gfx"); }
} // namespace

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
  if (!QUIT_TO_DESKTOP_ENABLED)
    return;

  HWND window = GAME_WINDOW ? GAME_WINDOW : GetGameWindow();
  LogMessage(LogLevel::Info, false, "Quit to Desktop: back at the title screen, closing the game.");
  if (!window || !PostMessageW(window, WM_CLOSE, 0, 0))
    LogMessage(LogLevel::Error, false, "Quit to Desktop: could not close the game window (error %lu).", GetLastError());
}
