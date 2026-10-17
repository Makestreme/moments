// Moments firmware - sounds, notifications (ntfy / Discord / Telegram / webhook) and BLE owner presence.
#pragma once
#include "ai.h"

// ---------------------------------------------------------------------------
// Sounds
struct Note { uint16_t f, ms; };
static const Note SND_CHIME[] = {{2093, 120}, {2637, 120}, {3136, 240}};
static const Note SND_DOUBLE[] = {{2700, 90}, {0, 80}, {2700, 90}};
static const Note SND_ALERT[] = {{3000, 150}, {0, 80}, {3000, 150}, {0, 80}, {3000, 150}};
static const Note SND_BEEP[] = {{2700, 80}};
static const Note SND_CHIRP[] = {{3000, 40}, {3600, 60}};
static const Note SND_WAKE[] = {{2093, 150}, {2349, 150}, {2637, 150}, {3136, 300}, {0, 150}, {2637, 150}, {3136, 400}, {0, 500}};

static void playNotes(const Note* n, int count, uint8_t vol) {
  for (int i = 0; i < count; i++) buzTone(n[i].f, n[i].ms, vol);
}
static bool quietNow() {
  JsonObjectConst s = cfg["sound"];
  if (!(s["quiet"] | false)) return false;
  return inWindow(modOf(nowT()), parseHHMM(s["quietStart"] | "22:00", 1320), parseHHMM(s["quietEnd"] | "07:00", 420));
}
static void playSiren(uint32_t ms, uint8_t vol) {
  uint32_t t0 = millis();
  while (millis() - t0 < ms) {
    buzTone(2400, 220, vol);
    buzTone(3200, 220, vol);
  }
}
static void playSound(const char* preset, bool force = false) {
  if (!force && quietNow()) return;
  uint8_t vol = cfg["sound"]["vol"] | 70;
  if (!strcmp(preset, "chime")) playNotes(SND_CHIME, 3, vol);
  else if (!strcmp(preset, "double")) playNotes(SND_DOUBLE, 3, vol);
  else if (!strcmp(preset, "alert")) playNotes(SND_ALERT, 5, vol);
  else if (!strcmp(preset, "chirp")) playNotes(SND_CHIRP, 2, vol);
  else if (!strcmp(preset, "siren")) playSiren(4000, 100);
  else if (!strcmp(preset, "wake")) { for (int i = 0; i < 3; i++) playNotes(SND_WAKE, 8, vol); }
  else playNotes(SND_BEEP, 1, vol);
  buzEnd();
}

// ---------------------------------------------------------------------------
// Template variables for notification texts
static String fillVars(String s) {
  char b[16];
  if (C.sensOk) {
    s.replace("{{temp}}", tempStr(unitsF() ? C.temp * 9 / 5 + 32 : C.temp, true));
    snprintf(b, sizeof(b), "%d", (int)lroundf(C.hum));
    s.replace("{{humidity}}", b);
  } else {
    s.replace("{{temp}}", "?");
    s.replace("{{humidity}}", "?");
  }
  s.replace("{{pose}}", C.pose >= 0 ? POSE_NAME[C.pose] : "none");
  s.replace("{{time}}", fmtTime(nowT()));
  s.replace("{{date}}", fmtDate(nowT(), 1));
  snprintf(b, sizeof(b), "%d", C.soc);
  s.replace("{{battery}}", b);
  s.replace("{{moment}}", R.moment);
  // degree marker back to UTF-8 for phones
  s.replace(String((char)0x7F), "\xC2\xB0");
  return s;
}

static bool notifyNtfy(const String& title, const String& msg, int pri, const String& tags, const String& click) {
  String topic, tok;
  if (!secretGet("ntfy_topic", topic)) return false;
  secretGet("ntfy_tok", tok);
  String server = cfg["notify"]["ntfyServer"] | "https://ntfy.sh";
  while (server.endsWith("/")) server.remove(server.length() - 1);
  Headers h;
  String t = asciiFold(title.c_str());
  t.replace(String((char)0x7F), "deg");
  h.push_back({"Title", t});
  h.push_back({"Priority", String(constrain(pri, 1, 5))});
  if (tags.length()) h.push_back({"Tags", tags});
  if (click.length()) h.push_back({"Click", click});
  if (tok.length()) h.push_back({"Authorization", "Bearer " + tok});
  String resp;
  int code = httpRequest("POST", server + "/" + urlEncode(topic), msg, "text/plain; charset=utf-8", &h, &resp, 2048);
  LOGF("[notify] ntfy %d\n", code);
  return code == 200;
}
static bool notifyDiscord(const String& title, const String& msg) {
  String url;
  if (!secretGet("discord", url)) return false;
  JsonDocument d;
  d["content"] = "**" + title + "**\n" + msg;
  String body;
  serializeJson(d, body);
  String resp;
  int code = httpRequest("POST", url, body, "application/json", nullptr, &resp, 2048);
  return code >= 200 && code < 300;
}
static bool notifyTelegram(const String& title, const String& msg) {
  String tok;
  const char* chat = cfg["notify"]["tgChat"] | "";
  if (!secretGet("tg_tok", tok) || !*chat) return false;
  JsonDocument d;
  d["chat_id"] = chat;
  d["text"] = title + "\n" + msg;
  String body;
  serializeJson(d, body);
  String resp;
  int code = httpRequest("POST", "https://api.telegram.org/bot" + tok + "/sendMessage", body, "application/json", nullptr, &resp, 2048);
  return code == 200;
}
static bool notifyWebhook(const String& title, const String& msg, int pri) {
  String url;
  if (!secretGet("webhook", url)) return false;
  JsonDocument d;
  d["title"] = title;
  d["message"] = msg;
  d["priority"] = pri;
  d["device"] = gDeviceId;
  String body;
  serializeJson(d, body);
  String resp;
  int code = httpRequest("POST", url, body, "application/json", nullptr, &resp, 2048);
  return code >= 200 && code < 300;
}

static bool notifySend(const String& title, const String& msg, int pri = 3, const String& tags = String(), const String& click = String()) {
  if (!wifiConnect()) { logEvent("Notify failed: no Wi-Fi"); return false; }
  JsonObjectConst n = cfg["notify"];
  bool any = false;
  if (n["ntfy"] | true) any |= notifyNtfy(title, msg, pri, tags, click);
  if (n["discord"] | false) any |= notifyDiscord(title, msg);
  if (n["telegram"] | false) any |= notifyTelegram(title, msg);
  if (n["webhook"] | false) any |= notifyWebhook(title, msg, pri);
  if (!any) logEvent("Notify failed");
  return any;
}

// ---------------------------------------------------------------------------
// BLE owner devices: static MAC or IRK (resolvable private address)
bool gBleInit = false;
struct BleHit { String addr, name; int rssi; };
struct OwnerKey { uint8_t type; uint8_t mac[6]; uint8_t irk[16]; };

static int hexVal(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}
static bool parseHexBytes(const char* s, uint8_t* out, int n) {
  int got = 0, hi = -1;
  for (; *s && got < n; s++) {
    int v = hexVal(*s);
    if (v < 0) continue;
    if (hi < 0) hi = v;
    else { out[got++] = (uint8_t)(hi << 4 | v); hi = -1; }
  }
  return got == n;
}
static bool rpaMatch(const uint8_t a[6], const uint8_t irk[16]) {
  if ((a[0] & 0xC0) != 0x40) return false;
  uint8_t pt[16] = {0}, out[16];
  pt[13] = a[0]; pt[14] = a[1]; pt[15] = a[2];
  mbedtls_aes_context ctx;
  mbedtls_aes_init(&ctx);
  mbedtls_aes_setkey_enc(&ctx, irk, 128);
  mbedtls_aes_crypt_ecb(&ctx, MBEDTLS_AES_ENCRYPT, pt, out);
  mbedtls_aes_free(&ctx);
  return out[13] == a[3] && out[14] == a[4] && out[15] == a[5];
}
static std::vector<OwnerKey> loadOwners() {
  std::vector<OwnerKey> v;
  for (JsonObjectConst o : cfg["owners"].as<JsonArrayConst>()) {
    OwnerKey k;
    memset(&k, 0, sizeof(k));
    const char* val = o["v"] | "";
    if (!strcmp(o["type"] | "mac", "irk")) {
      if (!parseHexBytes(val, k.irk, 16)) continue;
      k.type = 1;
    } else {
      if (!parseHexBytes(val, k.mac, 6)) continue;
      k.type = 0;
    }
    v.push_back(k);
  }
  return v;
}
static bool isOwnerAddr(const uint8_t a[6], const std::vector<OwnerKey>& keys) {
  for (auto& k : keys) {
    if (k.type == 0) {
      if (!memcmp(a, k.mac, 6)) return true;
    } else {
      if (rpaMatch(a, k.irk)) return true;
      uint8_t rev[16];
      for (int i = 0; i < 16; i++) rev[i] = k.irk[15 - i];
      if (rpaMatch(a, rev)) return true;
    }
  }
  return false;
}

// Scans for secs seconds. Returns true if an owner device was seen. Optionally lists everything seen.
static bool bleScan(uint32_t secs, std::vector<BleHit>* all) {
  if (!gBleInit) {
    BLEDevice::init("");
    gBleInit = true;
  }
  std::vector<OwnerKey> keys = loadOwners();
  BLEScan* sc = BLEDevice::getScan();
  sc->setActiveScan(all != nullptr);
  sc->setInterval(100);
  sc->setWindow(99);
  BLEScanResults* res = sc->start(secs, false);
  bool found = false;
  if (res) {
    int n = res->getCount();
    for (int i = 0; i < n; i++) {
      BLEAdvertisedDevice d = res->getDevice(i);
      String a = String(d.getAddress().toString().c_str());
      uint8_t mac[6];
      if (parseHexBytes(a.c_str(), mac, 6) && isOwnerAddr(mac, keys)) found = true;
      if (all && all->size() < 60) all->push_back({a, d.haveName() ? String(d.getName().c_str()) : String(), d.getRSSI()});
    }
  }
  sc->clearResults();
  return found;
}

// Owner presence check. Updates C.ownerHome.
static bool ownerCheck(uint32_t secs) {
  R.lastPresence = time(nullptr);
  if (!cfg["owners"].size()) { C.ownerHome = -1; return false; }
  bool f = bleScan(secs, nullptr);
  if (f) R.ownerSeenAt = time(nullptr);
  C.ownerHome = f ? 1 : 0;
  return f;
}
