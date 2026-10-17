// Moments firmware - predefined apps (Chef, Lists, Timer, Gallery, Guardian) and system screens.
// Apps have fixed layouts for both orientations; settings and content come from the console.
#pragma once
#include "widgets.h"

#define AM 24  // app margin

bool gChefBusy = false;
bool gChefRegenReq = false;
RTC_DATA_ATTR uint8_t gChefFav = 0;
RTC_DATA_ATTR uint8_t gTimerDone = 0;
String gToast;
uint32_t gToastUntil = 0;
String gApSsid;

static void toast(const String& s) {
  gToast = s;
  gToastUntil = millis() + 4000;
}
static void drawToast() {
  if (!gToast.length() || millis() >= gToastUntil) return;
  int W = cv->width(), H = cv->height();
  int tw = min(W - 40, textW(gToast, B12) + 40);
  int x = (W - tw) / 2, y = H - 76;
  cv->fillRoundRect(x, y, tw, 48, 14, FG);
  drawTextA(x, y + (48 - fontAsc(B12)) / 2, tw, fitText(gToast, B12, tw - 20), B12, 1, 1, BG);
}
static int appHeader(const String& title, const String& sub) {
  int W = cv->width();
  int y = AM;
  drawText(AM, y, fitText(title, B18, W - 2 * AM), B18);
  y += fontLine(B18);
  if (sub.length()) {
    drawText(AM, y - 4, fitText(sub, F12, W - 2 * AM), F12);
    y += fontLine(F12);
  }
  return y + 6;
}

// ---------------------------------------------------------------------------
// Chef
JsonDocument chefDoc(&g_psAlloc), favDoc(&g_psAlloc);
bool chefLoaded = false, favLoaded = false;
static JsonDocument& chefData() {
  if (!chefLoaded) { readJson("/cache/chef.json", chefDoc); chefLoaded = true; }
  return chefDoc;
}
static JsonDocument& favData() {
  if (!favLoaded) {
    if (!readJson("/favs.json", favDoc) || !favDoc.is<JsonArray>()) favDoc.to<JsonArray>();
    favLoaded = true;
  }
  return favDoc;
}
static JsonArrayConst chefList() {
  if (gChefFav) return favData().as<JsonArrayConst>();
  return chefData()["recipes"].as<JsonArrayConst>();
}

static void addMissingToList(JsonObjectConst r) {
  JsonArray lists = listsDoc.as<JsonArray>();
  JsonObject shop;
  for (JsonObject l : lists) {
    String n = l["name"] | "";
    if (n.equalsIgnoreCase("shopping")) { shop = l; break; }
  }
  if (shop.isNull()) {
    shop = lists.add<JsonObject>();
    shop["name"] = "Shopping";
    shop["items"].to<JsonArray>();
  }
  JsonArray items = shop["items"];
  int added = 0;
  for (JsonVariantConst m : r["missing"].as<JsonArrayConst>()) {
    String t = m | "";
    bool exists = false;
    for (JsonObject it : items)
      if (t.equalsIgnoreCase(it["t"] | "")) { exists = true; break; }
    if (!exists && t.length() && items.size() < 60) {
      JsonObject it = items.add<JsonObject>();
      it["t"] = t;
      it["d"] = false;
      added++;
    }
  }
  saveLists();
  if (added) toast("Added " + String(added) + " to Shopping");
  else toast("Already on your list");
}
static void saveFavorite(JsonObjectConst r) {
  JsonArray favs = favData().as<JsonArray>();
  String title = r["title"] | "";
  for (JsonObject f : favs)
    if (title == (f["title"] | "")) { toast("Already saved"); return; }
  if (favs.size() >= 20) favs.remove(0);
  favs.add(r);
  writeJson("/favs.json", favDoc);
  toast("Saved to favorites");
}

static void drawChef() {
  int W = cv->width(), H = cv->height();
  bool P = H > W;
  time_t now = nowT();
  JsonArrayConst rs = chefList();
  int n = rs.size();
  if (R.app.sel >= n) { R.app.sel = 0; R.app.page = 0; }
  if (R.app.page == 0 || n == 0) {
    String meal = gChefFav ? String("Favorite") : jstr(chefData()["meal"]);
    if (!meal.length()) meal = mealNow(now);
    meal.setCharAt(0, toupper(meal[0]));
    int y = appHeader(meal + (gChefFav ? " recipes" : " ideas"), n ? String("Tap a recipe to start") : String());
    if (gChefBusy) {
      drawTextA(0, H / 2 - 30, W, "Thinking of recipes...", B18, 1);
      drawTextA(0, H / 2 + 10, W, "using your pantry", F12, 1);
      return;
    }
    if (!n) {
      drawWrapped(AM, y + 20, W - 2 * AM, 5,
                  aiConfigured() ? "No ideas yet. Tap 'New ideas' (needs Wi-Fi)." : "Add an AI key in the console (Connections) for recipe ideas, or save favorites.",
                  F12);
    } else {
      int cols = P ? 1 : 2, rowH = P ? 112 : 132, footer = 84;
      int cw = (W - 2 * AM - (cols - 1) * 16) / cols;
      int rows = max(1, (H - y - footer) / rowH);
      for (int i = 0; i < n && i < rows * cols; i++) {
        JsonObjectConst r = rs[i];
        int cx = AM + (i % cols) * (cw + 16), cy = y + (i / cols) * rowH;
        cv->drawRoundRect(cx, cy, cw, rowH - 12, 14, FG);
        cv->drawRoundRect(cx + 1, cy + 1, cw - 2, rowH - 14, 13, FG);
        int ty = cy + 12 + drawWrapped(cx + 14, cy + 12, cw - 28, 2, r["title"] | "", B12);
        char meta[64];
        snprintf(meta, sizeof(meta), "%d min  -  uses %d  -  missing %d", (int)(r["minutes"] | 0), (int)r["uses"].size(), (int)r["missing"].size());
        drawText(cx + 14, min(ty, cy + rowH - 12 - fontLine(F12)), fitText(meta, F12, cw - 28), F12);
        addHit(cx, cy, cw, rowH - 12, 100 + i);
      }
    }
    int bw = (W - 3 * AM) / 2;
    if (aiConfigured()) button(AM, H - 70, bw, 52, "New ideas", 10, true);
    button(2 * AM + bw, H - 70, bw, 52, gChefFav ? "AI ideas" : "Favorites", 11);
    return;
  }
  JsonObjectConst r = rs[R.app.sel];
  JsonArrayConst steps = r["steps"];
  int ns = steps.size();
  if (R.app.page == 1) {
    int y = AM;
    y += drawWrapped(AM, y, W - 2 * AM, 2, r["title"] | "", B18) + 2;
    char meta[64];
    snprintf(meta, sizeof(meta), "%d min  -  serves %d  -  %d steps", (int)(r["minutes"] | 0), (int)(r["servings"] | 2), ns);
    drawText(AM, y, meta, F12);
    y += fontLine(F12) + 10;
    int lines = P ? 4 : 2;
    drawText(AM, y, "You have", B12);
    y += fontLine(B12) - 2;
    y += drawWrapped(AM, y, W - 2 * AM, lines, joinArr(r["uses"].as<JsonArrayConst>()), F12) + 8;
    drawText(AM, y, "Missing", B12);
    y += fontLine(B12) - 2;
    String miss = joinArr(r["missing"].as<JsonArrayConst>());
    drawWrapped(AM, y, W - 2 * AM, lines, miss.length() ? miss : String("Nothing - you have it all!"), F12);
    int bw = (W - 3 * AM) / 2;
    button(AM, H - 136, W - 2 * AM, 54, "Start cooking", 20, true);
    if (r["missing"].size()) button(AM, H - 70, bw, 52, "Add missing to list", 21);
    else button(AM, H - 70, bw, 52, "Back", 23);
    if (!gChefFav) button(2 * AM + bw, H - 70, bw, 52, "Save recipe", 22);
    else button(2 * AM + bw, H - 70, bw, 52, "Back", 23);
    return;
  }
  int si = constrain(R.app.page - 2, 0, max(0, ns - 1));
  addHit(0, 0, W / 4, H, 32);
  addHit(W - W / 4, 0, W / 4, H, 33);
  drawText(AM, AM, "Step " + String(si + 1) + " of " + String(ns), B12);
  drawTextA(AM, AM, W - 2 * AM, fitText(r["title"] | "", F9, W / 2), F9, 2);
  String text = ns ? String(steps[si]["text"] | "") : String("No steps.");
  int top = AM + fontLine(B12) + 14;
  int timerH = 80;
  int avail = H - top - timerH - 50;
  uint8_t f = B24;
  if ((int)wrapLines(text, f, W - 2 * AM, 99).size() * fontLine(f) > avail) f = B18;
  if ((int)wrapLines(text, f, W - 2 * AM, 99).size() * fontLine(f) > avail) f = B12;
  drawWrapped(AM, top, W - 2 * AM, max(1, avail / fontLine(f)), text, f);
  int tsec = ns ? (int)(steps[si]["timer"] | 0) : 0;
  int ty = H - timerH - 44;
  if (R.app.chefTimerEnd > (uint32_t)now) {
    long left = (long)(R.app.chefTimerEnd - now);
    cv->fillRoundRect(AM, ty, W - 2 * AM, 60, 14, FG);
    drawTextA(AM, ty + (60 - fontAsc(B18)) / 2, W - 2 * AM, "Timer " + fmtDur(left) + " left - ends " + fmtTime(R.app.chefTimerEnd), B18, 1, 1, BG);
    addHit(AM, ty, W - 2 * AM, 60, 31);
  } else if (tsec > 0) {
    button(AM, ty, W - 2 * AM, 60, "Start " + fmtDur(tsec) + " timer", 30, false, B18);
  }
  pageDots(W / 2, H - 28, ns, si);
}

static bool chefInput(const InputEv& e, int hit) {
  JsonArrayConst rs = chefList();
  int n = rs.size();
  int ns = n ? (int)rs[R.app.sel]["steps"].size() : 0;
  int pg = R.app.page;
  time_t now = nowT();
  if (e.k == IN_TAP) {
    if (hit >= 100 && hit < 100 + n) { R.app.sel = hit - 100; R.app.page = 1; return true; }
    switch (hit) {
      case 10: gChefRegenReq = true; gChefFav = 0; R.app.page = 0; return true;
      case 11: gChefFav = !gChefFav; R.app.sel = 0; R.app.page = 0; return true;
      case 20: R.app.page = 2; return true;
      case 21: if (n) addMissingToList(rs[R.app.sel]); return true;
      case 22: if (n) saveFavorite(rs[R.app.sel]); return true;
      case 23: R.app.page = 0; return true;
      case 30: {
        int si = constrain(pg - 2, 0, max(0, ns - 1));
        int tsec = rs[R.app.sel]["steps"][si]["timer"] | 0;
        if (tsec > 0) { R.app.chefTimerEnd = now + tsec; toast("Timer started"); }
        return true;
      }
      case 31: R.app.chefTimerEnd = 0; toast("Timer cancelled"); return true;
      case 32: if (pg > 2) { R.app.page--; return true; } if (pg == 2) { R.app.page = 1; return true; } return false;
      case 33: if (pg >= 2 && pg - 2 < ns - 1) { R.app.page++; return true; } return false;
    }
    return false;
  }
  if (e.k == IN_BTN_AI) {
    if (pg >= 2) R.app.page = 1;
    else if (pg == 1) R.app.page = 0;
    else return false;
    return true;
  }
  bool next = e.k == IN_SWIPE_L || e.k == IN_SWIPE_U || e.k == IN_BTN_DOWN;
  bool prev = e.k == IN_SWIPE_R || e.k == IN_SWIPE_D || e.k == IN_BTN_UP;
  if (pg == 0 || !n) return false;
  if (next) {
    if (pg == 1) R.app.page = 2;
    else if (pg - 2 < ns - 1) R.app.page++;
    else return false;
    return true;
  }
  if (prev) {
    if (pg >= 2) R.app.page--;
    else R.app.page = 0;
    return true;
  }
  return false;
}

// ---------------------------------------------------------------------------
// Lists
static void drawLists() {
  int W = cv->width(), H = cv->height();
  JsonArray lists = listsDoc.as<JsonArray>();
  int nl = lists.size();
  if (!nl) {
    int y = appHeader("Lists", "");
    drawWrapped(AM, y + 10, W - 2 * AM, 3, "Create lists in the console (Content > Lists).", F12);
    return;
  }
  R.app.listIdx = constrain((int)R.app.listIdx, 0, nl - 1);
  JsonObject l = lists[R.app.listIdx];
  JsonArray items = l["items"];
  int n = items.size(), done = 0;
  for (JsonObject it : items) if (it["d"] | false) done++;
  int y = appHeader(asciiFold(l["name"] | "List"), String(done) + " of " + String(n) + " done");
  int rowH = 60, footer = 96;
  int per = max(1, (H - y - footer) / rowH);
  int pages = max(1, (n + per - 1) / per);
  R.app.listPage = constrain((int)R.app.listPage, 0, pages - 1);
  for (int k = 0; k < per; k++) {
    int i = R.app.listPage * per + k;
    if (i >= n) break;
    JsonObject it = items[i];
    bool d = it["d"] | false;
    int ry = y + k * rowH;
    checkBox(AM, ry + (rowH - 34) / 2 - 4, 34, d, FG, BG);
    int tw = drawText(AM + 52, ry + (rowH - fontAsc(B12)) / 2 - 4, fitText(asciiFold(it["t"] | ""), B12, W - 2 * AM - 52), B12);
    if (d) cv->drawFastHLine(AM + 52, ry + rowH / 2 - 4, tw, FG);
    addHit(AM, ry, W - 2 * AM, rowH - 6, 1000 + i);
  }
  if (!n) drawText(AM, y + 10, "This list is empty.", F12);
  if (done) button(AM, H - 84, (W - 3 * AM) / 2, 50, "Clear ticked", 260);
  if (pages > 1) drawTextA(W / 2, H - 70, W / 2 - AM, "Page " + String(R.app.listPage + 1) + "/" + String(pages), F12, 2);
  pageDots(W / 2, H - 20, nl, R.app.listIdx);
}
static bool listsInput(const InputEv& e, int hit) {
  JsonArray lists = listsDoc.as<JsonArray>();
  int nl = lists.size();
  if (!nl) return false;
  JsonArray items = lists[R.app.listIdx]["items"];
  if (e.k == IN_TAP) {
    if (hit >= 1000 && hit < 1000 + (int)items.size()) {
      JsonObject it = items[hit - 1000];
      it["d"] = !(it["d"] | false);
      saveLists();
      return true;
    }
    if (hit == 260) {
      for (int i = (int)items.size() - 1; i >= 0; i--)
        if (items[i]["d"] | false) items.remove(i);
      saveLists();
      toast("Cleared");
      return true;
    }
    return false;
  }
  if (e.k == IN_SWIPE_L) { R.app.listIdx = (R.app.listIdx + 1) % nl; R.app.listPage = 0; return true; }
  if (e.k == IN_SWIPE_R) { R.app.listIdx = (R.app.listIdx + nl - 1) % nl; R.app.listPage = 0; return true; }
  if (e.k == IN_BTN_DOWN || e.k == IN_SWIPE_U) { R.app.listPage++; return true; }
  if ((e.k == IN_BTN_UP || e.k == IN_SWIPE_D) && R.app.listPage > 0) { R.app.listPage--; return true; }
  return false;
}

// ---------------------------------------------------------------------------
// Timer (focus / quick timers)
static void focusStart(int minutes, bool brk) {
  R.app.timerEnd = nowT() + minutes * 60;
  R.app.timerLeft = 0;
  R.app.timerMin = minutes;
  R.app.timerBreak = brk;
  gTimerDone = 0;
}
static void drawTimer() {
  int W = cv->width(), H = cv->height();
  bool P = H > W;
  time_t now = nowT();
  JsonObjectConst ts = cfg["apps"]["timer"];
  uint16_t today = (uint16_t)(dayStart(now) / 86400);
  if (R.app.focusDay != today) { R.app.focusDay = today; R.app.focusCount = 0; }
  int bw = (W - 3 * AM) / 2;
  if (R.app.timerEnd || R.app.timerLeft) {
    bool paused = !R.app.timerEnd;
    long left = paused ? (long)R.app.timerLeft : max(0L, (long)(R.app.timerEnd - now));
    int mins = (int)((left + 59) / 60);
    String sub = "Paused";
    if (!paused) sub = "Ends at " + fmtTime(R.app.timerEnd);
    appHeader(R.app.timerBreak ? "Break" : "Focus", sub);
    String big = String(mins);
    uint8_t sz = P ? 4 : 3;
    int asc = fontAsc(B24, sz);
    int y = (H - asc) / 2 - 30;
    drawTextA(0, y, W, big, B24, 1, sz);
    drawTextA(0, y + asc + 16, W, mins == 1 ? "minute left" : "minutes left", B12, 1);
    button(AM, H - 84, bw, 56, paused ? "Resume" : "Pause", paused ? 322 : 320, true);
    button(2 * AM + bw, H - 84, bw, 56, "Stop", 321);
    return;
  }
  if (gTimerDone) {
    appHeader("Time's up!", String("Focus sessions today: ") + R.app.focusCount);
    drawTextA(0, H / 2 - 40, W, "Done", B24, 1, P ? 3 : 2);
    button(AM, H - 84, bw, 56, "Break " + String((int)(ts["brk"] | 5)) + " min", 310, true);
    button(2 * AM + bw, H - 84, bw, 56, "Back", 323);
    return;
  }
  int y = appHeader("Timer", String("Focus sessions today: ") + R.app.focusCount);
  JsonArrayConst presets = ts["presets"];
  int np = min(6, (int)presets.size());
  int cols = 2, bh = P ? 96 : 72, gap = 16;
  int cw = (W - 2 * AM - gap) / cols;
  for (int i = 0; i < np; i++) {
    int x = AM + (i % cols) * (cw + gap), yy = y + 10 + (i / cols) * (bh + gap);
    button(x, yy, cw, bh, String((int)(presets[i] | 25)) + " min", 300 + i, false, B18);
  }
  button(AM, H - 84, W - 2 * AM, 56, "Break " + String((int)(ts["brk"] | 5)) + " min", 310);
}
static bool timerInput(const InputEv& e, int hit) {
  if (e.k != IN_TAP) return false;
  JsonObjectConst ts = cfg["apps"]["timer"];
  time_t now = nowT();
  if (hit >= 300 && hit < 306) { focusStart(ts["presets"][hit - 300] | 25, false); return true; }
  switch (hit) {
    case 310: focusStart(ts["brk"] | 5, true); return true;
    case 320: R.app.timerLeft = (uint32_t)max(0L, (long)(R.app.timerEnd - now)); R.app.timerEnd = 0; return true;
    case 321: R.app.timerEnd = 0; R.app.timerLeft = 0; gTimerDone = 0; return true;
    case 322: R.app.timerEnd = now + R.app.timerLeft; R.app.timerLeft = 0; return true;
    case 323: gTimerDone = 0; return true;
  }
  return false;
}

// ---------------------------------------------------------------------------
// Gallery
static std::vector<String> listImages() {
  std::vector<String> v;
  File d = LittleFS.open("/img");
  if (!d || !d.isDirectory()) return v;
  File f = d.openNextFile();
  while (f) {
    String n = f.name();
    int slash = n.lastIndexOf('/');
    if (slash >= 0) n = n.substring(slash + 1);
    if (n.endsWith(".bin")) v.push_back(n.substring(0, n.length() - 4));
    f = d.openNextFile();
  }
  std::sort(v.begin(), v.end(), [](const String& a, const String& b) { return a < b; });
  return v;
}
static String galleryCurrent() {
  std::vector<String> imgs = listImages();
  if (imgs.empty()) return String();
  int n = imgs.size();
  R.app.galIdx = ((R.app.galIdx % n) + n) % n;
  return imgs[R.app.galIdx];
}
static bool galleryWantsGray() { return (cfg["apps"]["gallery"]["gray"] | true) && galleryCurrent().length(); }

// Draws one bit plane of a 2bpp image (plane 1 = high bit, 0 = low bit), white elsewhere.
static void drawImagePlane(const String& path, int plane) {
  int W = cv->width(), H = cv->height();
  cv->fillScreen(1);
  File f = LittleFS.open(path, "r");
  if (!f) return;
  uint8_t hdr[4];
  if (f.read(hdr, 4) != 4) { f.close(); return; }
  int iw = hdr[0] | (hdr[1] << 8), ih = hdr[2] | (hdr[3] << 8);
  size_t len = (size_t)((iw + 3) / 4) * ih;
  if (iw <= 0 || ih <= 0 || len > 200000) { f.close(); return; }
  uint8_t* buf = (uint8_t*)psAlloc(len);
  if (!buf) { f.close(); return; }
  bool ok = f.read(buf, len) == len;
  f.close();
  bool swap = sysBool("grayswap", false);
  if (ok) {
    float s = min((float)iw / W, (float)ih / H);
    int dw = (int)(iw / s), dh = (int)(ih / s), ox = (W - dw) / 2, oy = (H - dh) / 2, stride = (iw + 3) / 4;
    for (int yy = max(0, oy); yy < min(H, oy + dh); yy++) {
      int sy = min(ih - 1, (int)((yy - oy) * s));
      for (int xx = max(0, ox); xx < min(W, ox + dw); xx++) {
        int sx = min(iw - 1, (int)((xx - ox) * s));
        int v = (buf[sy * stride + (sx >> 2)] >> (6 - 2 * (sx & 3))) & 3;
        if (swap && (v == 1 || v == 2)) v = 3 - v;
        cv->drawPixel(xx, yy, (v >> plane) & 1);
      }
    }
  }
  free(buf);
}
static void drawGallery() {
  int W = cv->width(), H = cv->height();
  std::vector<String> imgs = listImages();
  if (imgs.empty()) {
    int y = appHeader("Gallery", "");
    drawWrapped(AM, y + 10, W - 2 * AM, 3, "Upload photos in the console (Content > Images).", F12);
    return;
  }
  String id = galleryCurrent();
  drawImage("/img/" + id + ".bin", 0, 0, W, H, true);
}
static bool galleryInput(const InputEv& e, int hit) {
  (void)hit;
  int dir = 0;
  if (e.k == IN_SWIPE_L || e.k == IN_BTN_DOWN || e.k == IN_TAP) dir = 1;
  if (e.k == IN_SWIPE_R || e.k == IN_BTN_UP) dir = -1;
  if (!dir) return false;
  R.app.galIdx += dir;
  R.app.galNext = nowT() + (uint32_t)(cfg["apps"]["gallery"]["interval"] | 60) * 60;
  return true;
}

// ---------------------------------------------------------------------------
// Guardian screen
static void drawShield(int cx, int cy, int s, bool filled) {
  int w = s, h = s * 115 / 100;
  int x = cx - w / 2, y = cy - h / 2;
  for (int pass = 0; pass < (filled ? 1 : 2); pass++) {
    int in = pass ? 7 : 0;
    uint16_t col = pass ? BG : FG;
    cv->fillRoundRect(x + in, y + in, w - 2 * in, h * 55 / 100, 10, col);
    cv->fillTriangle(x + in, y + h * 50 / 100, x + w - in, y + h * 50 / 100, cx, y + h - in * 2, col);
  }
  if (filled) {
    thickLine(cx - s / 5, cy, cx - s / 20, cy + s / 6, max(4, s / 12), BG);
    thickLine(cx - s / 20, cy + s / 6, cx + s / 4, cy - s / 6, max(4, s / 12), BG);
  }
}
static void drawGuardian() {
  int W = cv->width(), H = cv->height();
  bool P = H > W;
  JsonObjectConst g = cfg["apps"]["guardian"];
  int s = P ? 200 : 150;
  int cx = P ? W / 2 : AM + s / 2 + 20, cy = P ? AM + 40 + s / 2 : H / 2;
  int tx = P ? AM : cx + s / 2 + 40, tw = W - tx - AM;
  int ty = P ? cy + s * 60 / 100 + 30 : H / 2 - 90;
  bool armed = R.gState == G_ARMED;
  drawShield(cx, cy, s, armed);
  uint8_t al = P ? 1 : 0;
  if (armed) {
    drawTextA(tx, ty, tw, "Guardian active", B24, al);
    ty += fontLine(B24);
    drawTextA(tx, ty, tw, "since " + fmtTime(R.gArmedSince), B18, al);
    ty += fontLine(B18) + 10;
    drawWrapped(tx, ty, tw, 3, "Watching this door while you're away. Opening it without your phone nearby sends an alert.", F12, 1, FG, al);
  } else if (R.gState == G_CHECKING) {
    drawTextA(tx, ty, tw, "Checking...", B24, al);
    ty += fontLine(B24);
    drawWrapped(tx, ty, tw, 3, "Looking for your phone to see if you left (" + String(R.gCheckIdx) + "/4).", F12, 1, FG, al);
  } else {
    drawTextA(tx, ty, tw, "Guardian ready", B24, al);
    ty += fontLine(B24);
    String msg;
    if (!(g["on"] | false)) msg = "Guardian is off. Enable it in the console (Apps > Guardian).";
    else if (!cfg["owners"].size()) msg = "Add your phone or a BLE tag in the console (Connections > Owner devices).";
    else msg = "Arms automatically when you leave with your phone.";
    drawWrapped(tx, ty, tw, 4, msg, F12, 1, FG, al);
  }
  if (R.gLastAlert) drawTextA(AM, H - AM - fontLine(F9), W - 2 * AM, "Last alert " + fmtDate(R.gLastAlert, 1) + " " + fmtTime(R.gLastAlert), F9, al);
  drawTextA(AM, H - AM - 2 * fontLine(F9), W - 2 * AM, "Door notifier, not a security system.", F9, al);
}

// ---------------------------------------------------------------------------
// System screens (targets "sys:*")
static void drawConfigScreen() {
  int W = cv->width(), H = cv->height();
  bool P = H > W;
  drawText(AM, AM, "Moments setup", B24);
  int y = AM + fontLine(B24) + 6;
  String url = "http://192.168.4.1";
  if (gWifiUp) url = "http://" + WiFi.localIP().toString();
  int qs = P ? 200 : 180;
  if (gApMode) {
    String wifiQr = "WIFI:T:WPA;S:" + gApSsid + ";P:" + String(gApPass) + ";;";
    int x2 = P ? AM : W / 2 + 8;
    drawText(AM, y, "1. Join Wi-Fi", B12);
    drawQR(AM, y + fontLine(B12), qs, wifiQr.c_str());
    drawText(AM + qs + 12, y + fontLine(B12) + 10, gApSsid, B12);
    drawText(AM + qs + 12, y + fontLine(B12) + 10 + fontLine(B12), "Password", F9);
    drawText(AM + qs + 12, y + fontLine(B12) + 10 + fontLine(B12) + fontLine(F9), gApPass, B12);
    int y2 = P ? y + fontLine(B12) + qs + 20 : y;
    if (!P) x2 = W / 2 + 8;
    drawText(x2, y2, "2. Open", B12);
    drawQR(x2, y2 + fontLine(B12), qs, url.c_str());
    drawWrapped(x2 + qs + 12, y2 + fontLine(B12) + 10, (P ? W - AM : W - AM) - (x2 + qs + 12), 3, url, F12);
  } else {
    drawQR(AM, y, qs, url.c_str());
    drawText(AM + qs + 16, y + 10, "Open in a browser", F12);
    drawText(AM + qs + 16, y + 10 + fontLine(F12), "http://moments.local", B12);
    drawText(AM + qs + 16, y + 10 + fontLine(F12) + fontLine(B12), "or " + url, F12);
  }
  int py = H - AM - fontLine(B24) - fontLine(F12);
  drawText(AM, py, "Console PIN", F12);
  drawText(AM, py + fontLine(F12), gPin, B24);
  drawTextA(AM, py + fontLine(F12) + 10, W - 2 * AM, "AI button: exit", F9, 2);
}
static void drawSys(const String& which) {
  int W = cv->width(), H = cv->height();
  if (which == "config") { drawConfigScreen(); return; }
  int cy = H / 2 - 60;
  if (which == "lowbat" || which == "empty") {
    iconBattery(W / 2 - 70, cy - 40, 140, 70, C.soc, C.charging, FG);
    drawTextA(0, cy + 50, W, which == "empty" ? "Battery empty" : "Battery low", B24, 1);
    String sub = "Please charge me";
    if (C.soc >= 0) sub = String(C.soc) + "% - please charge me";
    drawTextA(0, cy + 50 + fontLine(B24), W, sub, F12, 1);
  } else if (which == "off") {
    drawTextA(0, cy, W, "Moments", B24, 1);
    drawTextA(0, cy + fontLine(B24), W, "is off", B18, 1);
    drawTextA(0, cy + fontLine(B24) + fontLine(B18) + 10, W, "Press the AI button to start", F12, 1);
  } else if (which == "hello") {
    drawTextA(0, cy, W, "Moments", B24, 1);
    drawTextA(0, cy + fontLine(B24), W, "Starting...", F12, 1);
  } else if (which == "cal1" || which == "cal2") {
    int ax = W / 2, ay = AM + 20;
    cv->fillTriangle(ax, ay, ax - 40, ay + 60, ax + 40, ay + 60, FG);
    cv->fillRect(ax - 12, ay + 60, 24, 80, FG);
    drawTextA(0, ay + 160, W, "This side up", B24, 1);
    drawWrapped(AM, ay + 160 + fontLine(B24) + 10, W - 2 * AM, 4,
                which == "cal1" ? "Hold Moments upright so this text reads normally. Keep it still and press Step 1 in the console."
                                : "Now turn Moments so this text reads normally. Keep it still and press Step 2 in the console.",
                F12, 1, FG, 1);
  } else {
    drawTextA(0, cy, W, which, B18, 1);
  }
}

// ---------------------------------------------------------------------------
// App registry
struct AppDef {
  const char* id;
  const char* name;
  void (*draw)();
  bool (*input)(const InputEv&, int);
};
static bool guardianInput(const InputEv&, int) { return false; }
static const AppDef APPS[] = {
    {"chef", "Chef", drawChef, chefInput},
    {"lists", "Lists", drawLists, listsInput},
    {"timer", "Timer", drawTimer, timerInput},
    {"gallery", "Gallery", drawGallery, galleryInput},
    {"guardian", "Guardian", drawGuardian, guardianInput},
};
static const int APP_COUNT = sizeof(APPS) / sizeof(APPS[0]);
static const AppDef* appDef(const String& id) {
  for (int i = 0; i < APP_COUNT; i++)
    if (id == APPS[i].id) return &APPS[i];
  return nullptr;
}

// Time-driven app events. Returns true when the visible app needs a redraw.
static bool appsTick(const String& target) {
  time_t now = nowT();
  bool redraw = false;
  if (R.app.chefTimerEnd && (uint32_t)now >= R.app.chefTimerEnd) {
    R.app.chefTimerEnd = 0;
    logEvent("Chef timer done");
    playSound("alert", true);
    toast("Timer done!");
    redraw = true;
  }
  if (R.app.timerEnd && (uint32_t)now >= R.app.timerEnd) {
    bool brk = R.app.timerBreak;
    R.app.timerEnd = 0;
    if (!brk) R.app.focusCount++;
    gTimerDone = brk ? 0 : 1;
    logEvent(brk ? "Break over" : "Focus session done");
    playSound(cfg["apps"]["timer"]["sound"] | "double", true);
    redraw = true;
  }
  if (target == "app:gallery") {
    uint32_t iv = cfg["apps"]["gallery"]["interval"] | 60;
    if (iv && R.app.galNext && (uint32_t)now >= R.app.galNext) {
      if (cfg["apps"]["gallery"]["shuffle"] | false) R.app.galIdx = esp_random() % 1000;
      else R.app.galIdx++;
      R.app.galNext = now + iv * 60;
      redraw = true;
    } else if (!R.app.galNext) R.app.galNext = now + iv * 60;
  }
  return redraw;
}
// Next time an app needs attention (0 = none).
static time_t appsNextChange(const String& target) {
  time_t t = 0;
  auto take = [&](time_t c) { if (c && (!t || c < t)) t = c; };
  take(R.app.chefTimerEnd);
  take(R.app.timerEnd);
  if (R.app.timerEnd) take(((nowT() / 60) + 1) * 60);
  if (target == "app:gallery") take(R.app.galNext);
  if (target == "app:chef" && R.app.chefTimerEnd) take(((nowT() / 60) + 1) * 60);
  return t;
}
