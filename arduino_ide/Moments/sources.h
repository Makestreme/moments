// Moments firmware - data sources and on-flash cache (weather, air, calendar, news, stocks, crypto, JSON).
#pragma once
#include "net.h"

// ---------------------------------------------------------------------------
// Files
static bool writeFile(const String& path, const String& data) {
  File f = LittleFS.open(path, "w");
  if (!f) return false;
  size_t n = f.print(data);
  f.close();
  return n == data.length();
}
static bool readJson(const String& path, JsonDocument& d) {
  d.clear();
  File f = LittleFS.open(path, "r");
  if (!f) return false;
  DeserializationError e = deserializeJson(d, f);
  f.close();
  return !e;
}
static bool writeJson(const String& path, const JsonDocument& d) {
  File f = LittleFS.open(path, "w");
  if (!f) return false;
  serializeJson(d, f);
  f.close();
  return true;
}
static String cachePath(const String& key) {
  String k = key;
  for (unsigned i = 0; i < k.length(); i++)
    if (!isalnum((uint8_t)k[i])) k.setCharAt(i, '_');
  if (k.length() > 40) k = k.substring(0, 30) + "_" + String(fnvStr(key.c_str()), HEX);
  return "/cache/" + k + ".json";
}

// Fetch timestamps: tsDoc[key] = {"t": fetched, "k": content key, "f": last failure}
JsonDocument tsDoc(&g_psAlloc);
bool gTsDirty = false;
static void tsLoad() {
  if (!readJson("/cache/ts.json", tsDoc) || !tsDoc.is<JsonObject>()) tsDoc.to<JsonObject>();
}
static void tsSave() {
  if (gTsDirty && gFsOk) { writeJson("/cache/ts.json", tsDoc); gTsDirty = false; }
}
static uint32_t tsGet(const String& k) { return tsDoc[k]["t"] | 0u; }
static String tsContentKey(const String& k) { return jstr(tsDoc[k]["k"]); }
static uint32_t tsFail(const String& k) { return tsDoc[k]["f"] | 0u; }
static void tsSet(const String& k, const String& key = String()) {
  JsonObject o = tsDoc[k].to<JsonObject>();
  o["t"] = (uint32_t)time(nullptr);
  if (key.length()) o["k"] = key;
  gTsDirty = true;
}
static void tsMarkFail(const String& k) {
  tsDoc[k]["f"] = (uint32_t)time(nullptr);
  gTsDirty = true;
}

// Per-render cached documents.
JsonDocument wxDoc(&g_psAlloc), calDoc(&g_psAlloc);
bool wxLoaded = false, calLoaded = false;
static JsonDocument& wx() {
  if (!wxLoaded) { readJson("/cache/wx.json", wxDoc); wxLoaded = true; }
  return wxDoc;
}
static JsonDocument& calData() {
  if (!calLoaded) { readJson("/cache/cal.json", calDoc); calLoaded = true; }
  return calDoc;
}
static void invalidateDocs() { wxLoaded = calLoaded = false; }
static bool wxOk() { return wx()["current"].is<JsonObjectConst>(); }

static String tempStr(float t, bool unit = false) {
  char b[12];
  snprintf(b, sizeof(b), "%d%c%s", (int)lroundf(t), 0x7F, unit ? (unitsF() ? "F" : "C") : "");
  return String(b);
}

// ---------------------------------------------------------------------------
// Weather & air (Open-Meteo, no key)
static bool fetchWeather() {
  if (!hasLocation()) return false;
  double lat = cfg["loc"]["lat"], lon = cfg["loc"]["lon"];
  char url[700];
  snprintf(url, sizeof(url),
           "https://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f"
           "&current=temperature_2m,apparent_temperature,relative_humidity_2m,weather_code,wind_speed_10m,is_day"
           "&hourly=temperature_2m,precipitation_probability,weather_code,is_day"
           "&daily=weather_code,temperature_2m_max,temperature_2m_min,apparent_temperature_max,apparent_temperature_min,"
           "precipitation_probability_max,sunrise,sunset,uv_index_max,wind_speed_10m_max"
           "&timezone=auto&forecast_days=7&forecast_hours=25&timeformat=unixtime%s",
           lat, lon, unitsF() ? "&temperature_unit=fahrenheit&wind_speed_unit=mph" : "");
  String body;
  int code = httpGet(url, body, 60000);
  if (code != 200) { LOGF("[wx] http %d\n", code); return false; }
  JsonDocument d(&g_psAlloc);
  if (deserializeJson(d, body) || !d["current"].is<JsonObject>()) return false;
  writeFile("/cache/wx.json", body);
  wxLoaded = false;
  return true;
}
static bool fetchAir() {
  if (!hasLocation()) return false;
  double lat = cfg["loc"]["lat"], lon = cfg["loc"]["lon"];
  char url[300];
  snprintf(url, sizeof(url),
           "https://air-quality-api.open-meteo.com/v1/air-quality?latitude=%.4f&longitude=%.4f"
           "&current=us_aqi,european_aqi,uv_index,pm2_5&timezone=auto&timeformat=unixtime",
           lat, lon);
  String body;
  if (httpGet(url, body, 16000) != 200) return false;
  JsonDocument d(&g_psAlloc);
  if (deserializeJson(d, body) || !d["current"].is<JsonObject>()) return false;
  return writeFile("/cache/air.json", body);
}

// ---------------------------------------------------------------------------
// News (RSS / Atom)
static void appendUtf8(String& o, uint32_t cp) {
  if (cp < 0x80) o += (char)cp;
  else if (cp < 0x800) { o += (char)(0xC0 | (cp >> 6)); o += (char)(0x80 | (cp & 0x3F)); }
  else if (cp < 0x10000) { o += (char)(0xE0 | (cp >> 12)); o += (char)(0x80 | ((cp >> 6) & 0x3F)); o += (char)(0x80 | (cp & 0x3F)); }
}
static String xmlText(const String& raw) {
  String t = raw;
  t.trim();
  if (t.startsWith("<![CDATA[")) {
    int e = t.indexOf("]]>");
    t = t.substring(9, e < 0 ? t.length() : e);
  }
  String o;
  bool tag = false;
  for (unsigned i = 0; i < t.length(); i++) {
    char c = t[i];
    if (c == '<') { tag = true; continue; }
    if (tag) { if (c == '>') tag = false; continue; }
    if (c == '&') {
      int semi = t.indexOf(';', i);
      if (semi > 0 && semi - (int)i < 10) {
        String ent = t.substring(i + 1, semi);
        i = semi;
        if (ent == "amp") o += '&';
        else if (ent == "lt") o += '<';
        else if (ent == "gt") o += '>';
        else if (ent == "quot") o += '"';
        else if (ent == "apos") o += '\'';
        else if (ent == "nbsp") o += ' ';
        else if (ent.startsWith("#x") || ent.startsWith("#X")) appendUtf8(o, strtoul(ent.c_str() + 2, nullptr, 16));
        else if (ent.startsWith("#")) appendUtf8(o, strtoul(ent.c_str() + 1, nullptr, 10));
        continue;
      }
    }
    o += c;
  }
  String f = asciiFold(o.c_str());
  f.replace("\n", " ");
  f.trim();
  return f;
}
static void parseFeed(const String& body, JsonArray items, int maxItems) {
  int pos = 0;
  while ((int)items.size() < maxItems) {
    int a = body.indexOf("<item", pos), b = body.indexOf("<entry", pos);
    int st = a < 0 ? b : (b < 0 ? a : min(a, b));
    if (st < 0) break;
    bool isItem = st == a;
    int en = body.indexOf(isItem ? "</item>" : "</entry>", st);
    if (en < 0) en = body.length();
    int t1 = body.indexOf("<title", st);
    if (t1 >= 0 && t1 < en) {
      int t2 = body.indexOf('>', t1), t3 = body.indexOf("</title>", t1);
      if (t2 > 0 && t3 > t2 && t3 <= en) {
        String title = xmlText(body.substring(t2 + 1, t3));
        if (title.length() > 3) items.add(title);
      }
    }
    pos = en + 1;
  }
}
static bool fetchNews(int i) {
  JsonObjectConst f = cfg["feeds"][i];
  const char* url = f["url"] | "";
  if (!*url) return false;
  String body;
  int code = httpGet(url, body, 90000);
  if (code != 200) { LOGF("[news] %d http %d\n", i, code); return false; }
  JsonDocument d(&g_psAlloc);
  d["name"] = f["name"] | "";
  parseFeed(body, d["items"].to<JsonArray>(), 10);
  if (!d["items"].size()) return false;
  return writeJson(cachePath("news:" + String(i)), d);
}

// ---------------------------------------------------------------------------
// Calendar (secret ICS URLs, stream-parsed, RRULE subset)
struct IcsRaw {
  String uid, title, loc, rrule;
  time_t s = 0, e = 0, recur = 0;
  int32_t dur = -1;
  bool allDay = false, cancelled = false, hasEnd = false;
  std::vector<time_t> ex;
};
struct CalOut { time_t s, e; bool ad; String t, l; };

class LineReader {
 public:
  LineReader(WiFiClient* s, size_t cap) : s_(s), cap_(cap) {}
  bool next(String& line) {
    line = "";
    while (true) {
      int c = getc();
      if (c < 0) return line.length() > 0;
      if (c == '\n') {
        if (line.endsWith("\r")) line.remove(line.length() - 1);
        return true;
      }
      if (line.length() < 3000) line += (char)c;
    }
  }

 private:
  WiFiClient* s_;
  size_t cap_, total_ = 0;
  uint8_t buf_[1024];
  int len_ = 0, pos_ = 0;
  int getc() {
    if (pos_ >= len_) {
      uint32_t t0 = millis();
      while (true) {
        if (total_ >= cap_) return -1;
        int av = s_->available();
        if (av > 0) {
          len_ = s_->read(buf_, min<int>(av, (int)sizeof(buf_)));
          pos_ = 0;
          if (len_ > 0) { total_ += len_; break; }
        } else if (!s_->connected() || millis() - t0 > 12000) {
          return -1;
        } else {
          delay(3);
        }
      }
    }
    return buf_[pos_++];
  }
};

static time_t icsTime(const String& params, const String& v, bool& allDay) {
  allDay = params.indexOf("VALUE=DATE") >= 0 && params.indexOf("VALUE=DATE-TIME") < 0;
  if (v.length() < 8) return 0;
  int y = v.substring(0, 4).toInt(), mo = v.substring(4, 6).toInt(), d = v.substring(6, 8).toInt();
  if (v.length() < 15) { allDay = true; return localMake(y, mo, d, 0, 0, 0); }
  int h = v.substring(9, 11).toInt(), mi = v.substring(11, 13).toInt(), s = v.substring(13, 15).toInt();
  if (v.endsWith("Z")) return mkUtc(y, mo, d, h, mi, s);
  return localMake(y, mo, d, h, mi, s);  // TZID treated as device local time
}
static int32_t icsDuration(const String& v) {
  int32_t total = 0, num = 0;
  bool neg = false;
  for (unsigned i = 0; i < v.length(); i++) {
    char c = v[i];
    if (c == '-') neg = true;
    else if (isdigit((uint8_t)c)) num = num * 10 + (c - '0');
    else {
      if (c == 'W') total += num * 7 * 86400;
      else if (c == 'D') total += num * 86400;
      else if (c == 'H') total += num * 3600;
      else if (c == 'M') total += num * 60;
      else if (c == 'S') total += num;
      num = 0;
    }
  }
  return neg ? -total : total;
}
static String icsUnescape(const String& v) {
  String o;
  for (unsigned i = 0; i < v.length(); i++) {
    char c = v[i];
    if (c == '\\' && i + 1 < v.length()) {
      char n = v[++i];
      o += (n == 'n' || n == 'N') ? ' ' : n;
    } else o += c;
  }
  return asciiFold(o.c_str());
}
static String ruleGet(const String& rule, const char* key) {
  String k = String(key) + "=";
  int p = rule.indexOf(k);
  if (p < 0) return String();
  if (p > 0 && rule[p - 1] != ';' && rule[p - 1] != ':') return String();
  int e = rule.indexOf(';', p);
  return rule.substring(p + k.length(), e < 0 ? rule.length() : e);
}
static int icsWeekday(const String& code) {  // returns tm_wday (0 = Sunday)
  static const char* const C[7] = {"SU", "MO", "TU", "WE", "TH", "FR", "SA"};
  for (int i = 0; i < 7; i++)
    if (code.endsWith(C[i])) return i;
  return -1;
}
static int daysInMonth(int y, int m) {
  int ny = m == 12 ? y + 1 : y, nm = m == 12 ? 1 : m + 1;
  return (int)(daysFromCivil(ny, nm, 1) - daysFromCivil(y, m, 1));
}
static int wdayOf(int y, int m, int d) { return localTm(localMake(y, m, d, 12, 0, 0)).tm_wday; }

typedef std::vector<std::pair<String, time_t>> OverrideList;

static void emitOcc(const IcsRaw& m, time_t occ, time_t w0, time_t w1, const OverrideList& ovr, std::vector<CalOut>& out) {
  if (occ + max<int32_t>(m.dur, 1) <= w0 || occ >= w1) return;
  for (time_t x : m.ex)
    if (llabs((long long)(x - occ)) < 60) return;
  for (auto& o : ovr)
    if (o.first == m.uid && llabs((long long)(o.second - occ)) < 60) return;
  if (out.size() < 200) out.push_back({occ, occ + max<int32_t>(m.dur, 0), m.allDay, m.title, m.loc});
}

static void expandRule(const IcsRaw& m, time_t w0, time_t w1, const OverrideList& ovr, std::vector<CalOut>& out) {
  String freq = ruleGet(m.rrule, "FREQ");
  int interval = max(1, (int)ruleGet(m.rrule, "INTERVAL").toInt());
  String cs = ruleGet(m.rrule, "COUNT");
  long count = cs.length() ? cs.toInt() : -1;
  time_t until = 0;
  String us = ruleGet(m.rrule, "UNTIL");
  if (us.length()) {
    bool ad;
    until = icsTime(String(), us, ad);
    if (ad) until += 86399;
  }
  String byday = ruleGet(m.rrule, "BYDAY");
  String bymd = ruleGet(m.rrule, "BYMONTHDAY");
  struct tm st = localTm(m.s);
  int sy = st.tm_year + 1900, smo = st.tm_mon + 1, sd = st.tm_mday, sh = st.tm_hour, smi = st.tm_min, ss = st.tm_sec;
  auto done = [&](time_t occ) { return (until && occ > until) || occ >= w1; };

  if (freq == "DAILY") {
    long k0 = 0;
    if (m.s < w0 - 86400) k0 = (long)((w0 - 86400 - m.s) / 86400) / interval;
    for (long k = k0; k < k0 + 400; k++) {
      if (count > 0 && k >= count) break;
      time_t occ = localMake(sy, smo, sd + (int)(k * interval), sh, smi, ss);
      if (done(occ)) break;
      emitOcc(m, occ, w0, w1, ovr, out);
    }
  } else if (freq == "WEEKLY") {
    uint8_t mask = 0;
    int start = 0;
    while (start < (int)byday.length()) {
      int c = byday.indexOf(',', start);
      String tok = byday.substring(start, c < 0 ? byday.length() : c);
      int wd = icsWeekday(tok);
      if (wd >= 0) mask |= (uint8_t)(1 << ((wd + 6) % 7));
      if (c < 0) break;
      start = c + 1;
    }
    if (!mask) mask = (uint8_t)(1 << weekdayMon0(st));
    int perWeek = __builtin_popcount(mask);
    int wd0 = weekdayMon0(st);
    long wk0 = 0;
    if (m.s < w0 - 8 * 86400) wk0 = (long)((w0 - 8 * 86400 - m.s) / (7 * 86400)) / interval;
    for (long wi = wk0; wi < wk0 + 60; wi++) {
      int rank = 0;
      for (int dd = 0; dd < 7; dd++) {
        if (!(mask & (1 << dd))) continue;
        long idx = wi * perWeek + rank++;
        time_t occ = localMake(sy, smo, sd - wd0 + (int)(wi * interval * 7) + dd, sh, smi, ss);
        if (occ < m.s) continue;
        if (count > 0 && idx >= count) return;
        if (done(occ)) return;
        emitOcc(m, occ, w0, w1, ovr, out);
      }
    }
  } else if (freq == "MONTHLY") {
    struct tm wt = localTm(w0);
    long months = (long)(wt.tm_year + 1900 - sy) * 12 + (wt.tm_mon + 1 - smo) - 1;
    long k0 = months > 0 ? months / interval : 0;
    int ord = 0, wdTarget = -1;
    if (byday.length()) {
      wdTarget = icsWeekday(byday);
      String num = byday.substring(0, byday.length() - 2);
      ord = num.length() ? num.toInt() : 1;
      if (ord == 0) ord = 1;
    }
    int dom = bymd.length() ? bymd.toInt() : sd;
    for (long k = k0; k < k0 + 14; k++) {
      if (count > 0 && k >= count) break;
      long mm0 = (long)(smo - 1) + k * interval;
      int yy = sy + (int)(mm0 / 12), mm = (int)(mm0 % 12) + 1;
      int dim = daysInMonth(yy, mm), day;
      if (wdTarget >= 0) {
        if (ord > 0) {
          int first = wdayOf(yy, mm, 1);
          day = 1 + ((wdTarget - first + 7) % 7) + (ord - 1) * 7;
        } else {
          int last = wdayOf(yy, mm, dim);
          day = dim - ((last - wdTarget + 7) % 7) + (ord + 1) * 7;
        }
      } else {
        day = dom > 0 ? dom : dim + dom + 1;
      }
      if (day < 1 || day > dim) continue;
      time_t occ = localMake(yy, mm, day, sh, smi, ss);
      if (occ < m.s) continue;
      if (done(occ)) break;
      emitOcc(m, occ, w0, w1, ovr, out);
    }
  } else if (freq == "YEARLY") {
    struct tm wt = localTm(w0);
    long k0 = max(0L, (long)(wt.tm_year + 1900 - sy - 1) / interval);
    for (long k = k0; k < k0 + 3; k++) {
      if (count > 0 && k >= count) break;
      int yy = sy + (int)(k * interval);
      if (sd > daysInMonth(yy, smo)) continue;
      time_t occ = localMake(yy, smo, sd, sh, smi, ss);
      if (done(occ)) break;
      emitOcc(m, occ, w0, w1, ovr, out);
    }
  }
}

static bool fetchIcs(String url, time_t w0, time_t w1, std::vector<CalOut>& out) {
  if (url.startsWith("webcal://")) url = "https://" + url.substring(9);
  WiFiClientSecure sc;
  WiFiClient pc;
  WiFiClient* cl = makeClient(url, sc, pc);
  HTTPClient http;
  http.setConnectTimeout(10000);
  http.setTimeout(20000);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.useHTTP10(true);
  if (!http.begin(*cl, url)) return false;
  int code = http.GET();
  if (code != 200) { LOGF("[cal] http %d\n", code); http.end(); return false; }
  LineReader lr(http.getStreamPtr(), 6 * 1024 * 1024);
  std::vector<IcsRaw> masters;
  OverrideList ovr;
  IcsRaw cur;
  bool inEv = false, inAlarm = false;
  String pending, line;
  uint32_t t0 = millis();

  auto process = [&](const String& ln) {
    if (ln.startsWith("BEGIN:VEVENT")) { cur = IcsRaw(); inEv = true; inAlarm = false; return; }
    if (!inEv) return;
    if (ln.startsWith("BEGIN:VALARM")) { inAlarm = true; return; }
    if (ln.startsWith("END:VALARM")) { inAlarm = false; return; }
    if (inAlarm) return;
    if (ln.startsWith("END:VEVENT")) {
      inEv = false;
      if (cur.cancelled || !cur.s) return;
      cur.dur = cur.hasEnd ? (int32_t)(cur.e - cur.s) : (cur.dur >= 0 ? cur.dur : (cur.allDay ? 86400 : 0));
      if (cur.recur) {
        ovr.push_back({cur.uid, cur.recur});
        if (cur.s + cur.dur > w0 && cur.s < w1 && out.size() < 200) out.push_back({cur.s, cur.s + cur.dur, cur.allDay, cur.title, cur.loc});
      } else if (cur.rrule.length()) {
        if (cur.s < w1 && masters.size() < 300) masters.push_back(cur);
      } else if (cur.s + max<int32_t>(cur.dur, 1) > w0 && cur.s < w1 && out.size() < 200) {
        out.push_back({cur.s, cur.s + cur.dur, cur.allDay, cur.title, cur.loc});
      }
      return;
    }
    int colon = -1;
    bool q = false;
    for (unsigned i = 0; i < ln.length(); i++) {
      if (ln[i] == '"') q = !q;
      else if (ln[i] == ':' && !q) { colon = i; break; }
    }
    if (colon < 0) return;
    String head = ln.substring(0, colon), val = ln.substring(colon + 1);
    int semi = head.indexOf(';');
    String name = semi < 0 ? head : head.substring(0, semi);
    String params = semi < 0 ? String() : head.substring(semi + 1);
    bool ad;
    if (name == "DTSTART") { cur.s = icsTime(params, val, ad); cur.allDay = ad; }
    else if (name == "DTEND") { cur.e = icsTime(params, val, ad); cur.hasEnd = cur.e > 0; }
    else if (name == "DURATION") cur.dur = icsDuration(val);
    else if (name == "SUMMARY") cur.title = icsUnescape(val);
    else if (name == "LOCATION") cur.loc = icsUnescape(val);
    else if (name == "UID") cur.uid = val;
    else if (name == "RRULE") cur.rrule = val;
    else if (name == "RECURRENCE-ID") cur.recur = icsTime(params, val, ad);
    else if (name == "STATUS") cur.cancelled = val == "CANCELLED";
    else if (name == "EXDATE") {
      int start = 0;
      while (start < (int)val.length()) {
        int c = val.indexOf(',', start);
        String one = val.substring(start, c < 0 ? val.length() : c);
        if (cur.ex.size() < 64) cur.ex.push_back(icsTime(params, one, ad));
        if (c < 0) break;
        start = c + 1;
      }
    }
  };

  while (lr.next(line)) {
    if (line.length() && (line[0] == ' ' || line[0] == '\t')) { pending += line.substring(1); continue; }
    if (pending.length()) process(pending);
    pending = line;
    if (millis() - t0 > 45000) break;
  }
  if (pending.length()) process(pending);
  http.end();
  for (auto& m : masters) expandRule(m, w0, w1, ovr, out);
  return true;
}

static bool fetchCalendars() {
  time_t now = nowT();
  time_t w0 = dayStart(now), w1 = dayStart(now, 8);
  std::vector<CalOut> out;
  bool any = false, okAll = true;
  for (int i = 0; i < 3; i++) {
    String url;
    char key[6];
    snprintf(key, sizeof(key), "ics%d", i);
    if (!secretGet(key, url)) continue;
    any = true;
    if (!fetchIcs(url, w0, w1, out)) okAll = false;
  }
  if (!any) return false;
  std::sort(out.begin(), out.end(), [](const CalOut& a, const CalOut& b) { return a.s < b.s; });
  JsonDocument d(&g_psAlloc);
  JsonArray ev = d["ev"].to<JsonArray>();
  int n = 0;
  for (auto& e : out) {
    if (n++ >= 80) break;
    JsonObject o = ev.add<JsonObject>();
    o["s"] = (uint32_t)e.s;
    o["e"] = (uint32_t)e.e;
    o["a"] = e.ad ? 1 : 0;
    o["t"] = e.t;
    if (e.l.length()) o["l"] = e.l;
  }
  if (!okAll && n == 0) return false;
  calLoaded = false;
  return writeJson("/cache/cal.json", d);
}

// ---------------------------------------------------------------------------
// Stocks (Twelve Data, free key) and crypto (CoinGecko, no key)
static void rangeParams(const String& range, const char*& interval, int& n) {
  if (range == "1D") { interval = "5min"; n = 78; }
  else if (range == "5D") { interval = "30min"; n = 65; }
  else if (range == "6M") { interval = "1day"; n = 126; }
  else { interval = "1day"; n = 22; }
}
static bool fetchStock(const String& sym, const String& range) {
  String key;
  if (!secretGet("stk_key", key)) return false;
  const char* interval;
  int n;
  rangeParams(range, interval, n);
  String url = "https://api.twelvedata.com/time_series?symbol=" + urlEncode(sym) + "&interval=" + interval +
               "&outputsize=" + String(n) + "&apikey=" + urlEncode(key);
  String body;
  if (httpGet(url, body, 60000) != 200) return false;
  JsonDocument d(&g_psAlloc);
  if (deserializeJson(d, body)) return false;
  if (strcmp(d["status"] | "", "ok") != 0) { LOGF("[stk] %s: %s\n", sym.c_str(), (const char*)(d["message"] | "")); return false; }
  JsonArrayConst vals = d["values"];
  if (!vals.size()) return false;
  JsonDocument o(&g_psAlloc);
  o["sym"] = sym;
  o["range"] = range;
  JsonArray c = o["c"].to<JsonArray>();
  for (int i = (int)vals.size() - 1; i >= 0; i--) c.add(atof(vals[i]["close"] | "0"));
  o["last"] = atof(vals[0]["close"] | "0");
  if (vals.size() > 1) o["prev"] = atof(vals[1]["close"] | "0");
  return writeJson(cachePath("stk:" + sym + ":" + range), o);
}
static bool fetchCrypto(const String& coins, const String& cur) {
  String url = "https://api.coingecko.com/api/v3/simple/price?ids=" + urlEncode(coins) + "&vs_currencies=" + urlEncode(cur) +
               "&include_24hr_change=true";
  String body;
  if (httpGet(url, body, 16000) != 200) return false;
  JsonDocument d(&g_psAlloc);
  if (deserializeJson(d, body) || !d.is<JsonObject>()) return false;
  return writeFile(cachePath("cg:" + coins + ":" + cur), body);
}

// ---------------------------------------------------------------------------
// Generic JSON value (widget-specific URL)
static JsonVariantConst jsonPath(JsonVariantConst v, const String& path) {
  int start = 0;
  while (start < (int)path.length() && !v.isNull()) {
    int dot = path.indexOf('.', start);
    String part = path.substring(start, dot < 0 ? path.length() : dot);
    if (v.is<JsonArrayConst>() && part.length() && isdigit((uint8_t)part[0])) v = v[part.toInt()];
    else v = v[part];
    if (dot < 0) break;
    start = dot + 1;
  }
  return v;
}
static bool fetchJsonValue(const String& wid, JsonObjectConst s) {
  String url = jstr(s["url"]);
  if (!url.length()) return false;
  Headers h;
  String hdr = jstr(s["header"]);
  int c = hdr.indexOf(':');
  if (c > 0) { String k = hdr.substring(0, c), v = hdr.substring(c + 1); k.trim(); v.trim(); h.push_back({k, v}); }
  String body;
  if (httpGet(url, body, 60000, &h) != 200) return false;
  JsonDocument d(&g_psAlloc);
  if (deserializeJson(d, body)) return false;
  JsonVariantConst v = jsonPath(d.as<JsonVariantConst>(), jstr(s["path"]));
  JsonDocument o(&g_psAlloc);
  if (v.is<float>()) o["n"] = v.as<float>();
  else if (v.is<const char*>()) o["v"] = asciiFold(v.as<const char*>()).substring(0, 200);
  else if (v.is<bool>()) o["v"] = v.as<bool>() ? "yes" : "no";
  else o["v"] = "--";
  return writeJson(cachePath("json:" + wid), o);
}

// ---------------------------------------------------------------------------
// Text summaries (for AI prompts and template text)
static String wxBrief() {
  if (!wxOk()) return "unknown";
  JsonObjectConst c = wx()["current"];
  JsonObjectConst d = wx()["daily"];
  char b[220];
  const char* u = unitsF() ? "F" : "C";
  snprintf(b, sizeof(b), "now %d%s %s (feels %d%s), today %d-%d%s, rain chance %d%%, max wind %d, UV %.0f",
           (int)lroundf(c["temperature_2m"] | 0.0f), u, weatherText(c["weather_code"] | 0),
           (int)lroundf(c["apparent_temperature"] | 0.0f), u, (int)lroundf(d["temperature_2m_min"][0] | 0.0f),
           (int)lroundf(d["temperature_2m_max"][0] | 0.0f), u, (int)(d["precipitation_probability_max"][0] | 0),
           (int)lroundf(d["wind_speed_10m_max"][0] | 0.0f), (double)(d["uv_index_max"][0] | 0.0f));
  return String(b);
}
static String agendaBrief(time_t from, time_t to, int maxN) {
  String o;
  int n = 0;
  for (JsonObjectConst e : calData()["ev"].as<JsonArrayConst>()) {
    time_t s = e["s"] | 0u, en = e["e"] | 0u;
    if (en <= from || s >= to) continue;
    if (n++ >= maxN) break;
    if (o.length()) o += "; ";
    o += (e["a"] | 0) ? String("all day") : fmtTime(s);
    o += " ";
    o += e["t"] | "";
  }
  return o.length() ? o : String("no events");
}
static String headlinesBrief(int feed, int n) {
  JsonDocument d(&g_psAlloc);
  String o;
  if (!readJson(cachePath("news:" + String(feed)), d)) return o;
  int i = 0;
  for (JsonVariantConst v : d["items"].as<JsonArrayConst>()) {
    if (i++ >= n) break;
    o += "- ";
    o += v.as<const char*>();
    o += "\n";
  }
  return o;
}
