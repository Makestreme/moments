// Moments firmware - time, time zones, dayparts, sun and moon.
#pragma once
#include "store.h"

time_t nowT() { return time(nullptr); }
static bool timeValid() { return time(nullptr) > 1704067200; }  // after 2024-01-01

static struct tm localTm(time_t t) {
  struct tm r;
  localtime_r(&t, &r);
  return r;
}
static time_t localMake(int y, int mo, int d, int h, int mi, int s) {
  struct tm t;
  memset(&t, 0, sizeof(t));
  t.tm_year = y - 1900;
  t.tm_mon = mo - 1;
  t.tm_mday = d;
  t.tm_hour = h;
  t.tm_min = mi;
  t.tm_sec = s;
  t.tm_isdst = -1;
  return mktime(&t);
}
static time_t dayStart(time_t t, int addDays = 0) {
  struct tm l = localTm(t);
  return localMake(l.tm_year + 1900, l.tm_mon + 1, l.tm_mday + addDays, 0, 0, 0);
}
static time_t atMinuteOfDay(time_t day, int mod, int addDays = 0) {
  struct tm l = localTm(day);
  return localMake(l.tm_year + 1900, l.tm_mon + 1, l.tm_mday + addDays, mod / 60, mod % 60, 0);
}
static int modOf(time_t t) {
  struct tm l = localTm(t);
  return l.tm_hour * 60 + l.tm_min;
}
static int weekdayMon0(const struct tm& t) { return (t.tm_wday + 6) % 7; }
// Next occurrence of HH:MM strictly after t.
static time_t nextAt(time_t t, int mod) {
  time_t c = atMinuteOfDay(t, mod);
  if (c <= t) c = atMinuteOfDay(t, mod, 1);
  return c;
}

static const char* const DOW_S[7] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
static const char* const DOW_L[7] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
static const char* const MON_S[12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
static const char* const MON_L[12] = {"January", "February", "March", "April", "May", "June",
                                      "July", "August", "September", "October", "November", "December"};

static String fmtHM(int h, int m, bool h24) {
  char b[12];
  if (h24) snprintf(b, sizeof(b), "%02d:%02d", h, m);
  else snprintf(b, sizeof(b), "%d:%02d%s", (h % 12) ? h % 12 : 12, m, h < 12 ? "am" : "pm");
  return String(b);
}
static String fmtTime(time_t t) {
  struct tm l = localTm(t);
  return fmtHM(l.tm_hour, l.tm_min, use24h());
}
static String fmtDate(time_t t, int style) {
  struct tm l = localTm(t);
  char b[40];
  if (style == 0) snprintf(b, sizeof(b), "%s, %d %s", DOW_L[l.tm_wday], l.tm_mday, MON_L[l.tm_mon]);
  else if (style == 1) snprintf(b, sizeof(b), "%s %d %s", DOW_S[l.tm_wday], l.tm_mday, MON_S[l.tm_mon]);
  else snprintf(b, sizeof(b), "%04d-%02d-%02d", l.tm_year + 1900, l.tm_mon + 1, l.tm_mday);
  return String(b);
}
static String dateKey(time_t t) { return fmtDate(t, 2); }
static String fmtAgo(long secs) {
  if (secs < 90) return "now";
  if (secs < 3600) return String(secs / 60) + "m";
  if (secs < 172800) return String(secs / 3600) + "h";
  return String(secs / 86400) + "d";
}
static String fmtDur(long secs) {
  if (secs < 0) secs = 0;
  long h = secs / 3600, m = (secs % 3600) / 60;
  if (h) return String(h) + "h " + String(m) + "m";
  return String(m) + " min";
}

// ---------------------------------------------------------------------------
// Sunrise / sunset (NOAA almanac approximation, ~1-2 min accuracy).
static bool sunUt(int y, int mo, int d, double lat, double lon, bool rise, double& ut) {
  const double R2D = 180.0 / PI, D2R = PI / 180.0;
  int N = (int)(daysFromCivil(y, mo, d) - daysFromCivil(y, 1, 1)) + 1;
  double lngHour = lon / 15.0;
  double t = N + ((rise ? 6.0 : 18.0) - lngHour) / 24.0;
  double M = 0.9856 * t - 3.289;
  double L = M + 1.916 * sin(M * D2R) + 0.020 * sin(2 * M * D2R) + 282.634;
  L = fmod(L + 720.0, 360.0);
  double RA = atan(0.91764 * tan(L * D2R)) * R2D;
  RA = fmod(RA + 720.0, 360.0);
  RA += floor(L / 90.0) * 90.0 - floor(RA / 90.0) * 90.0;
  RA /= 15.0;
  double sinDec = 0.39782 * sin(L * D2R);
  double cosDec = cos(asin(sinDec));
  double cosH = (cos(90.833 * D2R) - sinDec * sin(lat * D2R)) / (cosDec * cos(lat * D2R));
  if (cosH > 1 || cosH < -1) return false;
  double H = rise ? 360.0 - acos(cosH) * R2D : acos(cosH) * R2D;
  H /= 15.0;
  double T = H + RA - 0.06571 * t - 6.622;
  ut = fmod(T - lngHour + 48.0, 24.0);
  return true;
}
static bool sunTimes(time_t day, time_t& rise, time_t& set) {
  if (!hasLocation()) return false;
  double lat = cfg["loc"]["lat"], lon = cfg["loc"]["lon"];
  struct tm l = localTm(day);
  int y = l.tm_year + 1900, mo = l.tm_mon + 1, d = l.tm_mday;
  double ur, us;
  if (!sunUt(y, mo, d, lat, lon, true, ur) || !sunUt(y, mo, d, lat, lon, false, us)) return false;
  time_t u0 = mkUtc(y, mo, d, 0, 0, 0);
  time_t ds = dayStart(day), de = dayStart(day, 1);
  rise = u0 + (time_t)(ur * 3600);
  set = u0 + (time_t)(us * 3600);
  if (rise < ds) rise += 86400;
  if (rise >= de) rise -= 86400;
  if (set < ds) set += 86400;
  if (set >= de) set -= 86400;
  return true;
}
static double moonPhase(time_t t) {
  double days = (double)(t - 947182440) / 86400.0;  // new moon 2000-01-06 18:14 UTC
  double p = fmod(days / 29.530588853, 1.0);
  return p < 0 ? p + 1 : p;
}
static const char* moonName(double p) {
  static const char* const N[8] = {"New moon", "Waxing crescent", "First quarter", "Waxing gibbous",
                                   "Full moon", "Waning gibbous", "Last quarter", "Waning crescent"};
  return N[(int)(p * 8 + 0.5) % 8];
}

// ---------------------------------------------------------------------------
// Dayparts: {"id","name","start","end", optional "startA"/"endA" = "sunrise"|"sunset", "startO"/"endO" minutes}
static int dpEdge(JsonObjectConst dp, bool start, time_t day) {
  const char* a = dp[start ? "startA" : "endA"] | "";
  if (*a) {
    time_t r, s;
    if (sunTimes(day, r, s)) {
      int m = modOf(!strcmp(a, "sunset") ? s : r) + (int)(dp[start ? "startO" : "endO"] | 0);
      return ((m % 1440) + 1440) % 1440;
    }
  }
  return parseHHMM(dp[start ? "start" : "end"] | "00:00", 0);
}
static bool inWindow(int mod, int s, int e) {
  if (s == e) return true;
  if (s < e) return mod >= s && mod < e;
  return mod >= s || mod < e;
}
static int daypartIndex(time_t t) {
  int mod = modOf(t), i = 0;
  for (JsonObjectConst dp : cfg["dayparts"].as<JsonArrayConst>()) {
    if (inWindow(mod, dpEdge(dp, true, t), dpEdge(dp, false, t))) return i;
    i++;
  }
  return -1;
}
static const char* daypartId(int idx) {
  if (idx < 0) return "";
  return cfg["dayparts"][idx]["id"] | "";
}
static const char* daypartName(int idx) {
  if (idx < 0) return "";
  return cfg["dayparts"][idx]["name"] | "";
}
static time_t nextDaypartChange(time_t t) {
  time_t best = t + 86400;
  for (JsonObjectConst dp : cfg["dayparts"].as<JsonArrayConst>())
    for (int s = 0; s < 2; s++) {
      time_t c = nextAt(t, dpEdge(dp, s == 0, t));
      if (c < best) best = c;
    }
  return best;
}
