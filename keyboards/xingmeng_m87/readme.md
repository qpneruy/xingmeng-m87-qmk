# Xingmeng M87

*A custom QMK firmware port for the Xingmeng M87 (Attack Shark M87) mechanical keyboard.*

* **Keyboard Maintainer**: [HuuTinh (@qpneruy)](https://github.com/qpneruy)
* **Hardware Supported**: Xingmeng M87 / Attack Shark M87 (SONiX SN32F248BF / HFD2201KBA)
* **Hardware Availability**: [Attack Shark](https://attackshark.com)

Make example for this keyboard (after setting up your build environment):

    make xingmeng_m87:default

Flashing example for this keyboard:

    make xingmeng_m87:default:flash

See the [build environment setup](https://docs.qmk.fm/#/getting_started_build_tools) and the [make instructions](https://docs.qmk.fm/#/getting_started_make_guide) for more information. Brand new to QMK? Start with our [Complete Newbs Guide](https://docs.qmk.fm/#/newbs).

## Bootloader

Enter the bootloader in 2 ways:

* **Bootmagic reset**: Hold down the key at (0,0) in the matrix (`Escape`) and plug in the keyboard
* **Keycode in layout**: Press the key mapped to `QK_BOOT` (`Fn + Esc` on Layer 1)
---

## Hardware Overview & Specifications

| Feature | Details |
| :--- | :--- |
| **Commercial Model** | Xingmeng M87 (Export name: Attack Shark M87) |
| **Layout** | ANSI TKL 87-key standard (Tenkeyless) |
| **Mounting Style** | Gasket Mount, Flex-cut PCB |
| **Microcontroller (MCU)** | SONiX SN32F248BF / HFD2201KBA (ARM Cortex-M0 @ 48MHz, 64KB Flash, 8KB SRAM, LQFP-64) |
| **Wireless Coprocessor** | WCH CH582F (BLE 5.3 & 2.4GHz RF) |
| **Bootloader Mode** | Hardware Masked ROM ISP Bootloader (`0x0C45:0x7040`) |
| **USB Mode** | `VID: 0x0C45`, `PID: 0x8006` |
| **Polling Rate** | 500Hz (2ms) |

---

## Pin Mapping

### Key Switch Matrix
* **Diode Direction**: `COL2ROW`
* **Rows (6)**: `D11, D10, D9, D8, D7, D6`
* **Cols (17)**: `C12, C11, C10, C9, C8, C7, C6, C5, C4, C3, C2, C1, C0, C15, B15, B13, B12`

### RGB Matrix & Side Strips
* **Architecture**: Common Anode (Col transistors, 18-channel PWM Rows `[R, B, G]`)
* **Row Pins (18)**: `{ A8, A9, A10, A11, A12, A13, A14, A15, B0, B1, B2, B3, B4, B7, B8, D3, B14, B10 }`
* **Side Strips Anode Pins**:
  * Pin 40 (`B6`): Left strip (8 LEDs)
  * Pin 43 (`B9`): Right strip (8 LEDs)
  * Pin 45 (`B11`): Logo strip (6 LEDs)

---

## Default Shortcuts (Layer 1)

| Shortcut | Keycode | Function |
| :--- | :--- | :--- |
| **`Fn + Esc`** | `QK_BOOT` | Jump to Bootloader |
| **`Fn + 1..3`** | `M87_BT1..3` | Bluetooth Profile 1/2/3 (Short press: switch, hold 3s: pair) |
| **`Fn + 4`** | `M87_24G` | 2.4GHz RF Mode (Short press: switch, hold 3s: pair) |
| **`Fn + B`** / **`Fn + LAlt`** / **`Fn + RAlt`** | `BATTERY_LEVEL` | Display Battery Level on 1..0 Number Row (3s) |
| **`Fn + Backspace`** | `KEYLIGHT_TOGG` | Toggle 87 Key LEDs (Keep Side Strips & Logo ON) |
| **`Fn + \`** | `RM_NEXT` | Next RGB Effect |
| **`Fn + Del`** | `RM_HUEU` | Cycle Hue |
| **`Fn + End`** | `LIGHTBAR_TOGG` | Toggle Logo Lightbar |
| **`Fn + RCtrl`** | `SIDELIGHT_TOGG` | Toggle Left & Right Side RGB Strips |
| **`Fn + Up`** / **`Fn + Down`** | `RM_VALU` / `RM_VALD` | Increase / Decrease RGB Brightness |
| **`Fn + Right`** / **`Fn + Left`** | `RM_SPDU` / `RM_SPDD` | Increase / Decrease RGB Effect Speed |
| **`Fn + LWin`** | `GU_TOGG` | Toggle Windows Key Lock |
| **`Fn + F1..F4`** | `KC_MYCM`..`KC_MSEL` | My Computer / Web Browser / Calculator / Media Player |
| **`Fn + F5..F8`** | Media Controls | Prev / Next / Play-Pause / Stop Track |
| **`Fn + F9..F11`** | Volume Controls | Mute / Volume Down / Volume Up |
