// Moments firmware - widget catalog. Each widget draws itself into a pixel rect on the canvas.
// To add a widget: write a draw function, add a row to WIDGETS[] (settings schema is shown in the console),
// and declare its data needs in widgetNeeds().
#pragma once
#include "notify.h"

struct WCtx {
  int x, y, w, h;   // inner rect (padding applied)
  JsonObjectConst s; // settings
  String id;         // widget instance id
};
typedef void (*WDraw)(WCtx&);

RTC_DATA_ATTR uint8_t gOutfitIdx = 0;

static int ttlScale() {
  if (C.usb) return 1;
  const char* p = cfg["sys"]["profile"] | "balanced";
  return !strcmp(p, "eco") ? 4 : (!strcmp(p, "live") ? 1 : 2);
}

static int wHeader(WCtx& c, const String& title, const String& ageKey = String(), uint32_t ttl = 0) {
  String t = title;
  if (ageKey.length() && ttl) {
    uint32_t ts = tsGet(ageKey);
    if (ts) {
      long age = (long)(time(nullptr) - ts);
      if (age > (long)ttl * ttlScale() * 2) t += "  (" + fmtAgo(age) + " old)";
    }
  }
  drawText(c.x, c.y, fitText(t, B9, c.w), B9);
  return fontLine(B9) - 2;
}
static void noData(WCtx& c, int y0, const char* msg) { drawWrapped(c.x, c.y + y0 + 4, c.w, 3, msg, F9); }

static void hatch(int x, int y, int w, int h, uint16_t col) {
  for (int yy = y; yy < y + h; yy++)
    for (int xx = x + (yy & 1); xx < x + w; xx += 2) cv->drawPixel(xx, yy, col);
}
static void polyline(int x, int y, int w, int h, const std::vector<float>& v, int thick) {
  if (v.size() < 2) return;
  float mn = *std::min_element(v.begin(), v.end()), mx = *std::max_element(v.begin(), v.end());
  if (mx - mn < 1e-6f) { mx += 1; mn -= 1; }
  int px = 0, py = 0;
  for (size_t i = 0; i < v.size(); i++) {
    int xx = x + (int)((w - 1) * i / (float)(v.size() - 1));
    int yy = y + (int)((h - 1) * (mx - v[i]) / (mx - mn));
    if (i) thickLine(px, py, xx, yy, thick, FG);
    px = xx;
    py = yy;
  }
}
static std::vector<float> toVec(JsonArrayConst a) {
  std::vector<float> v;
  for (JsonVariantConst x : a) v.push_back(x.as<float>());
  return v;
}
static uint8_t fitFont(const String& s, int w, int h, uint8_t& size, bool allowScale) {
  const uint8_t cand[][2] = {{B24, 4}, {B24, 3}, {B24, 2}, {B24, 1}, {B18, 1}, {B12, 1}, {F12, 1}, {F9, 1}};
  for (auto& cd : cand) {
    if (!allowScale && cd[1] > 1) continue;
    if (textW(s, cd[0], cd[1]) <= w && fontAsc(cd[0], cd[1]) <= h) { size = cd[1]; return cd[0]; }
  }
  size = 1;
  return F9;
}

// ---------------------------------------------------------------------------
// Time
static void w_clock(WCtx& c) {
  time_t now = nowT();
  struct tm l = localTm(now);
  bool h24 = use24h();
  const char* hs = c.s["h24"] | "auto";
  if (!strcmp(hs, "12")) h24 = false;
  else if (!strcmp(hs, "24")) h24 = true;
  char t[8];
  int hh = h24 ? l.tm_hour : ((l.tm_hour % 12) ? l.tm_hour % 12 : 12);
  snprintf(t, sizeof(t), h24 ? "%02d:%02d" : "%d:%02d", hh, l.tm_min);
  bool date = c.s["date"] | false;
  bool big = strcmp(c.s["style"] | "big", "compact") != 0;
  int dateH = date ? fontLine(F12) + 6 : 0;
  String ampm = h24 ? String() : String(l.tm_hour < 12 ? "am" : "pm");
  int aw = ampm.length() ? textW(ampm, B12) + 8 : 0;
  uint8_t size;
  uint8_t font = fitFont(t, c.w - aw, c.h - dateH - 6, size, big);
  int asc = fontAsc(font, size);
  int y = c.y + (c.h - asc - dateH) / 2;
  int tw = textW(t, font, size);
  int x = c.x + (c.w - tw - aw) / 2;
  drawText(x, y, t, font, size);
  if (aw) drawText(x + tw + 8, y + asc - fontAsc(B12), ampm, B12);
  if (date) drawTextA(c.x, y + asc + 10, c.w, fitText(fmtDate(now, 0), F12, c.w), F12, 1);
}

static void w_date(WCtx& c) {
  const char* st = c.s["format"] | "long";
  String d = fmtDate(nowT(), !strcmp(st, "short") ? 1 : (!strcmp(st, "numeric") ? 2 : 0));
  uint8_t f = B18;
  if (textW(d, f) > c.w || fontAsc(B18) > c.h - 4) f = B12;
  if (textW(d, f) > c.w) f = F12;
  drawTextA(c.x, c.y + (c.h - fontAsc(f)) / 2, c.w, fitText(d, f, c.w), f, 1);
}

static void w_greeting(WCtx& c) {
  time_t now = nowT();
  int h = localTm(now).tm_hour;
  const char* g = (h >= 5 && h < 12) ? "Good morning" : (h >= 12 && h < 17) ? "Good afternoon" : (h >= 17 && h < 22) ? "Good evening" : "Good night";
  String name = c.s["name"] | "";
  if (!name.length()) name = cfg["profile"]["name"] | "";
  String txt = g;
  if (name.length()) txt += ", " + name;
  bool date = c.s["date"] | true;
  uint8_t f = textW(txt, B18) <= c.w ? B18 : B12;
  int lh = fontAsc(f) + (date ? fontLine(F12) + 4 : 0);
  int y = c.y + (c.h - lh) / 2;
  drawText(c.x, y, fitText(txt, f, c.w), f);
  if (date) drawText(c.x, y + fontAsc(f) + 8, fmtDate(now, 0), F12);
}

static void w_countdown(WCtx& c) {
  time_t now = nowT(), target = 0;
  String label = c.s["label"] | "";
  if (c.s["event"] | false) {
    for (JsonObjectConst e : calData()["ev"].as<JsonArrayConst>()) {
      time_t s = e["s"] | 0u;
      if (s > now) {
        target = s;
        if (!label.length()) label = e["t"] | "";
        break;
      }
    }
  } else {
    int y = 0, mo = 0, d = 0, hh = 0, mi = 0;
    const char* ts = c.s["target"] | "";
    if (sscanf(ts, "%d-%d-%d %d:%d", &y, &mo, &d, &hh, &mi) >= 3 && y > 2000) target = localMake(y, mo, d, hh, mi, 0);
  }
  int hy = wHeader(c, label.length() ? label : String("Countdown"));
  if (!target) { noData(c, hy, "Set a target date (YYYY-MM-DD HH:MM) or use the next calendar event."); return; }
  long d = (long)(target - now);
  String big, small;
  if (d <= 0) big = "Now!";
  else if (d >= 2 * 86400) { big = String(d / 86400); small = "days"; }
  else if (d >= 3600) big = String(d / 3600) + "h " + String((d % 3600) / 60) + "m";
  else { big = String((d + 59) / 60); small = "min"; }
  int sw = small.length() ? textW(small, B12) + 10 : 0;
  int avH = c.h - hy - fontLine(F9) - 6;
  uint8_t size;
  uint8_t f = fitFont(big, c.w - sw, avH, size, true);
  int asc = fontAsc(f, size), tw = textW(big, f, size);
  int x = c.x + (c.w - tw - sw) / 2, y = c.y + hy + (avH - asc) / 2 + 2;
  drawText(x, y, big, f, size);
  if (sw) drawText(x + tw + 10, y + asc - fontAsc(B12), small, B12);
  drawTextA(c.x, c.y + c.h - fontLine(F9) + 2, c.w, fmtDate(target, 1) + "  " + fmtTime(target), F9, 1);
}

struct CityTz { const char* name; const char* tz; };
static const CityTz CITY_TZ[] = {
    {"London", "GMT0BST,M3.5.0/1,M10.5.0"},     {"Dublin", "IST-1GMT0,M10.5.0,M3.5.0/1"},
    {"Lisbon", "WET0WEST,M3.5.0/1,M10.5.0"},    {"Paris", "CET-1CEST,M3.5.0,M10.5.0/3"},
    {"Berlin", "CET-1CEST,M3.5.0,M10.5.0/3"},   {"Madrid", "CET-1CEST,M3.5.0,M10.5.0/3"},
    {"Rome", "CET-1CEST,M3.5.0,M10.5.0/3"},     {"Amsterdam", "CET-1CEST,M3.5.0,M10.5.0/3"},
    {"Stockholm", "CET-1CEST,M3.5.0,M10.5.0/3"}, {"Athens", "EET-2EEST,M3.5.0/3,M10.5.0/4"},
    {"Helsinki", "EET-2EEST,M3.5.0/3,M10.5.0/4"}, {"Istanbul", "<+03>-3"},
    {"Moscow", "MSK-3"},                        {"Dubai", "<+04>-4"},
    {"Karachi", "PKT-5"},                       {"Mumbai", "IST-5:30"},
    {"Delhi", "IST-5:30"},                      {"Bangkok", "<+07>-7"},
    {"Singapore", "<+08>-8"},                   {"Hong Kong", "HKT-8"},
    {"Beijing", "CST-8"},                       {"Shanghai", "CST-8"},
    {"Tokyo", "JST-9"},                         {"Seoul", "KST-9"},
    {"Sydney", "AEST-10AEDT,M10.1.0,M4.1.0/3"}, {"Melbourne", "AEST-10AEDT,M10.1.0,M4.1.0/3"},
    {"Brisbane", "AEST-10"},                    {"Perth", "AWST-8"},
    {"Auckland", "NZST-12NZDT,M9.5.0,M4.1.0/3"}, {"Honolulu", "HST10"},
    {"Anchorage", "AKST9AKDT,M3.2.0,M11.1.0"},  {"Los Angeles", "PST8PDT,M3.2.0,M11.1.0"},
    {"San Francisco", "PST8PDT,M3.2.0,M11.1.0"}, {"Seattle", "PST8PDT,M3.2.0,M11.1.0"},
    {"Denver", "MST7MDT,M3.2.0,M11.1.0"},       {"Phoenix", "MST7"},
    {"Chicago", "CST6CDT,M3.2.0,M11.1.0"},      {"Mexico City", "CST6"},
    {"New York", "EST5EDT,M3.2.0,M11.1.0"},     {"Toronto", "EST5EDT,M3.2.0,M11.1.0"},
    {"Sao Paulo", "<-03>3"},                    {"Buenos Aires", "<-03>3"},
    {"Johannesburg", "SAST-2"},                 {"Cairo", "EET-2EEST,M4.5.5/0,M10.5.4/24"},
    {"Lagos", "WAT-1"},                         {"Nairobi", "EAT-3"},
    {"UTC", "UTC0"},
};
static const char* cityTz(const String& name) {
  for (auto& ct : CITY_TZ)
    if (name.equalsIgnoreCase(ct.name)) return ct.tz;
  return nullptr;
}
static struct tm tmInTz(const char* tz, time_t t) {
  const char* cur = getenv("TZ");
  String old = cur ? cur : "UTC0";
  setenv("TZ", tz, 1);
  tzset();
  struct tm r;
  localtime_r(&t, &r);
  setenv("TZ", old.c_str(), 1);
  tzset();
  return r;
}
static void w_worldclock(WCtx& c) {
  String zones = c.s["zones"] | "London,New York,Tokyo";
  int hy = wHeader(c, "World clock");
  std::vector<std::pair<String, String>> list;
  int start = 0;
  while (start < (int)zones.length() && list.size() < 4) {
    int cm = zones.indexOf(',', start);
    String e = zones.substring(start, cm < 0 ? zones.length() : cm);
    e.trim();
    int eq = e.indexOf('=');
    String name = eq > 0 ? e.substring(0, eq) : e;
    String tz = eq > 0 ? e.substring(eq + 1) : String(cityTz(e) ? cityTz(e) : "");
    if (name.length() && tz.length()) list.push_back({name, tz});
    if (cm < 0) break;
    start = cm + 1;
  }
  if (list.empty()) { noData(c, hy, "Add cities, e.g. London,Tokyo or Name=POSIX-TZ"); return; }
  time_t now = nowT();
  struct tm here = localTm(now);
  int rowH = (c.h - hy) / (int)list.size();
  for (size_t i = 0; i < list.size(); i++) {
    struct tm t = tmInTz(list[i].second.c_str(), now);
    int y = c.y + hy + (int)i * rowH;
    String tstr = fmtHM(t.tm_hour, t.tm_min, use24h());
    uint8_t f = rowH >= 50 ? B18 : B12;
    int ty = y + (rowH - fontAsc(f)) / 2;
    drawText(c.x, ty + fontAsc(f) - fontAsc(B12), fitText(list[i].first, B12, c.w / 2), B12);
    int dd = (t.tm_year * 400 + t.tm_yday) - (here.tm_year * 400 + here.tm_yday);
    if (dd > 0) tstr += " +1";
    else if (dd < 0) tstr += " -1";
    drawTextA(c.x, ty, c.w, tstr, f, 2);
  }
}

// ---------------------------------------------------------------------------
// Weather
static void w_weather_now(WCtx& c) {
  if (!hasLocation()) { noData(c, 0, "Set your location in Connections."); return; }
  if (!wxOk()) { noData(c, 0, "Waiting for weather data..."); return; }
  JsonObjectConst cur = wx()["current"];
  JsonObjectConst d = wx()["daily"];
  float t = cur["temperature_2m"] | 0.0f;
  int code = cur["weather_code"] | 0;
  bool day = (cur["is_day"] | 1) != 0;
  float fl = cur["apparent_temperature"] | t, hi = d["temperature_2m_max"][0] | t, lo = d["temperature_2m_min"][0] | t;
  float wind = cur["wind_speed_10m"] | 0.0f;
  int rain = d["precipitation_probability_max"][0] | 0;
  String ts = tempStr(t);
  char l2[40], l3[40], l4[32];
  snprintf(l2, sizeof(l2), "H %d%c  L %d%c", (int)lroundf(hi), 0x7F, (int)lroundf(lo), 0x7F);
  snprintf(l3, sizeof(l3), "Feels %d%c  Rain %d%%", (int)lroundf(fl), 0x7F, rain);
  snprintf(l4, sizeof(l4), "Wind %d %s", (int)lroundf(wind), unitsF() ? "mph" : "km/h");
  const char* lines[3] = {l2, l3, l4};
  if (c.w > c.h * 13 / 10) {
    int r = min(c.h, c.w / 3) * 42 / 100;
    weatherIcon(c.x + r + 4, c.y + c.h / 2, r, code, day);
    int tx = c.x + 2 * r + 18, tw = c.x + c.w - tx;
    uint8_t sz = 2;
    if (fontAsc(B24, 2) > c.h / 2 || textW(ts, B24, 2) > tw) sz = 1;
    int y = c.y + 2;
    drawText(tx, y, ts, B24, sz);
    y += fontAsc(B24, sz) + 10;
    drawText(tx, y, fitText(weatherText(code), B12, tw), B12);
    y += fontLine(B12);
    for (int i = 0; i < 3 && y + fontAsc(F12) <= c.y + c.h; i++) {
      drawText(tx, y, fitText(lines[i], F12, tw), F12);
      y += fontLine(F12) - 2;
    }
  } else {
    int r = min(c.w / 2, c.h / 3) * 75 / 100;
    weatherIcon(c.x + c.w / 2, c.y + r + 4, r, code, day);
    int y = c.y + 2 * r + 16;
    uint8_t sz = (fontAsc(B24, 2) + y - c.y + 80 <= c.h) ? 2 : 1;
    drawTextA(c.x, y, c.w, ts, B24, 1, sz);
    y += fontAsc(B24, sz) + 12;
    drawTextA(c.x, y, c.w, fitText(weatherText(code), B12, c.w), B12, 1);
    y += fontLine(B12);
    for (int i = 0; i < 3 && y + fontAsc(F12) <= c.y + c.h; i++) {
      drawTextA(c.x, y, c.w, lines[i], F12, 1);
      y += fontLine(F12) - 2;
    }
  }
}

static void w_hourly(WCtx& c) {
  if (!wxOk()) { noData(c, 0, hasLocation() ? "Waiting for weather data..." : "Set your location in Connections."); return; }
  JsonObjectConst h = wx()["hourly"];
  JsonArrayConst tt = h["time"], tp = h["temperature_2m"], pp = h["precipitation_probability"];
  int hours = constrain((int)(c.s["hours"] | 12), 4, 24);
  String show = c.s["show"] | "both";
  time_t now = nowT();
  int start = 0, total = (int)tt.size();
  while (start < total && (time_t)(tt[start] | 0u) + 3600 <= now) start++;
  int n = min(hours, total - start);
  int hy = wHeader(c, show == "rain" ? String("Rain next ") + n + "h" : String("Next ") + n + " hours", "wx", 1800);
  if (n < 2) { noData(c, hy, "No hourly data yet."); return; }
  bool showT = show != "rain", showR = show != "temp";
  int top = c.y + hy + 4, bottom = c.y + c.h - fontLine(F9) + 2;
  int chartH = bottom - top - 2;
  float colW = c.w / (float)n;
  if (showR) {
    int rh = showT ? chartH * 40 / 100 : chartH - 18;
    for (int i = 0; i < n; i++) {
      int p = pp[start + i] | 0;
      int bh = rh * p / 100;
      int bx = c.x + (int)(i * colW) + 2, bw = max(2, (int)colW - 4);
      if (bh > 0) { hatch(bx, bottom - bh, bw, bh, FG); cv->drawFastHLine(bx, bottom - bh, bw, FG); }
      if (!showT && p >= 20 && colW >= 26) drawTextA(bx - 4, bottom - bh - fontLine(F9) + 4, bw + 8, String(p), F9, 1);
    }
    cv->drawFastHLine(c.x, bottom, c.w, FG);
  }
  if (showT) {
    float mn = 1e9, mx = -1e9;
    for (int i = 0; i < n; i++) { float v = tp[start + i] | 0.0f; mn = min(mn, v); mx = max(mx, v); }
    if (mx - mn < 2) { mx += 1; mn -= 1; }
    int lineTop = top + fontLine(F9), lineH = (showR ? chartH * 55 / 100 : chartH) - fontLine(F9) - 6;
    int px = 0, py = 0;
    for (int i = 0; i < n; i++) {
      float v = tp[start + i] | 0.0f;
      int xx = c.x + (int)(colW * (i + 0.5f));
      int yy = lineTop + (int)(lineH * (mx - v) / (mx - mn));
      if (i) thickLine(px, py, xx, yy, 3, FG);
      bool label = (n <= 8) || (i % 3 == 0) || v == mx || v == mn;
      if (label && colW * (n <= 8 ? 1 : 3) >= 30) drawTextA(xx - 20, yy - fontLine(F9) - 2 + 6, 40, String((int)lroundf(v)), F9, 1);
      px = xx;
      py = yy;
    }
  }
  int step = n <= 8 ? 1 : (n <= 12 ? 2 : 3);
  for (int i = 0; i < n; i += step) {
    struct tm l = localTm((time_t)(tt[start + i] | 0u));
    String lb = String(use24h() ? l.tm_hour : ((l.tm_hour % 12) ? l.tm_hour % 12 : 12));
    if (!use24h()) lb += l.tm_hour < 12 ? "a" : "p";
    drawTextA(c.x + (int)(colW * i) - 10, c.y + c.h - fontLine(F9) + 6, (int)colW + 20, lb, F9, 1);
  }
}

static void w_daily(WCtx& c) {
  if (!wxOk()) { noData(c, 0, hasLocation() ? "Waiting for weather data..." : "Set your location in Connections."); return; }
  JsonObjectConst d = wx()["daily"];
  int days = constrain((int)(c.s["days"] | 5), 1, 7);
  int off = (c.s["tomorrow"] | false) ? 1 : 0;
  days = min(days, (int)d["time"].size() - off);
  if (days <= 0) { noData(c, 0, "No forecast."); return; }
  bool columns = c.h >= 140 && c.w / days >= 60 && days > 1;
  if (columns) {
    int colW = c.w / days;
    for (int i = 0; i < days; i++) {
      int k = i + off, x = c.x + i * colW;
      time_t t = (time_t)(d["time"][k] | 0u);
      String lb = (k == 0) ? String("Today") : String(DOW_S[localTm(t + 43200).tm_wday]);
      drawTextA(x, c.y, colW, lb, B12, 1);
      int r = min(colW / 3, (c.h - 110) / 2);
      weatherIcon(x + colW / 2, c.y + 34 + r, r, d["weather_code"][k] | 0, true);
      int y = c.y + 40 + 2 * r + 4;
      drawTextA(x, y, colW, tempStr(d["temperature_2m_max"][k] | 0.0f), B12, 1);
      y += fontLine(B12) - 4;
      drawTextA(x, y, colW, tempStr(d["temperature_2m_min"][k] | 0.0f), F12, 1);
      y += fontLine(F12) - 4;
      if (y + fontAsc(F9) <= c.y + c.h) drawTextA(x, y, colW, String((int)(d["precipitation_probability_max"][k] | 0)) + "%", F9, 1);
    }
  } else {
    int rowH = max(36, c.h / days);
    for (int i = 0; i < days && (i + 1) * rowH <= c.h + 4; i++) {
      int k = i + off, y = c.y + i * rowH;
      time_t t = (time_t)(d["time"][k] | 0u);
      String lb = k == 0 ? String("Today") : (k == 1 ? String("Tomorrow") : String(DOW_L[localTm(t + 43200).tm_wday]));
      int ty = y + (rowH - fontAsc(B12)) / 2;
      drawText(c.x, ty, lb, B12);
      int r = min(rowH / 2 - 3, 26);
      int ix = c.x + c.w * 40 / 100;
      weatherIcon(ix, y + rowH / 2, r, d["weather_code"][k] | 0, true);
      char b[32];
      snprintf(b, sizeof(b), "%d%c / %d%c", (int)lroundf(d["temperature_2m_max"][k] | 0.0f), 0x7F,
               (int)lroundf(d["temperature_2m_min"][k] | 0.0f), 0x7F);
      drawText(ix + r + 14, ty, b, B12);
      String rp = "rain " + String((int)(d["precipitation_probability_max"][k] | 0)) + "%";
      drawTextA(c.x, ty, c.w, rp, F12, 2);
    }
  }
}

static void w_sunmoon(WCtx& c) {
  time_t now = nowT(), r, s;
  int hy = wHeader(c, "Sun & moon");
  if (!sunTimes(now, r, s)) { noData(c, hy, "Set your location in Connections."); return; }
  int y = c.y + hy + 6;
  drawText(c.x, y, "Rise " + fmtTime(r), B12);
  y += fontLine(B12);
  drawText(c.x, y, "Set  " + fmtTime(s), B12);
  y += fontLine(B12);
  long len = (long)(s - r);
  if (y + fontAsc(F12) <= c.y + c.h) drawText(c.x, y, "Day " + fmtDur(len), F12);
  double p = moonPhase(now);
  int mr = min(c.h - hy - 10, c.w / 3) / 2 - 2;
  if (mr >= 10) {
    int mx = c.x + c.w - mr - 4, my = c.y + hy + 4 + mr;
    for (int dy = -mr; dy <= mr; dy++) {
      float hw = sqrtf((float)(mr * mr - dy * dy));
      float a = hw * cosf(2 * PI * p);
      float x0, x1;
      if (p < 0.5f) { x0 = a; x1 = hw; } else { x0 = -hw; x1 = -a; }
      if (x1 > x0) cv->drawFastHLine(mx + (int)x0, my + dy, (int)(x1 - x0) + 1, FG);
    }
    cv->drawCircle(mx, my, mr, FG);
    if (my + mr + fontLine(F9) <= c.y + c.h + 6) drawTextA(mx - 70, my + mr + 2, 140, moonName(p), F9, 1);
  }
}

static const char* aqiLabel(int a) {
  return a <= 50 ? "Good" : a <= 100 ? "Moderate" : a <= 150 ? "Unhealthy (sensitive)" : a <= 200 ? "Unhealthy" : a <= 300 ? "Very unhealthy" : "Hazardous";
}
static const char* uvLabel(float u) { return u < 3 ? "Low" : u < 6 ? "Moderate" : u < 8 ? "High" : u < 11 ? "Very high" : "Extreme"; }
static void w_air(WCtx& c) {
  JsonDocument a(&g_psAlloc);
  int hy = wHeader(c, "Air & UV", "air", 3600);
  if (!readJson("/cache/air.json", a)) { noData(c, hy, hasLocation() ? "Waiting for air data..." : "Set your location in Connections."); return; }
  int aqi = a["current"]["us_aqi"] | -1;
  float uv = a["current"]["uv_index"] | -1.0f;
  String show = c.s["show"] | "both";
  int y = c.y + hy + 4;
  int half = (show == "both") ? c.w / 2 : c.w;
  int x = c.x;
  if (show != "uv" && aqi >= 0) {
    drawText(x, y, "AQI " + String(aqi), B18);
    drawText(x, y + fontLine(B18), fitText(aqiLabel(aqi), F12, half - 8), F12);
    x += half;
  }
  if (show != "aqi" && uv >= 0) {
    char b[16];
    snprintf(b, sizeof(b), "UV %.0f", uv);
    drawText(x, y, b, B18);
    drawText(x, y + fontLine(B18), uvLabel(uv), F12);
  }
}

// ---------------------------------------------------------------------------
// Calendar
static bool calConfigured() { return secretHas("ics0") || secretHas("ics1") || secretHas("ics2"); }
static void w_agenda(WCtx& c) {
  String range = c.s["range"] | "today";
  int days = constrain((int)(c.s["days"] | 3), 1, 7), maxN = constrain((int)(c.s["max"] | 5), 1, 12);
  time_t now = nowT(), from = now, to;
  String title;
  if (range == "tomorrow") { from = dayStart(now, 1); to = dayStart(now, 2); title = "Tomorrow"; }
  else if (range == "days") { to = dayStart(now, days); title = "Next " + String(days) + " days"; }
  else { to = dayStart(now, 1); title = "Today"; }
  int y = c.y + wHeader(c, title, "cal", 1800) + 4;
  if (!calConfigured()) { noData(c, y - c.y - 4, "Add a calendar (secret ICS link) in Connections."); return; }
  bool multi = range == "days";
  int lineH = fontLine(F12), count = 0, lastDay = -1;
  int tcol = use24h() ? 70 : 92;
  for (JsonObjectConst e : calData()["ev"].as<JsonArrayConst>()) {
    time_t s = e["s"] | 0u, en = e["e"] | 0u;
    if (en <= from || s >= to) continue;
    if (count >= maxN) break;
    struct tm sl = localTm(max(s, from));
    if (multi && sl.tm_yday != lastDay) {
      if (y + fontLine(B9) + lineH > c.y + c.h) break;
      lastDay = sl.tm_yday;
      String dl = (dayStart(max(s, from)) == dayStart(now)) ? String("Today") : fmtDate(max(s, from), 1);
      drawText(c.x, y + 2, dl, B9);
      y += fontLine(B9);
    }
    if (y + fontAsc(F12) > c.y + c.h) break;
    bool ad = (e["a"] | 0) != 0;
    String tl = ad ? String("All day") : (s < now && en > now ? String("Now") : fmtTime(s));
    drawText(c.x, y, tl, B12);
    drawText(c.x + tcol, y, fitText(e["t"] | "", F12, c.w - tcol), F12);
    y += lineH;
    count++;
  }
  if (!count) drawText(c.x, y, range == "tomorrow" ? "Nothing scheduled tomorrow" : "Nothing scheduled", F12);
}

static void w_next_event(WCtx& c) {
  int hy = wHeader(c, "Next up", "cal", 1800);
  if (!calConfigured()) { noData(c, hy, "Add a calendar in Connections."); return; }
  time_t now = nowT();
  JsonObjectConst best, allday;
  for (JsonObjectConst e : calData()["ev"].as<JsonArrayConst>()) {
    time_t en = e["e"] | 0u;
    if (en <= now) continue;
    if (e["a"] | 0) { if (allday.isNull()) allday = e; continue; }
    best = e;
    break;
  }
  if (best.isNull()) best = allday;
  if (best.isNull()) { noData(c, hy, "No upcoming events."); return; }
  time_t s = best["s"] | 0u, en = best["e"] | 0u;
  int y = c.y + hy + 4;
  int avail = c.h - hy - fontLine(F12) - 6;
  uint8_t f = avail >= 2 * fontLine(B18) ? B18 : B12;
  int lines = max(1, min(2, avail / fontLine(f)));
  y += drawWrapped(c.x, y, c.w, lines, best["t"] | "", f);
  String when;
  if (best["a"] | 0) when = "All day " + fmtDate(s, 1);
  else if (s <= now) when = "Now - until " + fmtTime(en);
  else if (s - now < 12 * 3600) when = "in " + fmtDur((long)(s - now)) + "  -  " + fmtTime(s);
  else when = fmtDate(s, 1) + "  " + fmtTime(s);
  const char* loc = best["l"] | "";
  if (*loc) when += "  -  " + String(loc);
  drawText(c.x, y + 2, fitText(when, F12, c.w), F12);
}

// ---------------------------------------------------------------------------
// News
static void w_headlines(WCtx& c) {
  int feed = c.s["feed"] | 0, n = constrain((int)(c.s["count"] | 4), 1, 10);
  JsonDocument d(&g_psAlloc);
  bool ok = readJson(cachePath("news:" + String(feed)), d);
  String name = d["name"] | (cfg["feeds"][feed]["name"] | "News");
  int y = c.y + wHeader(c, name, "news:" + String(feed), 3600) + 4;
  if (!ok) { noData(c, y - c.y - 4, cfg["feeds"][feed].isNull() ? "Add a news feed in Connections." : "Waiting for headlines..."); return; }
  int i = 0;
  for (JsonVariantConst v : d["items"].as<JsonArrayConst>()) {
    if (i++ >= n) break;
    int left = c.y + c.h - y;
    int maxLines = min(2, left / fontLine(F12));
    if (maxLines <= 0) break;
    cv->fillCircle(c.x + 4, y + fontAsc(F12) / 2 + 1, 3, FG);
    y += drawWrapped(c.x + 14, y, c.w - 14, maxLines, v.as<const char*>(), F12) + 4;
  }
}
static void w_news_summary(WCtx& c) {
  int feed = c.s["feed"] | 0;
  JsonDocument d(&g_psAlloc);
  if (!readJson(cachePath("ai_news:" + String(feed)), d) || !(d["text"] | "")[0]) { w_headlines(c); return; }
  String name = cfg["feeds"][feed]["name"] | "News";
  int hy = wHeader(c, name + " - summary", "ai_news:" + String(feed), 6 * 3600);
  int lines = (c.h - hy - 4) / fontLine(F12);
  drawWrapped(c.x, c.y + hy + 4, c.w, max(1, lines), d["text"] | "", F12);
}

// ---------------------------------------------------------------------------
// Markets
static std::vector<String> splitList(const String& s, int maxN) {
  std::vector<String> v;
  int start = 0;
  while (start < (int)s.length() && (int)v.size() < maxN) {
    int cm = s.indexOf(',', start);
    String e = s.substring(start, cm < 0 ? s.length() : cm);
    e.trim();
    if (e.length()) v.push_back(e);
    if (cm < 0) break;
    start = cm + 1;
  }
  return v;
}
static void drawChange(int x, int y, float pct) {
  int s = 7;
  int cy = y + fontAsc(F12) / 2;
  if (pct >= 0) cv->fillTriangle(x, cy + s / 2, x + s, cy + s / 2, x + s / 2, cy - s / 2 - 1, FG);
  else cv->fillTriangle(x, cy - s / 2, x + s, cy - s / 2, x + s / 2, cy + s / 2 + 1, FG);
  char b[16];
  snprintf(b, sizeof(b), "%.2f%%", fabsf(pct));
  drawText(x + s + 4, y, b, F12);
}
static String fmtPrice(float p) {
  char b[20];
  if (p >= 1000) snprintf(b, sizeof(b), "%.0f", p);
  else if (p >= 1) snprintf(b, sizeof(b), "%.2f", p);
  else snprintf(b, sizeof(b), "%.4f", p);
  return String(b);
}
static void w_stocks(WCtx& c) {
  std::vector<String> syms = splitList(c.s["symbols"] | "AAPL,MSFT", 6);
  String ageKey;
  if (syms.size()) ageKey = "stk:" + syms[0] + ":1M";
  int hy = wHeader(c, "Watchlist", ageKey, 900);
  if (!secretHas("stk_key")) { noData(c, hy, "Add a Twelve Data API key in Connections."); return; }
  if (syms.empty()) { noData(c, hy, "Add symbols, e.g. AAPL,MSFT"); return; }
  bool spark = c.s["spark"] | true;
  int n = (int)syms.size();
  int rowH = min(56, (c.h - hy - 2) / n);
  std::vector<bool> ok(n, false);
  std::vector<float> lasts(n, 0), pcts(n, 0);
  std::vector<std::vector<float>> series(n);
  int symW = 0, priceW = textW("--", F12), chgW = 0;
  for (int i = 0; i < n; i++) {
    symW = max(symW, textW(syms[i], B12));
    JsonDocument d(&g_psAlloc);
    if (!readJson(cachePath("stk:" + syms[i] + ":1M"), d)) continue;
    ok[i] = true;
    float last = d["last"] | 0.0f, prev = d["prev"] | last;
    lasts[i] = last;
    pcts[i] = prev ? (last - prev) / prev * 100 : 0;
    if (spark) series[i] = toVec(d["c"]);
    char b[16];
    snprintf(b, sizeof(b), "%.2f%%", fabsf(pcts[i]));
    priceW = max(priceW, textW(fmtPrice(last), F12));
    chgW = max(chgW, 11 + textW(b, F12));
  }
  const int gap = 14;
  int px = c.x + symW + gap;
  int cx = px + priceW + gap;
  int sx = cx + chgW + gap;
  int sw = c.x + c.w - sx;
  if (sw < 48) spark = false;
  if (!spark) cx = max(cx, c.x + c.w - chgW);
  for (int i = 0; i < n; i++) {
    int y = c.y + hy + 2 + i * rowH;
    int ty = y + (rowH - fontAsc(B12)) / 2;
    drawText(c.x, ty, syms[i], B12);
    if (!ok[i]) { drawText(px, ty, "--", F12); continue; }
    String p = fmtPrice(lasts[i]);
    drawText(px + priceW - textW(p, F12), ty, p, F12);
    drawChange(cx, ty, pcts[i]);
    if (spark && series[i].size() >= 2) polyline(sx, y + 6, sw, rowH - 12, series[i], 2);
  }
}
static void w_stock_chart(WCtx& c) {
  String sym = c.s["symbol"] | "AAPL", range = c.s["range"] | "1M";
  int hy = wHeader(c, sym + "  " + range, "stk:" + sym + ":" + range, 900);
  if (!secretHas("stk_key")) { noData(c, hy, "Add a Twelve Data API key in Connections."); return; }
  JsonDocument d(&g_psAlloc);
  if (!readJson(cachePath("stk:" + sym + ":" + range), d)) { noData(c, hy, "Waiting for market data..."); return; }
  std::vector<float> v = toVec(d["c"]);
  if (v.size() < 2) { noData(c, hy, "No data."); return; }
  float first = v.front(), last = v.back();
  float pct = first ? (last - first) / first * 100 : 0;
  int y = c.y + hy + 2;
  int pw = drawText(c.x, y, fmtPrice(last), B18);
  drawChange(c.x + pw + 12, y + fontAsc(B18) - fontAsc(F12), pct);
  int top = y + fontLine(B18) - 4, h = c.y + c.h - top;
  float mn = *std::min_element(v.begin(), v.end()), mx = *std::max_element(v.begin(), v.end());
  int lw = max(textW(fmtPrice(mx), F9), textW(fmtPrice(mn), F9)) + 6;
  polyline(c.x, top + 4, c.w - lw, h - 8, v, 2);
  drawText(c.x + c.w - lw + 6, top, fmtPrice(mx), F9);
  drawText(c.x + c.w - lw + 6, top + h - fontLine(F9), fmtPrice(mn), F9);
  for (int xx = c.x; xx < c.x + c.w - lw; xx += 6) cv->drawPixel(xx, top + h - 4, FG);
}
static void w_crypto(WCtx& c) {
  String coins = c.s["coins"] | "bitcoin,ethereum", curr = c.s["currency"] | "usd";
  int hy = wHeader(c, "Crypto", "cg:" + coins + ":" + curr, 900);
  JsonDocument d(&g_psAlloc);
  if (!readJson(cachePath("cg:" + coins + ":" + curr), d)) { noData(c, hy, "Waiting for prices..."); return; }
  std::vector<String> list = splitList(coins, 6);
  int rowH = min(48, (c.h - hy - 2) / max(1, (int)list.size()));
  for (size_t i = 0; i < list.size(); i++) {
    int y = c.y + hy + 2 + (int)i * rowH + (rowH - fontAsc(B12)) / 2;
    String nm = list[i];
    nm.setCharAt(0, toupper(nm[0]));
    drawText(c.x, y, fitText(nm, B12, c.w * 40 / 100), B12);
    String chKey = curr + "_24h_change";
    float p = d[list[i]][curr] | 0.0f, ch = d[list[i]][chKey] | 0.0f;
    drawText(c.x + c.w * 42 / 100, y, fmtPrice(p), F12);
    drawChange(c.x + c.w * 72 / 100, y, ch);
  }
}

// ---------------------------------------------------------------------------
// AI
static void aiTextBlock(WCtx& c, const String& title, const String& key, const char* path, const String& fallback) {
  JsonDocument d(&g_psAlloc);
  int hy = wHeader(c, title, key, 12 * 3600);
  String text;
  if (readJson(path, d)) text = d["text"] | "";
  if (!text.length()) text = fallback;
  int lines = (c.h - hy - 4) / fontLine(F12);
  uint8_t f = F12;
  if (wrapLines(text, B18, c.w, 99).size() * fontLine(B18) <= (size_t)(c.h - hy - 4)) f = B18;
  else lines = max(1, lines);
  drawWrapped(c.x, c.y + hy + 4, c.w, f == B18 ? 99 : lines, text, f);
}
static void w_ai_text(WCtx& c) {
  String fb = aiConfigured() ? "Generating..." : "Add an AI key in Connections to use this widget.";
  aiTextBlock(c, c.s["title"] | "AI", "ai:" + c.id, cachePath("ai:" + c.id).c_str(), fb);
}
static String templateBriefing() {
  time_t now = nowT();
  String s = "Today is " + fmtDate(now, 0) + ".";
  if (wxOk()) {
    JsonObjectConst cur = wx()["current"];
    JsonObjectConst d = wx()["daily"];
    s += " It's " + tempStr(cur["temperature_2m"] | 0.0f) + " and " + String(weatherText(cur["weather_code"] | 0)) + ", up to " +
         tempStr(d["temperature_2m_max"][0] | 0.0f) + ".";
    int rain = d["precipitation_probability_max"][0] | 0;
    if (rain >= 40) s += " Rain chance " + String(rain) + "%.";
  }
  int n = 0;
  time_t first = 0;
  String ft;
  for (JsonObjectConst e : calData()["ev"].as<JsonArrayConst>()) {
    time_t st = e["s"] | 0u, en = e["e"] | 0u;
    if (en <= now || st >= dayStart(now, 1)) continue;
    if (!n && !(e["a"] | 0)) { first = st; ft = e["t"] | ""; }
    n++;
  }
  if (n) {
    s += " You have " + String(n) + (n == 1 ? " event" : " events") + " today";
    if (first) s += ", first at " + fmtTime(first) + ": " + ft;
    s += ".";
  }
  return s;
}
static void w_briefing(WCtx& c) { aiTextBlock(c, "Daily briefing", "ai_brief", "/cache/ai_brief.json", templateBriefing()); }

static bool drawThumb(const String& id, int x, int y) {
  File f = LittleFS.open("/thumb/" + id + ".bin", "r");
  if (!f) return false;
  uint8_t buf[512];
  bool ok = f.read(buf, 512) == 512;
  f.close();
  if (ok) cv->drawBitmap(x, y, buf, 64, 64, BG, FG);
  return ok;
}
static void w_outfit(WCtx& c) {
  JsonDocument o(&g_psAlloc);
  bool ok = readJson("/cache/outfit.json", o) && dateKey(nowT()) == (o["date"] | "");
  if (!ok) {
    if (!wxOk()) { noData(c, 0, hasLocation() ? "Waiting for the forecast..." : "Set your location in Connections."); return; }
    ruleOutfit(o);
  }
  JsonArrayConst outs = o["outfits"];
  int n = outs.size();
  if (!n) { noData(c, 0, "Add clothes to your Wardrobe in the console."); return; }
  int idx = gOutfitIdx % n;
  JsonObjectConst of = outs[idx];
  String title = "Today's outfit";
  if (n > 1) title += "  (" + String(idx + 1) + "/" + String(n) + ", tap for another)";
  int y = c.y + wHeader(c, title) + 4;
  y += drawWrapped(c.x, y, c.w, 2, of["reason"] | "", B12) + 8;
  JsonArrayConst items = of["items"];
  int cols = c.w >= 420 ? 2 : 1;
  int tileW = c.w / cols, tileH = 74;
  int i = 0;
  for (JsonVariantConst v : items) {
    JsonObjectConst it = wardrobeItem(v | "");
    if (it.isNull()) continue;
    int tx = c.x + (i % cols) * tileW, ty = y + (i / cols) * tileH;
    if (ty + tileH > c.y + c.h + 6) break;
    if (!drawThumb(it["id"] | "", tx, ty + 4)) {
      cv->drawRoundRect(tx, ty + 4, 64, 64, 10, FG);
      String cat = it["cat"] | "?";
      cat.toUpperCase();
      drawTextA(tx, ty + 4 + (64 - fontAsc(B18)) / 2, 64, cat.substring(0, 1), B18, 1);
    }
    drawText(tx + 74, ty + 10, fitText(it["name"] | "", B12, tileW - 80), B12);
    drawText(tx + 74, ty + 10 + fontLine(B12), fitText(String(it["cat"] | "") + "  " + String(it["color"] | ""), F9, tileW - 80), F9);
    i++;
  }
  addHit(c.x, c.y, c.w, c.h, 900);
}

// ---------------------------------------------------------------------------
// Home & device
static String comfortLabel(float t, float h) {
  if (h > 65) return "Humid";
  if (h < 30) return "Dry";
  if (t < 18) return "Cool";
  if (t > 26) return "Warm";
  return "Comfortable";
}
static void w_indoor(WCtx& c) {
  if (!C.sensOk) { noData(c, 0, "Indoor sensor unavailable."); return; }
  float t = unitsF() ? C.temp * 9 / 5 + 32 : C.temp;
  String ts = tempStr(t) + (unitsF() ? "F" : "C");
  String hs = String((int)lroundf(C.hum)) + "%";
  bool comfort = c.s["comfort"] | true;
  if (c.h < 90) {
    int y = c.y + 2;
    int w1 = drawText(c.x, y, ts, B18);
    drawText(c.x + w1 + 16, y, hs, B18);
    if (comfort) drawText(c.x, y + fontLine(B18) - 2, "Indoor - " + comfortLabel(C.temp, C.hum), F9);
  } else {
    int hy = wHeader(c, "Indoor");
    int y = c.y + hy + 6;
    uint8_t sz = fontAsc(B24, 2) + hy + 40 < c.h ? 2 : 1;
    int w1 = drawText(c.x, y, ts, B24, sz);
    drawText(c.x + w1 + 20, y + fontAsc(B24, sz) - fontAsc(B18), hs, B18);
    if (comfort) drawText(c.x, y + fontAsc(B24, sz) + 12, comfortLabel(C.temp, C.hum), B12);
  }
}
static void w_climate_history(WCtx& c) {
  String show = c.s["show"] | "both";
  int hy = wHeader(c, "Indoor - last 24 h");
  std::vector<std::pair<uint32_t, int>> pts;
  uint32_t now = time(nullptr);
  for (int i = 0; i < HIST_SLOTS; i++)
    if (R.histT[i] && now - R.histT[i] < 86400) pts.push_back({R.histT[i], i});
  std::sort(pts.begin(), pts.end());
  if (pts.size() < 2) { noData(c, hy, "Collecting samples (every 15 min)..."); return; }
  std::vector<float> tv, hv;
  for (auto& p : pts) {
    float t = R.histTemp[p.second] / 10.0f;
    tv.push_back(unitsF() ? t * 9 / 5 + 32 : t);
    hv.push_back(R.histHum[p.second]);
  }
  int top = c.y + hy + 4, h = c.h - hy - 4;
  bool both = show == "both";
  if (show != "hum") {
    int ch = both ? h / 2 - 4 : h;
    polyline(c.x + 60, top + 4, c.w - 60, ch - 8, tv, 2);
    drawText(c.x, top + ch / 2 - 8, tempStr(tv.back()), B12);
    top += both ? h / 2 : 0;
  }
  if (show != "temp") {
    int ch = both ? h / 2 - 4 : h;
    polyline(c.x + 60, top + 4, c.w - 60, ch - 8, hv, 2);
    drawText(c.x, top + ch / 2 - 8, String((int)hv.back()) + "%", B12);
  }
}
static void w_status(WCtx& c) {
  int y = c.y + (c.h - 22) / 2;
  int x = c.x;
  iconBattery(x, y, 40, 22, C.soc, C.charging, FG);
  x += 46;
  String socStr = "--";
  if (C.soc >= 0) socStr = String(C.soc) + "%";
  x += drawText(x, y + 3, socStr, B12) + 14;
  bool wifiRecent = gWifiUp || (tsGet("wx") && time(nullptr) - tsGet("wx") < 4 * 3600);
  iconWifi(x, y + 2, 20, wifiRecent, FG);
  x += 30;
  uint32_t wts = tsGet("wx");
  String age = wts ? fmtAgo((long)(time(nullptr) - wts)) : String("--");
  x += drawText(x, y + 3, age == "now" ? String("fresh") : age, F12) + 14;
  if (sysBool("showName", true) && R.moment[0] && x + 40 < c.x + c.w)
    drawTextA(x, y + 3, c.x + c.w - x, fitText(R.moment, F12, c.x + c.w - x), F12, 2);
}

// ---------------------------------------------------------------------------
// Personal
static void w_note(WCtx& c) {
  const char* id = c.s["note"] | "";
  JsonObjectConst n = findById(cfg["content"]["notes"].as<JsonArray>(), id);
  if (n.isNull()) n = cfg["content"]["notes"][0];
  if (n.isNull()) { noData(c, 0, "Add a note in Content."); return; }
  int hy = wHeader(c, n["name"] | "Note");
  int lines = (c.h - hy - 4) / fontLine(F12);
  uint8_t f = F12;
  String text = asciiFold(n["text"] | "");
  if ((int)wrapLines(text, B18, c.w, 99).size() * fontLine(B18) <= c.h - hy - 4) { f = B18; lines = 99; }
  drawWrapped(c.x, c.y + hy + 4, c.w, max(1, lines), text, f);
}
static void w_list_preview(WCtx& c) {
  int idx = c.s["list"] | 0, maxN = c.s["max"] | 6;
  JsonObjectConst l = listsDoc[idx];
  if (l.isNull()) { noData(c, 0, "Create a list in Content."); return; }
  int y = c.y + wHeader(c, asciiFold(l["name"] | "List")) + 4;
  int n = 0, rowH = fontLine(F12) + 2;
  for (JsonObjectConst it : l["items"].as<JsonArrayConst>()) {
    if (n >= maxN || y + rowH > c.y + c.h + 4) break;
    bool done = it["d"] | false;
    checkBox(c.x, y + 2, 20, done, FG, BG);
    int tw = drawText(c.x + 30, y, fitText(asciiFold(it["t"] | ""), F12, c.w - 30), F12);
    if (done) cv->drawFastHLine(c.x + 30, y + fontAsc(F12) / 2, tw, FG);
    y += rowH;
    n++;
  }
  if (!n) drawText(c.x, y, "All done!", F12);
}
static void pickQuote(JsonObjectConst s, String& q, String& a) {
  String src = s["source"] | "both";
  JsonArrayConst mine = cfg["content"]["quotes"];
  bool useB = src != "mine" && (cfg["content"]["builtinQuotes"] | true);
  bool useM = src != "builtin";
  int nb = useB ? QUOTE_COUNT : 0, nm = useM ? (int)mine.size() : 0;
  if (nb + nm == 0) { q = "Add your own quotes in Content."; a = ""; return; }
  uint32_t seed;
  if (!strcmp(s["change"] | "daily", "wake")) seed = R.wakes;
  else seed = (uint32_t)(dayStart(nowT()) / 86400) * 7919u;
  int i = (int)(seed % (uint32_t)(nb + nm));
  if (i < nb) { q = QUOTES[i].q; a = QUOTES[i].a; }
  else {
    String line = mine[i - nb] | "";
    int dash = line.lastIndexOf(" - ");
    if (dash > 0) { q = line.substring(0, dash); a = line.substring(dash + 3); }
    else { q = line; a = ""; }
  }
  q = asciiFold(q.c_str());
  a = asciiFold(a.c_str());
}
static void w_quote(WCtx& c) {
  String q, a;
  pickQuote(c.s, q, a);
  String txt = "\"" + q + "\"";
  int ah = a.length() ? fontLine(F12) + 4 : 0;
  uint8_t fonts[3] = {B18, B12, F12};
  uint8_t f = F12;
  for (uint8_t ff : fonts) {
    std::vector<String> ls = wrapLines(txt, ff, c.w, 99);
    if ((int)ls.size() * fontLine(ff) <= c.h - ah) { f = ff; break; }
  }
  std::vector<String> ls = wrapLines(txt, f, c.w, max(1, (c.h - ah) / fontLine(f)));
  int bh = (int)ls.size() * fontLine(f) + ah;
  int y = c.y + max(0, (c.h - bh) / 2);
  for (auto& l : ls) { drawText(c.x, y, l, f); y += fontLine(f); }
  if (a.length()) drawTextA(c.x, y + 4, c.w, "- " + a, F12, 2);
}
static time_t nextWakeTime(time_t after) {
  for (int d = 0; d < 3; d++) {
    time_t day = dayStart(after, d);
    int wd = weekdayMon0(localTm(day + 43200));
    int w = parseHHMM(cfg["profile"]["wake"][wd] | "07:00", 420);
    time_t t = atMinuteOfDay(day + 43200, w);
    if (t > after) return t;
  }
  return after + 8 * 3600;
}
static void w_sleep_calc(WCtx& c) {
  int fall = constrain((int)(c.s["fall"] | 15), 0, 120);
  time_t now = nowT(), asleep = now + fall * 60;
  time_t wake = nextWakeTime(asleep + 1800);
  long dur = (long)(wake - asleep);
  int hy = wHeader(c, "Sleep");
  int y = c.y + hy + 4;
  String big = fmtDur(dur) + " of sleep";
  uint8_t f = textW(big, B24) <= c.w ? B24 : B18;
  drawText(c.x, y, big, f);
  y += fontLine(f);
  drawText(c.x, y, fitText("if asleep by " + fmtTime(asleep) + ", waking at " + fmtTime(wake), F12, c.w), F12);
  y += fontLine(F12);
  if (y + fontAsc(F12) <= c.y + c.h) {
    String cyc = "Best bedtimes:";
    for (int k = 6; k >= 4; k--) {
      time_t bt = wake - k * 90 * 60 - fall * 60;
      if (bt > now) cyc += " " + fmtTime(bt) + " (" + String(k) + ")";
    }
    if (cyc.length() > 15) drawText(c.x, y, fitText(cyc, F12, c.w), F12);
  }
}
static void w_qr(WCtx& c) {
  String text = c.s["text"] | "";
  String label = c.s["label"] | "";
  if (!text.length()) { noData(c, 0, "Set the QR text or URL."); return; }
  int lh = label.length() ? fontLine(F12) : 0;
  int side = min(c.w, c.h - lh);
  int drawn = drawQR(c.x + (c.w - side) / 2, c.y, side, text.c_str());
  if (!drawn) { noData(c, 0, "Text too long for QR."); return; }
  if (lh) drawTextA(c.x, c.y + side + 2, c.w, fitText(label, F12, c.w), F12, 1);
}

// 2bpp image file: u16 w, u16 h, then rows MSB-first (0 = black .. 3 = white). Ordered dither to 1 bit.
static const uint8_t BAYER4[4][4] = {{0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};
static bool drawImage(const String& path, int x, int y, int w, int h, bool cover) {
  File f = LittleFS.open(path, "r");
  if (!f) return false;
  uint8_t hdr[4];
  if (f.read(hdr, 4) != 4) { f.close(); return false; }
  int iw = hdr[0] | (hdr[1] << 8), ih = hdr[2] | (hdr[3] << 8);
  size_t len = (size_t)((iw + 3) / 4) * ih;
  if (iw <= 0 || ih <= 0 || len > 200000) { f.close(); return false; }
  uint8_t* buf = (uint8_t*)psAlloc(len);
  if (!buf) { f.close(); return false; }
  bool ok = f.read(buf, len) == len;
  f.close();
  if (ok) {
    float sx = (float)iw / w, sy = (float)ih / h;
    float s = cover ? min(sx, sy) : max(sx, sy);
    int dw = (int)(iw / s), dh = (int)(ih / s);
    int ox = x + (w - dw) / 2, oy = y + (h - dh) / 2;
    int stride = (iw + 3) / 4;
    for (int yy = max(y, oy); yy < min(y + h, oy + dh); yy++) {
      int syy = min(ih - 1, (int)((yy - oy) * s));
      for (int xx = max(x, ox); xx < min(x + w, ox + dw); xx++) {
        int sxx = min(iw - 1, (int)((xx - ox) * s));
        int v = (buf[syy * stride + (sxx >> 2)] >> (6 - 2 * (sxx & 3))) & 3;
        bool white = v * 16 > BAYER4[yy & 3][xx & 3] * 3 + 1;
        cv->drawPixel(xx, yy, white ? 1 : 0);
      }
    }
  }
  free(buf);
  return ok;
}
static void w_image(WCtx& c) {
  String id = c.s["image"] | "";
  bool cover = String(c.s["fit"] | "cover") == "cover";
  if (!id.length() || !drawImage("/img/" + id + ".bin", c.x, c.y, c.w, c.h, cover))
    noData(c, 0, "Upload an image in Content and pick it here.");
}

// ---------------------------------------------------------------------------
// Custom data
static void w_json_value(WCtx& c) {
  String label = c.s["label"] | "Value", unit = c.s["unit"] | "";
  int hy = wHeader(c, label, "json:" + c.id, (uint32_t)(c.s["ttl"] | 30) * 60);
  JsonDocument d(&g_psAlloc);
  if (!readJson(cachePath("json:" + c.id), d)) { noData(c, hy, jstr(c.s["url"]).length() ? "Waiting for data..." : "Set a URL and JSON path."); return; }
  String v;
  if (d["n"].is<float>()) {
    char b[24];
    snprintf(b, sizeof(b), "%.*f", constrain((int)(c.s["dec"] | 1), 0, 4), d["n"].as<float>());
    v = b;
  } else v = d["v"] | "--";
  int uw = unit.length() ? textW(unit, B12) + 8 : 0;
  uint8_t size;
  uint8_t f = fitFont(v, c.w - uw, c.h - hy - 6, size, true);
  int asc = fontAsc(f, size);
  int y = c.y + hy + (c.h - hy - asc) / 2;
  int tw = drawText(c.x, y, v, f, size);
  if (uw) drawText(c.x + tw + 8, y + asc - fontAsc(B12), unit, B12);
}
static String expandTemplate(String t) {
  time_t now = nowT();
  t.replace("{{date}}", fmtDate(now, 0));
  t.replace("{{time}}", fmtTime(now));
  t.replace("{{name}}", cfg["profile"]["name"] | "");
  t.replace("{{moment}}", R.moment);
  t.replace("{{battery}}", String(C.soc));
  if (C.sensOk) {
    t.replace("{{temp}}", tempStr(unitsF() ? C.temp * 9 / 5 + 32 : C.temp));
    t.replace("{{humidity}}", String((int)lroundf(C.hum)) + "%");
  }
  if (t.indexOf("{{weather.") >= 0 && wxOk()) {
    JsonObjectConst cur = wx()["current"];
    JsonObjectConst d = wx()["daily"];
    t.replace("{{weather.temp}}", tempStr(cur["temperature_2m"] | 0.0f));
    t.replace("{{weather.desc}}", weatherText(cur["weather_code"] | 0));
    t.replace("{{weather.hi}}", tempStr(d["temperature_2m_max"][0] | 0.0f));
    t.replace("{{weather.lo}}", tempStr(d["temperature_2m_min"][0] | 0.0f));
  }
  int p;
  while ((p = t.indexOf("{{json.")) >= 0) {
    int e = t.indexOf("}}", p);
    if (e < 0) break;
    String wid = t.substring(p + 7, e);
    JsonDocument d(&g_psAlloc);
    String v = "--";
    if (readJson(cachePath("json:" + wid), d)) v = d["n"].is<float>() ? String(d["n"].as<float>(), 1) : String(d["v"] | "--");
    t = t.substring(0, p) + v + t.substring(e + 2);
  }
  return asciiFold(t.c_str());
}
static void w_text(WCtx& c) {
  String t = expandTemplate(c.s["text"] | "");
  const char* sz = c.s["size"] | "medium";
  uint8_t f = !strcmp(sz, "small") ? F12 : (!strcmp(sz, "large") ? B24 : B18);
  uint8_t al = !strcmp(c.s["align"] | "left", "center") ? 1 : 0;
  int lines = max(1, c.h / fontLine(f));
  std::vector<String> ls = wrapLines(t, f, c.w, lines);
  int y = c.y + (al ? max(0, (c.h - (int)ls.size() * fontLine(f)) / 2) : 0);
  for (auto& l : ls) { drawTextA(c.x, y, c.w, l, f, al); y += fontLine(f); }
}

// ---------------------------------------------------------------------------
// Registry. Schema fields: k key, l label, t type (bool,int,text,textarea,select,feed,note,list,image), d default, o options.
struct WidgetDef {
  const char* type;
  const char* name;
  const char* group;
  uint8_t minW, minH, maxW, maxH;
  bool tick;   // content depends on the clock
  const char* schema;
  WDraw draw;
};
static const WidgetDef WIDGETS[] = {
    {"clock", "Clock", "Time", 2, 1, 10, 6, true,
     R"S([{"k":"style","l":"Style","t":"select","o":["big","compact"],"d":"big"},{"k":"h24","l":"Hours","t":"select","o":["auto","12","24"],"d":"auto"},{"k":"date","l":"Show date","t":"bool","d":false}])S",
     w_clock},
    {"date", "Date", "Time", 2, 1, 10, 2, true, R"S([{"k":"format","l":"Format","t":"select","o":["long","short","numeric"],"d":"long"}])S", w_date},
    {"greeting", "Greeting", "Time", 3, 1, 10, 2, true,
     R"S([{"k":"name","l":"Name (default: profile)","t":"text","d":""},{"k":"date","l":"Show date","t":"bool","d":true}])S", w_greeting},
    {"countdown", "Countdown", "Time", 2, 1, 6, 4, true,
     R"S([{"k":"label","l":"Label","t":"text","d":"Holiday"},{"k":"target","l":"Target (YYYY-MM-DD HH:MM)","t":"text","d":""},{"k":"event","l":"Use next calendar event","t":"bool","d":false}])S",
     w_countdown},
    {"worldclock", "World clock", "Time", 3, 2, 10, 4, true,
     R"S([{"k":"zones","l":"Cities (comma separated, or Name=POSIX-TZ)","t":"text","d":"London,New York,Tokyo"}])S", w_worldclock},
    {"weather_now", "Weather now", "Weather", 3, 2, 10, 6, false, R"S([])S", w_weather_now},
    {"hourly", "Hourly forecast", "Weather", 3, 2, 10, 4, true,
     R"S([{"k":"hours","l":"Hours (4-24)","t":"int","d":12,"min":4,"max":24},{"k":"show","l":"Show","t":"select","o":["both","temp","rain"],"d":"both"}])S",
     w_hourly},
    {"daily", "Daily forecast", "Weather", 3, 1, 10, 6, false,
     R"S([{"k":"days","l":"Days","t":"int","d":5,"min":1,"max":7},{"k":"tomorrow","l":"Start tomorrow","t":"bool","d":false}])S", w_daily},
    {"sunmoon", "Sun & moon", "Weather", 3, 2, 6, 3, false, R"S([])S", w_sunmoon},
    {"air", "Air & UV", "Weather", 3, 1, 6, 2, false, R"S([{"k":"show","l":"Show","t":"select","o":["both","aqi","uv"],"d":"both"}])S", w_air},
    {"agenda", "Agenda", "Calendar", 3, 2, 10, 10, true,
     R"S([{"k":"range","l":"Range","t":"select","o":["today","tomorrow","days"],"d":"today"},{"k":"days","l":"Days (for 'days')","t":"int","d":3,"min":1,"max":7},{"k":"max","l":"Max items","t":"int","d":5,"min":1,"max":12}])S",
     w_agenda},
    {"next_event", "Next event", "Calendar", 3, 1, 10, 3, true, R"S([])S", w_next_event},
    {"headlines", "Headlines", "News & markets", 3, 2, 10, 10, false,
     R"S([{"k":"feed","l":"Feed","t":"feed","d":0},{"k":"count","l":"Items","t":"int","d":4,"min":1,"max":10}])S", w_headlines},
    {"news_summary", "News summary (AI)", "News & markets", 3, 2, 10, 6, false, R"S([{"k":"feed","l":"Feed","t":"feed","d":0}])S", w_news_summary},
    {"stocks", "Stock watchlist", "News & markets", 3, 2, 10, 6, false,
     R"S([{"k":"symbols","l":"Symbols (comma separated)","t":"text","d":"AAPL,MSFT,NVDA"},{"k":"spark","l":"Sparklines","t":"bool","d":true}])S", w_stocks},
    {"stock_chart", "Stock chart", "News & markets", 3, 2, 10, 6, false,
     R"S([{"k":"symbol","l":"Symbol","t":"text","d":"AAPL"},{"k":"range","l":"Range","t":"select","o":["1D","5D","1M","6M"],"d":"1M"}])S", w_stock_chart},
    {"crypto", "Crypto", "News & markets", 3, 1, 10, 4, false,
     R"S([{"k":"coins","l":"CoinGecko ids","t":"text","d":"bitcoin,ethereum"},{"k":"currency","l":"Currency","t":"text","d":"usd"}])S", w_crypto},
    {"ai_text", "AI text", "AI", 3, 1, 10, 6, false,
     R"S([{"k":"title","l":"Title","t":"text","d":"Thought for today"},{"k":"prompt","l":"Prompt ({weather} {agenda} {date} {name})","t":"textarea","d":"Write one short, kind thought for {date}."},{"k":"refresh","l":"Refresh","t":"select","o":["daily","daypart"],"d":"daily"}])S",
     w_ai_text},
    {"briefing", "Daily briefing (AI)", "AI", 3, 2, 10, 6, false, R"S([])S", w_briefing},
    {"outfit", "Outfit suggestion", "AI", 4, 3, 10, 10, false, R"S([])S", w_outfit},
    {"indoor", "Indoor climate", "Home & device", 2, 1, 6, 3, false, R"S([{"k":"comfort","l":"Comfort label","t":"bool","d":true}])S", w_indoor},
    {"climate_history", "Climate history", "Home & device", 3, 2, 10, 4, false,
     R"S([{"k":"show","l":"Show","t":"select","o":["both","temp","hum"],"d":"both"}])S", w_climate_history},
    {"status", "Battery & status", "Home & device", 3, 1, 10, 1, false, R"S([])S", w_status},
    {"note", "Note", "Personal", 2, 1, 10, 10, false, R"S([{"k":"note","l":"Note","t":"note","d":""}])S", w_note},
    {"list_preview", "List preview", "Personal", 2, 2, 10, 10, false,
     R"S([{"k":"list","l":"List","t":"list","d":0},{"k":"max","l":"Max items","t":"int","d":6,"min":1,"max":20}])S", w_list_preview},
    {"quote", "Quote", "Personal", 3, 1, 10, 6, false,
     R"S([{"k":"source","l":"Source","t":"select","o":["both","builtin","mine"],"d":"both"},{"k":"change","l":"Change","t":"select","o":["daily","wake"],"d":"daily"}])S",
     w_quote},
    {"sleep_calc", "Sleep calculator", "Personal", 3, 1, 10, 3, true,
     R"S([{"k":"fall","l":"Minutes to fall asleep","t":"int","d":15,"min":0,"max":120}])S", w_sleep_calc},
    {"qr", "QR code", "Personal", 2, 2, 6, 6, false,
     R"S([{"k":"text","l":"Text / URL (Wi-Fi: WIFI:T:WPA;S:name;P:pass;;)","t":"text","d":""},{"k":"label","l":"Label","t":"text","d":""}])S", w_qr},
    {"image", "Image", "Personal", 1, 1, 10, 10, false,
     R"S([{"k":"image","l":"Image","t":"image","d":""},{"k":"fit","l":"Fit","t":"select","o":["cover","contain"],"d":"cover"}])S", w_image},
    {"json_value", "JSON value", "Custom data", 2, 1, 10, 4, false,
     R"S([{"k":"label","l":"Label","t":"text","d":"Value"},{"k":"url","l":"URL","t":"text","d":""},{"k":"header","l":"Header (Name: value)","t":"text","d":""},{"k":"path","l":"JSON path (a.b.0.c)","t":"text","d":""},{"k":"unit","l":"Unit","t":"text","d":""},{"k":"dec","l":"Decimals","t":"int","d":1,"min":0,"max":4},{"k":"ttl","l":"Refresh (min)","t":"int","d":30,"min":5,"max":1440}])S",
     w_json_value},
    {"text", "Text", "Custom data", 1, 1, 10, 10, true,
     R"S([{"k":"text","l":"Text ({{date}} {{time}} {{weather.temp}} {{json.<widget id>}} ...)","t":"textarea","d":"Hello"},{"k":"size","l":"Size","t":"select","o":["small","medium","large"],"d":"medium"},{"k":"align","l":"Align","t":"select","o":["left","center"],"d":"left"}])S",
     w_text},
};
static const int WIDGET_COUNT = sizeof(WIDGETS) / sizeof(WIDGETS[0]);

static const WidgetDef* widgetDef(const char* type) {
  for (int i = 0; i < WIDGET_COUNT; i++)
    if (!strcmp(WIDGETS[i].type, type)) return &WIDGETS[i];
  return nullptr;
}

// Data keys a widget needs (see fetchNeed()).
static void widgetNeeds(JsonObjectConst w, std::vector<String>& out) {
  const char* t = w["t"] | "";
  JsonObjectConst s = w["s"];
  String id = w["id"] | "";
  auto add = [&](const String& k) {
    for (auto& e : out)
      if (e == k) return;
    out.push_back(k);
  };
  if (!strcmp(t, "weather_now") || !strcmp(t, "hourly") || !strcmp(t, "daily")) add("wx");
  else if (!strcmp(t, "air")) add("air");
  else if (!strcmp(t, "agenda") || !strcmp(t, "next_event") || (!strcmp(t, "countdown") && (s["event"] | false))) add("cal");
  else if (!strcmp(t, "headlines")) add("news:" + String((int)(s["feed"] | 0)));
  else if (!strcmp(t, "news_summary")) { add("news:" + String((int)(s["feed"] | 0))); add("ai_news:" + String((int)(s["feed"] | 0))); }
  else if (!strcmp(t, "briefing")) { add("wx"); add("cal"); add("news:0"); add("ai_brief"); }
  else if (!strcmp(t, "outfit")) { add("wx"); add("cal"); add("outfit"); }
  else if (!strcmp(t, "stocks")) { for (auto& sym : splitList(s["symbols"] | "AAPL,MSFT", 6)) add("stk:" + sym + ":1M"); }
  else if (!strcmp(t, "stock_chart")) add("stk:" + String(s["symbol"] | "AAPL") + ":" + String(s["range"] | "1M"));
  else if (!strcmp(t, "crypto")) add("cg:" + String(s["coins"] | "bitcoin,ethereum") + ":" + String(s["currency"] | "usd"));
  else if (!strcmp(t, "ai_text")) add("ai:" + id);
  else if (!strcmp(t, "json_value")) add("json:" + id);
}
