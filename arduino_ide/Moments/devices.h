// Moments firmware - SSD1677 e-paper driver and GT911 touch.
#pragma once
#include "hal.h"

// ---------------------------------------------------------------------------
// SSD1677 800x480 (sequence follows Seeed's seeed_epaper driver for the Sticky).
static uint8_t rev8(uint8_t v) {
  v = (uint8_t)((v >> 4) | (v << 4));
  v = (uint8_t)(((v & 0xCC) >> 2) | ((v & 0x33) << 2));
  v = (uint8_t)(((v & 0xAA) >> 1) | ((v & 0x55) << 1));
  return v;
}

class Epd {
 public:
  uint8_t* prev = nullptr;   // RAM-format frame currently on the panel
  uint8_t* ram = nullptr;    // RAM-format scratch frame
  bool prevValid = false;
  bool lastOk = true;

  void begin() {
    if (!prev) prev = (uint8_t*)psAlloc(EPD_BYTES);
    if (!ram) ram = (uint8_t*)psAlloc(EPD_BYTES);
    pinMode(PIN_EPD_EN, OUTPUT);
    digitalWrite(PIN_EPD_EN, LOW);
  }

  // Viewer-frame 1bpp buffer (MSB = left, 1 = white) -> controller RAM layout.
  void toRam(const uint8_t* src, uint8_t* dst) {
#if EPD_MIRROR_X
    for (int y = 0; y < EPD_H; y++) {
      const uint8_t* s = src + y * EPD_STRIDE;
      uint8_t* d = dst + y * EPD_STRIDE;
      for (int i = 0; i < EPD_STRIDE; i++) d[EPD_STRIDE - 1 - i] = rev8(s[i]);
    }
#else
    memcpy(dst, src, EPD_BYTES);
#endif
  }

  // mode: 0 = full, 1 = partial (falls back to full without a valid previous frame)
  bool showRam(const uint8_t* frame, int mode) {
    bool partial = (mode == 1) && prevValid;
    pinsOn();
    bool ok = initPanel(!partial);
    if (ok) {
      setWindow();
      cmd(0x26);
      data(partial ? prev : frame, EPD_BYTES);
      setWindow();
      cmd(0x24);
      data(frame, EPD_BYTES);
      cmd(0x22);
      data1(partial ? 0xFF : 0xF7);
      cmd(0x20);
      ok = waitBusy(partial ? 6000 : 15000);
    }
    sleepPanel();
    if (ok && frame != prev) memcpy(prev, frame, EPD_BYTES);
    prevValid = ok;
    lastOk = ok;
    if (!ok) LOGF("[epd] refresh failed (busy timeout)\n");
    return ok;
  }

  // 4-level gray: plane1 = high bit (0x24), plane2 = low bit (0x26), both RAM layout.
  bool showGray(const uint8_t* plane1, const uint8_t* plane2) {
    pinsOn();
    bool ok = initPanel(false);
    if (ok) {
      setWindow();
      cmd(0x24);
      data(plane1, EPD_BYTES);
      setWindow();
      cmd(0x26);
      data(plane2, EPD_BYTES);
      const uint8_t tc[2] = {0x67, 0x00};
      cmd(0x1A);
      data(tc, 2);
      cmd(0x22);
      data1(0xD7);
      cmd(0x20);
      ok = waitBusy(20000);
    }
    sleepPanel();
    prevValid = false;
    lastOk = ok;
    return ok;
  }

 private:
  bool spiOn = false;

  void pinsOn() {
    pinMode(PIN_EPD_CS, OUTPUT);
    digitalWrite(PIN_EPD_CS, HIGH);
    pinMode(PIN_EPD_DC, OUTPUT);
    digitalWrite(PIN_EPD_DC, HIGH);
    pinMode(PIN_EPD_RST, OUTPUT);
    digitalWrite(PIN_EPD_RST, HIGH);
    pinMode(PIN_EPD_BUSY, INPUT);
    digitalWrite(PIN_EPD_EN, HIGH);
    if (!spiOn) {
      SPI.begin(PIN_SPI_SCK, PIN_SPI_MISO, PIN_SPI_MOSI, -1);
      spiOn = true;
    }
    delay(10);
  }

  void pinsOff() {
    if (spiOn) { SPI.end(); spiOn = false; }
    // Inputs, so the switched-off panel is not back-powered through its IO pins.
    const uint8_t pins[] = {PIN_EPD_CS, PIN_EPD_DC, PIN_EPD_RST, PIN_SPI_SCK, PIN_SPI_MOSI};
    for (uint8_t p : pins) pinMode(p, INPUT);
    digitalWrite(PIN_EPD_EN, LOW);
  }

  void cmd(uint8_t c) {
    SPI.beginTransaction(SPISettings(EPD_SPI_HZ, MSBFIRST, SPI_MODE0));
    digitalWrite(PIN_EPD_DC, LOW);
    digitalWrite(PIN_EPD_CS, LOW);
    SPI.transfer(c);
    digitalWrite(PIN_EPD_CS, HIGH);
    digitalWrite(PIN_EPD_DC, HIGH);
    SPI.endTransaction();
  }
  void data(const uint8_t* d, size_t n) {
    SPI.beginTransaction(SPISettings(EPD_SPI_HZ, MSBFIRST, SPI_MODE0));
    digitalWrite(PIN_EPD_DC, HIGH);
    digitalWrite(PIN_EPD_CS, LOW);
    SPI.writeBytes(d, n);
    digitalWrite(PIN_EPD_CS, HIGH);
    SPI.endTransaction();
  }
  void data1(uint8_t v) { data(&v, 1); }

  bool waitBusy(uint32_t timeoutMs) {
    uint32_t t0 = millis();
    delay(1);
    while (digitalRead(PIN_EPD_BUSY) == HIGH) {
      if (millis() - t0 > timeoutMs) return false;
      delay(2);
    }
    return true;
  }

  void setWindow() {
    const uint8_t x[4] = {0x00, 0x00, (uint8_t)((EPD_W - 1) & 0xFF), (uint8_t)((EPD_W - 1) >> 8)};
    const uint8_t y[4] = {0x00, 0x00, (uint8_t)((EPD_H - 1) & 0xFF), (uint8_t)((EPD_H - 1) >> 8)};
    const uint8_t z[2] = {0x00, 0x00};
    cmd(0x44); data(x, 4);
    cmd(0x45); data(y, 4);
    cmd(0x4E); data(z, 2);
    cmd(0x4F); data(z, 2);
  }

  bool initPanel(bool full) {
    digitalWrite(PIN_EPD_RST, LOW);
    delay(10);
    digitalWrite(PIN_EPD_RST, HIGH);
    delay(10);
    if (!waitBusy(3000)) return false;
    if (full) {
      cmd(0x12);  // SW reset
      if (!waitBusy(3000)) return false;
    }
    cmd(0x18); data1(0x80);  // internal temperature sensor
    cmd(0x3C); data1(0x01);  // border waveform
    const uint8_t ss[5] = {0xAE, 0xC7, 0xC3, 0xC0, 0x80};
    cmd(0x0C); data(ss, 5);
    const uint8_t dout[3] = {(uint8_t)((EPD_H - 1) & 0xFF), (uint8_t)((EPD_H - 1) >> 8), 0x02};
    cmd(0x01); data(dout, 3);
    cmd(0x11); data1(0x03);  // X+, Y+
    setWindow();
    return waitBusy(3000);
  }

  void sleepPanel() {
    cmd(0x10);
    data1(0x03);
    delay(50);
    pinsOff();
  }
};
Epd epd;

// ---------------------------------------------------------------------------
// GT911 touch on Wire1. Reports into an 800x480 range on the Sticky.
struct TouchCal { bool swap = true, ix = true, iy = false; };
TouchCal gTouchCal;
int8_t gShownRot = 0;   // rotation of the frame currently on the panel (touch maps against it)

class Gt911 {
 public:
  uint8_t addr = 0;
  uint16_t xmax = 800, ymax = 480;

  bool begin() {
    pinMode(PIN_TP_EN, OUTPUT);
    digitalWrite(PIN_TP_EN, HIGH);
    // Seeed's driver waits 250 ms after enabling the load switch before touching the GT911.
    delay(250);
    Wire1.begin(PIN_TP_SDA, PIN_TP_SCL, 100000);
    addr = 0;
    for (uint8_t cand : {(uint8_t)0x5D, (uint8_t)0x14}) {
      resetFor(cand);
      if (i2cProbe(Wire1, cand)) { addr = cand; break; }
    }
    if (!addr) { LOGF("[touch] GT911 not found\n"); end(); return false; }
    uint8_t pid[4] = {0};
    rd(0x8140, pid, 4);
    LOGF("[touch] product id %c%c%c%c\n", pid[0], pid[1], pid[2], pid[3]);
    uint8_t r[4];
    if (rd(0x8048, r, 4)) {
      uint16_t xm = (uint16_t)(r[0] | (r[1] << 8)), ym = (uint16_t)(r[2] | (r[3] << 8));
      if (xm > 100 && ym > 100) { xmax = xm; ymax = ym; }
    }
    wr8(0x814E, 0);
    LOGF("[touch] GT911 @0x%02X range %ux%u\n", addr, xmax, ymax);
    return true;
  }

  void end() {
    Wire1.end();
    pinMode(PIN_TP_RST, INPUT);
    pinMode(PIN_TP_INT, INPUT);
    pinMode(PIN_TP_SDA, INPUT);
    pinMode(PIN_TP_SCL, INPUT);
    digitalWrite(PIN_TP_EN, LOW);
  }

  // -1 = no new report, 0 = released, 1 = touching (x/y raw)
  int poll(int& x, int& y) {
    uint8_t s;
    if (!rd(0x814E, &s, 1)) return -1;
    if (!(s & 0x80)) return -1;
    int n = s & 0x0F;
    int res = 0;
    if (n > 0 && n <= 5) {
      uint8_t p[8];
      if (rd(0x814F, p, 8)) {
        x = p[1] | (p[2] << 8);
        y = p[3] | (p[4] << 8);
        res = 1;
      }
    }
    wr8(0x814E, 0);
    return res;
  }

 private:
  // INT level at the RST rising edge selects the address: low -> 0x5D, high -> 0x14.
  void resetFor(uint8_t a) {
    pinMode(PIN_TP_RST, OUTPUT);
    pinMode(PIN_TP_INT, OUTPUT);
    digitalWrite(PIN_TP_RST, LOW);
    digitalWrite(PIN_TP_INT, a == 0x14 ? HIGH : LOW);
    delay(20);
    digitalWrite(PIN_TP_RST, HIGH);
    delay(20);
    digitalWrite(PIN_TP_INT, LOW);
    delay(50);
    pinMode(PIN_TP_INT, INPUT);
    delay(30);
  }
  bool rd(uint16_t reg, uint8_t* d, size_t n) {
    Wire1.beginTransmission(addr);
    Wire1.write((uint8_t)(reg >> 8));
    Wire1.write((uint8_t)(reg & 0xFF));
    if (Wire1.endTransmission(false) != 0) return false;
    if (Wire1.requestFrom((uint16_t)addr, (size_t)n, true) != n) return false;
    for (size_t i = 0; i < n; i++) d[i] = (uint8_t)Wire1.read();
    return true;
  }
  bool wr8(uint16_t reg, uint8_t v) {
    Wire1.beginTransmission(addr);
    Wire1.write((uint8_t)(reg >> 8));
    Wire1.write((uint8_t)(reg & 0xFF));
    Wire1.write(v);
    return Wire1.endTransmission() == 0;
  }
};
Gt911 gt;

// Raw touch -> logical coordinates of the frame currently shown.
static void touchToLogical(int rx, int ry, int& lx, int& ly) {
  float fx = constrain(rx / (float)gt.xmax, 0.0f, 1.0f);
  float fy = constrain(ry / (float)gt.ymax, 0.0f, 1.0f);
  float u = gTouchCal.swap ? fy : fx, v = gTouchCal.swap ? fx : fy;
  if (gTouchCal.ix) u = 1.0f - u;
  if (gTouchCal.iy) v = 1.0f - v;
  int ramx = (int)(u * (EPD_W - 1) + 0.5f), ramy = (int)(v * (EPD_H - 1) + 0.5f);
  int vx = EPD_MIRROR_X ? (EPD_W - 1 - ramx) : ramx, vy = ramy;
  switch (gShownRot) {
    case 1: lx = vy; ly = EPD_W - 1 - vx; break;
    case 2: lx = EPD_W - 1 - vx; ly = EPD_H - 1 - vy; break;
    case 3: lx = EPD_H - 1 - vy; ly = vx; break;
    default: lx = vx; ly = vy; break;
  }
}

struct Gesture { bool active; int sx, sy, lx, ly; uint32_t last; };
Gesture gGest = {false, 0, 0, 0, 0, 0};
uint32_t gLastTouchMs = 0;

static bool touchEnable() {
  if (gTouchOn) return true;
  gTouchOn = gt.begin();
  gGest.active = false;
  return gTouchOn;
}
static void touchDisable() {
  if (!gTouchOn) return;
  gt.end();
  gTouchOn = false;
}

static void touchTick() {
  if (!gTouchOn) return;
  static uint32_t lastPoll = 0;
  uint32_t ms = millis();
  if (ms - lastPoll < 20) return;
  lastPoll = ms;
  int rx = 0, ry = 0;
  int r = gt.poll(rx, ry);
  if (r == 1) {
    int lx, ly;
    touchToLogical(rx, ry, lx, ly);
    if (!gGest.active) { gGest.active = true; gGest.sx = lx; gGest.sy = ly; }
    gGest.lx = lx;
    gGest.ly = ly;
    gGest.last = ms;
    gLastTouchMs = ms;
  } else if (gGest.active && (r == 0 || ms - gGest.last > 200)) {
    gGest.active = false;
    int dx = gGest.lx - gGest.sx, dy = gGest.ly - gGest.sy;
    int adx = abs(dx), ady = abs(dy);
    if (max(adx, ady) >= 60) {
      if (adx * 5 > ady * 6) pushInput(dx < 0 ? IN_SWIPE_L : IN_SWIPE_R);
      else if (ady * 5 > adx * 6) pushInput(dy < 0 ? IN_SWIPE_U : IN_SWIPE_D);
    } else {
      pushInput(IN_TAP, gGest.sx, gGest.sy);
    }
  }
}
