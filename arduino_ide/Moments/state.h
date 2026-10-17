// Moments firmware - shared state, helpers and forward declarations.
#pragma once
#include "config.h"

// ---------------------------------------------------------------------------
// ArduinoJson allocator that prefers PSRAM (keeps internal RAM free for Wi-Fi/TLS/BLE).
struct PsAlloc : ArduinoJson::Allocator {
  void* allocate(size_t n) override {
    void* p = heap_caps_malloc(n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    return p ? p : malloc(n);
  }
  void deallocate(void* p) override { free(p); }
  void* reallocate(void* p, size_t n) override {
    void* q = heap_caps_realloc(p, n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    return q ? q : realloc(p, n);
  }
};
static PsAlloc g_psAlloc;

static void* psAlloc(size_t n) {
  void* p = heap_caps_malloc(n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  return p ? p : malloc(n);
}

// ---------------------------------------------------------------------------
enum : int8_t { POSE_NONE = -1, POSE_PS = 0, POSE_LS = 1, POSE_PV = 2, POSE_LV = 3 };
static const char* const POSE_CODE[4] = {"PS", "LS", "PV", "LV"};
static const char* const POSE_NAME[4] = {"Portrait Stand", "Landscape Stand", "Portrait Vertical", "Landscape Vertical"};
static inline bool posePortrait(int p) { return p == POSE_PS || p == POSE_PV; }

enum : uint8_t { G_IDLE = 0, G_CHECKING = 1, G_ARMED = 2 };
static const char* const G_STATE_NAME[3] = {"idle", "checking", "armed"};

enum InKind : uint8_t { IN_TAP, IN_SWIPE_L, IN_SWIPE_R, IN_SWIPE_U, IN_SWIPE_D, IN_BTN_AI, IN_BTN_UP, IN_BTN_DOWN };
struct InputEv { InKind k; int16_t x, y; };

struct EventRec { uint32_t t; char msg[40]; };

struct AppRtc {
  int16_t page;
  int16_t sel;
  uint32_t chefTimerEnd;
  uint32_t timerEnd;
  uint32_t timerLeft;
  uint16_t timerMin;
  uint8_t timerBreak;
  uint8_t focusCount;
  uint16_t focusDay;
  int16_t galIdx;
  uint32_t galNext;
  int16_t listIdx;
  int16_t listPage;
  uint8_t chefRegen;
};

#define RTC_MAGIC 0x4D4F4D32u  // "MOM2"

// Survives deep sleep (lost on hibernate / power loss).
struct RtcState {
  uint32_t magic;
  uint32_t boots, wakes, refreshes, fullRefreshes;
  int8_t pose, rot;
  uint8_t frameValid;
  uint32_t frameHash;
  uint8_t partials;
  uint8_t lastWasGray;
  char target[32];
  char moment[32];
  char ovTarget[32];
  uint32_t ovUntil;
  int8_t cycleIdx;
  uint32_t cycleUntil;
  int8_t prevDaypart;
  uint8_t prevUsb;
  int8_t prevOwner;
  int8_t prevPose;
  int16_t prevSoc;
  uint32_t lastEval;
  uint32_t lastSensor;
  float temp, hum;
  uint8_t sensOk;
  uint32_t lastBatt;
  uint32_t lastNtp;
  uint32_t netFailUntil;
  uint32_t autoHash;
  uint32_t autoLast[MAX_AUTOS];
  uint8_t autoArmed[MAX_AUTOS];
  uint8_t gState;
  uint32_t gMotionAt;
  uint8_t gCheckIdx;
  uint8_t gMisses;
  uint32_t gArmedSince;
  uint32_t gLastAlert;
  uint32_t gNextCheck;
  uint32_t gLastSlow;
  uint32_t ownerSeenAt;
  uint32_t lastPresence;
  AppRtc app;
  uint8_t bssid[6];
  int32_t channel;
  char ssid[33];
  EventRec ev[MAX_EVENTS];
  uint8_t evHead, evCount;
  uint32_t histT[HIST_SLOTS];
  int16_t histTemp[HIST_SLOTS];
  uint8_t histHum[HIST_SLOTS];
  uint32_t quoteDay;
  uint16_t quoteIdx;
};
RTC_DATA_ATTR RtcState R;

// Live context, rebuilt every wake.
struct Ctx {
  time_t now;
  struct tm lt;
  int mod;          // minutes of day
  int8_t pose;      // POSE_*
  int8_t rot;       // 0..3 GFX rotation
  int8_t daypart;   // index into cfg dayparts, -1 none
  float temp, hum;
  bool sensOk;
  int soc, mv, ma, avgMa, tte;
  bool battOk;
  bool usb, charging;
  int8_t ownerHome; // -1 unknown, 0 away, 1 home
};
Ctx C;

enum Mode : uint8_t { MODE_RUN, MODE_INTERACTIVE, MODE_CONFIG, MODE_SERVICE };
Mode gMode = MODE_RUN;

JsonDocument cfg(&g_psAlloc);   // main configuration (no secrets)
JsonDocument listsDoc(&g_psAlloc);

bool gDirty = true;        // re-render requested
bool gForceFull = false;   // next refresh must be full
bool gWifiUp = false;
bool gApMode = false;
bool gWebOn = false;
bool gTouchOn = false;
bool gFsOk = false;
uint32_t gInteractiveUntil = 0;  // millis()
uint32_t gWakeCause = 0;
bool gColdBoot = true;
char gPin[8] = "";
char gApPass[12] = "";
String gDeviceId;

// ---------------------------------------------------------------------------
// Touch hit regions registered while drawing (logical coordinates).
struct Hit { int16_t x, y, w, h; uint16_t id; };
std::vector<Hit> gHits;
static void addHit(int x, int y, int w, int h, uint16_t id) { gHits.push_back({(int16_t)x, (int16_t)y, (int16_t)w, (int16_t)h, id}); }
static int hitTest(int x, int y) {
  for (int i = (int)gHits.size() - 1; i >= 0; i--) {
    const Hit& h = gHits[i];
    if (x >= h.x - 6 && x < h.x + h.w + 6 && y >= h.y - 6 && y < h.y + h.h + 6) return h.id;
  }
  return -1;
}

// ---------------------------------------------------------------------------
// Forward declarations (single translation unit, modules included in order).
time_t nowT();
void logEvent(const char* fmt, ...);
bool secretGet(const char* key, String& out);
bool saveConfig();
void markDirty(bool full = false);
void requestInteractive(uint32_t ms);

// ---------------------------------------------------------------------------
static uint32_t fnv32(const uint8_t* d, size_t n, uint32_t h = 2166136261u) {
  for (size_t i = 0; i < n; i++) { h ^= d[i]; h *= 16777619u; }
  return h;
}
static uint32_t fnvStr(const char* s, uint32_t h = 2166136261u) {
  if (!s) return h;
  while (*s) { h ^= (uint8_t)*s++; h *= 16777619u; }
  return h;
}

// Fold UTF-8 to the printable ASCII glyph set of the bitmap fonts. 0x7F is our degree sign.
static const char LATIN1_FOLD[] = "AAAAAAACEEEEIIIIDNOOOOOxOUUUUYTsaaaaaaaceeeeiiiidnooooo/ouuuuyty";
static String asciiFold(const char* s) {
  String o;
  if (!s) return o;
  o.reserve(strlen(s));
  const uint8_t* p = (const uint8_t*)s;
  while (*p) {
    uint8_t c = *p;
    if (c < 0x80) {
      if (c == '\r' || c == '\t') o += ' ';
      else if (c >= 0x20 || c == '\n') o += (char)c;
      p++;
      continue;
    }
    if (c == 0xC2 && p[1]) {
      uint8_t d = p[1];
      if (d == 0xB0 || d == 0xBA) o += (char)0x7F;
      else if (d == 0xA0) o += ' ';
      else if (d == 0xBD) o += "1/2";
      else if (d == 0xBC) o += "1/4";
      else if (d == 0xBE) o += "3/4";
      else if (d == 0xB7) o += '-';
      else if (d == 0xA3) o += "GBP";
      else if (d == 0xAB || d == 0xBB) o += '"';
      p += 2;
      continue;
    }
    if (c == 0xC3 && p[1] >= 0x80 && p[1] <= 0xBF) {
      o += LATIN1_FOLD[p[1] - 0x80];
      p += 2;
      continue;
    }
    if (c == 0xE2 && p[1] && p[2]) {
      uint8_t d = p[1], e = p[2];
      if (d == 0x80) {
        if (e == 0x98 || e == 0x99 || e == 0xB2) o += '\'';
        else if (e == 0x9C || e == 0x9D || e == 0xB3) o += '"';
        else if (e == 0x93 || e == 0x94 || e == 0x90 || e == 0x91) o += '-';
        else if (e == 0xA6) o += "...";
        else if (e == 0xA2) o += '*';
        else if (e == 0xAF || e == 0x89 || e == 0x8A) o += ' ';
      } else if (d == 0x82 && e == 0xAC) {
        o += "EUR";
      } else if (d == 0x84 && e == 0x83) {
        o += (char)0x7F; o += 'C';
      } else if (d == 0x85 && e >= 0x93 && e <= 0x9E) {
        o += '/';
      }
      p += 3;
      continue;
    }
    // Skip any other multi-byte sequence (emoji, CJK...).
    int len = (c >= 0xF0) ? 4 : (c >= 0xE0) ? 3 : (c >= 0xC0) ? 2 : 1;
    while (len-- && *p) p++;
  }
  return o;
}

static String jstr(JsonVariantConst v, const char* def = "") {
  const char* s = v.as<const char*>();
  return String(s ? s : def);
}

static int parseHHMM(const char* s, int def = -1) {
  if (!s || !*s) return def;
  int h = atoi(s);
  const char* c = strchr(s, ':');
  int m = c ? atoi(c + 1) : 0;
  if (h < 0 || h > 24 || m < 0 || m > 59) return def;
  return (h * 60 + m) % 1440;
}

static String randomDigits(int n) {
  String s;
  for (int i = 0; i < n; i++) s += (char)('0' + (esp_random() % 10));
  return s;
}

static String randomToken(int n) {
  static const char A[] = "abcdefghijkmnopqrstuvwxyz23456789";
  String s;
  for (int i = 0; i < n; i++) s += A[esp_random() % (sizeof(A) - 1)];
  return s;
}
