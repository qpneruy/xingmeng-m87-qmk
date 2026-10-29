// Copyright 2026 HuuTinh (@qpneruy)
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "quantum.h"

// Operating Modes (Read from hardware slide switch: Pin 24/A7 & Pin 23/A6)
typedef enum {
    M87_MODE_WIRED = 0,
    M87_MODE_BT    = 1,
    M87_MODE_24G   = 2
} m87_mode_t;

// Bluetooth Profiles
typedef enum {
    M87_BT_PRF_1 = 1,
    M87_BT_PRF_2 = 2,
    M87_BT_PRF_3 = 3
} m87_bt_profile_t;

// Custom Keycodes for Keyboard / Wireless Control (using official QMK keyboard range)
enum m87_custom_keycodes {
    M87_BT1 = QK_KB_0,
    M87_BT2,
    M87_BT3,
    M87_24G,
    M87_BAT,
    SIDELIGHT_TOGG,
    LIGHTBAR_TOGG,
    BATTERY_LEVEL,
    KEYLIGHT_TOGG,
    M87_SAFE_RANGE = QK_KB_9
};

void ch582f_init(void);
void ch582f_task(void);
void ch582f_set_mode(m87_mode_t mode);
m87_mode_t ch582f_get_mode(void);
void ch582f_set_bt_profile(m87_bt_profile_t profile);
m87_bt_profile_t ch582f_get_bt_profile(void);
void ch582f_start_pairing(void);
bool ch582f_is_pairing(void);
uint8_t ch582f_get_battery_level(void);
void ch582f_poll_battery(void);
bool ch582f_is_vbus_connected(void);
bool ch582f_is_connected(void);
uint32_t ch582f_get_connected_time(void);
void ch582f_set_os_mode(bool is_windows);
bool ch582f_process_record(uint16_t keycode, keyrecord_t *record);
void power_on_wave_start(void);
bool power_on_wave_is_active(void);
