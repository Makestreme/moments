# Moments

Free, open firmware for the **Seeed reTerminal Sticky** (ESP32-S3, 3.97" e-paper). The Sticky shows the right screen for where it is placed and the time of day. Everything runs on the device, with no cloud and no subscription.

**Install and guide:** https://YOUR-USERNAME.github.io/YOUR-REPO/

## Install

1. Open the site above in Chrome or Edge on a computer.
2. Plug in the Sticky with a USB-C data cable.
3. Click **Install Moments**. Tick "erase" for the first install only.

To update without a cable, use the console: **System › Firmware**, then upload `firmware/moments.bin`.

## What's in this folder

| Path | What |
|---|---|
| `index.html`, `style.css` | The site: web installer plus user guide |
| `manifest.json` | ESP Web Tools manifest (flash layout) |
| `firmware/` | The firmware binaries (see below) |
| `img/` | Photos for the site: `hero.jpg`, `setup.jpg`, `poses.jpg`, `apps.jpg`, `console.jpg` |

Any missing photo shows as a dashed placeholder. To show the demo video, set `data-youtube="VIDEO_ID"` on the `.video` div in `index.html`.

## Adding a firmware build

Build in Arduino IDE using the settings at the top of `Moments.ino` (ESP32S3 Dev Module, Flash 32MB, DIO, OPI PSRAM, USB CDC on boot disabled). Then run **Sketch › Export Compiled Binary** and copy these files into `firmware/`:

| From | To `firmware/` |
|---|---|
| `build/esp32.esp32.esp32s3/Moments.ino.bootloader.bin` | `bootloader.bin` |
| `build/esp32.esp32.esp32s3/Moments.ino.partitions.bin` | `partitions.bin` |
| `build/esp32.esp32.esp32s3/Moments.ino.bin` | `moments.bin` |
| `%LOCALAPPDATA%\Arduino15\packages\esp32\hardware\esp32\<version>\tools\partitions\boot_app0.bin` | `boot_app0.bin` |

Bump `version` in `manifest.json` and the version text in `index.html` to match.

> The `merged.bin` is not used on purpose. It would overwrite the settings partition (NVS at `0x9000`). Flashing the four parts separately keeps Wi-Fi, keys, settings and photos when "erase" is left unticked.

## Publish on GitHub Pages

1. Push this folder as the root of a repository.
2. Go to **Settings › Pages**, choose **Deploy from a branch**, then select `main` and `/ (root)`.
3. The site is live at `https://<user>.github.io/<repo>/`. The installer needs https, which Pages provides.

## Notes

- Unofficial firmware, not affiliated with Seeed Studio. Use at your own risk.
- Guardian is a door notifier, not a security system.
- The console uses plain HTTP on your local network and is protected by a PIN shown on the device.
