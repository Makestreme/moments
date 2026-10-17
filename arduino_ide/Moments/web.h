// Moments firmware - web console server (REST API + embedded single-page UI).
#pragma once
#include "engine.h"
#include "webui.h"

WebServer server(80);
DNSServer gDns;
std::vector<String> gTokens;
uint32_t gWebActivity = 0;
uint8_t gLoginFails = 0;
uint32_t gLoginBlockUntil = 0;
bool gRoutesAdded = false;
bool gMdnsOn = false;
bool gRebootPending = false;
bool gExitConfigReq = false;
File gUpFile;
String gUpErr, gOtaErr;

static void loadTokens() {
  gTokens.clear();
  String s = devGet("tok");
  int start = 0;
  while (start < (int)s.length()) {
    int c = s.indexOf(',', start);
    String t = s.substring(start, c < 0 ? s.length() : c);
    if (t.length() >= 16) gTokens.push_back(t);
    if (c < 0) break;
    start = c + 1;
  }
}
static void saveTokens() {
  String s;
  for (auto& t : gTokens) { if (s.length()) s += ','; s += t; }
  devSet("tok", s);
}
static bool authed() {
  String t = server.header("X-Token");
  if (t.length() < 16) return false;
  for (auto& k : gTokens)
    if (k == t) return true;
  return false;
}
static void sendJson(int code, const JsonDocument& d) {
  String s;
  serializeJson(d, s);
  server.send(code, "application/json", s);
}
static void sendOk() { server.send(200, "application/json", "{\"ok\":true}"); }
static void sendErr(int code, const String& msg) {
  JsonDocument d;
  d["error"] = msg;
  sendJson(code, d);
}
static bool requireAuth() {
  gWebActivity = millis();
  if (authed()) return true;
  sendErr(401, "auth");
  return false;
}
static bool bodyJson(JsonDocument& d, size_t cap = 98304) {
  const String& b = server.arg("plain");
  if (b.length() > cap) { sendErr(413, "too large"); return false; }
  if (deserializeJson(d, b)) { sendErr(400, "bad json"); return false; }
  return true;
}
static String sanitizeId(const String& s) {
  String o;
  for (unsigned i = 0; i < s.length() && o.length() < 24; i++) {
    char c = s[i];
    if (isalnum((uint8_t)c) || c == '_' || c == '-') o += c;
  }
  return o;
}

// 1-bit BMP of the canvas (logical orientation) - the device-rendered preview.
static void put16(uint8_t* p, uint16_t v) { p[0] = v & 0xFF; p[1] = v >> 8; }
static void put32(uint8_t* p, uint32_t v) { for (int i = 0; i < 4; i++) p[i] = (v >> (8 * i)) & 0xFF; }
static void sendCanvasBmp() {
  int W = cv->width(), H = cv->height();
  int rowBytes = ((W + 31) / 32) * 4;
  size_t img = (size_t)rowBytes * H, total = 62 + img;
  uint8_t* buf = (uint8_t*)psAlloc(total);
  if (!buf) { sendErr(500, "no memory"); return; }
  memset(buf, 0, total);
  buf[0] = 'B';
  buf[1] = 'M';
  put32(buf + 2, total);
  put32(buf + 10, 62);
  put32(buf + 14, 40);
  put32(buf + 18, W);
  put32(buf + 22, H);
  put16(buf + 26, 1);
  put16(buf + 28, 1);
  put32(buf + 34, img);
  put32(buf + 38, 9252);
  put32(buf + 42, 9252);
  put32(buf + 46, 2);
  buf[58] = buf[59] = buf[60] = 255;
  for (int y = 0; y < H; y++) {
    uint8_t* row = buf + 62 + (size_t)(H - 1 - y) * rowBytes;
    for (int x = 0; x < W; x++)
      if (cv->getPixel(x, y)) row[x >> 3] |= (uint8_t)(0x80 >> (x & 7));
  }
  server.sendHeader("Cache-Control", "no-store");
  server.send_P(200, "image/bmp", (const char*)buf, total);
  free(buf);
}

static void fillState(JsonDocument& d) {
  d["ver"] = MOMENTS_VERSION;
  d["id"] = gDeviceId;
  d["time"] = (uint32_t)nowT();
  d["timeValid"] = timeValid();
  d["local"] = fmtDate(nowT(), 1) + " " + fmtTime(nowT());
  d["pose"] = C.pose;
  d["poseName"] = C.pose >= 0 ? POSE_NAME[C.pose] : "none";
  d["rawPose"] = R.pose;
  d["rot"] = R.rot;
  d["moment"] = R.moment;
  d["target"] = R.target;
  d["daypart"] = daypartName(C.daypart);
  d["soc"] = C.soc;
  d["mv"] = C.mv;
  d["ma"] = C.ma;
  d["avgMa"] = C.avgMa;
  d["tte"] = C.tte;
  d["usb"] = C.usb;
  d["charging"] = C.charging;
  if (C.sensOk) { d["temp"] = C.temp; d["hum"] = C.hum; }
  JsonObject w = d["wifi"].to<JsonObject>();
  w["up"] = gWifiUp;
  w["ssid"] = gWifiUp ? WiFi.SSID() : String();
  w["ip"] = gWifiUp ? WiFi.localIP().toString() : String();
  w["rssi"] = gWifiUp ? WiFi.RSSI() : 0;
  d["ap"] = gApMode;
  d["mode"] = gMode == MODE_CONFIG ? "config" : (C.usb ? "docked" : (gMode == MODE_INTERACTIVE ? "interactive" : "run"));
  JsonObject g = d["guardian"].to<JsonObject>();
  g["state"] = G_STATE_NAME[R.gState % 3];
  g["since"] = R.gArmedSince;
  g["enabled"] = guardianEnabled();
  g["lastAlert"] = R.gLastAlert;
  d["owner"] = C.ownerHome;
  JsonArray ev = d["events"].to<JsonArray>();
  for (int i = 0; i < R.evCount; i++) {
    int idx = (R.evHead - 1 - i + MAX_EVENTS * 2) % MAX_EVENTS;
    JsonObject e = ev.add<JsonObject>();
    e["t"] = R.ev[idx].t;
    e["m"] = R.ev[idx].msg;
  }
  JsonObject s = d["stats"].to<JsonObject>();
  s["boots"] = R.boots;
  s["wakes"] = R.wakes;
  s["refreshes"] = R.refreshes;
  s["full"] = R.fullRefreshes;
  s["heap"] = ESP.getFreeHeap();
  s["psram"] = ESP.getFreePsram();
  s["fsUsed"] = gFsOk ? (uint32_t)LittleFS.usedBytes() : 0;
  s["fsTotal"] = gFsOk ? (uint32_t)LittleFS.totalBytes() : 0;
  s["uptime"] = millis() / 1000;
  d["imuCal"] = cfg["imu"]["cal"] | false;
  d["epdOk"] = epd.lastOk;
  d["touch"] = gTouchOn;
}

static void handleLogin() {
  gWebActivity = millis();
  if (millis() < gLoginBlockUntil) { sendErr(429, "Too many attempts, wait a minute"); return; }
  JsonDocument d;
  if (!bodyJson(d, 256)) return;
  String pin = d["pin"] | "";
  if (!gPin[0] || pin != gPin) {
    if (++gLoginFails >= 5) { gLoginBlockUntil = millis() + 60000; gLoginFails = 0; }
    sendErr(403, "Wrong PIN. Hold the AI button 3 s on the device to show the PIN.");
    return;
  }
  gLoginFails = 0;
  String tok = randomToken(32);
  gTokens.push_back(tok);
  while (gTokens.size() > 4) gTokens.erase(gTokens.begin());
  saveTokens();
  JsonDocument o;
  o["token"] = tok;
  sendJson(200, o);
}

static void applyConfigChange() {
  applyHwConfig();
  computeOwnerUsed();
  invalidateDocs();
  chefLoaded = false;
  markDirty(true);
}

static void handleConfigPost() {
  if (!requireAuth()) return;
  JsonDocument d(&g_psAlloc);
  if (!bodyJson(d)) return;
  if (!d["screens"].is<JsonArray>() || !d["moments"].is<JsonArray>() || !d["dayparts"].is<JsonArray>()) { sendErr(400, "invalid config"); return; }
  d["v"] = 1;
  JsonDocument def(&g_psAlloc);
  deserializeJson(def, DEFAULT_CONFIG);
  mergeDefaults(d.as<JsonObject>(), def.as<JsonObjectConst>());
  cfg.set(d);
  if (!saveConfig()) { sendErr(500, "save failed"); return; }
  applyConfigChange();
  sendOk();
}

static void handleSecretsGet() {
  if (!requireAuth()) return;
  JsonDocument d;
  for (int i = 0; i < SECRET_COUNT; i++) d[SECRET_KEYS[i]] = secretHas(SECRET_KEYS[i]);
  sendJson(200, d);
}
static void handleSecretsPost() {
  if (!requireAuth()) return;
  JsonDocument d;
  if (!bodyJson(d, 8192)) return;
  for (JsonPair kv : d.as<JsonObject>()) {
    if (!isSecretKey(kv.key().c_str())) continue;
    String v = kv.value() | "";
    if (v.length() > 600) continue;
    secretSet(kv.key().c_str(), v);
  }
  R.netFailUntil = 0;
  markDirty();
  sendOk();
}

static void handleWidgets() {
  if (!requireAuth()) return;
  JsonDocument d(&g_psAlloc);
  JsonArray a = d.to<JsonArray>();
  for (int i = 0; i < WIDGET_COUNT; i++) {
    const WidgetDef& w = WIDGETS[i];
    JsonObject o = a.add<JsonObject>();
    o["type"] = w.type;
    o["name"] = w.name;
    o["group"] = w.group;
    o["minW"] = w.minW;
    o["minH"] = w.minH;
    o["maxW"] = w.maxW;
    o["maxH"] = w.maxH;
    JsonDocument s;
    deserializeJson(s, w.schema);
    o["schema"] = s.as<JsonVariantConst>();
  }
  sendJson(200, d);
}

static void previewDone() {
  sendCanvasBmp();
  gHits.clear();
  markDirty();
}
static void handlePreviewGet() {
  if (!requireAuth()) return;
  String target = server.arg("target");
  if (!target.length()) target = R.target;
  int8_t savedRot = C.rot;
  if (server.hasArg("o")) C.rot = server.arg("o") == "P" ? 1 : 0;
  invalidateDocs();
  renderTarget(target);
  C.rot = savedRot;
  previewDone();
}
static void handlePreviewPost() {
  if (!requireAuth()) return;
  JsonDocument d(&g_psAlloc);
  if (!bodyJson(d, 32768)) return;
  bool P = !strcmp(d["o"] | "P", "P");
  cv->setRotation(P ? 1 : 0);
  gHits.clear();
  invalidateDocs();
  renderScreenObj(d.as<JsonObjectConst>());
  previewDone();
}

static void handleAction() {
  if (!requireAuth()) return;
  JsonDocument d;
  if (!bodyJson(d, 2048)) return;
  String a = d["a"] | "";
  JsonDocument o;
  o["ok"] = true;
  if (a == "refresh") { gManualRefresh = true; markDirty(true); }
  else if (a == "test") {
    JsonObject au = findById(cfg["autos"].as<JsonArray>(), d["id"] | "");
    if (au.isNull()) { sendErr(404, "automation not found"); return; }
    runActions(au);
  } else if (a == "sound") playSound(d["preset"] | "chime", true);
  else if (a == "notifyTest") o["ok"] = notifySend("Moments test", "Notifications work!", 3, "tada");
  else if (a == "guardianArm") guardianArm();
  else if (a == "guardianDisarm") guardianDisarm("console");
  else if (a == "calibrate") {
    String msg;
    o["ok"] = calibrateStep(d["step"] | 1, msg);
    o["msg"] = msg;
  } else if (a == "calibrateShow") {
    R.cycleUntil = 0;
    strlcpy(R.ovTarget, (d["step"] | 1) == 2 ? "sys:cal2" : "sys:cal1", sizeof(R.ovTarget));
    R.ovUntil = nowT() + 300;
    markDirty(true);
  } else if (a == "show") {
    strlcpy(R.ovTarget, d["what"] | "", sizeof(R.ovTarget));
    R.ovUntil = nowT() + (uint32_t)(d["min"] | 5) * 60;
    markDirty(true);
  } else if (a == "clearShow") { R.ovTarget[0] = 0; R.ovUntil = 0; markDirty(true); }
  else if (a == "chef") { gChefRegenReq = true; markDirty(); }
  else if (a == "ntfyTopic") {
    String t = "moments-" + randomToken(20);
    secretSet("ntfy_topic", t);
    o["topic"] = t;
  } else if (a == "logoutAll") { gTokens.clear(); saveTokens(); }
  else if (a == "exitConfig") gExitConfigReq = true;
  else if (a == "reboot") gRebootPending = true;
  else if (a == "factoryReset") { sendJson(200, o); delay(300); factoryReset(); return; }
  else { sendErr(400, "unknown action"); return; }
  sendJson(200, o);
}

static void handleWifiScan() {
  if (!requireAuth()) return;
  if (WiFi.getMode() == WIFI_AP) WiFi.mode(WIFI_AP_STA);
  else if (WiFi.getMode() == WIFI_OFF) WiFi.mode(WIFI_STA);
  int n = WiFi.scanNetworks();
  JsonDocument d;
  JsonArray a = d.to<JsonArray>();
  for (int i = 0; i < n && i < 30; i++) {
    JsonObject o = a.add<JsonObject>();
    o["ssid"] = WiFi.SSID(i);
    o["rssi"] = WiFi.RSSI(i);
    o["open"] = WiFi.encryptionType(i) == WIFI_AUTH_OPEN;
  }
  WiFi.scanDelete();
  sendJson(200, d);
}
static void handleBleScan() {
  if (!requireAuth()) return;
  std::vector<BleHit> hits;
  bleScan(4, &hits);
  std::sort(hits.begin(), hits.end(), [](const BleHit& a, const BleHit& b) { return a.rssi > b.rssi; });
  JsonDocument d;
  JsonArray a = d.to<JsonArray>();
  for (auto& h : hits) {
    JsonObject o = a.add<JsonObject>();
    o["addr"] = h.addr;
    o["name"] = h.name;
    o["rssi"] = h.rssi;
  }
  sendJson(200, d);
}

static void handleUploadData() {
  HTTPUpload& up = server.upload();
  if (up.status == UPLOAD_FILE_START) {
    gUpErr = "";
    if (!authed()) { gUpErr = "auth"; return; }
    String kind = server.arg("kind"), id = sanitizeId(server.arg("id"));
    if (!id.length()) { gUpErr = "missing id"; return; }
    LittleFS.mkdir("/img");
    LittleFS.mkdir("/thumb");
    String path = (kind == "thumb" ? "/thumb/" : "/img/") + id + ".bin";
    gUpFile = LittleFS.open(path, "w");
    if (!gUpFile) gUpErr = "fs";
  } else if (up.status == UPLOAD_FILE_WRITE) {
    if (gUpFile && !gUpErr.length()) {
      if (up.totalSize > 200000) { gUpErr = "too large"; gUpFile.close(); }
      else gUpFile.write(up.buf, up.currentSize);
    }
  } else if (up.status == UPLOAD_FILE_END) {
    if (gUpFile) gUpFile.close();
  } else if (up.status == UPLOAD_FILE_ABORTED) {
    if (gUpFile) gUpFile.close();
    gUpErr = "aborted";
  }
}
static void handleUploadDone() {
  gWebActivity = millis();
  if (gUpErr == "auth") { sendErr(401, "auth"); return; }
  if (gUpErr.length()) { sendErr(400, gUpErr); return; }
  markDirty();
  sendOk();
}
static void handleImages() {
  if (!requireAuth()) return;
  JsonDocument d;
  JsonArray a = d.to<JsonArray>();
  for (auto& id : listImages()) a.add(id);
  sendJson(200, d);
}
static void handleImageDelete() {
  if (!requireAuth()) return;
  JsonDocument d;
  if (!bodyJson(d, 256)) return;
  String id = sanitizeId(d["id"] | "");
  String kind = d["kind"] | "img";
  if (id.length()) LittleFS.remove((kind == "thumb" ? "/thumb/" : "/img/") + id + ".bin");
  markDirty();
  sendOk();
}

static void handleListsGet() {
  if (!requireAuth()) return;
  sendJson(200, listsDoc);
}
static void handleListsPost() {
  if (!requireAuth()) return;
  JsonDocument d(&g_psAlloc);
  if (!bodyJson(d, 32768)) return;
  if (!d.is<JsonArray>()) { sendErr(400, "expected array"); return; }
  listsDoc.set(d);
  saveLists();
  markDirty();
  sendOk();
}

static void handleBackup() {
  if (!requireAuth()) return;
  JsonDocument d(&g_psAlloc);
  d.set(cfg);
  for (JsonObject o : d["owners"].as<JsonArray>()) o.remove("v");
  server.sendHeader("Content-Disposition", "attachment; filename=moments-config.json");
  sendJson(200, d);
}

static void handleOtaData() {
  HTTPUpload& up = server.upload();
  if (up.status == UPLOAD_FILE_START) {
    gOtaErr = "";
    if (!authed()) { gOtaErr = "auth"; return; }
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) gOtaErr = Update.errorString();
  } else if (up.status == UPLOAD_FILE_WRITE) {
    if (!gOtaErr.length() && Update.write(up.buf, up.currentSize) != up.currentSize) gOtaErr = Update.errorString();
  } else if (up.status == UPLOAD_FILE_END) {
    if (!gOtaErr.length() && !Update.end(true)) gOtaErr = Update.errorString();
  } else if (up.status == UPLOAD_FILE_ABORTED) {
    Update.abort();
    gOtaErr = "aborted";
  }
}
static void handleOtaDone() {
  if (gOtaErr == "auth") { sendErr(401, "auth"); return; }
  if (gOtaErr.length()) { sendErr(400, gOtaErr); return; }
  sendOk();
  logEvent("Firmware updated");
  gRebootPending = true;
}

static void handleWhy() {
  if (!requireAuth()) return;
  JsonDocument d;
  d["target"] = R.target;
  d["moment"] = R.moment;
  d["pose"] = C.pose >= 0 ? POSE_NAME[C.pose] : "none";
  d["daypart"] = daypartName(C.daypart);
  if (R.gState == G_ARMED) d["override"] = "Guardian is armed";
  else if (R.ovTarget[0] && (uint32_t)nowT() < R.ovUntil) d["override"] = String("Showing ") + R.ovTarget;
  else if (R.cycleIdx >= 0 && (uint32_t)nowT() < R.cycleUntil) d["override"] = "Screen picked by swipe (10 min)";
  explainMoments(d["moments"].to<JsonArray>());
  sendJson(200, d);
}

static void captiveRedirect() {
  server.sendHeader("Location", "http://" + WiFi.softAPIP().toString() + "/", true);
  server.send(302, "text/plain", "");
}

static void addRoutes() {
  if (gRoutesAdded) return;
  gRoutesAdded = true;
  static const char* hdrs[] = {"X-Token"};
  server.collectHeaders(hdrs, 1);
  server.on("/", HTTP_GET, []() {
    gWebActivity = millis();
    server.sendHeader("Cache-Control", "no-store");
    server.send_P(200, "text/html", INDEX_HTML, sizeof(INDEX_HTML) - 1);
  });
  for (const char* p : {"/generate_204", "/gen_204", "/hotspot-detect.html", "/connecttest.txt", "/ncsi.txt", "/fwlink", "/redirect"})
    server.on(p, HTTP_GET, captiveRedirect);
  server.on("/api/login", HTTP_POST, handleLogin);
  server.on("/api/state", HTTP_GET, []() {
    if (!requireAuth()) return;
    JsonDocument d(&g_psAlloc);
    fillState(d);
    sendJson(200, d);
  });
  server.on("/api/config", HTTP_GET, []() {
    if (!requireAuth()) return;
    sendJson(200, cfg);
  });
  server.on("/api/config", HTTP_POST, handleConfigPost);
  server.on("/api/secrets", HTTP_GET, handleSecretsGet);
  server.on("/api/secrets", HTTP_POST, handleSecretsPost);
  server.on("/api/widgets", HTTP_GET, handleWidgets);
  server.on("/api/preview", HTTP_GET, handlePreviewGet);
  server.on("/api/preview", HTTP_POST, handlePreviewPost);
  server.on("/api/action", HTTP_POST, handleAction);
  server.on("/api/wifi/scan", HTTP_GET, handleWifiScan);
  server.on("/api/ble/scan", HTTP_GET, handleBleScan);
  server.on("/api/upload", HTTP_POST, handleUploadDone, handleUploadData);
  server.on("/api/images", HTTP_GET, handleImages);
  server.on("/api/images/delete", HTTP_POST, handleImageDelete);
  server.on("/api/lists", HTTP_GET, handleListsGet);
  server.on("/api/lists", HTTP_POST, handleListsPost);
  server.on("/api/backup", HTTP_GET, handleBackup);
  server.on("/api/ota", HTTP_POST, handleOtaDone, handleOtaData);
  server.on("/api/why", HTTP_GET, handleWhy);
  server.onNotFound([]() {
    if (gApMode) {
      String host = server.hostHeader();
      if (host != WiFi.softAPIP().toString() && host != String(MDNS_NAME) + ".local") { captiveRedirect(); return; }
    }
    server.send(404, "text/plain", "Not found");
  });
}

static void mdnsTick() {
  if (gWebOn && gWifiUp && !gMdnsOn && MDNS.begin(MDNS_NAME)) {
    MDNS.addService("http", "tcp", 80);
    gMdnsOn = true;
  }
}
static void startWeb() {
  if (gWebOn) return;
  addRoutes();
  loadTokens();
  server.begin();
  gWebOn = true;
  gWebActivity = millis();
  mdnsTick();
  LOGF("[web] console started\n");
}
static void stopWeb() {
  if (!gWebOn) return;
  server.stop();
  if (gMdnsOn) MDNS.end();
  gMdnsOn = false;
  gWebOn = false;
}
static void startAP() {
  if (gApMode) return;
  gApMode = true;
  WiFi.mode(cfg["wifi"].size() ? WIFI_AP_STA : WIFI_AP);
  WiFi.softAP(gApSsid.c_str(), gApPass);
  delay(100);
  gDns.start(53, "*", WiFi.softAPIP());
  LOGF("[ap] %s up at %s\n", gApSsid.c_str(), WiFi.softAPIP().toString().c_str());
}
static void stopAP() {
  if (!gApMode) return;
  gDns.stop();
  WiFi.softAPdisconnect(true);
  gApMode = false;
  if (!gWifiUp) WiFi.mode(WIFI_OFF);
  else WiFi.mode(WIFI_STA);
}
static void webTick() {
  if (!gWebOn) return;
  if (gApMode) gDns.processNextRequest();
  server.handleClient();
}
