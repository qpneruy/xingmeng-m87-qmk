# Xingmeng M87 (Attack Shark M87) Standalone QMK/VIA Firmware Repository

[![License: GPL v2](https://img.shields.io/badge/License-GPLv2-blue.svg)](https://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html)
[![SonixQMK Base](https://img.shields.io/badge/Base-SonixQMK%20sn32__develop-orange.svg)](https://github.com/SonixQMK/qmk_firmware)
[![VIA Compatible](https://img.shields.io/badge/VIA-v3%20Compatible-green.svg)](https://usevia.app)
[![Polling Rate](https://img.shields.io/badge/Polling%20Rate-500Hz%20(Native)-brightgreen.svg)](#)

A streamlined, standalone QMK/VIA firmware repository dedicated exclusively to the **Xingmeng M87** (internationally marketed as **Attack Shark M87**) tri-mode mechanical keyboard.

All other keyboard directories have been pruned from this repository to deliver a lightweight, single-keyboard workspace that compiles rapidly and supports VIA v3 out of the box.

---

## Hardware Overview

The Xingmeng M87 / Attack Shark M87 is a gasket-mounted, flex-cut ANSI TKL (87-key) mechanical keyboard powered by a dual-MCU architecture:

* **Primary Keyboard MCU**: **HFD2201KBA** (rebranded **SONiX SN32F248BF**, ARM Cortex-M0 @ 48MHz, 64KB Flash, 8KB SRAM, LQFP-64).
* **Wireless Coprocessor**: **WCH CH582F** (BLE 5.3 & 2.4GHz RF, interfaced via UART2 @ 115200 bps).
* **USB Polling Rate**: Native **500Hz (2ms)** configured in the USB descriptor (`bInterval = 2`) for low latency without CPU starvation.
* **Bootloader**: Hardware masked ROM ISP bootloader (`VID: 0x0C45, PID: 0x7040`).
* **Normal Operating Mode**: `VID: 0x0C45, PID: 0x8006`.

---

## Features

### 1. Full Tri-Mode Wireless & Low-Latency Bridge
* **Wired Mode**: Standard USB Type-C connection with full NKRO.
* **Bluetooth 5.3**: Supports 3 distinct host profiles (`BT1`, `BT2`, `BT3`) with rapid switching and pairing.
* **2.4GHz RF Dongle Mode**: Dedicated low-latency wireless connection.
* **Hardware Optimizations**:
  * Non-blocking boot state machine eliminates delays when plugging in the USB cable.
  * Direct 32-bit register reads on Port 0 (`SN_GPIO0->DATA`) for single-cycle slide switch debouncing.
  * Atomic status indicator register writes (`SN_GPIO0->BSET/BCLR`) prevent unnecessary bus transactions.
  * De Bruijn bit-scanning table (`ctz_lut`) accelerates wireless NKRO-to-6KRO report conversions.

### 2. Multi-Zone Independent Lighting Control
* **87 Key Switch RGB Matrix**: Full QMK RGB Matrix animation suite.
* **Independent Keylight Toggle (`Fn + Backspace`)**:
  * Shuts down all 87 key switch LEDs completely by driving the 17 column PWM channels to 0%.
  * **Leaves the side strips and logo lightbar running normally**, preserving ambient RGB glow while eliminating key glare.
* **Dual Side Underglow Strips (`Fn + RCtrl`)**: Left and Right strips (`B6` and `B9`) with hardware color correction (mirrored from matrix edges with zero extra RAM overhead).
* **Front Logo Lightbar (`Fn + End`)**: Dedicated toggleable lightbar on channel `B11`.
* **Battery Level Meter (`Fn + B` / `Fn + LAlt` / `Fn + RAlt`)**: Displays remaining battery charge on number keys `1..0` (10% increments in Red/Orange/Green for 3 seconds).
* **EEPROM Persistence**: Lighting and wireless states survive power cycles via wear-leveled flash emulation.

### 3. VIA Support
* Full support for VIA.

---

## Default Shortcuts (Fn Layer)

| Shortcut | Keycode | Function |
| :--- | :--- | :--- |
| **`Fn + Esc`** | `QK_BOOT` | Jump into ISP Bootloader mode |
| **`Fn + 1`** | `M87_BT1` | Bluetooth Profile 1 *(Hold 3s to Pair)* |
| **`Fn + 2`** | `M87_BT2` | Bluetooth Profile 2 *(Hold 3s to Pair)* |
| **`Fn + 3`** | `M87_BT3` | Bluetooth Profile 3 *(Hold 3s to Pair)* |
| **`Fn + 4`** | `M87_24G` | 2.4GHz RF Mode *(Hold 3s to Pair)* |
| **`Fn + Backspace`** | `KEYLIGHT_TOGG` | **Toggle 87 Key LEDs** *(Leaves Side Strips & Logo ON)* |
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

## Technical Pinout Reference

### Switch Matrix
* **Diode Direction**: `COL2ROW`
* **Rows (6)**: `D11, D10, D9, D8, D7, D6`
* **Columns (17)**: `C12, C11, C10, C9, C8, C7, C6, C5, C4, C3, C2, C1, C0, C15, B15, B13, B12`

### RGB Matrix & Strips
* **18 Row Pins (R, B, G)**: `{ A8, A9, A10, A11, A12, A13, A14, A15, B0, B1, B2, B3, B4, B7, B8, D3, B14, B10 }`
* **PWM Peripheral Outputs**:
  * Pin 40 (`B6` - PWM Ch 22): Left Side RGB Strip (8 LEDs).
  * Pin 43 (`B9` - PWM Ch 1): Right Side RGB Strip (8 LEDs).
  * Pin 45 (`B11` - PWM Ch 3): Logo Lightbar (6 LEDs).

### Wireless & Power Management
* **Slide Switch**: Pin 24 (`A7` / `P0.7`) = Bluetooth, Pin 23 (`A6` / `P0.6`) = 2.4GHz RF.
* **VBUS Detection**: Pin 19 (`A2` / `P0.2`) = 5V USB Sense (floating input).
* **Indicators**: Pin 20 (`A3` / `P0.3`) = CapsLock LED, Pin 21 (`A4` / `P0.4`) = WinLock LED.

---

## License

This project is licensed under the **GNU General Public License v2.0 or later** ([GPL-2.0-or-later](https://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html)).

Reverse engineering, firmware architecture, and hardware optimizations by [qpneruy](https://github.com/qpneruy).
