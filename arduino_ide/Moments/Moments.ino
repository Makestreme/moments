/*
  Moments - context-aware firmware for the Seeed reTerminal Sticky (ESP32-S3, 3.97" e-paper)

  Arduino IDE setup (esp32 by Espressif 3.3.x):
    Board ............ ESP32S3 Dev Module
    USB CDC On Boot .. Disabled          (serial is UART0 via the CH343P)
    CPU Frequency .... 240 MHz
    Flash Mode ....... DIO 80MHz
    Flash Size ....... 32MB (256Mb)
    Partition Scheme . any scheme with >= 3 MB app (e.g. "32M Flash (4.8MB APP/22MB LITTLEFS)" or "Huge APP");
                       the partitions.csv in this folder is used automatically
    PSRAM ............ OPI PSRAM
    Upload Speed ..... 921600 (460800 if uploads fail)
    Erase All Flash .. Enabled for the first upload only

  Libraries (Library Manager):
    - ArduinoJson by Benoit Blanchon (7.x)
    - Adafruit GFX Library (+ Adafruit BusIO when asked)
  Everything else (WiFi, WebServer, BLE, LittleFS, HTTPClient, Update, Preferences) ships with the core.

  First boot: the screen shows a Wi-Fi QR + PIN. Join "Moments-XXXX", open http://192.168.4.1, enter the PIN.
  Later: hold the AI button 3 s for setup mode (PIN + QR), 8 s to power off.
  Hold Up + Down while powering on for the service screen (diagnostics, orientation, factory reset).

  Code map (single translation unit, included in dependency order):
    config.h   pins, constants, includes         hal.h      power latch, I2C devices, buzzer, buttons
    devices.h  SSD1677 e-paper + GT911 touch     gfx.h      fonts, text layout, icons, QR codes
    store.h    config defaults, NVS secrets      timeutil.h time zones, dayparts, sun, moon
    net.h      Wi-Fi, NTP, HTTPS                 sources.h  weather, calendar, news, stocks, JSON
    ai.h       AI (Chef, outfit, summaries)      notify.h   sounds, notifications, BLE owners
    widgets.h  widget catalog                    apps.h     Chef, Lists, Timer, Gallery, Guardian
    engine.h   poses, Moments, automations       web.h      REST API (+ webui.h console)
    runtime.h  modes, sleep, service screen
*/
#include "config.h"
#include "state.h"
#include "runtime.h"

// HTTPS (mbedTLS) + JSON + drawing run on the loop task; the default 8 KB is tight.
SET_LOOP_TASK_STACK_SIZE(16 * 1024);

void setup() { momentsSetup(); }

void loop() { momentsLoop(); }
