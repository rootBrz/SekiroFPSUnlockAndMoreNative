# Sekiro FPS Unlock And More Native

Based on <https://github.com/rootBrz/SekiroImGui>

A mod for Sekiro: Shadows Die Twice (Steam, version 1.06). Its settings are rows in the game's own Settings menu, so there is no overlay and no separate program.

## Install

Copy `d3dcompiler_43.dll` into the game folder next to `sekiro.exe`. The game loads it in place of its own `d3dcompiler_43.dll`, which it forwards to the system one. Delete the file to uninstall.

On Linux/Proton nothing else is needed.

## Settings

The mod's rows are shown in the game's language: English, French, Italian, German, Spanish, Portuguese (Brazil), Polish, Russian, Japanese, Korean, Simplified Chinese, Traditional Chinese or Thai.

All settings are in the game's **Settings** menu (main menu and pause menu) and saved to `SekiroFPSUnlockAndMoreNative.ini` next to the game. The log is `SekiroFPSUnlockAndMoreNative.log`.

| Page             | Options                                                                                          |
| ---------------- | ------------------------------------------------------------------------------------------------ |
| Game Options     | Autoloot, Skip Logos (restart), Skip Press Any Button, Disable Death Penalties, Disable Dragonrot |
| Camera Options   | Disable Camera Reset, Disable Camera Autorotate, Field of View, Hide Lock-on Dot                 |
| Graphics Options | FPS Limit (unlock), VSync, Aspect Ratio, HUD Layout                                              |

- **Screen Mode** also offers Borderless Windowed: the window always covers the screen while the game renders at the selected resolution.
- **Screen Resolution** lists every resolution the display supports, also in windowed mode.
- **HDR** also works in windowed and borderless mode when HDR is turned on for the display in Windows (the game only has it in exclusive fullscreen). The game's swap chain is then created with the flip model; `EnableWindowedHdr = 0` in the INI turns this off, e.g. if another mod that hooks the swap chain does not get along with it.
- **Aspect Ratio** letterboxes the picture to 4:3, 16:10, 16:9 or 21:9, or fills the screen.
- **HUD Layout** fits the HUD to a picture that is not 16:9. **Screen Edges** (default) moves its elements to the edges of the picture: the health, the boss bars and the counters to the sides on wider pictures and to the top and bottom on narrower ones, with the posture bars kept in the centre. **Stretch** stretches the HUD over the whole screen. **16:9** keeps it at 16:9. The vignettes and awareness markers are fixed for wider and narrower pictures, and the black screen, the fades and the black background of the loading screens always cover the whole picture.
- The game starts with your sound volumes instead of its defaults, which it would otherwise use until the save is loaded (the logos and the title played loud). The volumes are remembered in the INI (`SoundVolume*`) whenever they change; the first start after installing still plays at the defaults.
- **Quit Game to Desktop** is a third choice in the confirmation of the pause menu's **Quit Game** (Yes, Quit Game to Desktop; B or Esc answers No). It saves like Yes and goes back to the title screen; the mod closes the window (like Alt+F4) as soon as the title screen comes up, once the save is done, so there is no second Quit Game at the title and no wait for the network connection there.
- **Skip Press Any Button** (on by default) goes from the logos to the main menu without waiting for the title screen or a button press, without the Information dialog and without the notice at the start that the game was not closed with Quit Game (e.g. after Alt+F4). It takes effect at once. The INI key is `EnablePressStartSkip`.
- The frame rate is unlocked up to 300 fps, with the game's speed fixes (running speed, sprint speed) applied. The frame rate is also unlocked on loading screens.

## Build

Needs `x86_64-w64-mingw32-g++` and `make`. `7z` is needed for `make dist`.

```
make                              # build/d3dcompiler_43.dll
make install SEKIRO_PATH=<game>   # also copies it into the game folder
make dist                         # build/SekiroFPSUnlockAndMoreNative.7z
```

## Research

`research/` has notes on the game's Scaleform movies (which ones the HUD Layout handles and how to find the code
behind one), the addresses the features use, and the scripts used to find them.

## Release

Bump `VERSION` on `main` and add a `## <version>` section to `CHANGELOG.md`; the release workflow builds the archive and publishes it as a GitHub release with that section as notes. A manual run of the workflow only builds, and publishes nothing.

## License

GPLv3, see `LICENSE`.

## Credits

- Lyall - for SekiroFix (<https://codeberg.org/Lyall/SekiroFix>, MIT): animation culling fix for wider aspect ratios, sprint speed fix above 60 fps, fullscreen 60 hz startup patches, and the idea of an unrestricted resolution list.
- MDAWec - for sound fix, translations, refresh rate change and hud screen edges mod.
- Uberhalit - for SekiroFpsUnlockAndMore.
- arsura - for the Quit Game to Desktop idea and the first version of it.
- Katalash - for logos skip offset from Sekiro Mod Engine.
- Thevayl - inspiration for FPS unlock during loading screens.
- Cielos - for some camera adjustment offsets.
- jackfuste - for FOV offset and basic running speed fix.
- TsudaKageyu - for MinHook.
- metayeti - for mINI.
