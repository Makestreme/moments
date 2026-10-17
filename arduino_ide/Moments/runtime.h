// Moments firmware - runtime: boot, operating modes (Run / Interactive / Config / Docked / Service), sleep.
#pragma once
#include "web.h"

uint32_t gBootMs = 0;
uint32_t gConfigUntil = 0;
uint32_t gLastFetchTry = 0;
bool gFetchedThisWake = false;
bool gWasUsb = false;
uint8_t gWakeKind = 0;  // 0 cold/reset, 1 timer, 2 button, 3 motion, 4 USB

void logEvent(const char* fmt, ...) {
  char buf[40];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  int idx = R.evHead % MAX_EVENTS;
  R.ev[idx].t = (uint32_t)time(nullptr);
  strlcpy(R.ev[idx].msg, buf, sizeof(R.ev[idx].msg));
  R.evHead = (uint8_t)((R.evHead + 1) % MAX_EVENTS);
  if (R.evCount < MAX_EVENTS) R.evCount++;
  LOGF("[event] %s\n", buf);
}

static uint32_t interactiveMs() {
  const char* p = cfg["sys"]["profile"] | "balanced";
  return !strcmp(p, "eco") ? 30000 : (!strcmp(p, "live") ? 120000 : 60000);
}
void requestInteractive(uint32_t ms) {
  uint32_t until = millis() + ms;
  if ((int32_t)(until - gInteractiveUntil) > 0) gInteractiveUntil = until;
  if (gMode == MODE_RUN) gMode = MODE_INTERACTIVE;
  touchEnable();
}

static void rtcStateInit() {
  memset(&R, 0, sizeof(R));
  R.magic = RTC_MAGIC;
  R.pose = POSE_NONE;
  R.rot = 0;
  R.cycleIdx = -1;
  R.prevDaypart = -2;
  R.prevUsb = 2;
  R.prevOwner = -2;
  R.prevPose = -2;
  R.prevSoc = -1;
  memset(R.autoArmed, 1, sizeof(R.autoArmed));
}

// ---------------------------------------------------------------------------
// Config mode (web console with on-screen PIN / QR)
static void enterConfig() {
  gMode = MODE_CONFIG;
  strlcpy(gPin, randomDigits(6).c_str(), sizeof(gPin));
  if (cfg["wifi"].size()) { R.netFailUntil = 0; wifiConnect(15000); }
  if (!gWifiUp) startAP();
  startWeb();
  gConfigUntil = millis() + 10UL * 60 * 1000;
  gWebActivity = millis();
  requestInteractive(60000);
  logEvent("Setup mode");
  markDirty(true);
}
static void exitConfig() {
  if (gMode != MODE_CONFIG) return;
  stopAP();
  gMode = MODE_INTERACTIVE;
  if (C.usb) {
    wifiConnect();
    startWeb();
  } else {
    stopWeb();
    wifiOff();
  }
  gFetchedThisWake = false;
  logEvent("Setup mode off");
  markDirty(true);
}

// ---------------------------------------------------------------------------
// Power off (hibernate) and deep sleep
static void powerOff(const char* screen) {
  logEvent("Power off");
  R.ovTarget[0] = 0;
  renderAndShow(String("sys:") + screen, true);
  tsSave();
  stopWeb();
  stopAP();
  wifiOff();
  touchDisable();
  buzEnd();
  if (gImuOk) {
    i2cWrite8(Wire, ADDR_IMU, 0x10, 0x00);
    i2cWrite8(Wire, ADDR_IMU, 0x11, 0x00);
  }
  if (gRtcOk) i2cWrite8(Wire, ADDR_RTC, 0x01, 0x00);
  while (digitalRead(PIN_BTN_AI) == LOW) delay(10);
  delay(100);
  Serial.flush();
  powerLatch(false);
  delay(1500);
  // Still running: USB keeps the rail up. Sleep until the AI button is pressed.
  esp_sleep_enable_ext1_wakeup(1ULL << PIN_BTN_AI, ESP_EXT1_WAKEUP_ANY_LOW);
  esp_deep_sleep_start();
}

static void goToSleep(uint32_t secs) {
  saveLastFrame();
  tsSave();
  stopWeb();
  stopAP();
  wifiOff();
  touchDisable();
  buzEnd();
  setChargeEnabled(true);
  imuLowPower((uint8_t)constrain(sysInt("motionThs", 3), 1, 63));
  uint64_t mask = (1ULL << PIN_BTN_AI) | (1ULL << PIN_BTN_UP) | (1ULL << PIN_BTN_DOWN);
  if (gImuOk && digitalRead(PIN_IMU_INT) == HIGH) mask |= (1ULL << PIN_IMU_INT);
  else if (gImuOk) {
    LOGF("[sleep] GPIO7 held low - motion wake disabled this cycle\n");
    secs = min<uint32_t>(secs, 60);
  }
  esp_sleep_enable_timer_wakeup((uint64_t)secs * 1000000ULL);
  esp_sleep_enable_ext1_wakeup(mask, ESP_EXT1_WAKEUP_ANY_LOW);
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_9, 1);
  LOGF("[sleep] %lu s (target %s)\n", (unsigned long)secs, R.target);
  Serial.flush();
  esp_deep_sleep_start();
}

static bool canSleep() {
  if (C.usb) return false;
  if (gMode == MODE_CONFIG) {
    if (millis() - gWebActivity > 10UL * 60 * 1000 && (int32_t)(millis() - gConfigUntil) > 0) exitConfig();
    return false;
  }
  if (gMode == MODE_SERVICE || gDirty || anyButtonDown() || gGest.active) return false;
  if ((int32_t)(gInteractiveUntil - millis()) > 0) return false;
  if (gMode == MODE_INTERACTIVE) gMode = MODE_RUN;
  if (!gPoseSettled && (int32_t)(gPoseWaitUntil - millis()) > 0) return false;
  if (!gFetchedThisWake && cfg["wifi"].size()) return false;
  if (millis() - gBootMs < 400) return false;
  return true;
}

static void chargeLimitTick() {
  if (!C.usb || !sysBool("chargeLimit", false) || C.soc < 0) { setChargeEnabled(true); return; }
  static bool limited = false;
  if (C.soc >= 80) limited = true;
  else if (C.soc <= 75) limited = false;
  setChargeEnabled(!limited);
}

static void onUsbChange() {
  if (C.usb) {
    logEvent("Docked (USB)");
    if (cfg["wifi"].size()) { R.netFailUntil = 0; wifiConnect(); }
    startWeb();
    touchEnable();
  } else {
    logEvent("On battery");
    if (gMode != MODE_CONFIG) { stopWeb(); wifiOff(); }
    setChargeEnabled(true);
  }
  markDirty();
}

// ---------------------------------------------------------------------------
// Service screen (Up + Down held at power-on): works without touch or Wi-Fi.
static void drawService(int sel, const char* const* items, int n, const String& msg) {
  cv->setRotation(0);
  FG = 0;
  BG = 1;
  cv->fillScreen(1);
  gHits.clear();
  drawText(20, 16, "Moments service", B18);
  drawText(560, 22, "v" MOMENTS_VERSION, F9);
  for (int i = 0; i < n; i++) {
    int y = 70 + i * 44;
    if (i == sel) cv->fillRoundRect(14, y - 6, 330, 38, 8, 0);
    drawText(26, y + 2, items[i], B12, 1, i == sel ? 1 : 0);
  }
  drawText(20, 452, "Up/Down: select   AI: run", F9);
  int x = 370, y = 70;
  float a[3] = {0, 0, 0};
  bool ia = imuAccel(a);
  int8_t rot = 0;
  int8_t p = ia ? classifyPose(a, rot) : POSE_NONE;
  batteryRead();
  float t = 0, hm = 0;
  bool sh = shtRead(t, hm);
  time_t rt = 0;
  bool rtcOk = rtcRead(rt);
  char b[96];
  snprintf(b, sizeof(b), "Battery %d%%  %d mV  %d mA%s", C.soc, C.mv, C.ma, C.usb ? "  USB" : "");
  drawText(x, y, b, F12); y += 30;
  snprintf(b, sizeof(b), "SHT40 %s  %.1f C  %.0f %%", sh ? "ok" : "FAIL", t, hm);
  drawText(x, y, b, F12); y += 30;
  snprintf(b, sizeof(b), "IMU %s  %.2f %.2f %.2f g", ia ? "ok" : "FAIL", a[0], a[1], a[2]);
  drawText(x, y, b, F12); y += 30;
  snprintf(b, sizeof(b), "Pose %s  rot %d  cal %s", p >= 0 ? POSE_CODE[p] : "none", rot, (cfg["imu"]["cal"] | false) ? "yes" : "no");
  drawText(x, y, b, F12); y += 30;
  snprintf(b, sizeof(b), "RTC %s  %s", rtcOk ? "ok" : "unset", rtcOk ? (fmtDate(rt, 2) + " " + fmtTime(rt)).c_str() : "");
  drawText(x, y, b, F12); y += 30;
  snprintf(b, sizeof(b), "Touch %s @%02X  GPIO7 %s", gTouchOn ? "ok" : "FAIL", gt.addr, digitalRead(PIN_IMU_INT) ? "high" : "LOW");
  drawText(x, y, b, F12); y += 30;
  snprintf(b, sizeof(b), "FS %s  %u/%u KB", gFsOk ? "ok" : "FAIL", gFsOk ? (unsigned)(LittleFS.usedBytes() / 1024) : 0, gFsOk ? (unsigned)(LittleFS.totalBytes() / 1024) : 0);
  drawText(x, y, b, F12); y += 30;
  snprintf(b, sizeof(b), "PSRAM free %u KB  heap %u KB", (unsigned)(ESP.getFreePsram() / 1024), (unsigned)(ESP.getFreeHeap() / 1024));
  drawText(x, y, b, F12); y += 36;
  if (msg.length()) drawWrapped(x, y, 410, 3, msg, B12);
}
// Returns true if the user picked Setup mode.
static bool runService() {
  static const char* const items[] = {"Refresh diagnostics", "Setup mode (console)", "Orientation step 1", "Orientation step 2",
                                      "Buzzer test", "Factory reset", "Exit (restart)"};
  const int n = 7;
  int sel = 0;
  bool redraw = true;
  uint32_t lastDraw = 0, resetArm = 0;
  String msg = "Diagnostics. Touch the screen to test touch.";
  touchEnable();
  imuActive(3);
  while (true) {
    buttonsTick();
    touchTick();
    InputEv e;
    while (popInput(e)) {
      if (e.k == IN_BTN_UP) sel = (sel + n - 1) % n;
      else if (e.k == IN_BTN_DOWN) sel = (sel + 1) % n;
      else if (e.k == IN_TAP) msg = "Touch at " + String(e.x) + ", " + String(e.y);
      else if (e.k == IN_SWIPE_L || e.k == IN_SWIPE_R || e.k == IN_SWIPE_U || e.k == IN_SWIPE_D) msg = "Swipe detected";
      else if (e.k == IN_BTN_AI) {
        String m2;
        switch (sel) {
          case 0: msg = "Refreshed."; break;
          case 1: return true;
          case 2: calibrateStep(1, m2); msg = m2; break;
          case 3: calibrateStep(2, m2); msg = m2; break;
          case 4: playSound("chime", true); msg = "Did you hear a chime?"; break;
          case 5:
            if (resetArm && millis() - resetArm < 5000) { msg = "Resetting..."; drawService(sel, items, n, msg); displayCanvas(true); factoryReset(); }
            resetArm = millis();
            msg = "Press AI again within 5 s to ERASE everything.";
            break;
          default: ESP.restart();
        }
      }
      redraw = true;
    }
    gHold3 = gHold8 = false;
    if (redraw || millis() - lastDraw > 20000) {
      drawService(sel, items, n, msg);
      displayCanvas(false);
      lastDraw = millis();
      redraw = false;
    }
    delay(10);
  }
}

// ---------------------------------------------------------------------------
static void momentsSetup() {
  powerLatch(true);
  gBootMs = millis();
  peripheralsSafeDefaults();
  Serial.begin(115200);
  esp_sleep_wakeup_cause_t wc = esp_sleep_get_wakeup_cause();
  gColdBoot = wc == ESP_SLEEP_WAKEUP_UNDEFINED;
  if (R.magic != RTC_MAGIC) rtcStateInit();
  R.boots++;
  if (!gColdBoot) R.wakes++;
  LOGF("\n[boot] Moments %s wake=%d\n", MOMENTS_VERSION, (int)wc);

  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, 400000);
  rtcInit();
  time_t rt;
  if (gRtcOk && rtcRead(rt)) {
    struct timeval tv = {rt, 0};
    settimeofday(&tv, nullptr);
  }
  imuInit(gColdBoot);

  int wakeBtn = -1;
  if (wc == ESP_SLEEP_WAKEUP_TIMER) gWakeKind = 1;
  else if (wc == ESP_SLEEP_WAKEUP_EXT0) gWakeKind = 4;
  else if (wc == ESP_SLEEP_WAKEUP_EXT1) {
    uint64_t st = esp_sleep_get_ext1_wakeup_status();
    if (st & (1ULL << PIN_IMU_INT)) gWakeKind = 3;
    if (st & (1ULL << PIN_BTN_AI)) { gWakeKind = 2; wakeBtn = 0; }
    if (st & (1ULL << PIN_BTN_UP)) { gWakeKind = 2; wakeBtn = 1; }
    if (st & (1ULL << PIN_BTN_DOWN)) { gWakeKind = 2; wakeBtn = 2; }
  }

  uint64_t mac = ESP.getEfuseMac();
  char id[8];
  snprintf(id, sizeof(id), "%02X%02X", (uint8_t)(mac >> 32), (uint8_t)(mac >> 40));
  gDeviceId = id;
  gApSsid = String(AP_PREFIX) + id;

  gFsOk = LittleFS.begin(true);
  if (gFsOk) {
    LittleFS.mkdir("/cache");
    LittleFS.mkdir("/img");
    LittleFS.mkdir("/thumb");
  } else LOGF("[fs] LittleFS mount failed - check partitions.csv / flash size\n");
  ensureNamespaces();
  String ap = devGet("appass");
  if (ap.length() < 8) { ap = randomDigits(8); devSet("appass", ap); }
  strlcpy(gApPass, ap.c_str(), sizeof(gApPass));
  loadConfig();
  loadLists();
  tsLoad();
  applyHwConfig();
  computeOwnerUsed();

  cv = new GFXcanvas1(EPD_W, EPD_H);
  if (!cv || !cv->getBuffer()) { LOGF("[gfx] canvas allocation failed - enable OPI PSRAM\n"); while (true) delay(1000); }
  epd.begin();

  uint8_t ths = (uint8_t)constrain(sysInt("motionThs", 3), 1, 63);
  imuActive(ths);
  if (gWakeKind == 3) {
    gMotionFlag = true;
    gGuardMotion = true;
    gPoseWaitUntil = millis() + 5000;
    requestInteractive(15000);
  } else if (!gColdBoot) {
    // A motion wake can be missed (GPIO7 held low at sleep), so check the pose on every wake.
    float a[3];
    int8_t rot = R.rot;
    int8_t p = imuAccel(a) ? classifyPose(a, rot) : POSE_NONE;
    if (p >= 0 && (p != R.pose || rot != R.rot)) gPoseWaitUntil = millis() + 3000;
  }
  batteryRead();
  gLastBattMs = millis();

  // Power-on press of the AI button must not count as a long press.
  if (gColdBoot && digitalRead(PIN_BTN_AI) == LOW) {
    gBtn[0].raw = gBtn[0].down = true;
    gBtn[0].fired = 2;
    gBtn[0].t0 = millis();
  }
  if (gColdBoot && digitalRead(PIN_BTN_UP) == LOW && digitalRead(PIN_BTN_DOWN) == LOW) {
    gMode = MODE_SERVICE;
    LOGF("[boot] service mode\n");
    bool setup = runService();
    gMode = MODE_RUN;
    gBtn[1].fired = gBtn[2].fired = 1;
    if (setup) enterConfig();
  }
  if (wakeBtn >= 0) {
    Btn& b = gBtn[wakeBtn];
    if (digitalRead(b.pin) == LOW) { b.raw = b.down = true; b.t0 = millis(); b.fired = 0; b.chg = millis(); }
    else pushInput(wakeBtn == 0 ? IN_BTN_AI : (wakeBtn == 1 ? IN_BTN_UP : IN_BTN_DOWN));
  }

  if (!C.usb && C.battOk && C.soc >= 0 && C.soc < 3) powerOff("empty");

  updateContext();
  if (gColdBoot && !cfg["wifi"].size() && gMode != MODE_CONFIG) enterConfig();
  gDirty = true;
  if (gColdBoot) gForceFull = true;
}

static void momentsLoop() {
  uint32_t ms = millis();
  buttonsTick();
  touchTick();
  poseTick();
  motionTick();
  webTick();
  if (gRebootPending) {
    delay(800);
    tsSave();
    ESP.restart();
  }
  if (gExitConfigReq) {
    gExitConfigReq = false;
    exitConfig();
  }

  if (gHold3) {
    gHold3 = false;
    if (gMode == MODE_CONFIG) exitConfig();
    else enterConfig();
  }
  if (gHold8) { gHold8 = false; powerOff("off"); }

  InputEv e;
  while (popInput(e)) {
    requestInteractive(interactiveMs());
    if (gMode == MODE_CONFIG && e.k == IN_BTN_AI) { exitConfig(); continue; }
    if (gMode == MODE_CONFIG) continue;
    handleInput(e);
  }

  if (gGuardMotion) {
    gGuardMotion = false;
    guardianOnMotion();
  }

  if (!gLastSec || ms - gLastSec >= 1000) {
    gLastSec = ms;
    if (WiFi.getMode() != WIFI_OFF) gWifiUp = WiFi.status() == WL_CONNECTED;
    mdnsTick();
    updateContext();
    if (C.usb != gWasUsb) { gWasUsb = C.usb; onUsbChange(); }
    chargeLimitTick();
    evalAutomations();
    guardianTick();
    if (C.lt.tm_min != gLastMinute) {
      bool first = gLastMinute < 0;
      gLastMinute = C.lt.tm_min;
      if (!first && targetHasTick(R.target) && (C.usb || (C.mod % tickMinutes()) == 0)) markDirty();
    }
    if (appsTick(R.target)) markDirty();
    if (gToast.length() && (int32_t)(millis() - gToastUntil) > 0) { gToast = ""; markDirty(); }
    if (gOwnerUsed && time(nullptr) - R.lastPresence > 900 && gMode != MODE_CONFIG) ownerCheck(4);
    if (R.ovTarget[0] && (uint32_t)nowT() >= R.ovUntil) { R.ovTarget[0] = 0; markDirty(true); }
    if (R.cycleUntil && (uint32_t)nowT() >= R.cycleUntil) { R.cycleUntil = 0; markDirty(true); }
  }

  String name;
  String target;
  if (gMode == MODE_CONFIG) { target = "sys:config"; name = "Setup"; }
  else target = chooseTarget(name);
  if (target != R.target) {
    bool isApp = target.startsWith("app:");
    if (isApp) { R.app.page = 0; R.app.sel = 0; R.app.listPage = 0; gChefFav = 0; }
    strlcpy(R.target, target.c_str(), sizeof(R.target));
    strlcpy(R.moment, name.c_str(), sizeof(R.moment));
    if (gMode != MODE_CONFIG) logEvent("Moment: %s", R.moment);
    markDirty(true);
  } else if (name != R.moment) {
    strlcpy(R.moment, name.c_str(), sizeof(R.moment));
  }

  if (gDirty) {
    bool full = gForceFull;
    gDirty = false;
    gForceFull = false;
    renderAndShow(target, full);
  }

  if (gMode != MODE_SERVICE && cfg["wifi"].size()) {
    bool manual = gManualRefresh;
    bool doFetch = manual || gChefRegenReq || (C.usb ? (!gLastFetchTry || ms - gLastFetchTry > 60000) : !gFetchedThisWake);
    if (gMode == MODE_CONFIG && !manual && !gChefRegenReq && !gWifiUp) doFetch = false;
    bool posePending = !gPoseSettled && (int32_t)(gPoseWaitUntil - ms) > 0;
    if (doFetch && !gDirty && !posePending) {
      gLastFetchTry = ms;
      gFetchedThisWake = true;
      gManualRefresh = false;
      if (manual) R.netFailUntil = 0;
      bool chefReq = gChefRegenReq;
      if (runFetches(manual)) markDirty();
      if (chefReq && gChefRegenReq) {
        gChefRegenReq = false;
        toast(aiConfigured() ? "No connection - try again" : "Add an AI key first");
        markDirty();
      }
      if (!C.usb && gMode != MODE_CONFIG && gMode != MODE_INTERACTIVE) wifiOff();
    }
  } else {
    gFetchedThisWake = true;
    gManualRefresh = false;
  }

  if (canSleep()) goToSleep(planSleepSeconds());
  delay(gTouchOn ? 5 : 10);
}
