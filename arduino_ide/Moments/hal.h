// Moments firmware - hardware abstraction: power latch, I2C devices, buzzer, buttons.
#pragma once
#include "state.h"

// ---------------------------------------------------------------------------
// Power latch (74AHC1G79): D = GPIO45, CLK = GPIO46. Must run first thing at boot.
static void powerLatch(bool on) {
  pinMode(PIN_PWR_LOCK, OUTPUT);
  digitalWrite(PIN_PWR_LOCK, LOW);
  pinMode(PIN_PWR_HOLD, OUTPUT);
  digitalWrite(PIN_PWR_HOLD, on ? HIGH : LOW);
  delayMicroseconds(10);
  digitalWrite(PIN_PWR_LOCK, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_PWR_LOCK, LOW);
}

static void peripheralsSafeDefaults() {
  pinMode(PIN_MIC_EN, OUTPUT);
  digitalWrite(PIN_MIC_EN, LOW);
  pinMode(PIN_TP_EN, OUTPUT);
  digitalWrite(PIN_TP_EN, LOW);
  pinMode(PIN_EPD_EN, OUTPUT);
  digitalWrite(PIN_EPD_EN, LOW);
  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_BUZZER, LOW);
  pinMode(PIN_CHG_EN, INPUT);      // 100k pull-down keeps charging enabled
  pinMode(PIN_CHG_STAT, INPUT);
  pinMode(PIN_BTN_AI, INPUT);      // external 10k pull-ups
  pinMode(PIN_BTN_UP, INPUT);
  pinMode(PIN_BTN_DOWN, INPUT);
  pinMode(PIN_IMU_INT, INPUT);     // 10k pull-up R70
}

static bool usbPresent() {
  // VBUS/2 on GPIO9; 2.5 V is marginal as a digital high, so use the ADC.
  return analogReadMilliVolts(PIN_USB_IN) > 1500;
}

static bool chargingNow() { return usbPresent() && digitalRead(PIN_CHG_STAT) == LOW; }

static void setChargeEnabled(bool en) {
  if (en) {
    pinMode(PIN_CHG_EN, INPUT);
  } else {
    pinMode(PIN_CHG_EN, OUTPUT);
    digitalWrite(PIN_CHG_EN, HIGH);
  }
}

// ---------------------------------------------------------------------------
// I2C helpers
static bool i2cWrite(TwoWire& w, uint8_t addr, uint8_t reg, const uint8_t* d, size_t n) {
  w.beginTransmission(addr);
  w.write(reg);
  if (n) w.write(d, n);
  return w.endTransmission() == 0;
}
static bool i2cWrite8(TwoWire& w, uint8_t addr, uint8_t reg, uint8_t v) { return i2cWrite(w, addr, reg, &v, 1); }
static bool i2cRead(TwoWire& w, uint8_t addr, uint8_t reg, uint8_t* d, size_t n) {
  w.beginTransmission(addr);
  w.write(reg);
  if (w.endTransmission(false) != 0) return false;
  size_t got = w.requestFrom((uint16_t)addr, (size_t)n, true);
  if (got != n) return false;
  for (size_t i = 0; i < n; i++) d[i] = (uint8_t)w.read();
  return true;
}
static bool i2cProbe(TwoWire& w, uint8_t addr) {
  w.beginTransmission(addr);
  return w.endTransmission() == 0;
}

// ---------------------------------------------------------------------------
// Calendar math (UTC, no timegm in newlib)
static int64_t daysFromCivil(int y, unsigned m, unsigned d) {
  y -= m <= 2;
  const int64_t era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = (unsigned)(y - era * 400);
  const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + (int64_t)doe - 719468;
}
static time_t mkUtc(int y, int mo, int d, int h, int mi, int s) {
  return (time_t)(daysFromCivil(y, mo, d) * 86400LL + h * 3600 + mi * 60 + s);
}

// ---------------------------------------------------------------------------
// PCF8563 RTC (stores UTC)
static uint8_t bcd2bin(uint8_t v) { return (v >> 4) * 10 + (v & 0x0F); }
static uint8_t bin2bcd(uint8_t v) { return (uint8_t)(((v / 10) << 4) | (v % 10)); }
bool gRtcOk = false;
uint8_t gRtcFlagsAtBoot = 0;

static void rtcInit() {
  uint8_t c2 = 0;
  gRtcOk = i2cRead(Wire, ADDR_RTC, 0x01, &c2, 1);
  if (!gRtcOk) return;
  gRtcFlagsAtBoot = c2;
  i2cWrite8(Wire, ADDR_RTC, 0x00, 0x00);   // normal mode, clock running
  i2cWrite8(Wire, ADDR_RTC, 0x01, 0x00);   // clear AF/TF, disable AIE/TIE (INT releases power switch)
  i2cWrite8(Wire, ADDR_RTC, 0x0D, 0x00);   // CLKOUT off (saves ~150 uA via 10k pull-up)
  i2cWrite8(Wire, ADDR_RTC, 0x0E, 0x03);   // timer off, lowest-power source
  uint8_t off[4] = {0x80, 0x80, 0x80, 0x80};
  i2cWrite(Wire, ADDR_RTC, 0x09, off, 4);  // disable all alarms (stale alarms from old firmware)
}

static bool rtcRead(time_t& t) {
  uint8_t b[7];
  if (!i2cRead(Wire, ADDR_RTC, 0x02, b, 7)) return false;
  if (b[0] & 0x80) return false;  // VL: clock integrity lost
  int sec = bcd2bin(b[0] & 0x7F), mi = bcd2bin(b[1] & 0x7F), hr = bcd2bin(b[2] & 0x3F);
  int day = bcd2bin(b[3] & 0x3F), mon = bcd2bin(b[5] & 0x1F), yr = 2000 + bcd2bin(b[6]);
  if (yr < 2024 || mon < 1 || mon > 12 || day < 1 || day > 31 || hr > 23 || mi > 59 || sec > 59) return false;
  t = mkUtc(yr, mon, day, hr, mi, sec);
  return true;
}

static bool rtcWrite(time_t t) {
  struct tm g;
  gmtime_r(&t, &g);
  uint8_t b[7] = {bin2bcd(g.tm_sec), bin2bcd(g.tm_min), bin2bcd(g.tm_hour), bin2bcd(g.tm_mday),
                  (uint8_t)g.tm_wday, bin2bcd(g.tm_mon + 1), bin2bcd((uint8_t)(g.tm_year - 100))};
  return i2cWrite(Wire, ADDR_RTC, 0x02, b, 7);
}

// Timer in level mode (TI_TP = 0) so the power rail stays up until firmware latches it.
static void rtcSetWakeTimer(uint32_t secs) {
  uint8_t td, val;
  if (secs <= 255) { td = 0x02; val = (uint8_t)max<uint32_t>(secs, 1); }
  else { td = 0x03; val = (uint8_t)min<uint32_t>((secs + 59) / 60, 255); }
  i2cWrite8(Wire, ADDR_RTC, 0x0E, 0x00);
  i2cWrite8(Wire, ADDR_RTC, 0x0F, val);
  i2cWrite8(Wire, ADDR_RTC, 0x01, 0x01);   // TIE, flags cleared, level mode
  i2cWrite8(Wire, ADDR_RTC, 0x0E, (uint8_t)(0x80 | td));
}

// ---------------------------------------------------------------------------
// SHT40
static uint8_t shtCrc(const uint8_t* d) {
  uint8_t crc = 0xFF;
  for (int i = 0; i < 2; i++) {
    crc ^= d[i];
    for (int b = 0; b < 8; b++) crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x31) : (uint8_t)(crc << 1);
  }
  return crc;
}
static bool shtRead(float& t, float& h) {
  Wire.beginTransmission(ADDR_SHT40);
  Wire.write(0xFD);
  if (Wire.endTransmission() != 0) return false;
  delay(10);
  if (Wire.requestFrom((uint16_t)ADDR_SHT40, (size_t)6, true) != 6) return false;
  uint8_t b[6];
  for (int i = 0; i < 6; i++) b[i] = (uint8_t)Wire.read();
  if (shtCrc(b) != b[2] || shtCrc(b + 3) != b[5]) return false;
  uint16_t rt = (uint16_t)((b[0] << 8) | b[1]);
  uint16_t rh = (uint16_t)((b[3] << 8) | b[4]);
  t = -45.0f + 175.0f * rt / 65535.0f;
  h = -6.0f + 125.0f * rh / 65535.0f;
  if (h < 0) h = 0;
  if (h > 100) h = 100;
  return true;
}

// ---------------------------------------------------------------------------
// BQ27220 fuel gauge
static bool bqRead(uint8_t cmd, int16_t& v) {
  uint8_t b[2];
  if (!i2cRead(Wire, ADDR_BQ, cmd, b, 2)) return false;
  v = (int16_t)(b[0] | (b[1] << 8));
  return true;
}
static void batteryRead() {
  int16_t v;
  C.battOk = bqRead(0x2C, v);
  C.soc = C.battOk ? constrain((int)v, 0, 100) : -1;
  if (bqRead(0x08, v)) C.mv = (uint16_t)v;
  if (bqRead(0x0C, v)) C.ma = v;
  if (bqRead(0x14, v)) C.avgMa = v;
  if (bqRead(0x16, v)) C.tte = (uint16_t)v;
  C.usb = usbPresent();
  C.charging = C.usb && digitalRead(PIN_CHG_STAT) == LOW;
}

// ---------------------------------------------------------------------------
// LSM6DS3TR-C IMU
bool gImuOk = false;
static bool imuInit(bool cold) {
  uint8_t id = 0;
  if (!i2cRead(Wire, ADDR_IMU, 0x0F, &id, 1) || id != 0x6A) { gImuOk = false; return false; }
  if (cold) {
    i2cWrite8(Wire, ADDR_IMU, 0x12, 0x01);  // SW reset
    delay(20);
  }
  // BDU | IF_INC | H_LACTIVE (INT active low) | PP_OD (open drain): GPIO7 is shared and pulled up.
  i2cWrite8(Wire, ADDR_IMU, 0x12, 0x74);
  gImuOk = true;
  return true;
}
static void imuWakeConfig(uint8_t ths) {
  i2cWrite8(Wire, ADDR_IMU, 0x58, 0x81);               // INTERRUPTS_ENABLE | LIR (latched)
  i2cWrite8(Wire, ADDR_IMU, 0x5B, (uint8_t)(ths & 0x3F)); // 1 LSB = 31.25 mg at +-2 g
  i2cWrite8(Wire, ADDR_IMU, 0x5C, 0x00);
  i2cWrite8(Wire, ADDR_IMU, 0x5E, 0x20);               // wake-up -> INT1
}
static void imuActive(uint8_t ths) {
  if (!gImuOk) return;
  i2cWrite8(Wire, ADDR_IMU, 0x15, 0x00);  // XL high performance
  i2cWrite8(Wire, ADDR_IMU, 0x10, 0x40);  // XL 104 Hz, +-2 g
  i2cWrite8(Wire, ADDR_IMU, 0x11, 0x40);  // G 104 Hz, 250 dps
  imuWakeConfig(ths);
}
static uint8_t imuWakeSrc() {
  uint8_t s = 0;
  if (gImuOk) i2cRead(Wire, ADDR_IMU, 0x1B, &s, 1);  // reading clears the latch
  return s;
}
static void imuLowPower(uint8_t ths) {
  if (!gImuOk) return;
  i2cWrite8(Wire, ADDR_IMU, 0x11, 0x00);  // gyro off
  i2cWrite8(Wire, ADDR_IMU, 0x15, 0x10);  // XL_HM_MODE = 1 -> low power
  i2cWrite8(Wire, ADDR_IMU, 0x10, 0x20);  // XL 26 Hz
  imuWakeConfig(ths);
  delay(20);
  imuWakeSrc();
}
static bool imuAccel(float a[3]) {
  uint8_t b[6];
  if (!gImuOk || !i2cRead(Wire, ADDR_IMU, 0x28, b, 6)) return false;
  for (int i = 0; i < 3; i++) a[i] = (int16_t)(b[2 * i] | (b[2 * i + 1] << 8)) * 0.000061f;
  return true;
}
static bool imuGyro(float g[3]) {
  uint8_t b[6];
  if (!gImuOk || !i2cRead(Wire, ADDR_IMU, 0x22, b, 6)) return false;
  for (int i = 0; i < 3; i++) g[i] = (int16_t)(b[2 * i] | (b[2 * i + 1] << 8)) * 0.00875f;
  return true;
}

// ---------------------------------------------------------------------------
// Buzzer (magnetic, NMOS on GPIO48)
bool gBuzOn = false;
static void buzTone(uint32_t f, uint32_t ms, uint8_t vol) {
  if (!gBuzOn) { ledcAttach(PIN_BUZZER, 2700, 10); gBuzOn = true; }
  if (f == 0 || vol == 0) { ledcWrite(PIN_BUZZER, 0); delay(ms); return; }
  ledcWriteTone(PIN_BUZZER, f);
  ledcWrite(PIN_BUZZER, 8 + (uint32_t)min<uint8_t>(vol, 100) * 504 / 100);
  delay(ms);
  ledcWrite(PIN_BUZZER, 0);
}
static void buzEnd() {
  if (gBuzOn) { ledcWrite(PIN_BUZZER, 0); ledcDetach(PIN_BUZZER); gBuzOn = false; }
  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_BUZZER, LOW);
}

// ---------------------------------------------------------------------------
// Buttons (active low) + input queue
struct Btn { uint8_t pin; bool down; bool raw; uint32_t chg; uint32_t t0; uint8_t fired; };
Btn gBtn[3] = {{PIN_BTN_AI, false, false, 0, 0, 0}, {PIN_BTN_UP, false, false, 0, 0, 0}, {PIN_BTN_DOWN, false, false, 0, 0, 0}};
bool gHold3 = false, gHold8 = false;
InputEv gInQ[16];
uint8_t gInHead = 0, gInTail = 0;

static void pushInput(InKind k, int x = 0, int y = 0) {
  uint8_t n = (uint8_t)((gInHead + 1) % 16);
  if (n == gInTail) return;
  gInQ[gInHead] = {k, (int16_t)x, (int16_t)y};
  gInHead = n;
}
static bool popInput(InputEv& e) {
  if (gInTail == gInHead) return false;
  e = gInQ[gInTail];
  gInTail = (uint8_t)((gInTail + 1) % 16);
  return true;
}
static bool anyButtonDown() { return gBtn[0].down || gBtn[1].down || gBtn[2].down; }

static void buttonsTick() {
  uint32_t ms = millis();
  for (int i = 0; i < 3; i++) {
    Btn& b = gBtn[i];
    bool r = digitalRead(b.pin) == LOW;
    if (r != b.raw) { b.raw = r; b.chg = ms; }
    if (ms - b.chg >= 25 && r != b.down) {
      b.down = r;
      if (r) { b.t0 = ms; b.fired = 0; }
      else if (!b.fired) pushInput(i == 0 ? IN_BTN_AI : (i == 1 ? IN_BTN_UP : IN_BTN_DOWN));
    }
    if (b.down && i == 0) {
      if (b.fired < 1 && ms - b.t0 >= 3000) { b.fired = 1; gHold3 = true; }
      if (b.fired < 2 && ms - b.t0 >= 8000) { b.fired = 2; gHold8 = true; }
    }
  }
}
