// Moments firmware - drawing helpers: fonts, text layout, icons, QR codes.
#pragma once
#include "devices.h"
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSans12pt7b.h>
#include <Fonts/FreeSans18pt7b.h>
#include <Fonts/FreeSans24pt7b.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include <Fonts/FreeSansBold24pt7b.h>

GFXcanvas1* cv = nullptr;   // 800x480 viewer-frame canvas, rotated per orientation
uint16_t FG = 0, BG = 1;    // 0 = black, 1 = white (bit value in the frame buffer)

enum : uint8_t { F9, F12, F18, F24, B9, B12, B18, B24 };
static const GFXfont* const FONTS[8] = {&FreeSans9pt7b, &FreeSans12pt7b, &FreeSans18pt7b, &FreeSans24pt7b,
                                        &FreeSansBold9pt7b, &FreeSansBold12pt7b, &FreeSansBold18pt7b, &FreeSansBold24pt7b};

static int glyphAdv(const GFXfont* f, uint8_t c) {
  if (c == 0x7F) return f->yAdvance / 3;
  if (c < f->first || c > f->last) return 0;
  return f->glyph[c - f->first].xAdvance;
}
static int fontAsc(uint8_t font, uint8_t size = 1) {
  const GFXfont* f = FONTS[font];
  return -f->glyph['H' - f->first].yOffset * size;
}
static int fontLine(uint8_t font, uint8_t size = 1) { return FONTS[font]->yAdvance * size; }
static int textW(const char* s, uint8_t font, uint8_t size = 1) {
  const GFXfont* f = FONTS[font];
  int w = 0;
  for (const uint8_t* p = (const uint8_t*)s; *p; p++) w += glyphAdv(f, *p);
  return w * size;
}
static int textW(const String& s, uint8_t font, uint8_t size = 1) { return textW(s.c_str(), font, size); }

// Draws text with its cap-top at y. Returns advance width.
static int drawText(int x, int y, const char* s, uint8_t font, uint8_t size = 1, uint16_t color = 0xFFFF) {
  if (color == 0xFFFF) color = FG;
  const GFXfont* f = FONTS[font];
  cv->setFont(f);
  cv->setTextSize(size);
  int asc = fontAsc(font, size);
  int base = y + asc;
  int cx = x;
  for (const uint8_t* p = (const uint8_t*)s; *p; p++) {
    uint8_t c = *p;
    if (c == 0x7F) {
      int r = max(2, asc / 7);
      cv->drawCircle(cx + r + 1, y + r, r, color);
      if (r > 3) cv->drawCircle(cx + r + 1, y + r, r - 1, color);
      if (r > 6) cv->drawCircle(cx + r + 1, y + r, r - 2, color);
      cx += glyphAdv(f, c) * size;
      continue;
    }
    if (c < f->first || c > f->last) continue;
    cv->drawChar(cx, base, c, color, color, size);
    cx += f->glyph[c - f->first].xAdvance * size;
  }
  return cx - x;
}
static int drawText(int x, int y, const String& s, uint8_t font, uint8_t size = 1, uint16_t color = 0xFFFF) {
  return drawText(x, y, s.c_str(), font, size, color);
}
// align: 0 left, 1 center, 2 right (within [x, x+w))
static int drawTextA(int x, int y, int w, const String& s, uint8_t font, uint8_t align, uint8_t size = 1, uint16_t color = 0xFFFF) {
  int tw = textW(s, font, size);
  int dx = align == 1 ? (w - tw) / 2 : (align == 2 ? w - tw : 0);
  return drawText(x + dx, y, s, font, size, color);
}

static String fitText(const String& s, uint8_t font, int maxW, uint8_t size = 1) {
  if (textW(s, font, size) <= maxW) return s;
  String t = s;
  while (t.length() > 1 && textW(t + "...", font, size) > maxW) t.remove(t.length() - 1);
  t.trim();
  return t + "...";
}

static std::vector<String> wrapLines(const String& text, uint8_t font, int w, int maxLines, uint8_t size = 1) {
  std::vector<String> lines;
  String line;
  int i = 0, n = text.length();
  bool truncated = false;
  while (i < n) {
    if (text[i] == '\n') {
      lines.push_back(line);
      line = "";
      i++;
      if ((int)lines.size() >= maxLines) { truncated = i < n; break; }
      continue;
    }
    int j = i;
    while (j < n && text[j] != ' ' && text[j] != '\n') j++;
    String word = text.substring(i, j);
    String cand = line;
    if (cand.length()) cand += ' ';
    cand += word;
    if (textW(cand, font, size) <= w) {
      line = cand;
    } else if (!line.length()) {
      // single word wider than the box: hard break
      String part;
      for (int k = 0; k < (int)word.length(); k++) {
        if (textW(part + word[k], font, size) > w && part.length()) {
          lines.push_back(part);
          part = "";
          if ((int)lines.size() >= maxLines) break;
        }
        part += word[k];
      }
      line = part;
      if ((int)lines.size() >= maxLines) { truncated = true; break; }
    } else {
      lines.push_back(line);
      line = word;
      if ((int)lines.size() >= maxLines) { truncated = true; line = ""; break; }
    }
    i = j;
    while (i < n && text[i] == ' ') i++;
  }
  if (line.length() && (int)lines.size() < maxLines) lines.push_back(line);
  else if (line.length()) truncated = true;
  if (truncated && !lines.empty()) lines.back() = fitText(lines.back() + " ...", font, w, size);
  return lines;
}

// Returns height used.
static int drawWrapped(int x, int y, int w, int maxLines, const String& text, uint8_t font, uint8_t size = 1,
                       uint16_t color = 0xFFFF, uint8_t align = 0) {
  std::vector<String> lines = wrapLines(text, font, w, maxLines, size);
  int lh = fontLine(font, size);
  int yy = y;
  for (auto& l : lines) {
    drawTextA(x, yy, w, l, font, align, size, color);
    yy += lh;
  }
  return yy - y;
}

// ---------------------------------------------------------------------------
// Shapes & icons
static void thickLine(int x0, int y0, int x1, int y1, int t, uint16_t c) {
  if (t <= 1) { cv->drawLine(x0, y0, x1, y1, c); return; }
  float dx = x1 - x0, dy = y1 - y0, len = sqrtf(dx * dx + dy * dy);
  if (len < 0.5f) { cv->fillCircle(x0, y0, t / 2, c); return; }
  float nx = -dy / len * t / 2.0f, ny = dx / len * t / 2.0f;
  cv->fillTriangle(x0 + nx, y0 + ny, x1 + nx, y1 + ny, x1 - nx, y1 - ny, c);
  cv->fillTriangle(x0 + nx, y0 + ny, x1 - nx, y1 - ny, x0 - nx, y0 - ny, c);
  cv->fillCircle(x0, y0, t / 2, c);
  cv->fillCircle(x1, y1, t / 2, c);
}

static void iconSun(int cx, int cy, int r, uint16_t c, uint16_t bg) {
  int t = max(2, r / 7);
  int core = r * 45 / 100;
  cv->fillCircle(cx, cy, core, c);
  cv->fillCircle(cx, cy, core - t, bg);
  for (int i = 0; i < 8; i++) {
    float a = i * PI / 4;
    thickLine(cx + cosf(a) * r * 0.65f, cy + sinf(a) * r * 0.65f, cx + cosf(a) * r * 0.95f, cy + sinf(a) * r * 0.95f, t, c);
  }
}
static void iconMoon(int cx, int cy, int r, uint16_t c, uint16_t bg) {
  cv->fillCircle(cx, cy, r * 6 / 10, c);
  cv->fillCircle(cx + r * 3 / 10, cy - r * 2 / 10, r * 5 / 10, bg);
}
static void iconCloud(int cx, int cy, int r, uint16_t c, uint16_t bg) {
  int t = max(2, r / 8);
  int ax = cx - r * 35 / 100, ay = cy + r * 10 / 100, ar = r * 32 / 100;
  int bx = cx + r * 5 / 100, by = cy - r * 12 / 100, br = r * 45 / 100;
  int qx = cx + r * 45 / 100, qy = cy + r * 12 / 100, qr = r * 30 / 100;
  int rx0 = ax, rx1 = qx, ry0 = cy + r * 10 / 100, ry1 = cy + r * 42 / 100;
  for (int pass = 0; pass < 2; pass++) {
    int e = pass == 0 ? t : 0;
    uint16_t col = pass == 0 ? c : bg;
    cv->fillCircle(ax, ay, ar + e, col);
    cv->fillCircle(bx, by, br + e, col);
    cv->fillCircle(qx, qy, qr + e, col);
    cv->fillRect(rx0, ry0 - e, rx1 - rx0, ry1 - ry0 + 2 * e, col);
  }
}
static void iconDrops(int cx, int cy, int r, uint16_t c, int n) {
  int t = max(2, r / 9);
  for (int i = 0; i < n; i++) {
    int x = cx - r * 30 / 100 + i * r * 30 / 100;
    thickLine(x, cy + r * 55 / 100, x - r * 10 / 100, cy + r * 80 / 100, t, c);
  }
}
static void iconFlakes(int cx, int cy, int r, uint16_t c) {
  int s = max(3, r / 8);
  for (int i = 0; i < 3; i++) {
    int x = cx - r * 30 / 100 + i * r * 30 / 100, y = cy + r * 68 / 100;
    cv->drawLine(x - s, y, x + s, y, c);
    cv->drawLine(x, y - s, x, y + s, c);
    cv->drawLine(x - s * 7 / 10, y - s * 7 / 10, x + s * 7 / 10, y + s * 7 / 10, c);
    cv->drawLine(x - s * 7 / 10, y + s * 7 / 10, x + s * 7 / 10, y - s * 7 / 10, c);
  }
}
static void iconBolt(int cx, int cy, int r, uint16_t c) {
  int x = cx, y = cy + r * 40 / 100;
  cv->fillTriangle(x + r * 10 / 100, y, x - r * 18 / 100, y + r * 32 / 100, x + r * 2 / 100, y + r * 32 / 100, c);
  cv->fillTriangle(x - r * 2 / 100, y + r * 26 / 100, x + r * 16 / 100, y + r * 26 / 100, x - r * 12 / 100, y + r * 60 / 100, c);
}

static const char* weatherText(int code) {
  if (code == 0) return "Clear";
  if (code == 1) return "Mostly clear";
  if (code == 2) return "Partly cloudy";
  if (code == 3) return "Overcast";
  if (code == 45 || code == 48) return "Fog";
  if (code >= 51 && code <= 55) return "Drizzle";
  if (code == 56 || code == 57) return "Freezing drizzle";
  if (code >= 61 && code <= 65) return "Rain";
  if (code == 66 || code == 67) return "Freezing rain";
  if (code >= 71 && code <= 75) return "Snow";
  if (code == 77) return "Snow grains";
  if (code >= 80 && code <= 82) return "Showers";
  if (code == 85 || code == 86) return "Snow showers";
  if (code >= 95) return "Thunderstorm";
  return "--";
}

static void weatherIcon(int cx, int cy, int r, int code, bool day, uint16_t c = 0xFFFF, uint16_t bg = 0xFFFF) {
  if (c == 0xFFFF) c = FG;
  if (bg == 0xFFFF) bg = BG;
  if (code == 0) { if (day) iconSun(cx, cy, r, c, bg); else iconMoon(cx, cy, r, c, bg); return; }
  if (code == 1 || code == 2) {
    if (day) iconSun(cx - r * 30 / 100, cy - r * 25 / 100, r * 65 / 100, c, bg);
    else iconMoon(cx - r * 30 / 100, cy - r * 25 / 100, r * 65 / 100, c, bg);
    iconCloud(cx + r * 12 / 100, cy + r * 18 / 100, r * 75 / 100, c, bg);
    return;
  }
  if (code == 45 || code == 48) {
    int t = max(2, r / 8);
    for (int i = 0; i < 4; i++) {
      int y = cy - r * 45 / 100 + i * r * 30 / 100;
      thickLine(cx - r * 75 / 100 + (i % 2) * r / 6, y, cx + r * 75 / 100 - ((i + 1) % 2) * r / 6, y, t, c);
    }
    return;
  }
  bool precip = code >= 51;
  iconCloud(cx, precip ? cy - r * 22 / 100 : cy, precip ? r * 85 / 100 : r, c, bg);
  if (!precip) return;
  if ((code >= 71 && code <= 77) || code == 85 || code == 86) iconFlakes(cx, cy, r, c);
  else if (code >= 95) iconBolt(cx, cy, r, c);
  else iconDrops(cx, cy, r, c, code <= 57 ? 2 : 3);
}

static void iconBattery(int x, int y, int w, int h, int pct, bool charging, uint16_t c) {
  cv->drawRect(x, y, w - 3, h, c);
  cv->drawRect(x + 1, y + 1, w - 5, h - 2, c);
  cv->fillRect(x + w - 3, y + h / 4, 3, h / 2, c);
  if (pct >= 0) {
    int fw = (w - 9) * constrain(pct, 0, 100) / 100;
    cv->fillRect(x + 3, y + 3, fw, h - 6, c);
  }
  if (charging) {
    int cx = x + (w - 3) / 2, cy = y + h / 2;
    uint16_t inv = c ? 0 : 1;
    cv->fillTriangle(cx + 2, cy - h / 2 + 2, cx - 4, cy + 1, cx + 1, cy + 1, inv);
    cv->fillTriangle(cx - 1, cy - 1, cx + 4, cy - 1, cx - 2, cy + h / 2 - 2, inv);
  }
}
static void iconWifi(int x, int y, int s, bool on, uint16_t c) {
  int cx = x + s / 2, cy = y + s;
  for (int i = 1; i <= 3; i++) {
    int r = s * i / 3;
    for (int k = 0; k < 2; k++) cv->drawCircleHelper(cx, cy - k, r, 1 | 2, c);
  }
  cv->fillCircle(cx, cy - 1, max(2, s / 10), c);
  if (!on) thickLine(x, y, x + s, y + s, 2, c);
}
static void checkBox(int x, int y, int s, bool checked, uint16_t c, uint16_t bg) {
  cv->drawRoundRect(x, y, s, s, 4, c);
  cv->drawRoundRect(x + 1, y + 1, s - 2, s - 2, 3, c);
  if (checked) {
    cv->fillRoundRect(x, y, s, s, 4, c);
    thickLine(x + s * 22 / 100, y + s * 52 / 100, x + s * 42 / 100, y + s * 72 / 100, max(2, s / 8), bg);
    thickLine(x + s * 42 / 100, y + s * 72 / 100, x + s * 78 / 100, y + s * 30 / 100, max(2, s / 8), bg);
  }
}
// Rounded button with centered label; registers a touch hit region.
static void button(int x, int y, int w, int h, const String& label, uint16_t id, bool filled = false, uint8_t font = B12) {
  if (filled) cv->fillRoundRect(x, y, w, h, 12, FG);
  else { cv->drawRoundRect(x, y, w, h, 12, FG); cv->drawRoundRect(x + 1, y + 1, w - 2, h - 2, 11, FG); }
  drawTextA(x, y + (h - fontAsc(font)) / 2, w, fitText(label, font, w - 16), font, 1, 1, filled ? BG : FG);
  addHit(x, y, w, h, id);
}
static void pageDots(int cx, int y, int n, int cur) {
  if (n <= 1) return;
  if (n > 15) {
    char b[16];
    snprintf(b, sizeof(b), "%d / %d", cur + 1, n);
    drawTextA(cx - 60, y - 6, 120, b, F9, 1);
    return;
  }
  int gap = 18, x0 = cx - (n - 1) * gap / 2;
  for (int i = 0; i < n; i++) {
    if (i == cur) cv->fillCircle(x0 + i * gap, y, 5, FG);
    else cv->drawCircle(x0 + i * gap, y, 4, FG);
  }
}

// ---------------------------------------------------------------------------
// QR code encoder (byte mode, ECC L/M, versions 1..10), after Nayuki's reference design.
struct QrVer { uint8_t ec, b1, d1, b2, d2; };
static const QrVer QR_TAB[10][2] = {
    {{7, 1, 19, 0, 0}, {10, 1, 16, 0, 0}},     {{10, 1, 34, 0, 0}, {16, 1, 28, 0, 0}},
    {{15, 1, 55, 0, 0}, {26, 1, 44, 0, 0}},    {{20, 1, 80, 0, 0}, {18, 2, 32, 0, 0}},
    {{26, 1, 108, 0, 0}, {24, 2, 43, 0, 0}},   {{18, 2, 68, 0, 0}, {16, 4, 27, 0, 0}},
    {{20, 2, 78, 0, 0}, {18, 4, 31, 0, 0}},    {{24, 2, 97, 0, 0}, {22, 2, 38, 2, 39}},
    {{30, 2, 116, 0, 0}, {22, 3, 36, 2, 37}},  {{18, 2, 68, 2, 69}, {26, 4, 43, 1, 44}},
};
static const uint8_t QR_ALIGN[10][3] = {{0, 0, 0},  {6, 18, 0}, {6, 22, 0}, {6, 26, 0},  {6, 30, 0},
                                        {6, 34, 0}, {6, 22, 38}, {6, 24, 42}, {6, 26, 46}, {6, 28, 50}};

class QrCode {
 public:
  int size = 0;
  bool get(int x, int y) const { return mod[y][x]; }

  bool encode(const char* text) {
    size_t len = strlen(text);
    for (int ecl = 1; ecl >= 0; ecl--)
      for (int v = 1; v <= 10; v++) {
        const QrVer& t = QR_TAB[v - 1][ecl];
        int dataCw = t.b1 * t.d1 + t.b2 * t.d2;
        int cntBits = v < 10 ? 8 : 16;
        if (4 + cntBits + (int)len * 8 <= dataCw * 8) return build(text, len, v, ecl);
      }
    return false;
  }

 private:
  uint8_t mod[57][57];
  uint8_t fn[57][57];
  uint8_t buf[400];
  int bitlen = 0;

  static uint8_t gfMul(uint8_t x, uint8_t y) {
    int z = 0;
    for (int i = 7; i >= 0; i--) {
      z = (z << 1) ^ ((z >> 7) * 0x11D);
      z ^= ((y >> i) & 1) * x;
    }
    return (uint8_t)z;
  }
  static void rsDivisor(int deg, uint8_t* out) {
    memset(out, 0, deg);
    out[deg - 1] = 1;
    uint8_t root = 1;
    for (int i = 0; i < deg; i++) {
      for (int j = 0; j < deg; j++) {
        out[j] = gfMul(out[j], root);
        if (j + 1 < deg) out[j] ^= out[j + 1];
      }
      root = gfMul(root, 0x02);
    }
  }
  static void rsRemainder(const uint8_t* data, int len, const uint8_t* div, int deg, uint8_t* res) {
    memset(res, 0, deg);
    for (int i = 0; i < len; i++) {
      uint8_t f = data[i] ^ res[0];
      memmove(res, res + 1, deg - 1);
      res[deg - 1] = 0;
      for (int j = 0; j < deg; j++) res[j] ^= gfMul(div[j], f);
    }
  }
  void put(uint32_t val, int n) {
    for (int i = n - 1; i >= 0; i--) {
      if ((val >> i) & 1) buf[bitlen >> 3] |= (uint8_t)(0x80 >> (bitlen & 7));
      bitlen++;
    }
  }
  void setF(int x, int y, bool d) { mod[y][x] = d; fn[y][x] = 1; }
  void finder(int x, int y) {
    for (int dy = -4; dy <= 4; dy++)
      for (int dx = -4; dx <= 4; dx++) {
        int xx = x + dx, yy = y + dy;
        if (xx < 0 || yy < 0 || xx >= size || yy >= size) continue;
        int d = max(abs(dx), abs(dy));
        setF(xx, yy, d != 2 && d != 4);
      }
  }
  void align(int x, int y) {
    for (int dy = -2; dy <= 2; dy++)
      for (int dx = -2; dx <= 2; dx++) setF(x + dx, y + dy, max(abs(dx), abs(dy)) != 1);
  }
  void format(int ecl, int mask) {
    int data = ((ecl == 0 ? 1 : 0) << 3) | mask;
    int rem = data;
    for (int i = 0; i < 10; i++) rem = (rem << 1) ^ ((rem >> 9) * 0x537);
    int bits = ((data << 10) | rem) ^ 0x5412;
    auto bit = [&](int i) { return ((bits >> i) & 1) != 0; };
    for (int i = 0; i <= 5; i++) setF(8, i, bit(i));
    setF(8, 7, bit(6));
    setF(8, 8, bit(7));
    setF(7, 8, bit(8));
    for (int i = 9; i < 15; i++) setF(14 - i, 8, bit(i));
    for (int i = 0; i < 8; i++) setF(size - 1 - i, 8, bit(i));
    for (int i = 8; i < 15; i++) setF(8, size - 15 + i, bit(i));
    setF(8, size - 8, true);
  }
  void version(int ver) {
    int rem = ver;
    for (int i = 0; i < 12; i++) rem = (rem << 1) ^ ((rem >> 11) * 0x1F25);
    long bits = ((long)ver << 12) | rem;
    for (int i = 0; i < 18; i++) {
      bool b = ((bits >> i) & 1) != 0;
      int a = size - 11 + i % 3, c = i / 3;
      setF(a, c, b);
      setF(c, a, b);
    }
  }
  void applyMask(int m) {
    for (int y = 0; y < size; y++)
      for (int x = 0; x < size; x++) {
        if (fn[y][x]) continue;
        bool inv;
        switch (m) {
          case 0: inv = (x + y) % 2 == 0; break;
          case 1: inv = y % 2 == 0; break;
          case 2: inv = x % 3 == 0; break;
          case 3: inv = (x + y) % 3 == 0; break;
          case 4: inv = (x / 3 + y / 2) % 2 == 0; break;
          case 5: inv = x * y % 2 + x * y % 3 == 0; break;
          case 6: inv = (x * y % 2 + x * y % 3) % 2 == 0; break;
          default: inv = ((x + y) % 2 + x * y % 3) % 2 == 0; break;
        }
        if (inv) mod[y][x] ^= 1;
      }
  }
  long penalty() {
    long p = 0;
    for (int pass = 0; pass < 2; pass++)
      for (int a = 0; a < size; a++) {
        int run = 1;
        for (int b = 1; b < size; b++) {
          uint8_t c0 = pass ? mod[b - 1][a] : mod[a][b - 1], c1 = pass ? mod[b][a] : mod[a][b];
          if (c0 == c1) run++;
          else { if (run >= 5) p += run - 2; run = 1; }
        }
        if (run >= 5) p += run - 2;
      }
    for (int y = 0; y + 1 < size; y++)
      for (int x = 0; x + 1 < size; x++) {
        uint8_t c = mod[y][x];
        if (c == mod[y][x + 1] && c == mod[y + 1][x] && c == mod[y + 1][x + 1]) p += 3;
      }
    long dark = 0, total = (long)size * size;
    for (int y = 0; y < size; y++)
      for (int x = 0; x < size; x++) dark += mod[y][x];
    long k = (labs(dark * 20 - total * 10) + total - 1) / total - 1;
    p += k * 10;
    return p;
  }

  bool build(const char* text, size_t len, int ver, int ecl) {
    const QrVer& t = QR_TAB[ver - 1][ecl];
    int dataCw = t.b1 * t.d1 + t.b2 * t.d2;
    memset(buf, 0, sizeof(buf));
    bitlen = 0;
    put(4, 4);
    put((uint32_t)len, ver < 10 ? 8 : 16);
    for (size_t i = 0; i < len; i++) put((uint8_t)text[i], 8);
    int cap = dataCw * 8;
    put(0, min(4, cap - bitlen));
    bitlen = (bitlen + 7) & ~7;
    for (uint8_t pad = 0xEC; bitlen < cap; pad ^= (0xEC ^ 0x11)) put(pad, 8);

    int nb = t.b1 + t.b2;
    uint8_t gen[32], ecc[8][32];
    rsDivisor(t.ec, gen);
    int start[8], blen[8], off = 0;
    for (int b = 0; b < nb; b++) {
      blen[b] = b < t.b1 ? t.d1 : t.d2;
      start[b] = off;
      rsRemainder(buf + off, blen[b], gen, t.ec, ecc[b]);
      off += blen[b];
    }
    uint8_t out[400];
    int o = 0, maxD = max(t.d1, t.d2);
    for (int i = 0; i < maxD; i++)
      for (int b = 0; b < nb; b++)
        if (i < blen[b]) out[o++] = buf[start[b] + i];
    for (int i = 0; i < t.ec; i++)
      for (int b = 0; b < nb; b++) out[o++] = ecc[b][i];

    size = ver * 4 + 17;
    memset(mod, 0, sizeof(mod));
    memset(fn, 0, sizeof(fn));
    for (int i = 0; i < size; i++) { setF(6, i, i % 2 == 0); setF(i, 6, i % 2 == 0); }
    finder(3, 3);
    finder(size - 4, 3);
    finder(3, size - 4);
    int na = ver == 1 ? 0 : (ver < 7 ? 2 : 3);
    for (int i = 0; i < na; i++)
      for (int j = 0; j < na; j++) {
        if ((i == 0 && j == 0) || (i == 0 && j == na - 1) || (i == na - 1 && j == 0)) continue;
        align(QR_ALIGN[ver - 1][i], QR_ALIGN[ver - 1][j]);
      }
    format(ecl, 0);
    if (ver >= 7) version(ver);

    int bi = 0, bits = o * 8;
    for (int right = size - 1; right >= 1; right -= 2) {
      if (right == 6) right = 5;
      for (int vert = 0; vert < size; vert++)
        for (int j = 0; j < 2; j++) {
          int x = right - j;
          bool up = ((right + 1) & 2) == 0;
          int y = up ? size - 1 - vert : vert;
          if (!fn[y][x] && bi < bits) {
            mod[y][x] = (out[bi >> 3] >> (7 - (bi & 7))) & 1;
            bi++;
          }
        }
    }
    int best = 0;
    long bestP = 0x7FFFFFFF;
    for (int m = 0; m < 8; m++) {
      applyMask(m);
      format(ecl, m);
      long p = penalty();
      if (p < bestP) { bestP = p; best = m; }
      applyMask(m);
    }
    applyMask(best);
    format(ecl, best);
    return true;
  }
};
QrCode gQr;

// Draws a QR code fitting in a square of side px. Returns drawn side or 0.
static int drawQR(int x, int y, int px, const char* text) {
  if (!gQr.encode(text)) return 0;
  int n = gQr.size + 4;
  int m = max(1, px / n);
  int side = m * n;
  int ox = x + (px - side) / 2, oy = y + (px - side) / 2;
  cv->fillRect(ox, oy, side, side, 1);
  for (int yy = 0; yy < gQr.size; yy++)
    for (int xx = 0; xx < gQr.size; xx++)
      if (gQr.get(xx, yy)) cv->fillRect(ox + (xx + 2) * m, oy + (yy + 2) * m, m, m, 0);
  return side;
}
