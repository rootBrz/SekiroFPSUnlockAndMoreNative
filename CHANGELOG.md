# Changelog

Each release needs a `## <version>` section here matching `VERSION`. The release workflow publishes that section as the GitHub release notes and fails if it is missing.

## 1.5.0

- With HUD Layout Screen Edges, the frame of the player's posture bar and its posture break effects stay in the centre with the bar; they were moved to the left with the health bar on wider pictures.

## 1.4.0

- The red vignette and the fade to black of the death screen show again with HUD Layout 16:9 or Screen Edges on a picture wider than 16:9 (e.g. 21:9 or Fill Screen on an ultrawide); they only showed with Stretch. Their backgrounds still cover the whole picture.

## 1.3.0

- HDR works in windowed and borderless mode, not only in exclusive fullscreen, when HDR is turned on for the display in Windows. The game's swap chain is then created with the flip model and shown with the HDR10 colour space, also in fullscreen; without Windows HDR nothing changes. `EnableWindowedHdr = 0` in the INI turns it off, e.g. if another mod that hooks the swap chain does not get along with it.
- Disable Camera Autorotate no longer takes away the gamepad's camera stick: small tilts turned nothing and a full tilt only turned the camera after a delay. It now works the same with a gamepad and a mouse.

## 1.2.0

- The mod's settings rows are shown in the game's language: English, French, Italian, German, Spanish, Portuguese (Brazil), Polish, Russian, Japanese, Korean, Simplified Chinese, Traditional Chinese or Thai.
- HUD Layout replaces Stretch HUD, with a new choice, Screen Edges (the default): on a picture that is not 16:9 the HUD elements move to its edges, the health, boss bars and counters to the sides on wider pictures and to the top and bottom on narrower ones, while the posture bars stay in the centre. Stretch and 16:9 work as Stretch HUD on and off did. The setting stays under the INI key `StretchHud` (0 = 16:9, 1 = Stretch, 2 = Screen Edges).
- The black screen, the fades and the vignettes always cover the whole picture, and the black backgrounds of the loading screens, the death screen, the idol menu and the notifications (items, skills, events, messages, tutorials) cover it too. The save icon moves to its corner with Screen Edges.
- Refresh Rate in the graphics settings: Auto (left to the driver) or a rate of the display, applied right away in fullscreen.
- A chosen Aspect Ratio applies from the start; it applied only after a setting was changed.
- The HUD follows the render resolution also when the game creates it with another size, e.g. in fullscreen 2560 x 1080.

## 1.1.1

- The game starts with your sound volumes instead of its defaults, so the logos and the title no longer play loud until the save is loaded. The music, sound effects and voice volumes are remembered in `SekiroFPSUnlockAndMoreNative.ini` whenever they change; the first start after updating still uses the defaults.

## 1.0

- First release, based on SekiroImGui. The settings are rows in the game's own Settings menu instead of an overlay window, saved to `SekiroFPSUnlockAndMoreNative.ini` (log: `SekiroFPSUnlockAndMoreNative.log`).
- FPS unlock (up to 300 fps, also on loading screens) with the running and sprint speed fixes, VSync, autoloot, logos skip, death penalties and dragonrot toggles, camera reset and autorotate toggles, field of view.
- Screen Mode offers Borderless Windowed. A resolution change in borderless mode is applied to a plain window first and the screen is covered again afterwards, also when the resolution is the screen's own.
- Aspect Ratio (fill, 4:3, 16:10, 16:9, 21:9) and Stretch HUD. Stretch HUD applies at once, no resolution change needed. Off keeps the HUD at 16:9, with the vignettes, fades, awareness markers and animation culling fixed for other picture shapes.
- The game window is followed when the game recreates it, so changing the resolution does not close the game. Nothing is hooked or patched before the game has finished starting up.
