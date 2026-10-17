// Moments firmware - compile-time configuration, pins and library includes.
#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <HTTPClient.h>
#include <Update.h>
#include <Preferences.h>
#include <LittleFS.h>
#include <ArduinoJson.h>       // Library Manager: "ArduinoJson" by Benoit Blanchon, v7.x
#include <Adafruit_GFX.h>      // Library Manager: "Adafruit GFX Library" (+ Adafruit BusIO)
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <esp_sleep.h>
#include <esp_system.h>
#include <esp_heap_caps.h>
#include <esp_sntp.h>
#include <mbedtls/aes.h>
#include <time.h>
#include <sys/time.h>
#include <vector>
#include <algorithm>

#define MOMENTS_VERSION "1.0.0"

// ---- Pins (verified against schematic, see PROJECT.md section 2) ----
#define PIN_PWR_HOLD   45
#define PIN_PWR_LOCK   46
#define PIN_BTN_AI     4
#define PIN_BTN_UP     5
#define PIN_BTN_DOWN   6
#define PIN_IMU_INT    7
#define PIN_USB_IN     9
#define PIN_SPI_MISO   12
#define PIN_SPI_SCK    13
#define PIN_SPI_MOSI   14
#define PIN_EPD_CS     15
#define PIN_EPD_DC     16
#define PIN_EPD_RST    17
#define PIN_EPD_BUSY   18
#define PIN_EPD_EN     47
#define PIN_I2C_SDA    1
#define PIN_I2C_SCL    0
#define PIN_TP_SDA     3
#define PIN_TP_SCL     2
#define PIN_TP_INT     21
#define PIN_TP_RST     41
#define PIN_TP_EN      42
#define PIN_MIC_EN     38
#define PIN_CHG_EN     39
#define PIN_CHG_STAT   40
#define PIN_BUZZER     48

// ---- I2C addresses ----
#define ADDR_SHT40     0x44
#define ADDR_IMU       0x6A
#define ADDR_RTC       0x51
#define ADDR_BQ        0x55

// ---- Display ----
#define EPD_W          800
#define EPD_H          480
#define EPD_STRIDE     (EPD_W / 8)
#define EPD_BYTES      (EPD_STRIDE * EPD_H)
// RAM X is mirrored relative to the viewer (derived from Seeed's 2048 app: rotation 270 + mirror_x).
// If everything (text AND touch) appears mirrored on your unit, set this to 0.
#define EPD_MIRROR_X   1
#define EPD_SPI_HZ     10000000

// ---- Misc ----
#define MAX_AUTOS      16
#define MAX_EVENTS     24
#define HIST_SLOTS     96
#define CFG_PATH       "/config.json"
#define LISTS_PATH     "/lists.json"
#define LAST_FRAME     "/last.bin"
#define AP_PREFIX      "Moments-"
#define MDNS_NAME      "moments"

#define LOGF(...) Serial.printf(__VA_ARGS__)
