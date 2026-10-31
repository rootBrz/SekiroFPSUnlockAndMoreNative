#include "fixes.h"
#include "MinHook.h"
#include "hud.h"
#include "offset.h"
#include "utils.h"

#include <cmath>
#include <cstring>
#include <vector>

// Fixes for aspect ratios other than 16:9, the offsets and logic come from Lyall's SekiroFix
// (https://codeberg.org/Lyall/SekiroFix, MIT). SekiroFix hooks the middle of functions with safetyhook; here
// a small stub saves the registers, calls a C++ function with them and runs the overwritten instructions.

namespace
{
struct Ctx
{
  float xmm[8][4];
  uint64_t rax, rcx, rdx;
};
using MidCallback = void (*)(Ctx *);

constexpr float NATIVE_ASPECT = 16.0f / 9.0f;
constexpr size_t STUB_CALLBACK_OFFSET = 96; // imm64 of "mov rax, callback" in the stub

// pushfq, push volatile registers, save xmm0-7 and rax/rcx/rdx into a Ctx at rsp+0x20, call the callback, reload xmm0-7 and rdx
constexpr uint8_t stub[] = {
    0x9C, 0x50, 0x51, 0x52, 0x41, 0x50, 0x41, 0x51, 0x41, 0x52, 0x41, 0x53, 0x48, 0x81, 0xEC, 0x10, 0x01, 0x00, 0x00, 0x0F, 0x11, 0x44, 0x24, 0x20,
    0x0F, 0x11, 0x4C, 0x24, 0x30, 0x0F, 0x11, 0x54, 0x24, 0x40, 0x0F, 0x11, 0x5C, 0x24, 0x50, 0x0F, 0x11, 0x64, 0x24, 0x60, 0x0F, 0x11, 0x6C, 0x24,
    0x70, 0x0F, 0x11, 0xB4, 0x24, 0x80, 0x00, 0x00, 0x00, 0x0F, 0x11, 0xBC, 0x24, 0x90, 0x00, 0x00, 0x00, 0x48, 0x89, 0x84, 0x24, 0xA0, 0x00, 0x00,
    0x00, 0x48, 0x89, 0x8C, 0x24, 0xA8, 0x00, 0x00, 0x00, 0x48, 0x89, 0x94, 0x24, 0xB0, 0x00, 0x00, 0x00, 0x48, 0x8D, 0x4C, 0x24, 0x20, 0x48, 0xB8,
    0, 0, 0, 0, 0, 0, 0, 0, // callback
    0xFF, 0xD0, 0x48, 0x8B, 0x84, 0x24, 0xB0, 0x00, 0x00, 0x00, 0x48, 0x89, 0x84, 0x24, 0x30, 0x01, 0x00, 0x00, // mov rax,[rsp+0xB0]; mov [rsp+0x130],rax
    0x0F, 0x10, 0x44, 0x24, 0x20, 0x0F, 0x10, 0x4C, 0x24, 0x30, 0x0F, 0x10, 0x54, 0x24, 0x40, 0x0F, 0x10, 0x5C, 0x24, 0x50,
    0x0F, 0x10, 0x64, 0x24, 0x60, 0x0F, 0x10, 0x6C, 0x24, 0x70, 0x0F, 0x10, 0xB4, 0x24, 0x80, 0x00, 0x00, 0x00, 0x0F, 0x10, 0xBC, 0x24, 0x90, 0x00,
    0x00, 0x00, 0x48, 0x81, 0xC4, 0x10, 0x01, 0x00, 0x00, 0x41, 0x5B, 0x41, 0x5A, 0x41, 0x59, 0x41, 0x58, 0x5A, 0x59, 0x58, 0x9D};

static_assert(sizeof(stub) == 187 && stub[STUB_CALLBACK_OFFSET - 2] == 0x48 && stub[STUB_CALLBACK_OFFSET - 1] == 0xB8, "stub layout");

// Runs callback before the instructions at `offset`. `original` are the whole instructions that get moved, they must
// not be position dependent.
bool MidHook(uintptr_t offset, const uint8_t *original, size_t size, MidCallback callback, const char *name)
{
  uintptr_t target = GetAbsoluteAddress(offset);
  if (memcmp((void *)target, original, size) != 0)
    return LogMessage(LogLevel::Error, false, "%s: unexpected bytes, fix disabled", name), false;

  uintptr_t cave = AllocateMemoryNearAddress(target, sizeof(stub) + size + 5, true);
  if (!cave)
    return false;

  std::vector<uint8_t> code(stub, stub + sizeof(stub));
  uint64_t callback64 = (uint64_t)callback;
  memcpy(&code[STUB_CALLBACK_OFFSET], &callback64, sizeof(callback64));
  code.insert(code.end(), original, original + size);
  code.push_back(0xE9);
  int32_t back = (int32_t)((target + size) - (cave + code.size() + 4));
  code.insert(code.end(), (uint8_t *)&back, (uint8_t *)&back + 4);
  if (!WriteProtectedMemory(cave, code.data(), code.size(), name))
    return false;

  std::vector<uint8_t> jump = RelJmpBytes((int)size, (int32_t)(cave - (target + 5)));
  return WriteProtectedMemory(target, jump.data(), jump.size(), name);
}

// the picture (render target) of the render system, which the aspect ratio option letterboxes
float pictureWidth, pictureHeight, pictureAspect = NATIVE_ASPECT;

bool PictureWider() { return pictureAspect > NATIVE_ASPECT + 0.001f; }
// the awareness marker fixes are for a HUD on the 16:9 stage (kept at 16:9 or at the edges); a stretched one needs none
bool Wider() { return HUD_LAYOUT != HUD_STRETCH && PictureWider(); }
bool Narrower() { return HUD_LAYOUT != HUD_STRETCH && pictureAspect < NATIVE_ASPECT - 0.001f; }

// Animations are decimated too early because the culling call gets 16:9
void AnimationCulling(Ctx *ctx)
{
  if (PictureWider())
    ctx->xmm[3][0] = pictureAspect;
}

// Scaleform movie placement, edx is the view scale mode: 1 ShowAll (keeps 16:9), 2 ExactFit (fills the viewport).
// Movies are created with ShowAll, so the stretched HUD is made by switching the mode here, for every movie. The black
// screen, the fade and the vignettes always fill the viewport. xmm5 and xmm7 are the viewport width and height in twips (20 per pixel).
void HudViewport(Ctx *ctx)
{
  if (HUD_LAYOUT == HUD_STRETCH && ctx->rdx == 1)
    ctx->rdx = 2;
  else if (HUD_LAYOUT != HUD_STRETCH && ctx->rdx == 2)
    ctx->rdx = 1;

  uint64_t namePtr = 0;
  char name[64] = {};
  if (!ctx->rax || !ReadProcessMemory(GetCurrentProcess(), (LPCVOID)(ctx->rax + 0x48), &namePtr, sizeof(namePtr), nullptr) || !namePtr ||
      !ReadProcessMemory(GetCurrentProcess(), (LPCVOID)(namePtr + 0xB), name, sizeof(name) - 1, nullptr))
    return;

  // the HUD movie always, other movies when the viewport size changed
  static float lastWidth, lastHeight;
  bool isHud = strstr(name, "01_000_fe.gfx") != nullptr;
  if (isHud || ctx->xmm[5][0] != lastWidth || ctx->xmm[7][0] != lastHeight)
  {
    lastWidth = ctx->xmm[5][0];
    lastHeight = ctx->xmm[7][0];
    const uint32_t *res = (const uint32_t *)GetAbsoluteAddress(render_resolution_offset);
    LogMessage(LogLevel::Info, false, "Viewport %s %.0fx%.0f, render %ux%u, mode %llu, layout %d.", name, lastWidth / 20.0f, lastHeight / 20.0f, res[0],
               res[1], (unsigned long long)ctx->rdx, HUD_LAYOUT);
  }

  if (isHud)
  {
    // the HUD can be created with a 1920 x 1080 viewport that the game does not update for the render resolution
    // (e.g. fullscreen 2560 x 1080); the movies are given the render resolution again then
    const uint32_t *resolution = (const uint32_t *)GetAbsoluteAddress(render_resolution_offset);
    bool differs = fabsf(ctx->xmm[5][0] / 20.0f - resolution[0]) > 1 || fabsf(ctx->xmm[7][0] / 20.0f - resolution[1]) > 1;
    // not while the window is minimized or switching mode (zero size): the movies would be given an empty viewport
    if (differs && ctx->xmm[5][0] > 0 && ctx->xmm[7][0] > 0 && resolution[0] > 0 && resolution[1] > 0)
      RelayoutHud();
    else if (differs)
      LogMessage(LogLevel::Info, false, "HUD relayout skipped, empty size.");
  }
  MovieViewport(name, ctx->xmm[5][0], ctx->xmm[7][0]);

  if (strstr(name, "01_910_fade.gfx") || strstr(name, "01_900_black.gfx") || strstr(name, "01_200_dyingeffect.gfx") ||
      strstr(name, "01_201_stealtheffect.gfx"))
    ctx->rdx = 2;
}

// Awareness markers are positioned in 1080 high units with the 1920 wide stage centered, so their bounds are widened
void AwarenessTransitionHor(Ctx *ctx)
{
  if (Wider())
  {
    ctx->xmm[0][0] = -((1080.0f * pictureAspect) - 1920.0f) / 2.0f;
    ctx->xmm[1][0] = 1080.0f * pictureAspect;
  }
}

void AwarenessTransitionVert(Ctx *ctx)
{
  if (Narrower())
  {
    ctx->xmm[0][0] = -((1920.0f / pictureAspect) - 1080.0f) / 2.0f;
    ctx->xmm[4][0] = 1920.0f / pictureAspect;
  }
}

void AwarenessCulling(Ctx *ctx)
{
  if (Wider())
    ctx->xmm[2][0] += ((1080.0f * pictureAspect) - 1920.0f) / 2.0f;
  else if (Narrower())
    ctx->xmm[3][0] += ((1920.0f / pictureAspect) - 1080.0f) / 2.0f;
}
// The Scaleform manager updates all movie players (a pointer array at +2528, count at +2792) every frame. A movie only
// places itself again when its viewport is set, so a changed HUD stretch is applied by setting the viewport of each player.
using ManagerUpdateFn = uint64_t (*)(uint8_t *manager, void *a2, void *a3, void *a4);
using PlayerViewportFn = void (*)(uint8_t *player, void *rect);
ManagerUpdateFn fnManagerUpdate;
PlayerViewportFn fnPlayerViewport;
volatile bool hudRelayout;

uint64_t DetourManagerUpdate(uint8_t *manager, void *a2, void *a3, void *a4)
{
  // stays pending while the render resolution is zero (minimized), so it runs once the window is back
  uint32_t *width = (uint32_t *)GetAbsoluteAddress(render_resolution_offset);
  static bool deferredLogged;
  if (hudRelayout && !(width[0] > 0 && width[1] > 0) && !deferredLogged)
  {
    deferredLogged = true;
    LogMessage(LogLevel::Info, false, "HUD relayout waits, render size %ux%u.", width[0], width[1]);
  }
  if (hudRelayout && width[0] > 0 && width[1] > 0)
  {
    hudRelayout = false;
    deferredLogged = false;

    uint64_t count = *(uint64_t *)(manager + 2792);
    uint8_t *players = manager + 2528 + ((0 - (uintptr_t)(manager + 2528)) & 7);
    // a movie ignores a viewport equal to its current one, so go through a slightly wider viewport first
    uint32_t actual = *width;
    LogMessage(LogLevel::Info, false, "HUD relayout of %llu movie players at %ux%u.", (unsigned long long)count, actual, width[1]);
    for (uint32_t w : {actual + 1, actual})
    {
      *width = w;
      for (uint64_t i = 0; i < count && i < 64; i++)
        if (uint8_t *player = *(uint8_t **)(players + 8 * i); IsValidReadPtr((uintptr_t)player, 32))
        {
          alignas(16) uint8_t rect[128] = {};
          fnPlayerViewport(player, rect);
        }
    }
  }
  return fnManagerUpdate(manager, a2, a3, a4);
}
} // namespace

void RelayoutHud()
{
  LogMessage(LogLevel::Info, false, "HUD relayout requested.");
  hudRelayout = true;
}

void InstallFixes()
{
  InstallHudLayout();
  constexpr uint8_t animation[] = {0xF3, 0x0F, 0x10, 0x53, 0x50};             // movss xmm2,[rbx+50h]
  constexpr uint8_t viewport[] = {0x8B, 0xCA, 0x0F, 0x5B, 0xC9};              // mov ecx,edx; cvtdq2ps xmm1,xmm1
  constexpr uint8_t transitionHor[] = {0xF3, 0x0F, 0x10, 0x54, 0x24, 0x50};   // movss xmm2,[rsp+50h]
  constexpr uint8_t transitionVert[] = {0xF3, 0x0F, 0x10, 0x4C, 0x24, 0x54};  // movss xmm1,[rsp+54h]
  constexpr uint8_t culling[] = {0x0F, 0x2F, 0xD4, 0x0F, 0x29, 0x24, 0x24};   // comiss xmm2,xmm4; movaps [rsp],xmm4

  constexpr uint8_t managerUpdate[] = {0x48, 0x8B, 0xC4, 0x55, 0x56, 0x57, 0x41, 0x54};
  uintptr_t manager = GetAbsoluteAddress(scaleform_manager_update_offset);
  if (memcmp((void *)manager, managerUpdate, sizeof(managerUpdate)) != 0 ||
      MH_CreateHook((LPVOID)manager, (LPVOID)&DetourManagerUpdate, (LPVOID *)&fnManagerUpdate) != MH_OK || MH_EnableHook((LPVOID)manager) != MH_OK)
    LogMessage(LogLevel::Error, false, "Scaleform manager hook failed, the HUD stretch only applies on a resolution change");
  fnPlayerViewport = (PlayerViewportFn)GetAbsoluteAddress(scaleform_player_viewport_offset);

  MidHook(animation_culling_offset, animation, sizeof(animation), AnimationCulling, "Animation culling");

  MidHook(hud_vignette_offset, viewport, sizeof(viewport), HudViewport, "HUD vignettes");
  MidHook(awareness_transition_hor_offset, transitionHor, sizeof(transitionHor), AwarenessTransitionHor, "Awareness markers transition");
  MidHook(awareness_transition_vert_offset, transitionVert, sizeof(transitionVert), AwarenessTransitionVert, "Awareness markers transition");
  MidHook(awareness_culling_offset, culling, sizeof(culling), AwarenessCulling, "Awareness markers culling");
}

// Logs the window and picture state whenever it changes (focus, minimize, resize, screen mode): what the game and the mod saw around an Alt+Tab
static void LogWindowState()
{
  static char last[256];
  char now[256];
  RECT window = {}, client = {};
  GetWindowRect(GAME_WINDOW, &window);
  GetClientRect(GAME_WINDOW, &client); // the borderless client size the game gets, not the real one
  const uint32_t *res = (const uint32_t *)GetAbsoluteAddress(render_resolution_offset);
  snprintf(now, sizeof(now), "window %p foreground %d iconic %d rect %ld,%ld %ldx%ld client %ldx%ld, render %ux%u, picture %.0fx%.0f, fullscreen %d.",
           (void *)GAME_WINDOW, GetForegroundWindow() == GAME_WINDOW, IsIconic(GAME_WINDOW), window.left, window.top, window.right - window.left,
           window.bottom - window.top, client.right, client.bottom, res[0], res[1], pictureWidth, pictureHeight, FULLSCREEN_STATE);
  if (strcmp(now, last) != 0)
  {
    strcpy_s(last, now);
    LogMessage(LogLevel::Info, false, "State: %s", now);
  }
}

void UpdateFixes()
{
  LogWindowState();
  const uint8_t *system = *(const uint8_t **)GetAbsoluteAddress(render_system_offset);
  if (!IsValidReadPtr((uintptr_t)system + 160, sizeof(uintptr_t)))
    return;
  const uint8_t *output = *(const uint8_t **)(system + 160);
  if (!IsValidReadPtr((uintptr_t)output + 272, 2 * sizeof(uint32_t)))
    return;

  uint32_t width = *(const uint32_t *)(output + 272), height = *(const uint32_t *)(output + 276);
  if (width && height)
  {
    pictureWidth = (float)width;
    pictureHeight = (float)height;
    pictureAspect = pictureWidth / pictureHeight;
  }
}
