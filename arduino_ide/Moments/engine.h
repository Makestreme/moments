// Moments firmware - the Moments engine: pose, context, Moment selection, automations, Guardian,
// rendering pipeline, data refresh and wake planning.
#pragma once
#include "apps.h"

bool gMotionFlag = false;     // motion seen since last automation evaluation
bool gGuardMotion = false;    // motion not yet handled by Guardian
bool gPoseLost = false;
bool gPoseSettled = false;    // a stillness-gated pose decision was made this wake
uint32_t gPoseWaitUntil = 0;  // stay awake until then so a new pose can settle
bool gManualRefresh = false;
bool gOwnerUsed = false;
uint32_t gLastSec = 0, gLastFetchCheck = 0, gLastBattMs = 0;
int gLastMinute = -1;

void markDirty(bool full) {
  gDirty = true;
  if (full) gForceFull = true;
}

// ---------------------------------------------------------------------------
// Pose detection (4 built-in poses, stillness gate, auto-rotation)
struct PoseCand { int8_t pose; int8_t rot; uint32_t since; };
PoseCand gCand = {-2, 0, 0};

static void imuToView(const float a[3], float& vx, float& vy, float& vz) {
  JsonObjectConst m = cfg["imu"];
  int xa = m["xa"] | 0, ya = m["ya"] | 1, xs = m["xs"] | 1, ys = m["ys"] | -1;
  if (xa < 0 || xa > 2) xa = 0;
  if (ya < 0 || ya > 2 || ya == xa) ya = (xa + 1) % 3;
  int za = 3 - xa - ya;
  vx = xs * a[xa];
  vy = ys * a[ya];
  vz = a[za];
}
// Accelerometer at rest reads +1 g pointing up; "down" in the view frame is the negated in-plane vector.
static int8_t classifyPose(const float a[3], int8_t& rot) {
  float vx, vy, vz;
  imuToView(a, vx, vy, vz);
  float g = sqrtf(vx * vx + vy * vy + vz * vz);
  if (g < 0.75f || g > 1.25f) return POSE_NONE;
  float inplane = sqrtf(vx * vx + vy * vy);
  float tilt = atan2f(fabsf(vz), inplane) * 180.0f / PI;
  bool vertical = tilt <= sysFloat("vertMax", 20);
  bool stand = tilt >= sysFloat("standMin", 30) && tilt <= sysFloat("standMax", 60);
  if (!vertical && !stand) return POSE_NONE;
  float dx = -vx, dy = -vy;
  bool portrait;
  if (fabsf(dx) > fabsf(dy) * 1.3f) portrait = true;
  else if (fabsf(dy) > fabsf(dx) * 1.3f) portrait = false;
  else return POSE_NONE;
  if (portrait) rot = dx < 0 ? 1 : 3;
  else rot = dy > 0 ? 0 : 2;
  if (portrait) return vertical ? POSE_PV : POSE_PS;
  return vertical ? POSE_LV : POSE_LS;
}

static void poseTick() {
  if (!gImuOk) return;
  static uint32_t last = 0;
  uint32_t ms = millis();
  if (ms - last < 50) return;
  last = ms;
  float a[3], g[3];
  if (!imuAccel(a) || !imuGyro(g)) return;
  float gm = sqrtf(g[0] * g[0] + g[1] * g[1] + g[2] * g[2]);
  bool still = gm < sysFloat("still", 10.0f);
  int8_t rot = R.rot;
  int8_t p = classifyPose(a, rot);
  if (!still) { gCand.pose = -2; return; }
  if (p != gCand.pose || rot != gCand.rot) { gCand = {p, rot, ms}; return; }
  if (ms - gCand.since < 1200) return;
  gPoseSettled = true;
  if (p == POSE_NONE) {
    if (R.pose >= 0 && !gPoseLost) gPoseLost = true;
    return;
  }
  gPoseLost = false;
  if (p != R.pose || rot != R.rot) {
    bool poseChanged = p != R.pose;
    R.pose = p;
    R.rot = rot;
    R.cycleUntil = 0;
    if (poseChanged) logEvent("Pose: %s", POSE_NAME[p]);
    markDirty(true);
  }
}

static void motionTick() {
  if (!gImuOk) return;
  static uint32_t last = 0;
  if (millis() - last < 200) return;
  last = millis();
  if (imuWakeSrc() & 0x08) {
    gMotionFlag = true;
    gGuardMotion = true;
    // Still being carried: keep waiting for the new pose (capped so a bag or car can't hold it awake).
    if (millis() < 60000) { gPoseSettled = false; gPoseWaitUntil = millis() + 5000; }
  }
}

// Orientation calibration: step 1 with the device upright in landscape (rotation 0),
// step 2 rotated so that rotation 1 (portrait) reads upright.
static bool calibrateStep(int step, String& msg) {
  float a[3], s[3] = {0, 0, 0};
  for (int i = 0; i < 10; i++) {
    if (!imuAccel(a)) { msg = "IMU not available"; return false; }
    for (int k = 0; k < 3; k++) s[k] += a[k] / 10;
    delay(20);
  }
  int ax = 0;
  for (int k = 1; k < 3; k++)
    if (fabsf(s[k]) > fabsf(s[ax])) ax = k;
  float down = -s[ax];
  JsonObject m = cfg["imu"].as<JsonObject>();
  if (m.isNull()) m = cfg["imu"].to<JsonObject>();
  if (step == 1) {
    m["ya"] = ax;
    m["ys"] = down > 0 ? 1 : -1;
    msg = "Step 1 saved. Now rotate the device a quarter turn and press step 2.";
  } else {
    if (ax == (int)(m["ya"] | 1)) { msg = "Same axis as step 1 - rotate the device 90 degrees and retry."; return false; }
    m["xa"] = ax;
    m["xs"] = down > 0 ? -1 : 1;
    m["cal"] = true;
    msg = "Calibrated.";
  }
  saveConfig();
  return true;
}

// ---------------------------------------------------------------------------
// Context
static int sensorInterval() {
  if (C.usb) return 300;
  const char* p = cfg["sys"]["profile"] | "balanced";
  return !strcmp(p, "eco") ? 1800 : (!strcmp(p, "live") ? 300 : 900);
}
static void sensorsSample(bool force) {
  uint32_t now = time(nullptr);
  if (force || !R.sensOk || now - R.lastSensor >= (uint32_t)sensorInterval()) {
    float t, h;
    if (shtRead(t, h)) {
      R.temp = t + sysFloat("tempOff", 0);
      R.hum = constrain(h + sysFloat("humOff", 0), 0.0f, 100.0f);
      R.sensOk = 1;
      R.lastSensor = now;
      if (timeValid()) {
        int slot = (now / 900) % HIST_SLOTS;
        R.histT[slot] = now;
        R.histTemp[slot] = (int16_t)lroundf(R.temp * 10);
        R.histHum[slot] = (uint8_t)lroundf(R.hum);
      }
    }
  }
  C.temp = R.temp;
  C.hum = R.hum;
  C.sensOk = R.sensOk;
}
static void updateContext() {
  C.now = nowT();
  C.lt = localTm(C.now);
  C.mod = C.lt.tm_hour * 60 + C.lt.tm_min;
  C.daypart = daypartIndex(C.now);
  C.pose = R.pose;
  C.rot = R.rot;
  if (millis() - gLastBattMs > 30000 || !gLastBattMs) {
    gLastBattMs = millis();
    batteryRead();
    R.lastBatt = time(nullptr);
  }
  sensorsSample(false);
  uint32_t t = time(nullptr);
  if (R.ownerSeenAt && t - R.ownerSeenAt < 1200) C.ownerHome = 1;
  else if (R.lastPresence && t - R.lastPresence < 3600) C.ownerHome = 0;
  else C.ownerHome = -1;
}

// ---------------------------------------------------------------------------
// Conditions and Moments
static float userTemp(float c) { return unitsF() ? c * 9 / 5 + 32 : c; }
static bool cmpOp(const char* op, float x, float v, float v2) {
  if (!strcmp(op, "below")) return x < v;
  if (!strcmp(op, "between")) return x >= min(v, v2) && x <= max(v, v2);
  return x > v;
}
static bool arrHas(JsonArrayConst a, const char* s) {
  for (JsonVariantConst v : a)
    if (!strcmp(v | "", s)) return true;
  return false;
}
static time_t sunEvent(time_t day, const char* ev, int off) {
  time_t r, s;
  if (!sunTimes(day, r, s)) return 0;
  return (!strcmp(ev, "sunset") ? s : r) + off * 60;
}
static bool condOk(JsonObjectConst c, String* why) {
  const char* t = c["t"] | "";
  const char* op = c["op"] | "above";
  float v = c["v"] | 0.0f, v2 = c["v2"] | 0.0f;
  bool ok = true;
  if (!strcmp(t, "temp")) ok = C.sensOk && cmpOp(op, userTemp(C.temp), v, v2);
  else if (!strcmp(t, "hum")) ok = C.sensOk && cmpOp(op, C.hum, v, v2);
  else if (!strcmp(t, "battery")) ok = C.soc >= 0 && cmpOp(op, C.soc, v, v2);
  else if (!strcmp(t, "power")) ok = !strcmp(c["v"] | "docked", "docked") ? C.usb : !C.usb;
  else if (!strcmp(t, "owner")) ok = !strcmp(c["v"] | "home", "home") ? C.ownerHome == 1 : C.ownerHome == 0;
  else if (!strcmp(t, "pose")) ok = C.pose >= 0 && arrHas(c["v"], POSE_CODE[C.pose]);
  else if (!strcmp(t, "days")) ok = ((int)(c["v"] | 127) >> weekdayMon0(C.lt)) & 1;
  else if (!strcmp(t, "daypart")) ok = arrHas(c["v"], daypartId(C.daypart));
  else if (!strcmp(t, "time")) ok = inWindow(C.mod, parseHHMM(c["from"] | "00:00", 0), parseHHMM(c["to"] | "23:59", 1439));
  else if (!strcmp(t, "sun")) {
    time_t e = sunEvent(C.now, c["ev"] | "sunrise", c["off"] | 0);
    ok = e && (!strcmp(op, "before") ? C.now < e : C.now >= e);
  }
  if (!ok && why) *why = String("condition '") + t + "' not met";
  return ok;
}
static bool condsOk(JsonArrayConst a, String* why) {
  for (JsonObjectConst c : a)
    if (!condOk(c, why)) return false;
  return true;
}
static bool targetPortrait(const String& target) {
  if (target.startsWith("screen:")) {
    JsonObject s = findScreen(target.c_str() + 7);
    if (!s.isNull()) return !strcmp(s["o"] | "P", "P");
  }
  return C.pose >= 0 ? posePortrait(C.pose) : (C.rot & 1) == 1;
}
static bool momentMatches(JsonObjectConst m, String* why) {
  if (!(m["on"] | true)) { if (why) *why = "disabled"; return false; }
  JsonArrayConst poses = m["poses"];
  if (poses.size()) {
    if (C.pose < 0) { if (why) *why = "no pose detected"; return false; }
    if (!arrHas(poses, POSE_CODE[C.pose])) { if (why) *why = "pose is " + String(POSE_NAME[C.pose]); return false; }
  }
  JsonArrayConst dps = m["dp"];
  if (dps.size() && !arrHas(dps, daypartId(C.daypart))) {
    if (why) *why = "daypart is " + String(C.daypart >= 0 ? daypartName(C.daypart) : "none");
    return false;
  }
  int days = m["days"] | 127;
  if (!((days >> weekdayMon0(C.lt)) & 1)) { if (why) *why = "not active today"; return false; }
  if (!condsOk(m["cond"], why)) return false;
  String what = m["what"] | "";
  if (what.startsWith("screen:") && C.pose >= 0) {
    JsonObject s = findScreen(what.c_str() + 7);
    if (s.isNull()) { if (why) *why = "screen missing"; return false; }
    if (!strcmp(s["o"] | "P", "P") != posePortrait(C.pose)) { if (why) *why = "orientation mismatch"; return false; }
  }
  return true;
}
static std::vector<String> screensOfOrientation(bool portrait) {
  std::vector<String> v;
  for (JsonObjectConst s : cfg["screens"].as<JsonArrayConst>())
    if (!strcmp(s["o"] | "P", portrait ? "P" : "L")) v.push_back(s["id"] | "");
  return v;
}
static bool currentPortrait() { return C.pose >= 0 ? posePortrait(C.pose) : (C.rot & 1) == 1; }

// Pure: picks the target for the current context C.
static String chooseTarget(String& name) {
  uint32_t now = (uint32_t)C.now;
  if (R.gState == G_ARMED) { name = "Guardian"; return "app:guardian"; }
  if (R.ovTarget[0] && now < R.ovUntil) { name = R.ovTarget[0] == 's' && !strncmp(R.ovTarget, "sys:", 4) ? "Notice" : "Show"; return R.ovTarget; }
  bool P = currentPortrait();
  if (R.cycleIdx >= 0 && now < R.cycleUntil) {
    std::vector<String> ids = screensOfOrientation(P);
    if (!ids.empty()) {
      JsonObject s = findScreen(ids[R.cycleIdx % ids.size()].c_str());
      name = s["name"] | "";
      return "screen:" + ids[R.cycleIdx % ids.size()];
    }
  }
  for (JsonObjectConst m : cfg["moments"].as<JsonArrayConst>()) {
    if (momentMatches(m, nullptr)) {
      name = m["name"] | "";
      return m["what"] | "screen:glance_p";
    }
  }
  name = "Glance";
  const char* gid = P ? "glance_p" : "glance_l";
  if (!findScreen(gid).isNull()) return "screen:" + String(gid);
  std::vector<String> ids = screensOfOrientation(P);
  if (!ids.empty()) return "screen:" + ids[0];
  return "sys:No screens";
}
static String predictTarget(time_t t) {
  Ctx saved = C;
  C.now = t;
  C.lt = localTm(t);
  C.mod = C.lt.tm_hour * 60 + C.lt.tm_min;
  C.daypart = daypartIndex(t);
  String name, tg = chooseTarget(name);
  C = saved;
  return tg;
}
// Explains why each Moment does or does not match (console "Why this Moment?").
static void explainMoments(JsonArray out) {
  for (JsonObjectConst m : cfg["moments"].as<JsonArrayConst>()) {
    JsonObject o = out.add<JsonObject>();
    o["name"] = m["name"] | "";
    String why;
    bool ok = momentMatches(m, &why);
    o["match"] = ok;
    o["why"] = ok ? String("matches") : why;
  }
}

static void cycleScreens(int dir) {
  bool P = (gShownRot & 1) == 1;
  std::vector<String> ids = screensOfOrientation(P);
  if (ids.size() < 2) return;
  int n = ids.size(), idx = -1;
  String cur = R.target;
  for (int i = 0; i < n; i++)
    if (cur == "screen:" + ids[i]) idx = i;
  if (idx < 0) idx = 0;
  R.cycleIdx = (idx + dir + n) % n;
  R.cycleUntil = C.now + 600;
  markDirty(true);
}

// ---------------------------------------------------------------------------
// Rendering
static bool isTickWidget(const char* type) {
  const WidgetDef* d = widgetDef(type);
  return d && d->tick;
}
static bool targetHasTick(const String& target) {
  if (!target.startsWith("screen:")) return false;
  JsonObject s = findScreen(target.c_str() + 7);
  for (JsonObjectConst w : s["w"].as<JsonArrayConst>())
    if (isTickWidget(w["t"] | "")) return true;
  return false;
}
static int8_t rotFor(bool portrait) {
  bool curP = (C.rot & 1) == 1;
  if (curP == portrait) return C.rot;
  return portrait ? 1 : 0;
}
static void renderScreenObj(JsonObjectConst s) {
  bool dark = !strcmp(s["theme"] | "light", "dark");
  FG = dark ? 1 : 0;
  BG = dark ? 0 : 1;
  cv->fillScreen(BG);
  for (JsonObjectConst w : s["w"].as<JsonArrayConst>()) {
    const WidgetDef* d = widgetDef(w["t"] | "");
    if (!d) continue;
    int gx = w["x"] | 0, gy = w["y"] | 0, gw = max(1, (int)(w["w"] | 1)), gh = max(1, (int)(w["h"] | 1));
    int px = gx * 80, py = gy * 80, pw = gw * 80, ph = gh * 80;
    if (gy > 0)
      for (int xx = px + 10; xx < px + pw - 10; xx += 4) cv->drawPixel(xx, py, FG);
    WCtx c;
    c.x = px + 10;
    c.y = py + 8;
    c.w = pw - 20;
    c.h = ph - 14;
    c.s = w["s"];
    c.id = w["id"] | "";
    d->draw(c);
  }
}
// Renders target into the canvas. Returns the rotation used.
static int8_t renderTarget(const String& target) {
  bool portrait = targetPortrait(target);
  int8_t rot = rotFor(portrait);
  if (target == "sys:cal1") rot = 0;
  else if (target == "sys:cal2") rot = 1;
  cv->setRotation(rot);
  gHits.clear();
  FG = 0;
  BG = 1;
  if (target.startsWith("screen:")) {
    JsonObject s = findScreen(target.c_str() + 7);
    if (s.isNull()) { cv->fillScreen(BG); drawSys("Screen not found"); }
    else renderScreenObj(s);
  } else if (target.startsWith("app:")) {
    cv->fillScreen(BG);
    const AppDef* a = appDef(target.substring(4));
    if (a) a->draw();
    else drawSys("App not found");
  } else {
    cv->fillScreen(BG);
    drawSys(target.startsWith("sys:") ? target.substring(4) : target);
  }
  drawToast();
  return rot;
}

static bool loadLastFrame() {
  if (!gFsOk || !epd.prev) return false;
  File f = LittleFS.open(LAST_FRAME, "r");
  if (!f) return false;
  bool ok = f.read(epd.prev, EPD_BYTES) == EPD_BYTES;
  f.close();
  epd.prevValid = ok;
  return ok;
}
static void saveLastFrame() {
  if (!gFsOk || !epd.prevValid || !sysBool("partialAfterSleep", true)) return;
  File f = LittleFS.open(LAST_FRAME, "w");
  if (!f) return;
  f.write(epd.prev, EPD_BYTES);
  f.close();
}

static void displayCanvas(bool forceFull) {
  uint32_t h = fnv32(cv->getBuffer(), EPD_BYTES, 2166136261u ^ (uint32_t)cv->getRotation());
  if (!forceFull && R.frameValid && !R.lastWasGray && h == R.frameHash) return;
  if (!forceFull && !epd.prevValid && R.frameValid && !R.lastWasGray && sysBool("partialAfterSleep", true)) loadLastFrame();
  bool full = forceFull || !epd.prevValid || R.partials >= sysInt("fullEvery", 10);
  epd.toRam(cv->getBuffer(), epd.ram);
  bool ok = epd.showRam(epd.ram, full ? 0 : 1);
  if (full) { R.partials = 0; R.fullRefreshes++; }
  else R.partials++;
  R.refreshes++;
  R.frameHash = h;
  R.frameValid = ok;
  R.lastWasGray = 0;
  gShownRot = cv->getRotation();
}

static void displayGallery() {
  String id = galleryCurrent();
  uint32_t h = fnvStr(id.c_str(), 0x9E3779B9u ^ (uint32_t)cv->getRotation());
  if (R.lastWasGray && R.frameValid && h == R.frameHash) return;
  uint8_t* p2 = (uint8_t*)psAlloc(EPD_BYTES);
  if (!p2) { displayCanvas(true); return; }
  String path = "/img/" + id + ".bin";
  drawImagePlane(path, 1);
  epd.toRam(cv->getBuffer(), epd.ram);
  drawImagePlane(path, 0);
  epd.toRam(cv->getBuffer(), p2);
  epd.showGray(epd.ram, p2);
  free(p2);
  R.frameHash = h;
  R.frameValid = 1;
  R.lastWasGray = 1;
  R.partials = 0;
  R.refreshes++;
  gShownRot = cv->getRotation();
}

static void renderAndShow(const String& target, bool forceFull) {
  invalidateDocs();
  renderTarget(target);
  if (target == "app:gallery" && galleryWantsGray()) displayGallery();
  else displayCanvas(forceFull);
}

// ---------------------------------------------------------------------------
// Data refresh
static bool isAiNeed(const String& k) {
  return k.startsWith("ai_") || k.startsWith("ai:") || k == "outfit" || k == "chef";
}
static uint32_t needTtl(const String& k) {
  if (k == "wx" || k == "cal") return 1800;
  if (k == "air" || k.startsWith("news:")) return 3600;
  if (k.startsWith("stk:")) {
    int wd = weekdayMon0(C.lt);
    return wd >= 5 ? 6 * 3600 : 900;
  }
  if (k.startsWith("cg:")) return 900;
  if (k.startsWith("json:")) {
    String wid = k.substring(5);
    for (JsonObjectConst s : cfg["screens"].as<JsonArrayConst>())
      for (JsonObjectConst w : s["w"].as<JsonArrayConst>())
        if (wid == (w["id"] | "")) return (uint32_t)(w["s"]["ttl"] | 30) * 60;
    return 1800;
  }
  return 3600;
}
static JsonObjectConst findWidget(const String& wid) {
  for (JsonObjectConst s : cfg["screens"].as<JsonArrayConst>())
    for (JsonObjectConst w : s["w"].as<JsonArrayConst>())
      if (wid == (w["id"] | "")) return w;
  return JsonObjectConst();
}
static String needKey(const String& k) {
  time_t now = C.now;
  String dp = String(C.daypart);
  if (k.startsWith("ai_news:") || k == "ai_brief") return dateKey(now) + "/" + dp;
  if (k == "outfit") return dateKey(now);
  if (k == "chef") return chefKey(now);
  if (k.startsWith("ai:")) {
    JsonObjectConst w = findWidget(k.substring(3));
    if (!strcmp(w["s"]["refresh"] | "daily", "daypart")) return dateKey(now) + "/" + dp;
    return dateKey(now);
  }
  return String();
}
static bool needStale(const String& k, bool manual) {
  uint32_t now = time(nullptr);
  if (isAiNeed(k) && !aiConfigured()) return false;
  if ((k == "wx" || k == "air" || k == "outfit") && !hasLocation()) return false;
  if (k.startsWith("stk:") && !secretHas("stk_key")) return false;
  if (k == "cal" && !secretHas("ics0") && !secretHas("ics1") && !secretHas("ics2")) return false;
  uint32_t f = tsFail(k);
  if (f && now - f < 600 && !manual) return false;
  String ck = needKey(k);
  if (ck.length()) return tsContentKey(k) != ck;
  uint32_t t = tsGet(k);
  if (!t || manual) return true;
  return now - t >= needTtl(k) * ttlScale();
}
static bool fetchNeed(const String& k) {
  bool ok = false;
  LOGF("[fetch] %s\n", k.c_str());
  if (k == "wx") ok = fetchWeather();
  else if (k == "air") ok = fetchAir();
  else if (k == "cal") ok = fetchCalendars();
  else if (k.startsWith("news:")) ok = fetchNews(k.substring(5).toInt());
  else if (k.startsWith("stk:")) {
    int c2 = k.lastIndexOf(':');
    ok = fetchStock(k.substring(4, c2), k.substring(c2 + 1));
  } else if (k.startsWith("cg:")) {
    int c2 = k.lastIndexOf(':');
    ok = fetchCrypto(k.substring(3, c2), k.substring(c2 + 1));
  } else if (k.startsWith("json:")) {
    JsonObjectConst w = findWidget(k.substring(5));
    ok = !w.isNull() && fetchJsonValue(k.substring(5), w["s"]);
  } else if (k.startsWith("ai_news:")) ok = genNewsSummary(k.substring(8).toInt());
  else if (k == "ai_brief") ok = genBriefing();
  else if (k == "outfit") ok = wxOk() && genOutfit();
  else if (k == "chef") { ok = genChef(); chefLoaded = false; }
  else if (k.startsWith("ai:")) {
    JsonObjectConst w = findWidget(k.substring(3));
    ok = !w.isNull() && genAiText(k.substring(3), w["s"]);
  }
  if (ok) tsSet(k, needKey(k));
  else tsMarkFail(k);
  return ok;
}
static void collectNeeds(const String& target, std::vector<String>& out) {
  if (target.startsWith("screen:")) {
    JsonObject s = findScreen(target.c_str() + 7);
    for (JsonObjectConst w : s["w"].as<JsonArrayConst>()) widgetNeeds(w, out);
  } else if (target == "app:chef") {
    out.push_back("chef");
  }
}
static void staleNeeds(std::vector<String>& stale, bool manual) {
  std::vector<String> needs;
  collectNeeds(R.target, needs);
  time_t nb = nextDaypartChange(C.now);
  if (nb - C.now <= 600) collectNeeds(predictTarget(nb + 30), needs);
  if (gChefRegenReq) needs.push_back("chef");
  std::vector<String> ai;
  for (auto& k : needs) {
    bool dup = false;
    for (auto& s : stale) dup |= s == k;
    for (auto& s : ai) dup |= s == k;
    if (dup) continue;
    bool st = (k == "chef" && gChefRegenReq) ? aiConfigured() : needStale(k, manual && !isAiNeed(k));
    if (!st) continue;
    if (isAiNeed(k)) ai.push_back(k);
    else stale.push_back(k);
  }
  for (auto& k : ai) stale.push_back(k);
}
// Fetches everything stale for the visible (and soon-visible) target. Returns true if data changed.
static bool runFetches(bool manual) {
  std::vector<String> stale;
  staleNeeds(stale, manual);
  bool needNtp = !timeValid() || (time(nullptr) - R.lastNtp > 86400);
  if (stale.empty() && !needNtp) return false;
  if (!cfg["wifi"].size()) return false;
  if (!wifiConnect()) return false;
  if (needNtp) ntpSync();
  bool any = false;
  uint32_t t0 = millis();
  for (auto& k : stale) {
    if (millis() - t0 > 90000) break;
    if (k == "chef") {
      gChefBusy = true;
      if (String(R.target) == "app:chef") renderAndShow(R.target, false);
    }
    any |= fetchNeed(k);
    if (k == "chef") { gChefBusy = false; gChefRegenReq = false; any = true; }
  }
  tsSave();
  invalidateDocs();
  return any;
}

// ---------------------------------------------------------------------------
// Automations
static void runActions(JsonObjectConst a) {
  logEvent("%s", (a["name"] | "Automation"));
  for (JsonObjectConst act : a["act"].as<JsonArrayConst>()) {
    const char* t = act["t"] | "";
    if (!strcmp(t, "notify")) {
      notifySend(fillVars(act["title"] | "Moments"), fillVars(act["msg"] | ""), act["pri"] | 3, act["tags"] | "", act["click"] | "");
    } else if (!strcmp(t, "sound")) {
      playSound(act["preset"] | "chime");
    } else if (!strcmp(t, "show")) {
      strlcpy(R.ovTarget, act["what"] | "", sizeof(R.ovTarget));
      R.ovUntil = (uint32_t)nowT() + (uint32_t)(act["min"] | 10) * 60;
      markDirty(true);
    }
  }
}
static bool crossed(time_t last, time_t now, time_t at) { return at > last && at <= now; }
static bool timeCrossed(time_t last, time_t now, int mod) {
  if (mod < 0) return false;
  return crossed(last, now, atMinuteOfDay(now, mod)) || crossed(last, now, atMinuteOfDay(now, mod, -1));
}
static bool trigFired(JsonObjectConst tr, int i, time_t last, time_t now) {
  const char* t = tr["t"] | "";
  float v = tr["v"] | 0.0f;
  if (!strcmp(t, "time")) return timeCrossed(last, now, parseHHMM(tr["at"] | "", -1));
  if (!strcmp(t, "wake_time")) {
    int wd = weekdayMon0(C.lt);
    return timeCrossed(last, now, parseHHMM(cfg["profile"]["wake"][wd] | "07:00", -1));
  }
  if (!strcmp(t, "daypart_start")) return R.prevDaypart != -2 && C.daypart != R.prevDaypart && !strcmp(daypartId(C.daypart), tr["v"] | "");
  if (!strcmp(t, "daypart_end")) return R.prevDaypart >= 0 && C.daypart != R.prevDaypart && !strcmp(daypartId(R.prevDaypart), tr["v"] | "");
  if (!strcmp(t, "sun")) {
    const char* ev = tr["ev"] | "sunrise";
    int off = tr["off"] | 0;
    return crossed(last, now, sunEvent(now, ev, off)) || crossed(last, now, sunEvent(now - 86400, ev, off));
  }
  if (!strcmp(t, "motion")) return gMotionFlag;
  if (!strcmp(t, "pose_became")) return C.pose >= 0 && C.pose != R.prevPose && R.prevPose != -2 && arrHas(tr["v"], POSE_CODE[C.pose]);
  if (!strcmp(t, "pose_lost")) return gPoseLost;
  if (!strcmp(t, "owner_left")) return R.prevOwner == 1 && C.ownerHome == 0;
  if (!strcmp(t, "owner_arrived")) return R.prevOwner == 0 && C.ownerHome == 1;
  if (!strcmp(t, "usb_on")) return R.prevUsb == 0 && C.usb;
  if (!strcmp(t, "usb_off")) return R.prevUsb == 1 && !C.usb;
  // Threshold triggers with hysteresis (autoArmed[i] re-arms past the band).
  float x;
  bool valid, above;
  float hyst = tr["hyst"] | 0.0f;
  if (!strcmp(t, "temp_above") || !strcmp(t, "temp_below")) {
    valid = C.sensOk; x = userTemp(C.temp); above = t[5] == 'a';
    if (hyst <= 0) hyst = unitsF() ? 1.0f : 0.5f;
  } else if (!strcmp(t, "hum_above") || !strcmp(t, "hum_below")) {
    valid = C.sensOk; x = C.hum; above = t[4] == 'a';
    if (hyst <= 0) hyst = 3;
  } else if (!strcmp(t, "battery_below")) {
    valid = C.soc >= 0; x = C.soc; above = false;
    if (hyst <= 0) hyst = 3;
  } else return false;
  if (!valid) return false;
  if (above) {
    if (x < v - hyst) R.autoArmed[i] = 1;
    if (R.autoArmed[i] && x > v) { R.autoArmed[i] = 0; return true; }
  } else {
    if (x > v + hyst) R.autoArmed[i] = 1;
    if (R.autoArmed[i] && x <= v) { R.autoArmed[i] = 0; return true; }
  }
  return false;
}
static void evalAutomations() {
  String js;
  serializeJson(cfg["autos"], js);
  uint32_t h = fnvStr(js.c_str());
  time_t now = C.now;
  if (h != R.autoHash) {
    R.autoHash = h;
    memset(R.autoLast, 0, sizeof(R.autoLast));
    memset(R.autoArmed, 1, sizeof(R.autoArmed));
  }
  time_t last = R.lastEval ? (time_t)R.lastEval : now;
  if (last > now || now - last > 6 * 3600) last = now - 1;
  int i = 0;
  for (JsonObjectConst a : cfg["autos"].as<JsonArrayConst>()) {
    if (i >= MAX_AUTOS) break;
    if (a["on"] | true) {
      bool fired = trigFired(a["trig"], i, last, now);
      uint32_t cool = (uint32_t)(a["cool"] | 0) * 60;
      if (fired && condsOk(a["cond"], nullptr) && (!R.autoLast[i] || (uint32_t)now - R.autoLast[i] >= cool)) {
        R.autoLast[i] = now;
        runActions(a);
      }
    }
    i++;
  }
  R.lastEval = now;
  R.prevDaypart = C.daypart;
  R.prevUsb = C.usb;
  R.prevOwner = C.ownerHome;
  R.prevSoc = C.soc;
  R.prevPose = C.pose;
  gMotionFlag = false;
  gPoseLost = false;
}

// ---------------------------------------------------------------------------
// Guardian (door notifier)
static bool guardianEnabled() { return (cfg["apps"]["guardian"]["on"] | false) && cfg["owners"].size() > 0; }
static bool inArmingPose() { return C.pose >= 0 && arrHas(cfg["apps"]["guardian"]["poses"], POSE_CODE[C.pose]); }
static const uint16_t G_OFFS[4] = {30, 60, 120, 300};

static void guardianArm() {
  R.gState = G_ARMED;
  R.gArmedSince = nowT();
  logEvent("Guardian armed");
  playSound("chirp", true);
  markDirty(true);
  notifySend("Guardian active", "You left - Moments is watching the door.", 2, "shield");
}
static void guardianDisarm(const char* why) {
  if (R.gState == G_IDLE) return;
  R.gState = G_IDLE;
  logEvent("Guardian off: %s", why);
  markDirty(true);
}
static void guardianAlarm() {
  JsonObjectConst g = cfg["apps"]["guardian"];
  int grace = constrain((int)(g["grace"] | 20), 5, 120);
  uint8_t vol = cfg["sound"]["vol"] | 70;
  logEvent("Guardian: door moved");
  uint32_t t0 = millis();
  bool found = false;
  while (millis() - t0 < (uint32_t)grace * 1000) {
    buzTone(3000, 120, vol);
    if (cfg["owners"].size()) {
      if (bleScan(1, nullptr)) { found = true; break; }
    } else {
      delay(900);
    }
  }
  buzEnd();
  if (found) {
    R.ownerSeenAt = time(nullptr);
    C.ownerHome = 1;
    guardianDisarm("owner back");
    toast("Welcome back");
    playSound("chime", true);
    return;
  }
  if (time(nullptr) - R.gLastAlert >= 120) {
    R.gLastAlert = time(nullptr);
    logEvent("Guardian ALERT");
    if (g["siren"] | true) playSiren((uint32_t)constrain((int)(g["sirenSec"] | 10), 1, 60) * 1000, 100);
    buzEnd();
    notifySend("Door opened", "Door opened at " + fmtTime(nowT()) + " while you were away.", 5, "rotating_light");
    markDirty();
  }
}
static void guardianOnMotion() {
  if (!guardianEnabled()) return;
  if (R.gState == G_ARMED) { guardianAlarm(); return; }
  // Someone is using the touch screen right now, so the owner is clearly home.
  if (gLastTouchMs && millis() - gLastTouchMs < 60000) return;
  if (R.gState == G_IDLE && inArmingPose()) {
    R.gState = G_CHECKING;
    R.gMotionAt = time(nullptr);
    R.gCheckIdx = 0;
    R.gMisses = 0;
    R.gNextCheck = R.gMotionAt + G_OFFS[0];
    logEvent("Guardian: checking");
    markDirty();
  }
}
static void guardianTick() {
  if (!guardianEnabled()) {
    if (R.gState != G_IDLE) guardianDisarm("disabled");
    return;
  }
  uint32_t now = time(nullptr);
  if (R.gState == G_CHECKING && now >= R.gNextCheck) {
    bool seen = ownerCheck(5);
    R.gMisses = seen ? 0 : R.gMisses + 1;
    R.gCheckIdx++;
    if (R.gMisses >= 2) guardianArm();
    else if (R.gCheckIdx >= 4) { R.gState = G_IDLE; markDirty(); }
    else R.gNextCheck = R.gMotionAt + G_OFFS[R.gCheckIdx];
  }
  if (R.gState == G_IDLE && inArmingPose() && now - R.gLastSlow >= 900) {
    R.gLastSlow = now;
    if (!ownerCheck(5)) {
      R.gState = G_CHECKING;
      R.gMotionAt = now - G_OFFS[0];
      R.gCheckIdx = 1;
      R.gMisses = 1;
      R.gNextCheck = R.gMotionAt + G_OFFS[1];
    }
  }
}

// ---------------------------------------------------------------------------
// Inputs on screens/apps
static void handleInput(const InputEv& e) {
  String target = R.target;
  if (target.startsWith("app:")) {
    const AppDef* a = appDef(target.substring(4));
    int hit = e.k == IN_TAP ? hitTest(e.x, e.y) : -1;
    if (a && a->input(e, hit)) { markDirty(); return; }
    if (e.k == IN_BTN_AI) { gManualRefresh = true; markDirty(true); }
    return;
  }
  if (e.k == IN_TAP) {
    if (hitTest(e.x, e.y) == 900) { gOutfitIdx++; markDirty(); return; }
    gManualRefresh = true;
    markDirty();
    return;
  }
  if (e.k == IN_BTN_AI) { gManualRefresh = true; markDirty(true); return; }
  if (e.k == IN_SWIPE_L || e.k == IN_SWIPE_U || e.k == IN_BTN_DOWN) cycleScreens(1);
  else cycleScreens(-1);
}

// ---------------------------------------------------------------------------
// Wake planning: seconds until the next moment something needs to happen.
static int tickMinutes() {
  if (C.usb) return 1;
  const char* p = cfg["sys"]["profile"] | "balanced";
  return !strcmp(p, "eco") ? 15 : (!strcmp(p, "live") ? 1 : 5);
}
static uint32_t planSleepSeconds() {
  time_t now = nowT();
  time_t best = now + 3600;
  auto take = [&](time_t t) { if (t > now && t < best) best = t; };
  take(nextDaypartChange(now));
  take(nextDaypartChange(now) - 300);
  take(dayStart(now, 1) + 5);
  if (R.ovTarget[0]) take(R.ovUntil);
  if (R.cycleUntil) take(R.cycleUntil);
  String tg = R.target;
  if (targetHasTick(tg)) {
    long iv = tickMinutes() * 60;
    take((now / iv + 1) * iv + 2);
  }
  take(appsNextChange(tg));
  std::vector<String> needs;
  collectNeeds(tg, needs);
  uint32_t real = time(nullptr);
  for (auto& k : needs) {
    if (isAiNeed(k)) continue;
    uint32_t t = tsGet(k), f = tsFail(k);
    if (f && real - f < 600) take(now + (600 - (real - f)) + 5);
    else if (t) take(now + (long)(t + needTtl(k) * ttlScale()) - (long)real + 5);
  }
  for (JsonObjectConst a : cfg["autos"].as<JsonArrayConst>()) {
    if (!(a["on"] | true)) continue;
    JsonObjectConst tr = a["trig"];
    const char* t = tr["t"] | "";
    if (!strcmp(t, "time")) take(nextAt(now, parseHHMM(tr["at"] | "00:00", 0)));
    else if (!strcmp(t, "wake_time")) take(nextWakeTime(now));
    else if (!strcmp(t, "sun")) {
      take(sunEvent(now, tr["ev"] | "sunrise", tr["off"] | 0));
      take(sunEvent(now + 86400, tr["ev"] | "sunrise", tr["off"] | 0));
    } else if (!strncmp(t, "temp", 4) || !strncmp(t, "hum", 3)) take(now + sensorInterval());
  }
  if (guardianEnabled()) {
    if (R.gState == G_CHECKING) take(now + ((long)R.gNextCheck - (long)real));
    else if (R.gState == G_IDLE && inArmingPose()) take(now + ((long)(R.gLastSlow + 900) - (long)real));
  }
  if (gOwnerUsed) take(now + ((long)(R.lastPresence + 900) - (long)real));
  long s = (long)(best - now);
  return (uint32_t)constrain(s, 10L, 3600L);
}

static void computeOwnerUsed() {
  String a, b;
  serializeJson(cfg["moments"], a);
  serializeJson(cfg["autos"], b);
  gOwnerUsed = (a.indexOf("\"owner") >= 0 || b.indexOf("\"owner") >= 0) && cfg["owners"].size() > 0;
}
