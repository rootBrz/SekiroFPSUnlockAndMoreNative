#pragma once
#include <string>

// Quit Game to Desktop: a third choice in the confirmation of the pause menu's Quit Game (Yes, No, Quit Game to Desktop).
// It does what Yes does (save, back to the title screen) and the game is closed as soon as the title screen comes up,
// so the save is done by then.
void InstallQuitHooks();
// The text of that choice, for the game's text lookup (id and name of the lookup); false for any other lookup.
bool QuitDialogLabel(unsigned id, const wchar_t *name, std::wstring &text);
// Called with the file name of every movie that gets a viewport (e.g. "data1:/menu/01_000_fe.gfx") and whether it is
// the in-game HUD.
void QuitToDesktopViewport(const char *name, bool isHud);
