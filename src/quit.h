#pragma once

// Quit to Desktop: the game's Quit Game saves and goes back to the title screen. With the option on, the game is closed
// as soon as the title screen comes up after the player was in the game, so the save is done by then.
// Called with the file name of every movie that gets a viewport (e.g. "data1:/menu/01_000_fe.gfx") and whether it is
// the in-game HUD.
void QuitToDesktopViewport(const char *name, bool isHud);
