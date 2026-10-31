#include "hud.h"
#include "MinHook.h"
#include "offset.h"
#include "utils.h"

#include <atomic>
#include <cmath>
#include <cstring>
#include <map>
#include <string>

// The movies are AS3, laid out on a 1920 x 1080 stage that Scaleform fits into the viewport (ShowAll), which leaves
// room around the stage on pictures that are not 16:9. The HUD (01_000_fe) has its elements as clips below the root
// (PlayerHUD/Gauge, PlayerHUD/ItemPanel, BossList, ...); Screen Edges moves them into that room, and the save icon
// with them. The black backgrounds of the loading screens, the death screen, the idol menu and the notifications are
// stretched over it in every layout but Stretch.
// When Scaleform recomputes the layout of one of these movies for a new viewport (the HUD viewport hook in
// fixes.cpp), the room is remembered, and the movie is done during the game's next call into it: the hooked
// GetMember/SetMember/Invoke of the AS3 ObjectInterface, through which the game works with its movies all the time.
// A movie is recognised by the file it was loaded from; no pointer to a movie object is kept, a reloaded movie can be
// a new one at the address of the old one. The death screen is driven by its timeline, so it is also done in its own
// per frame update.

namespace
{
// GFx::Value as the game lays it out: list links (while managed), ObjectInterface*, type, data. A managed value is
// linked into a list by its address, so values are never copied, only filled and released in place.
struct GValue
{
  int64_t link[2] = {-1, -1};
  void ***oi = nullptr; // object whose first field is its vtable
  uint32_t type = 0;
  uint32_t pad = 0;
  union
  {
    void *data;
    double number;
  };
  GValue() : data(nullptr) {}
  GValue(const GValue &) = delete;
  GValue &operator=(const GValue &) = delete;
};

constexpr uint32_t VT_BOOLEAN = 2, VT_INT = 3, VT_NUMBER = 5, VT_STRING = 6, VT_OBJECT = 8, VT_DISPLAY_OBJECT = 10, VT_TYPE_MASK = 0x8f, VT_MANAGED = 0x40;

// ObjectInterface vtable slots
using GetMemberFn = bool (*)(void *oi, void *data, const char *name, GValue *out, bool isDisplayObject);
using SetMemberFn = bool (*)(void *oi, void *data, const char *name, const GValue *value, bool isDisplayObject);
using InvokeFn = bool (*)(void *oi, void *data, GValue *result, const char *name, const GValue *args, size_t count, bool isDisplayObject);
using ReleaseFn = void (*)(void *oi, GValue *val, void *data);
GetMemberFn fnGetMember;
SetMemberFn fnSetMember;
InvokeFn fnInvoke;

bool IsDisplayObject(const GValue &v) { return v.oi && (v.type & VT_TYPE_MASK) == VT_DISPLAY_OBJECT; }

bool GetMember(const GValue &obj, const char *name, GValue &out) { return fnGetMember(obj.oi, obj.data, name, &out, true); }

void Release(GValue &v)
{
  if (v.type & VT_MANAGED)
    ((ReleaseFn)v.oi[0][2])(v.oi, &v, v.data);
  v.type = 0;
  v.data = nullptr;
}

double Number(const GValue &obj, const char *name)
{
  GValue value;
  double n = 0;
  if (GetMember(obj, name, value))
    n = (value.type & VT_TYPE_MASK) == VT_NUMBER ? value.number : (value.type & VT_TYPE_MASK) == VT_INT ? (int)(intptr_t)value.data : 0;
  Release(value);
  return n;
}

bool Boolean(const GValue &obj, const char *name)
{
  GValue value;
  bool b = GetMember(obj, name, value) && (value.type & VT_TYPE_MASK) == VT_BOOLEAN && ((uintptr_t)value.data & 0xff);
  Release(value);
  return b;
}

std::string String(const GValue &value)
{
  if ((value.type & VT_TYPE_MASK) != VT_STRING || !value.data)
    return "";
  // a managed string points to its node, whose first field is the text
  return (value.type & VT_MANAGED) ? *(const char **)value.data : (const char *)value.data;
}

// the file a movie was loaded from (e.g. "data1:/menu/01_000_fe.gfx"), from the AS3 loaderInfo of its root
std::string Url(const GValue &root)
{
  GValue info, url;
  std::string s;
  if (GetMember(root, "loaderInfo", info) && (info.type & VT_TYPE_MASK) >= VT_OBJECT && fnGetMember(info.oi, info.data, "url", &url, false))
    s = String(url);
  Release(url);
  Release(info);
  return s;
}

// calls f with each display object child of obj
template <typename F>
void ForEachChild(const GValue &obj, F &&f)
{
  int count = (int)Number(obj, "numChildren");
  for (int i = 0; i < count && i < 256; i++)
  {
    GValue index, child;
    index.type = VT_INT;
    index.data = (void *)(intptr_t)i;
    if (fnInvoke(obj.oi, obj.data, &child, "getChildAt", &index, 1, true) && IsDisplayObject(child))
      f(child);
    Release(child);
  }
}

// calls f with the clip at an "a/b/c" path below obj, and returns false when it is missing
template <typename F>
bool AtPath(const GValue &obj, const char *path, F &&f)
{
  const char *slash = strchr(path, '/');
  std::string name = slash ? std::string(path, slash) : std::string(path);
  GValue child;
  bool ok = GetMember(obj, name.c_str(), child) && IsDisplayObject(child);
  if (ok)
    ok = slash ? AtPath(child, slash + 1, f) : (f(child), true);
  Release(child);
  return ok;
}

// the room a viewport has around the 16:9 stage, per side, in stage units: to the left and right on wider pictures,
// above and below on narrower ones
struct Room
{
  double x = 0, y = 0;
};

Room EdgeRoom(float width, float height)
{
  Room room;
  if (width <= 0 || height <= 0)
    return room;
  if (width * 9 > height * 16)
    room.x = (1080.0 * width / height - 1920.0) / 2;
  else if (width * 9 < height * 16)
    room.y = (1920.0 * height / width - 1080.0) / 2;
  return room;
}

// clip and property -> its value without the shift, the shift applied
std::map<std::pair<void *, std::string>, std::pair<double, double>> placed;

// a numeric property of a clip as the movie set it, without our shift
double Unshifted(const GValue &clip, const char *property)
{
  auto it = placed.find(std::make_pair(clip.data, std::string(property)));
  double now = Number(clip, property);
  // positions are kept in twips, so a written value reads back rounded to 0.05; a value that does not read back as
  // ours was set by the game (also in a new movie)
  return it != placed.end() && fabs(now - (it->second.first + it->second.second)) < 0.1 ? it->second.first : now;
}

// sets a numeric property of a clip to its own value plus shift
void Shift(const GValue &clip, const char *property, double shift)
{
  GValue value;
  if (!GetMember(clip, property, value) || (value.type & VT_TYPE_MASK) != VT_NUMBER)
    return Release(value);
  double now = value.number;
  auto key = std::make_pair(clip.data, std::string(property));
  auto it = placed.find(key);
  bool ours = it != placed.end() && fabs(now - (it->second.first + it->second.second)) < 0.1;
  if (ours && it->second.second == shift)
    return;
  double base = ours ? it->second.first : now;
  placed[key] = {base, shift};
  GValue v;
  v.type = VT_NUMBER;
  v.number = base + shift;
  fnSetMember(clip.oi, clip.data, property, &v, true);
}

struct Anchor
{
  const char *path; // instance names below the HUD root
  int sideX;        // -1 left edge, +1 right edge, 0 stays
  int sideY;        // -1 top edge, +1 bottom edge, 0 stays
};
constexpr Anchor anchors[] = {
    {"PlayerHUD/Gauge", -1, +1},
    // the player's posture stays in the centre: the bar, its frame and the posture break effects
    {"PlayerHUD/Gauge/SP", +1, 0}, {"PlayerHUD/Gauge/SPFrame", +1, 0}, {"PlayerHUD/Gauge/Damage", +1, 0},
    {"PlayerHUD/ItemPanel", +1, +1},
    {"Money", +1, +1}, {"PlayerCount", +1, +1}, {"GetItemList", +1, +1},
    {"SkillPoint", +1, -1}, {"BloodGhostRec", +1, -1},
    {"BossList", -1, -1}, {"SemiBossList", -1, -1},
};
constexpr const char *bossPosture[] = {"SP", "SPFrame", "SPCenter", "Damage"}; // stays in the centre

void PlaceHud(const GValue &root, Room room)
{
  for (const Anchor &a : anchors)
    AtPath(root, a.path, [&](const GValue &clip) {
      Shift(clip, "x", a.sideX * room.x);
      Shift(clip, "y", a.sideY * room.y);
    });
  for (const char *list : {"BossList", "SemiBossList"})
    for (int i = 0; i < 3; i++)
      for (const char *part : bossPosture)
      {
        char path[64];
        snprintf(path, sizeof(path), "%s/Item_%d_0/%s", list, i, part);
        AtPath(root, path, [&](const GValue &clip) { Shift(clip, "x", room.x); });
      }
}

// A movie with corner elements (the save icon): each clip of its root goes to the edges it is nearest to.
void AnchorChildren(const GValue &root, Room room)
{
  ForEachChild(root, [&](const GValue &clip) {
    Shift(clip, "x", (Unshifted(clip, "x") < 960 ? -1 : 1) * room.x);
    Shift(clip, "y", (Unshifted(clip, "y") < 540 ? -1 : 1) * room.y);
  });
}

// a background at the top left of the stage (the black of a loading screen) covers the stage and the room on each side
void StretchBackground(const GValue &root, const char *path, Room room)
{
  AtPath(root, path, [&](const GValue &background) {
    Shift(background, "x", -room.x);
    Shift(background, "y", -room.y);
    Shift(background, "width", 2 * room.x);
    Shift(background, "height", 2 * room.y);
  });
}

// the bounds of a clip in the coordinates of space (its parent when null), from AS3 getBounds
bool Bounds(const GValue &clip, const GValue *space, double &left, double &top, double &width, double &height)
{
  GValue parent, rect;
  bool ok = (space || (GetMember(clip, "parent", parent) && IsDisplayObject(parent))) &&
            fnInvoke(clip.oi, clip.data, &rect, "getBounds", space ? space : &parent, 1, true) && (rect.type & VT_TYPE_MASK) >= VT_OBJECT;
  if (ok)
  {
    auto get = [&](const char *name) {
      GValue v;
      double n = fnGetMember(rect.oi, rect.data, name, &v, false) && (v.type & VT_TYPE_MASK) == VT_NUMBER ? v.number : 0;
      Release(v);
      return n;
    };
    left = get("x"), top = get("y"), width = get("width"), height = get("height");
  }
  Release(rect);
  Release(parent);
  return ok;
}

// whether a clip covers the stage: a clip stretched before did; otherwise its bounds on the stage tell
bool Covers(const GValue &clip, const GValue &root)
{
  double left, top, width, height;
  return placed.count(std::make_pair(clip.data, std::string("width"))) ||
         (Bounds(clip, &root, left, top, width, height) && left <= 20 && top <= 20 && left + width >= 1900 && top + height >= 1060);
}

// A backdrop (a clip that covers the stage) is stretched to cover the room on each side too: scaled by its width plus
// twice the room, with its registration point moved so that its left edge goes out by the room. Its size and the
// place of its registration point are taken back to what the movie set, from the stretch applied before.
// Only the innermost clips that cover the stage are stretched: a clip whose position or size is set by script is no
// longer animated by its timeline, alpha included, and the vignettes of the death screen (Begin, Loop, WaitEnd) fade
// in by the alpha of the clip around the picture.
bool StretchCovering(const GValue &clip, const GValue &root, Room room)
{
  if (!Covers(clip, root))
    return false;
  bool inner = false;
  ForEachChild(clip, [&](const GValue &child) { inner |= StretchCovering(child, root, room); });
  if (inner)
    return true;
  double left, top, width, height;
  if (!Bounds(clip, nullptr, left, top, width, height) || width <= 0 || height <= 0)
    return false;
  double x = Number(clip, "x"), y = Number(clip, "y");
  double baseWidth = Unshifted(clip, "width"), baseHeight = Unshifted(clip, "height");
  double scaleX = baseWidth > 0 ? Number(clip, "width") / baseWidth : 1, scaleY = baseHeight > 0 ? Number(clip, "height") / baseHeight : 1;
  // the registration point from the left and top edges, and the size, as the movie set them
  double fromLeft = (x - left) / scaleX, fromTop = (y - top) / scaleY;
  width /= scaleX, height /= scaleY;
  Shift(clip, "x", fromLeft * 2 * room.x / width - room.x);
  Shift(clip, "y", fromTop * 2 * room.y / height - room.y);
  Shift(clip, "width", 2 * room.x);
  Shift(clip, "height", 2 * room.y);
  return true;
}

// The backdrops of a movie: the clips of its root that cover the stage, only unnamed ones (e.g. the semi-transparent
// black behind the idol menu), or any down to depth levels below the root (the death screen, whose backgrounds come
// with its timeline).
void StretchBackdrops(const GValue &obj, const GValue &root, Room room, bool anyName, int depth)
{
  ForEachChild(obj, [&](const GValue &clip) {
    GValue name;
    std::string n = GetMember(clip, "name", name) ? String(name) : "";
    Release(name);
    if ((anyName || n.rfind("instance", 0) == 0) && StretchCovering(clip, root, room))
      return;
    if (depth > 1)
      StretchBackdrops(clip, root, room, anyName, depth - 1);
  });
}

// What is done to a movie after Scaleform recomputed its layout, during the game's next call into it.
enum Job
{
  JOB_HUD,
  JOB_SAVE_ICON,
  JOB_LOADING,
  JOB_DEATH,
  JOB_BACKDROPS,
};
struct MovieJob
{
  const char *movie; // part of the file name
  Job job;
};
constexpr MovieJob jobs[] = {
    {"01_000_fe.gfx", JOB_HUD},
    {"01_002_fe_saveicon.gfx", JOB_SAVE_ICON},
    {"02_903_nowloading2.gfx", JOB_LOADING},
    {"02_904_nowloading3.gfx", JOB_LOADING},
    {"02_905_nowloading_thanks.gfx", JOB_LOADING},
    {"02_990_nowloading_userguide.gfx", JOB_LOADING},
    {"01_210_dead.gfx", JOB_DEATH},
    // the semi-transparent black behind the idol menu and the notifications (an item, a skill, an event such as
    // Dragonrot spreading, messages and tutorials)
    {"03_202_bonfire.gfx", JOB_BACKDROPS},
    {"02_280_getitem.gfx", JOB_BACKDROPS},
    {"02_281_getitem_middle.gfx", JOB_BACKDROPS},
    {"02_282_getitem_high.gfx", JOB_BACKDROPS},
    {"02_283_getitem_skill.gfx", JOB_BACKDROPS},
    {"02_284_getitem_event.gfx", JOB_BACKDROPS},
    {"02_210_fullscreenmsgdialog.gfx", JOB_BACKDROPS},
    {"01_010_messagebox.gfx", JOB_BACKDROPS},
    {"01_011_messagebox_small.gfx", JOB_BACKDROPS},
    {"01_013_messagebox_small_nb.gfx", JOB_BACKDROPS},
    {"01_250_tutorialmessage.gfx", JOB_BACKDROPS},
    {"01_251_tutorialmsgdialog.gfx", JOB_BACKDROPS},
};
constexpr size_t JOB_COUNT = sizeof(jobs) / sizeof(jobs[0]);
Room jobRoom[JOB_COUNT];
std::atomic<bool> jobPending[JOB_COUNT];
std::atomic<int> pendingJobs;
std::map<void *, int> rootJobs; // movie root -> its job (-1 none); forgotten when a movie gets a viewport
std::atomic<bool> forgetRoots;

void RunJob(Job job, const GValue &root, Room room)
{
  if (placed.size() > 4000) // clips of movies that are gone
    placed.clear();
  switch (job)
  {
  case JOB_HUD:
    return PlaceHud(root, room);
  case JOB_SAVE_ICON:
    return AnchorChildren(root, room);
  case JOB_LOADING:
    return StretchBackground(root, "instance1", room);
  case JOB_DEATH:
    return StretchBackdrops(root, root, room, true, 3);
  case JOB_BACKDROPS:
    return StretchBackdrops(root, root, room, false, 1);
  }
}

// data is a display object the game works with right now; the movie it is in is done if it has a job to do
thread_local bool placing;
void PlaceFrom(void *oi, void *data)
{
  if (!pendingJobs || placing)
    return;
  placing = true;
  GValue obj, root;
  obj.oi = (void ***)oi;
  obj.type = VT_DISPLAY_OBJECT; // borrowed from the call, not managed
  obj.data = data;
  if (GetMember(obj, "root", root) && IsDisplayObject(root))
  {
    if (forgetRoots.exchange(false))
      rootJobs.clear(); // a new movie can have the root address of one that is gone
    auto known = rootJobs.find(root.data);
    int i = -1;
    if (known != rootJobs.end())
      i = known->second;
    else
    {
      std::string url = Url(root);
      for (size_t j = 0; j < JOB_COUNT && i < 0; j++)
        if (url.find(jobs[j].movie) != std::string::npos)
          i = (int)j;
      rootJobs[root.data] = i;
    }
    if (i >= 0 && jobPending[i] && jobPending[i].exchange(false))
    {
      pendingJobs--;
      RunJob(jobs[i].job, root, jobRoom[i]);
    }
  }
  Release(root);
  placing = false;
}

bool DetourGetMember(void *oi, void *data, const char *name, GValue *out, bool isDisplayObject)
{
  if (isDisplayObject)
    PlaceFrom(oi, data);
  return fnGetMember(oi, data, name, out, isDisplayObject);
}

bool DetourSetMember(void *oi, void *data, const char *name, const GValue *value, bool isDisplayObject)
{
  if (isDisplayObject)
    PlaceFrom(oi, data);
  return fnSetMember(oi, data, name, value, isDisplayObject);
}

bool DetourInvoke(void *oi, void *data, GValue *result, const char *name, const GValue *args, size_t count, bool isDisplayObject)
{
  if (isDisplayObject)
    PlaceFrom(oi, data);
  return fnInvoke(oi, data, result, name, args, count, isDisplayObject);
}

// The death screen's own update, every frame (FUN_140d9cd90): it shows the root of its movie and plays labels of the
// root timeline, whose frames bring in the black background (WaitBegin/instance1) at the size of the stage. After it,
// while the root is shown, the backgrounds are stretched over the room, through the root wrapper of the menu at +0x158,
// whose vtable[1] gives the holder of its GFx::Value at +8. Back on a 16:9 picture they are put back once.
using DeathUpdateFn = void (*)(uint8_t *menu);
DeathUpdateFn fnDeathUpdate;
size_t deathJob;
bool deathStretched;

void DetourDeathUpdate(uint8_t *menu)
{
  fnDeathUpdate(menu);
  void ***wrapper = (void ***)(menu + 0x158);
  if (!fnGetMember || !*wrapper)
    return;
  Room room = jobRoom[deathJob];
  bool stretch = room.x != 0 || room.y != 0;
  if (!stretch && !deathStretched)
    return;
  const GValue &root = *(const GValue *)(((uint8_t * (*)(void *))(*wrapper)[1])(wrapper) + 8);
  if (!IsDisplayObject(root) || !Boolean(root, "visible"))
    return;
  StretchBackdrops(root, root, room, true, 3);
  deathStretched = stretch;
}

// The game's lookup of a clip by name in its movie wrappers (the GFx::Value at +0x30). The first one shows the AS3
// ObjectInterface, whose calls are hooked from then on.
using ChildLookupFn = void *(*)(uint8_t *parent, uint8_t *out, void *name);
ChildLookupFn fnChildLookup;

template <typename T>
bool HookSlot(void **vtable, int slot, T detour, T *original)
{
  return MH_CreateHook(vtable[slot], (LPVOID)detour, (LPVOID *)original) == MH_OK && MH_EnableHook(vtable[slot]) == MH_OK;
}

void *DetourChildLookup(uint8_t *parent, uint8_t *out, void *name)
{
  void *r = fnChildLookup(parent, out, name);
  static bool hooked;
  const GValue &clip = *(const GValue *)(parent + 0x30);
  if (!hooked && IsDisplayObject(clip))
  {
    hooked = true;
    void **vtable = clip.oi[0];
    if (!HookSlot(vtable, 4, &DetourGetMember, &fnGetMember) || !HookSlot(vtable, 5, &DetourSetMember, &fnSetMember) ||
        !HookSlot(vtable, 6, &DetourInvoke, &fnInvoke))
      LogMessage(LogLevel::Error, false, "Scaleform ObjectInterface hooks failed, HUD Layout Screen Edges disabled");
  }
  return r;
}
} // namespace

void InstallHudLayout()
{
  constexpr uint8_t expected[] = {0x48, 0x89, 0x54, 0x24, 0x10, 0x53, 0x48, 0x83, 0xEC, 0x30};
  LPVOID target = (LPVOID)GetAbsoluteAddress(scaleform_child_lookup_offset);
  if (memcmp(target, expected, sizeof(expected)) != 0 || MH_CreateHook(target, (LPVOID)&DetourChildLookup, (LPVOID *)&fnChildLookup) != MH_OK ||
      MH_EnableHook(target) != MH_OK)
    LogMessage(LogLevel::Error, false, "Scaleform child lookup hook failed, HUD Layout Screen Edges disabled");

  while (deathJob < JOB_COUNT && jobs[deathJob].job != JOB_DEATH)
    deathJob++;
  constexpr uint8_t deathExpected[] = {0x40, 0x53, 0x48, 0x83, 0xEC, 0x20, 0x48, 0x8B, 0xD9};
  target = (LPVOID)GetAbsoluteAddress(death_screen_update_offset);
  if (memcmp(target, deathExpected, sizeof(deathExpected)) != 0 || MH_CreateHook(target, (LPVOID)&DetourDeathUpdate, (LPVOID *)&fnDeathUpdate) != MH_OK ||
      MH_EnableHook(target) != MH_OK)
    LogMessage(LogLevel::Error, false, "Death screen update hook failed, its background stays at 16:9");
}

void MovieViewport(const char *name, float width, float height)
{
  forgetRoots = true;
  for (size_t i = 0; i < JOB_COUNT; i++)
    if (strstr(name, jobs[i].movie))
    {
      // the HUD and the save icon stay on the stage unless the layout is Screen Edges; the backgrounds stretch over
      // the room in every layout but Stretch, which stretches the whole movie
      bool background = jobs[i].job != JOB_HUD && jobs[i].job != JOB_SAVE_ICON;
      bool edges = background ? HUD_LAYOUT != HUD_STRETCH : HUD_LAYOUT == HUD_EDGES;
      jobRoom[i] = edges ? EdgeRoom(width, height) : Room();
      if (!jobPending[i].exchange(true))
        pendingJobs++;
    }
}
