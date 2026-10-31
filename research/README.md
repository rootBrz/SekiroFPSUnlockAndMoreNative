# Research notes: Scaleform movies, HUD layout, sound, language, refresh rate

Notes from reversing Sekiro 1.06 (Steam) for the 1.2.0 features, kept so that a movie the HUD Layout misses, or a
new feature in the same area, can be handled quickly. Addresses are RVAs from the image base `0x140000000` unless
written in full. `tools/` has the scripts used; `movies.txt` lists the 116 movie names found in the executable.

## Getting at the code

- `sekiro.exe` on disk is encrypted by SteamStub (section `.bind`); disassembling the file gives garbage. Dump the
  decrypted image from a running game: `python tools/dumpexe.py <pid> sekiro_dump.exe`. The dump has its sections laid
  out as in memory, so file offset = RVA. The python tools read it from the `SEKIRO_DUMP` environment variable.
- Ghidra (12.2, JDK 25): full auto analysis of the 68 MB dump runs out of a 12 GB heap. Import with
  `tools/LightAnalysis.java` as pre script and `GHIDRA_MAXMEM=20G`; functions come from `.pdata` and decompilation
  works. `tools/Decompile.java` decompiles the functions containing given addresses into `decompiled.c`:
  `analyzeHeadless <dir> sekiro -process sekiro_dump.exe -noanalysis -readOnly -scriptPath tools -postScript Decompile.java 140d9cd90`.
  Ghidra 12 also needs `XDG_CONFIG_HOME` pointed at a folder of its own when the user settings folder trips its OSGi cache.
- Quick looks without Ghidra: `tools/disasm.py <rva> <len>`, `tools/pdata.py <rva>` (function bounds),
  `tools/xrefs.py <rva>` (direct calls), `tools/strref.py <text>` (code using a string, ascii and utf-16).
- In a running game: `tools/rstate.py`, `tools/peek.py`, `tools/memsearch.py`, `tools/findptr.py`, `tools/modes.py`.

## Scaleform in Sekiro

- Scaleform 4.x, every movie is **AS3** (`x`, `y`, `width`, `height`, `alpha`, `visible`, `numChildren`,
  `getChildAt`, `getBounds`, `root`, `parent`, `loaderInfo.url`, `name` all work through the ObjectInterface).
- Every movie has a 1920 x 1080 stage. Movies are created with the scale mode ShowAll, so on a picture that is not
  16:9 there is room around the stage: `(1080 * w / h - 1920) / 2` to the left and right, or
  `(1920 * h / w - 1080) / 2` above and below, in stage units.
- `GFx::Value` as the game lays it out (40 bytes): `int64 link[2]` (a list the movie keeps of managed values, -1 when
  not linked), `ObjectInterface *oi` (+0x10), `uint32 type` (+0x18; mask 0x8f: 2 bool, 3 int, 5 number, 6 string,
  8 object, 10 display object; bit 0x40 managed), data (+0x20). **Never copy a managed value**: the list holds its
  address, and a copied one corrupts Scaleform's lists (crashes much later, in hash lookups). Fill values in place and
  release them in place.
- ObjectInterface vtable slots (as in the public SF4 headers): 1 ObjectAddRef, 2 ObjectRelease(oi, value, data),
  4 GetMember(oi, data, name, out, isDisplayObject), 5 SetMember, 6 Invoke(oi, data, result, name, args, count,
  isDisplayObject), `0xD8` GetDisplayInfo (SetDisplayInfo is `FUN_141108cc0`), `0x148`/`0x150` go to a frame / label.
  The game's own helpers: `FUN_140c548c0(wrapper, visible)`, `FUN_1411073a0(value, alpha)`,
  `FUN_140c65c90(wrapper, frame)`, `FUN_140c65ce0(wrapper, label)`. These go through Get/SetDisplayInfo, **not**
  Get/SetMember, so hooks on slots 4-6 do not see a menu that is only shown, hidden or played.
- A string value: its data points to a node whose first field is the text when the value is managed, else to the text.
- The movie a display object is in: `root.loaderInfo.url`, e.g. `data1:/menu/01_000_fe.gfx` (lower case). This is how
  `src/hud.cpp` recognises movies; root addresses get reused by new movies after a reload.

## Game wrappers and hook points

| RVA | What |
| --- | --- |
| `0xC66B60` | The game's lookup of a clip by name: `(parent wrapper, out wrapper, name)`. A wrapper keeps its `GFx::Value` at +0x30; the name object holds the text inline, or on the heap at `[+0x38] + 0x10`. Used to learn the AS3 ObjectInterface vtable. |
| `0xC66510` | `GetChild(parent, out, printf format, ...)`, the game's usual way to bind clips by name. |
| wrapper `vtable[1]` | Returns a holder whose `GFx::Value` starts at +8 (oi +0x18, type +0x20, data +0x28). |
| `0x209C5CE` | Scaleform `MovieImpl::UpdateViewport`. The mod's mid hook at `0x209C622` (`HudViewport` in `src/fixes.cpp`) gets the movie definition in `rax` (name: `[[rax+0x48]+0xB]`), the scale mode in `rdx` (1 ShowAll, 2 ExactFit, 3 NoBorder), the viewport width / height in twips in `xmm5` / `xmm7`, and the movie in `r8` (in the stub's saved registers at `Ctx + 0x108`). |
| `[[game movie + 0x8] + 0x50]` | The Scaleform movie (`r8` above) of the game's movie object (wrapper +0x20). |
| `0x10FDE10` | Scaleform manager update, every frame: players at +2528 (aligned), count at +2792. |
| `0x10F1F00` | `(player, rect)`: sets a player's viewport to the render resolution (`0x3B1D100`); the mod's `RelayoutHud`. |

### Finding the class behind a movie

The menu objects are made by factories. To get from a movie to the code that drives it:

1. `strref.py <file name>` finds the loader by the utf-16 file name (`01_210_Dead` -> `FUN_140d97150`).
2. The loader gets a `std::function` factory; slot 2 of its vtable (`0x142AFB758` for the death screen) jumps to the
   creator (`FUN_140d93870`), which allocates the object and sets its vtable (`0x142AFAC20`).
3. Slot 2 of that vtable is the per frame `Update` (`FUN_140d9cd90`). The base menu (`FUN_140c5fd00`, vtable
   `0x142AAD3A8`) keeps the wrapper of the movie root at +0x158.

Classes with the same clip names exist: `FUN_140d902e0` (vtable `0x142AFAB00`, Update `0xD9CFA0`) also binds
`Begin`, `WaitBegin`, `WaitBegin_TextEffect`, `WaitEnd`, but it is not the death screen: it is `01_200_DyingEffect`,
whose clips (`Loop/instance1`, ...) the game fades by alpha every frame. Go from the file name.

## Movies

| Movie | Done | Why |
| --- | --- | --- |
| `01_000_fe` | Screen Edges moves clips (table below) | The whole in-game HUD |
| `01_002_fe_saveicon` | Screen Edges: each root clip to its nearest edges | Corner icon |
| `01_900_black`, `01_910_fade`, `01_200_dyingeffect`, `01_201_stealtheffect` | ExactFit always | Full screen fills and vignettes |
| `02_903_nowloading2`, `02_904_nowloading3`, `02_905_nowloading_thanks`, `02_990_nowloading_userguide` | `instance1` (black, top left registration) stretched | Loading screen background |
| `01_210_dead` | Backdrops (their innermost clips) stretched in its own Update hook (`0xD9CD90`), every frame while shown | Its timeline brings `WaitBegin/instance1` in |
| `03_202_bonfire`, `02_280`-`02_284_getitem*`, `02_210_fullscreenmsgdialog`, `01_010`/`01_011`/`01_013_messagebox*`, `01_250_tutorialmessage`, `01_251_tutorialmsgdialog` | Unnamed clips that cover the stage stretched (found with getBounds) | Semi-transparent black behind menus and notifications |
| `01_060_caption`, `01_061_talk`, `01_210` texts, other `02_*` / `03_*` menus, `05_*` title | Nothing | Centred content; stretching distorts it |

A missed background: dump the movie's tree (`tools/scaleform_tree_dump.cpp`), find the clip of about
`[0,0 1920x1080]`, and add the movie to `jobs` in `src/hud.cpp` (`JOB_BACKDROPS` for unnamed clips, or a named path
like the loading screens). If the movie is played by its timeline without the game calling into it (no effect until a
reload), hook its class's Update as for the death screen.

### Clip trees (stage units: x y of the clip, width height)

`01_000_fe` root (looked up by the game): `PlayerHUD` (960, 537; holds `Gauge` with `HP`, `SP`, `SPFrame`, `Damage` the player's
posture, `Resurrection2`, `Equip`, and `ItemPanel` with `Item_Top/Bottom/Left/Right` and their names), `BossList` and
`SemiBossList` (`Item_N_0` with `HP`, `HPGauge`, `Name` on the left and `SP`, `SPFrame`, `SPCenter`, `Damage` the
posture in the centre), `Money` (1702, 748), `SkillPoint` (1484, 88), `BloodGhostRec` (1692, 140), `PlayerCount`
(1850, 995), `GetItemList` (1502, 695). Placed from the 3D scene every frame, not to be moved: `EnemyTag0-7`,
`EnemyPartsTag0-7`, `WatchTarget0-7`, `LockOn`, `ActionSpot0-5`, `FindIcon0-7`, `WireTarget0-1`, `RemotePCTag0-6`.
Centred: `MapName`, `ActionGuide`, `CautionText`, `TextEffect`, `TalkWindow`.

`02_903_nowloading2`: `instance1` [0,0 1930x1090] black, `BackImage` [0,0 2172x1086] artwork, `Main` (1022, 496) text,
`Gauge` (180, 475).

`01_210_dead`: `WaitBegin` [0,0 1920x1080, alpha 0 until death] with `instance1` [0,0 1920x1080] the red vignette, `instance2`
the kanji (802, 339, 322x276), `Text` (229, 656), `Text_3` (116, 850), `Stagnation` (`LifeCount`, `Crack/State_0-4`).
The Update runs a state machine on `[+0xAB8]`, shows the root and plays a label through the wrapper at +0x170.
`WaitBegin`, `Begin`, `Loop`, `WaitEnd` and `instance25` (the fade to black), all [0,0 1920x1080], each hold one
unnamed clip and are faded in by their timeline, through the alpha of the outer clip. A clip whose `x`, `y`, `width` or
`height` is set by script is no longer animated by its timeline, its alpha included, so only the innermost clips that
cover the stage are stretched; stretching the outer ones kept the vignettes at alpha 0.

`03_202_bonfire`: `instance1` [960,540 1920x1080, centred] the dimming, `Base`, `MenuTitle`, `InfoPanel`.
`01_010_messagebox`: `instance1` [960,540 1920x1080], `ItemList`. `01_250_tutorialmessage`: `MessageWindow_0`,
`MessageTextureWindow_0`, `LineMessageWindow_0` (no full stage clip).

## Other findings

- **Render**: `0x3F3B400` render system; `+160` the output, its picture size at `+272`/`+276`; `+0x200`/`+0x204` the
  target size; `+0x218 + 40 * n` the aspect ratio pairs. The aspect ratio hook (`0x12ABD0`) is installed after the
  targets are built, so they are built again at startup when a ratio is chosen. A HUD can be created with a
  1920 x 1080 viewport the game does not update (fullscreen 2560 x 1080), see `HudViewport`.
- **Refresh rate**: the swap chain is not reachable from the render system objects (the DXGI object at `+0xC0` is not
  one). `IDXGISwapChain::Present` is hooked through a swap chain made on a hidden window, and `ResizeTarget` runs in
  it. The 60 hz immediates of Lyall's SekiroFix (`0x12CAAF`, `0x128FFFA`) and the numerator / denominator detour at
  `0x1B62D3F` take the chosen rate.
- **Sound**: `GameDataMan` `0x3D5AAC0` -> `+0x50` option data, bytes `+4..+6` music, sound effects, voice (0-10);
  `0x92F330 (dt)` applies them every frame. Before the save loads they hold the defaults (7). FMOD Ex: categories
  `master`, `music`, `Menu`, `SE`, `Voice`, `Default`.
- **Language**: `0x3D9C868` int32 locale, an index into `jpnJP, engGB, fraFR, itaIT, deuDE, spaES, spaAR, porBR,
  engUS, nldNL, polPL, rusRU, danDK, norNO, finFI, sweSE, turTR, porPT, zhoTW, korKR, zhoCN, thaTH` (`FUN_1411527e0`).
  Sekiro ships 13 of them (Steam store list).
