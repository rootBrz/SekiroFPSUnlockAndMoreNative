#pragma once
#include <cstdint>

// All offsets, unless credited otherwise, was found thanks to patterns from uberhalit's SekiroFpsUnlockAndMore
constexpr uintptr_t fps_offset = 0x11AB550;
constexpr uintptr_t speedfix_offset = 0x7DE8AD;
constexpr uintptr_t autoloot_offset = 0x91C606;
constexpr uintptr_t introskip_offset = 0xE1B51B;     // credits to 'Katalash' for original offset
constexpr uintptr_t fovsetting_offset = 0x73c688;    // credits to 'jackfuste' for original offset

constexpr uintptr_t resolution_pointer_offset = 0x1194d85;

constexpr uintptr_t camadjust_pitch_offset = 0x73e056;
constexpr uintptr_t camadjust_yaw_z_offset = 0x73e07c;
constexpr uintptr_t camreset_lockon_offset = 0x73dece;
constexpr uintptr_t dragonrot_effect_offset = 0x11d383f;
constexpr uintptr_t deathpenalties1_offset = 0x11d36c7;
constexpr uintptr_t deathpenalties2_offset = 0x11d378b;

// offsets found with patterns from Lyall's SekiroFix (MIT, https://codeberg.org/Lyall/SekiroFix)
constexpr uintptr_t sprint_speed_jump_offset = 0x7DE8C3;     // jbe over a movement delta check that slows sprinting above 60 fps
constexpr uintptr_t fullscreen_refresh_rate_offset = 0x12CAAF;         // mov [..],3Ch (60 hz) before resizing the target
constexpr uintptr_t fullscreen_refresh_rate_startup_offset = 0x128FFA; // same, for the swap chain created at startup

constexpr uintptr_t scaleform_manager_update_offset = 0x10FDE10; // updates every movie player once per frame
constexpr uintptr_t scaleform_player_viewport_offset = 0x10F1F00; // (player, rect): sets the movie viewport to the render resolution
constexpr uintptr_t death_screen_update_offset = 0xD9CD90; // (menu): per frame update of the death screen (01_210_Dead); the wrapper of its root at +0x158
constexpr uintptr_t scaleform_child_lookup_offset = 0xC66B60; // (parent, out, name): the game's lookup of a clip by name, GFx::Value at +0x30
constexpr uintptr_t animation_culling_offset = 0xF00EEE;     // movss xmm2,[rbx+50h] before the culling call that takes the aspect ratio in xmm3
constexpr uintptr_t hud_vignette_offset = 0x209C622;         // viewport size of a Scaleform movie, xmm5 = width, xmm7 = height
constexpr uintptr_t awareness_transition_hor_offset = 0xE36D70; // awareness marker bounds, left in xmm0, width in xmm1
constexpr uintptr_t awareness_transition_vert_offset = 0xE36D9A; // same, top in xmm0, height in xmm4
constexpr uintptr_t awareness_culling_offset = 0xC6BA9A;     // awareness marker culling

// mine (rootBrz) offsets
constexpr uintptr_t render_resolution_offset = 0x3B1D100; // int32 width, height of the current render resolution (also applied by scaleform_player_viewport_offset)
constexpr uintptr_t render_system_offset = 0x3F3B400; // pointer to the render system
constexpr uintptr_t render_targets_offset = 0x12ABD0; // builds the render targets, letterboxed to the aspect ratio pair
constexpr uintptr_t render_resize_offset = 0x12EBB0;  // (sys, width, height): resize path of the render system
constexpr uintptr_t window_resize_offset = 0x11B46B0; // (CSWindow): the handler of a window resize; gets the size, applies pending screen settings, calls render_resize_offset if it differs from the size at +96/+100
constexpr uintptr_t present_interval_offset = 0x21E760; // GXSwapChainCtrl: returns the DXGI sync interval, once per frame
constexpr uintptr_t window_singleton_offset = 0x3EA0BF8; // CSWindow pointer; dword +128 == 1 while fullscreen
constexpr uintptr_t is_game_in_loading_offset = 0x3D7ACB3;
constexpr uintptr_t game_data_man_offset = 0x3D5AAC0; // GameDataMan pointer; +0x50 option data, bytes +4..+6 music, sound effects, voice volume (0-10)
constexpr uintptr_t game_language_offset = 0x3D9C868; // int32 locale of the game's texts, an index into jpnJP, engGB, fraFR, ... thaTH
constexpr uintptr_t sound_volume_update_offset = 0x92F330; // (dt): once per frame, applies the volumes of the option data to the sound categories
constexpr uintptr_t fullscreen_refreshrate_num = 0x1B62D3F;
constexpr uintptr_t swapchain_create_call_offset = 0x1B624F4; // mov rax,[rcx]; call [rax+50h]: IDXGIFactory::CreateSwapChain of the game's swap chain
constexpr uintptr_t swapchain_resize_call_offset = 0x1B62C1E; // the stack arguments and call [rdi+68h] of IDXGISwapChain::ResizeBuffers
constexpr uintptr_t hdr_active_offset = 0x24C61C0;            // (swap chain): whether the HDR output is on, the game's HDR rendering follows it
constexpr uintptr_t options_hdr_row_offset = 0xCD9030;        // (dialog): state of the HDR row of the graphics settings, locked unless fullscreen

inline uintptr_t is_game_in_loading_addr;
