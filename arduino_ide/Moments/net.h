// Moments firmware - Wi-Fi, NTP and HTTP(S) helpers.
#pragma once
#include "timeutil.h"
#include "certs.h"

typedef std::vector<std::pair<String, String>> Headers;

static bool waitConnected(uint32_t ms) {
  uint32_t t0 = millis();
  while (millis() - t0 < ms) {
    if (WiFi.status() == WL_CONNECTED) return true;
    delay(50);
  }
  return WiFi.status() == WL_CONNECTED;
}

static bool wifiConnect(uint32_t budgetMs = 20000) {
  if (WiFi.status() == WL_CONNECTED) { gWifiUp = true; return true; }
  JsonArrayConst nets = cfg["wifi"].as<JsonArrayConst>();
  if (!nets.size()) return false;
  if (R.netFailUntil && time(nullptr) < (time_t)R.netFailUntil && !gApMode) return false;
  WiFi.setHostname(MDNS_NAME);
  WiFi.mode(gApMode ? WIFI_AP_STA : WIFI_STA);
  uint32_t t0 = millis();
  // Fast reconnect with cached BSSID/channel.
  if (R.ssid[0] && R.channel > 0) {
    int idx = 0;
    for (JsonObjectConst n : nets) {
      if (!strcmp(n["ssid"] | "", R.ssid)) {
        String pass;
        char key[8];
        snprintf(key, sizeof(key), "wifi%dp", idx);
        secretGet(key, pass);
        WiFi.begin(R.ssid, pass.c_str(), R.channel, R.bssid, true);
        if (waitConnected(6000)) goto connected;
        WiFi.disconnect();
        break;
      }
      idx++;
    }
  }
  {
    int idx = 0;
    for (JsonObjectConst n : nets) {
      if (idx >= 3) break;
      const char* ssid = n["ssid"] | "";
      if (*ssid) {
        String pass;
        char key[8];
        snprintf(key, sizeof(key), "wifi%dp", idx);
        secretGet(key, pass);
        WiFi.begin(ssid, pass.c_str());
        uint32_t left = budgetMs > (millis() - t0) ? budgetMs - (millis() - t0) : 0;
        if (left < 3000) left = 3000;
        if (waitConnected(min<uint32_t>(left, 12000))) {
          strlcpy(R.ssid, ssid, sizeof(R.ssid));
          goto connected;
        }
        WiFi.disconnect();
      }
      idx++;
    }
  }
  LOGF("[wifi] connect failed\n");
  R.netFailUntil = time(nullptr) + 600;
  R.channel = 0;
  if (!gApMode) WiFi.mode(WIFI_OFF);
  gWifiUp = false;
  return false;

connected:
  gWifiUp = true;
  R.netFailUntil = 0;
  R.channel = WiFi.channel();
  memcpy(R.bssid, WiFi.BSSID(), 6);
  LOGF("[wifi] connected %s ip %s\n", WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
  return true;
}

static void wifiOff() {
  if (gApMode) return;
  if (WiFi.getMode() != WIFI_OFF) {
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
  }
  gWifiUp = false;
}

static bool ntpSync() {
  if (!gWifiUp) return false;
  configTzTime(cfg["loc"]["tz"] | "UTC0", "pool.ntp.org", "time.google.com", "time.nist.gov");
  uint32_t t0 = millis();
  while (millis() - t0 < 8000 && sntp_get_sync_status() != SNTP_SYNC_STATUS_COMPLETED) delay(100);
  bool ok = sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED && timeValid();
  if (ok) {
    rtcWrite(time(nullptr));
    R.lastNtp = time(nullptr);
    LOGF("[ntp] synced\n");
  }
  return ok;
}

// ---------------------------------------------------------------------------
static String urlEncode(const String& s) {
  String o;
  const char* hex = "0123456789ABCDEF";
  for (size_t i = 0; i < s.length(); i++) {
    uint8_t c = (uint8_t)s[i];
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') o += (char)c;
    else { o += '%'; o += hex[c >> 4]; o += hex[c & 15]; }
  }
  return o;
}

static WiFiClient* makeClient(const String& url, WiFiClientSecure& sc, WiFiClient& pc) {
  if (url.startsWith("https://")) {
    if (sysBool("insecureTls", false)) sc.setInsecure();
    else sc.setCACert(ROOT_CAS);
    return &sc;
  }
  return &pc;
}

static size_t readBody(HTTPClient& http, String& out, size_t cap) {
  auto* s = http.getStreamPtr();
  int len = http.getSize();
  out = "";
  if (!s) return 0;
  size_t want = len > 0 ? min<size_t>((size_t)len, cap) : cap;
  out.reserve(min<size_t>(want, 65536));
  char buf[768];
  size_t total = 0;
  uint32_t last = millis();
  while (total < want) {
    int av = s->available();
    if (av > 0) {
      int n = s->read((uint8_t*)buf, min<int>((int)sizeof(buf), min<int>(av, (int)(want - total))));
      if (n > 0) { out.concat(buf, n); total += n; last = millis(); }
    } else {
      if (!s->connected()) break;
      if (millis() - last > 12000) break;
      delay(5);
    }
  }
  return total;
}

// Generic request. Returns HTTP status (<0 on transport error). Response truncated to cap bytes.
static int httpRequest(const char* method, const String& url, const String& body, const char* ctype,
                       const Headers* hdrs, String* resp, size_t cap = 65536) {
  if (!gWifiUp || !url.length()) return -1;
  WiFiClientSecure sc;
  WiFiClient pc;
  WiFiClient* cl = makeClient(url, sc, pc);
  HTTPClient http;
  http.setConnectTimeout(10000);
  http.setTimeout(20000);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setUserAgent("Moments/1.0 (ESP32; reTerminal Sticky)");
  http.useHTTP10(true);
  if (!http.begin(*cl, url)) return -2;
  if (ctype && *ctype) http.addHeader("Content-Type", ctype);
  if (hdrs)
    for (auto& h : *hdrs) http.addHeader(h.first, h.second);
  int code = (!strcmp(method, "GET")) ? http.GET() : http.sendRequest(method, body);
  if (resp) {
    if (code > 0) readBody(http, *resp, cap);
    else *resp = http.errorToString(code);
  }
  http.end();
  return code;
}

static int httpGet(const String& url, String& out, size_t cap = 65536, const Headers* hdrs = nullptr) {
  return httpRequest("GET", url, String(), nullptr, hdrs, &out, cap);
}
