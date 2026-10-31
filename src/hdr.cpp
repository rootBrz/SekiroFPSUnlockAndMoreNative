#include "hdr.h"
#include "MinHook.h"
#include "offset.h"
#include "utils.h"

#include <atomic>
#include <cstring>
#include <dxgi1_6.h>

// The game makes its swap chain HDR capable (R10G10B10A2, flag 4 in its own flags) whenever its NvAPI or AGS backend
// finds an HDR display, in any screen mode, but only turns the HDR output on in exclusive fullscreen: there the driver
// switches the display to HDR10 (PQ, BT.2020). In a window DWM composes the picture, so the swap chain has to be a flip
// model one with the HDR10 colour space, which Windows shows when "Use HDR" is on for the display.
//
// Other mods (ReShade, Sekiro Lighting Engine, ...) hook IDXGIFactory/IDXGISwapChain themselves, so nothing of DXGI is
// hooked here: the game's own calls of CreateSwapChain and ResizeBuffers are redirected to wrappers that still call
// through the interfaces, and whatever fails falls back to what the game does without the mod.
namespace
{
using HdrActiveFn = uint64_t (*)(int64_t *swapChain);
using HdrRowFn = void (*)(uint8_t *dialog);
using RenderResizeFn = void (*)(void *sys, int width, int height);
HdrActiveFn fnHdrActive;
HdrRowFn fnHdrRow;
RenderResizeFn fnRenderResize;

// the game's swap chain if it was created with the flip model
std::atomic<IDXGISwapChain *> flipChain;
// colour space set on it, -1 after a resize
std::atomic<int> appliedSpace{DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709};
// the display of the window shows HDR10 (Windows HDR on) and the game is not in exclusive fullscreen
std::atomic<bool> windowedHdrAvailable;

// Whether Windows HDR is on for the display of the window. Without it a window cannot show HDR anyway, and the swap chain
// stays the game's own, so the game's HDR in exclusive fullscreen works exactly as without the mod.
bool WindowsHdrOn(IDXGIFactory *factory, HWND window)
{
  HMONITOR monitor = MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST);
  bool on = false, found = false;
  IDXGIAdapter *adapter = nullptr;
  for (UINT a = 0; !found && SUCCEEDED(factory->EnumAdapters(a, &adapter)); a++)
  {
    IDXGIOutput *output = nullptr;
    for (UINT o = 0; !found && SUCCEEDED(adapter->EnumOutputs(o, &output)); o++)
    {
      DXGI_OUTPUT_DESC desc;
      IDXGIOutput6 *output6 = nullptr;
      if (SUCCEEDED(output->GetDesc(&desc)) && desc.Monitor == monitor)
      {
        found = true;
        DXGI_OUTPUT_DESC1 desc1;
        if (SUCCEEDED(output->QueryInterface(__uuidof(IDXGIOutput6), (void **)&output6)))
        {
          on = SUCCEEDED(output6->GetDesc1(&desc1)) && desc1.ColorSpace == DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020;
          output6->Release();
        }
      }
      output->Release();
    }
    adapter->Release();
  }
  return on;
}

HRESULT STDMETHODCALLTYPE CreateSwapChain(IDXGIFactory *factory, IUnknown *device, DXGI_SWAP_CHAIN_DESC *desc, IDXGISwapChain **swapChain)
{
  if (WINDOWED_HDR_ENABLED && desc->BufferDesc.Format == DXGI_FORMAT_R10G10B10A2_UNORM && desc->SampleDesc.Count == 1 &&
      desc->SwapEffect == DXGI_SWAP_EFFECT_DISCARD && WindowsHdrOn(factory, desc->OutputWindow))
  {
    DXGI_SWAP_CHAIN_DESC flip = *desc;
    flip.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    flip.BufferCount = std::max(flip.BufferCount, 2u);
    HRESULT hr = factory->CreateSwapChain(device, &flip, swapChain);
    if (SUCCEEDED(hr))
    {
      flipChain = *swapChain;
      appliedSpace = DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;
      LogMessage(LogLevel::Success, false, "Swap chain created with the flip model for windowed HDR.");
      return hr;
    }
    LogMessage(LogLevel::Error, false, "Flip model swap chain failed (0x%08lX), windowed HDR disabled", hr);
  }
  return factory->CreateSwapChain(device, desc, swapChain);
}

HRESULT STDMETHODCALLTYPE ResizeBuffers(IDXGISwapChain *swapChain, UINT count, UINT width, UINT height, DXGI_FORMAT format, UINT flags)
{
  if (swapChain == flipChain)
  {
    if (count == 1) // a flip model swap chain needs two buffers
      count = 2;
    appliedSpace = -1;
  }
  HRESULT hr = swapChain->ResizeBuffers(count, width, height, format, flags);
  LogMessage(SUCCEEDED(hr) ? LogLevel::Info : LogLevel::Error, false, "ResizeBuffers %ux%u, %u buffers, format %d, flags 0x%X, flip %d: 0x%08lX.", width,
             height, count, (int)format, flags, swapChain == flipChain, (unsigned long)hr);
  return hr;
}

// Whether the swap chain can show the colour space; the display of the window may change or have HDR turned off
bool ColorSpaceSupported(IDXGISwapChain3 *chain, DXGI_COLOR_SPACE_TYPE space)
{
  UINT support = 0;
  return SUCCEEDED(chain->CheckColorSpaceSupport(space, &support)) && (support & DXGI_SWAP_CHAIN_COLOR_SPACE_SUPPORT_FLAG_PRESENT);
}

// Per frame, from the game's update of its outputs: HDR10 in a window when the player's HDR option is on.
uint64_t DetourHdrActive(int64_t *swapChain)
{
  uint64_t active = fnHdrActive(swapChain);
  IDXGISwapChain *chain = (IDXGISwapChain *)swapChain[0];
  if (!chain || chain != flipChain)
    return active;

  static int nextCheck;
  static bool supported;
  static BOOL lastFullscreen;
  BOOL fullscreen = FALSE;
  if (FAILED(chain->GetFullscreenState(&fullscreen, nullptr)))
    return active;

  // Unlike the blt model, a flip model swap chain needs its buffers resized after it entered or left exclusive
  // fullscreen, otherwise every Present fails (black screen, and the game falls back to windowed). The game's own
  // resize of its render system does it, as when the resolution changes; it runs here right after the game applied
  // the screen mode, on the same thread.
  if (fullscreen != lastFullscreen)
  {
    LogMessage(LogLevel::Info, false, "Flip swap chain fullscreen %d -> %d.", lastFullscreen, fullscreen);
    lastFullscreen = fullscreen;
    uint8_t *sys = *(uint8_t **)GetAbsoluteAddress(render_system_offset);
    uint32_t width = *(uint32_t *)(sys + 0x200), height = *(uint32_t *)(sys + 0x204);
    if (width && height)
      fnRenderResize(sys, width, height);
  }

  IDXGISwapChain3 *chain3 = nullptr;
  if (FAILED(chain->QueryInterface(__uuidof(IDXGISwapChain3), (void **)&chain3)))
    return active;

  if (nextCheck-- <= 0)
  {
    supported = ColorSpaceSupported(chain3, DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020);
    nextCheck = 60;
  }
  // In exclusive fullscreen the game's own HDR (NvAPI / AGS) stays on as without the mod, but with Windows HDR on a flip
  // model swap chain is still shown in the colour space set on it: the default one would show the HDR10 picture as SDR
  // and washed out, so the swap chain gets HDR10 there too while the game's HDR is on.
  bool available = !fullscreen && supported;
  bool on = fullscreen ? (active & 1) && supported : available && *((uint8_t *)swapChain + 0x3058) == 0; // set when the HDR option is off
  windowedHdrAvailable = available;

  DXGI_COLOR_SPACE_TYPE space = on ? DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020 : DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;
  if (appliedSpace != (int)space)
  {
    if (SUCCEEDED(chain3->SetColorSpace1(space)))
      appliedSpace = space;
    else
      on = false;
  }
  chain3->Release();
  return fullscreen ? active : on;
}

// The HDR row is locked outside of exclusive fullscreen; unlocked like in fullscreen when a window can show HDR10.
void DetourHdrRow(uint8_t *dialog)
{
  fnHdrRow(dialog);
  if (dialog[0x1250] && windowedHdrAvailable)
  {
    dialog[0x1253] = 0;
    dialog[0x1254] = *(*(uint8_t **)(dialog + 0x1248) + 0x5a);
  }
}

// Redirects a call in the game's code to fn: stub (near the game) runs prefix, then jumps to fn with the game's return address.
bool RedirectCall(uintptr_t site, const uint8_t *expected, size_t size, const uint8_t *prefix, size_t prefixSize, void *fn, const char *name)
{
  if (memcmp((void *)site, expected, size) != 0)
    return LogMessage(LogLevel::Error, false, "%s: unexpected bytes, windowed HDR disabled", name), false;
  uintptr_t stub = AllocateMemoryNearAddress(site, 64, true);
  if (!stub)
    return false;

  std::vector<uint8_t> code(prefix, prefix + prefixSize);
  const uint8_t jmp[] = {0xFF, 0x25, 0x00, 0x00, 0x00, 0x00}; // jmp [rip]
  code.insert(code.end(), jmp, jmp + sizeof(jmp));
  code.insert(code.end(), (uint8_t *)&fn, (uint8_t *)&fn + sizeof(fn));
  memcpy((void *)stub, code.data(), code.size());

  // nops, then a call of the stub that ends where the replaced instructions did
  std::vector<uint8_t> patch(size, 0x90);
  int32_t rel = (int32_t)(stub - (site + size));
  patch[size - 5] = 0xE8;
  memcpy(&patch[size - 4], &rel, sizeof(rel));
  return WriteProtectedMemory(site, patch.data(), patch.size(), name);
}

bool Hook(uintptr_t rva, const uint8_t *expected, size_t size, void *detour, void **original, const char *name)
{
  LPVOID target = (LPVOID)GetAbsoluteAddress(rva);
  if (memcmp(target, expected, size) != 0)
    return LogMessage(LogLevel::Error, false, "%s: unexpected bytes, windowed HDR disabled", name), false;
  if (MH_CreateHook(target, detour, original) != MH_OK || MH_EnableHook(target) != MH_OK)
    return LogMessage(LogLevel::Error, false, "%s: hook failed", name), false;
  return true;
}
} // namespace

void InstallWindowedHdr()
{
  if (!WINDOWED_HDR_ENABLED)
    return;
  fnRenderResize = (RenderResizeFn)GetAbsoluteAddress(render_resize_offset);

  constexpr uint8_t createBytes[] = {0x48, 0x8B, 0x01, 0xFF, 0x50, 0x50}; // mov rax,[rcx]; call [rax+50h]
  uintptr_t create = GetAbsoluteAddress(swapchain_create_call_offset);
  for (int waited = 0; !CodeReady((LPVOID)create, createBytes, sizeof(createBytes)); waited++)
  {
    if (waited >= 30000)
      return LogMessage(LogLevel::Error, false, "Swap chain creation: unexpected bytes, windowed HDR disabled");
    Sleep(1);
  }

  // the stack arguments (format, flags) are moved by the 8 bytes of the return address of the call of the stub
  constexpr uint8_t resizeBytes[] = {0x89, 0x5C, 0x24, 0x28, 0x89, 0x44, 0x24, 0x20, 0xFF, 0x57, 0x68}; // mov [rsp+28h],ebx; mov [rsp+20h],eax; call [rdi+68h]
  constexpr uint8_t resizePrefix[] = {0x89, 0x5C, 0x24, 0x30, 0x89, 0x44, 0x24, 0x28};                // mov [rsp+30h],ebx; mov [rsp+28h],eax
  constexpr uint8_t activeBytes[] = {0x48, 0x8B, 0x91, 0x60, 0x30, 0x00, 0x00, 0xE9};
  constexpr uint8_t rowBytes[] = {0x40, 0x53, 0x48, 0x83, 0xEC, 0x20, 0x80, 0xB9, 0x50, 0x12};

  // the HDR state hooks first: without them the flip model swap chain would only show SDR
  if (Hook(hdr_active_offset, activeBytes, sizeof(activeBytes), (void *)DetourHdrActive, (void **)&fnHdrActive, "HDR state") &&
      Hook(options_hdr_row_offset, rowBytes, sizeof(rowBytes), (void *)DetourHdrRow, (void **)&fnHdrRow, "HDR settings row") &&
      RedirectCall(GetAbsoluteAddress(swapchain_resize_call_offset), resizeBytes, sizeof(resizeBytes), resizePrefix, sizeof(resizePrefix),
                   (void *)ResizeBuffers, "Swap chain resize"))
    RedirectCall(create, createBytes, sizeof(createBytes), nullptr, 0, (void *)CreateSwapChain, "Swap chain creation");
}
