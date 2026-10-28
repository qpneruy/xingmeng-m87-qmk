# Xingmeng M87 (Attack Shark M87) Standalone QMK/VIA Firmware Repository

[![License: GPL v2](https://img.shields.io/badge/License-GPLv2-blue.svg)](https://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html)
[![SonixQMK Base](https://img.shields.io/badge/Base-SonixQMK%20sn32__develop-orange.svg)](https://github.com/SonixQMK/qmk_firmware)
[![VIA Compatible](https://img.shields.io/badge/VIA-v3%20Compatible-green.svg)](https://usevia.app)
[![Polling Rate](https://img.shields.io/badge/Polling%20Rate-500Hz%20(Native)-brightgreen.svg)](#)

A streamlined, standalone firmware repository for the **Xingmeng M87** (Attack Shark M87) mechanical keyboard with full **QMK & VIA support**, **Tri-Mode wireless** (Bluetooth 5.3, 2.4GHz RF, Wired USB), and independent multi-zone RGB lighting.

---

## Hardware Overview

The Xingmeng M87 / Attack Shark M87 is a gasket-mounted, flex-cut ANSI TKL (87-key) mechanical keyboard powered by a dual-MCU architecture:

* **Primary Keyboard MCU**: **HFD2201KBA** (rebranded **SONiX SN32F248BF**, ARM Cortex-M0 @ 48MHz, 64KB Flash, 8KB SRAM, LQFP-64).
* **Wireless Coprocessor**: **WCH CH582F** (BLE 5.3 & 2.4GHz RF, interfaced via UART2 @ 115200 bps).
* **USB Polling Rate**: Native **500Hz (2ms)** configured in the USB descriptor (`bInterval = 2`) for low latency without CPU starvation.
* **Bootloader**: Hardware masked ROM ISP bootloader (`VID: 0x0C45, PID: 0x7040`).
* **Normal Operating Mode**: `VID: 0x0C45, PID: 0x8006`.

---

## Default Shortcuts (Fn Layer)

| Shortcut | Keycode | Function |
| :--- | :--- | :--- |
| **`Fn + Esc`** | `QK_BOOT` | Jump into ISP Bootloader mode |
| **`Fn + 1`** | `M87_BT1` | Bluetooth Profile 1 *(Hold 3s to Pair)* |
| **`Fn + 2`** | `M87_BT2` | Bluetooth Profile 2 *(Hold 3s to Pair)* |
| **`Fn + 3`** | `M87_BT3` | Bluetooth Profile 3 *(Hold 3s to Pair)* |
| **`Fn + 4`** | `M87_24G` | 2.4GHz RF Mode *(Hold 3s to Pair)* |
| **`Fn + Backspace`** | `KEYLIGHT_TOGG` | Toggle 87 Key LEDs |
| **`Fn + Right Ctrl`** | `SIDELIGHT_TOGG` | Toggle Dual Side RGB Strips |
| **`Fn + End`** | `LIGHTBAR_TOGG` | Toggle Logo Lightbar |
| **`Fn + B` / `Fn + Alt`** | `BATTERY_LEVEL` | Display Battery Level on `1..0` Number Row |
| **`Fn + \`** | `RM_NEXT` | Next RGB Matrix effect |
| **`Fn + Del`** | `RM_HUEU` | Cycle RGB Hue color |
| **`Fn + Up` / `Fn + Down`** | `RM_VALU / RM_VALD` | Increase / Decrease Brightness |
| **`Fn + Right` / `Fn + Left`** | `RM_SPDU / RM_SPDD` | Increase / Decrease Animation Speed |
| **`Fn + Left Win`** | `GU_TOGG` | Toggle Windows Key Lock |
| **`Fn + F1 .. F4`** | Shortcuts | My Computer / Web Browser / Calculator / Media Player |
| **`Fn + F5 .. F8`** | Media Controls | Previous Track / Next Track / Play-Pause / Stop |
| **`Fn + F9 .. F11`** | Volume Controls | Audio Mute / Volume Down / Volume Up |

---

## Flashing Guide

Using **Sonix Flasher** (Linux / Windows):

1. Put the keyboard into ISP Bootloader mode:
   * **Software**: Press **`Fn + Esc`** while connected via USB.
   * **Hardware**: Unplug the USB cable, hold down the **`Escape`** key, plug in the cable, and release Escape.
2. Verify that the device is detected (`VID: 0x0C45, PID: 0x7040`):
   ```bash
   ./sonixflasher -c
   ```
3. Flash the pre-compiled VIA firmware:
   ```bash
   ./sonixflasher -v 0c45/7040 -f firmware_binaries/xingmeng_m87_via.bin
   ```
4. The flasher will verify checksums and reboot the keyboard into User Mode.

---

## Web Configuration via VIA

1. Open **[usevia.app](https://usevia.app)** in a Chromium-based browser (Chrome, Edge, Brave).
2. Go to **Settings** (gear icon) and enable **Show Design tab**.
3. Open the **Design** tab (paintbrush icon).
4. Click **Load** and select `keyboards/xingmeng_m87/xingmeng_m87_via.json`.
5. Switch to the **Configure** tab, click **Authorize device**, and select `xingmeng_m87`.

---

## Building from Source

Because this repository is already a standalone QMK environment, you can build directly:

```bash
# Compile default firmware (includes VIA v3 by default):
qmk compile -kb xingmeng_m87 -km default

# Flash directly after compile:
qmk flash -kb xingmeng_m87 -km default
```

The resulting binary will be generated at the repository root as `xingmeng_m87_default.bin`.

---

## Hardware Specifications

For the comprehensive hardware teardown, full 64-pin MCU pinout mapping, switch matrix schematic, and wireless UART protocol specifications, see:
* **[Complete Hardware Specification (docs/HARDWARE_SPEC.md)](docs/HARDWARE_SPEC.md)**

---

## License

This project is licensed under the **GNU General Public License v2.0 or later** ([GPL-2.0-or-later](https://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html)).

Reverse engineering, firmware architecture, and hardware optimizations by [qpneruy](https://github.com/qpneruy).
