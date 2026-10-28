// Copyright 2026 HuuTinh (@qpneruy)
// SPDX-License-Identifier: GPL-2.0-or-later

#include "xingmeng_m87.h"

bool sidelight_enabled = true;
bool lightbar_enabled  = true;
bool keylight_enabled  = true;
static uint32_t battery_show_timer = 0;

void load_led_strip_settings(void) {
    if (eeconfig_is_enabled()) {
        uint32_t cfg = eeconfig_read_kb();
        sidelight_enabled = !(cfg & 0x04);
        lightbar_enabled  = !(cfg & 0x08);
        keylight_enabled  = !(cfg & 0x10);
    }
}

void save_led_strip_settings(void) {
    if (eeconfig_is_enabled()) {
        uint32_t cfg = eeconfig_read_kb() & ~0x1Cu;
        if (!sidelight_enabled) cfg |= 0x04;
        if (!lightbar_enabled)  cfg |= 0x08;
        if (!keylight_enabled)  cfg |= 0x10;
        eeconfig_update_kb(cfg);
    }
}

void keyboard_post_init_kb(void) {
    ch582f_init();
    load_led_strip_settings();
    keyboard_post_init_user();
}

void housekeeping_task_kb(void) {
    ch582f_task();
    housekeeping_task_user();
}

bool process_record_kb(uint16_t keycode, keyrecord_t *record) {
    if (!process_record_user(keycode, record) || !ch582f_process_record(keycode, record)) {
        return false;
    }

    if (record->event.pressed) {
        switch (keycode) {
            case SIDELIGHT_TOGG:
                sidelight_enabled = !sidelight_enabled;
                save_led_strip_settings();
                return false;
            case LIGHTBAR_TOGG:
                lightbar_enabled = !lightbar_enabled;
                save_led_strip_settings();
                return false;
            case BATTERY_LEVEL:
                ch582f_poll_battery();
                battery_show_timer = timer_read32();
                return false;
            case KEYLIGHT_TOGG:
                keylight_enabled = !keylight_enabled;
                save_led_strip_settings();
                return false;
        }
    }
    return true;
}

#if defined(RGB_MATRIX_ENABLE)
bool rgb_matrix_indicators_advanced_kb(uint8_t led_min, uint8_t led_max) {
    if (!rgb_matrix_indicators_advanced_user(led_min, led_max)) {
        return false;
    }

    uint32_t now = timer_read32();

    // -------------------------------------------------------------
    // 1. Battery Indicator on Number Row (Keys 1..0: LED indices 17..26)
    // -------------------------------------------------------------
    if (battery_show_timer != 0) {
        if ((uint32_t)(now - battery_show_timer) < 3000) {
            // Chỉ chạy khi dải render giao cắt với dải LED phím 1..0 (17..26)
            uint8_t start = (led_min > 17) ? led_min : 17;
            uint8_t end   = (led_max < 27) ? led_max : 27;

            if (start < end) {
                uint8_t pct = ch582f_get_battery_level();

                // Tính lit_count không dùng phép chia: (pct + 5) * 205 >> 11 tương đương (pct + 5) / 10
                uint8_t lit_count = (pct >= 95) ? 10 : (uint8_t)(((uint16_t)(pct + 5) * 205) >> 11);
                if (lit_count == 0) lit_count = 1;

                // Xác định màu 1 lần duy nhất bên ngoài vòng lặp
                uint8_t r = 0, g = 255;
                if (pct <= 20) {
                    r = 255; g = 0;        // Đỏ
                } else if (pct <= 50) {
                    r = 255; g = 180;      // Cam/Vàng
                }                          // > 50: Xanh lá (r=0, g=255)

                for (uint8_t idx = start; idx < end; idx++) {
                    uint8_t i = idx - 17;
                    if (i < lit_count) {
                        rgb_matrix_set_color(idx, r, g, 0);
                    } else {
                        rgb_matrix_set_color(idx, 0, 0, 0);
                    }
                }
            }
        } else {
            battery_show_timer = 0;
        }
    }

    // -------------------------------------------------------------
    // 2. Mode LED Indicator (Index 87, matrix [4, 14])
    // -------------------------------------------------------------
    if (87 >= led_min && 87 < led_max) {
        if (ch582f_get_mode() == M87_MODE_BT) {
            // Tra bảng màu tĩnh: BT1 = Blue, BT2 = Cyan, BT3 = Magenta (B luôn = 255)
            // M87_BT_PRF_1 = 1, M87_BT_PRF_2 = 2, M87_BT_PRF_3 = 3
            static const uint8_t bt_colors[4][2] = {
                {0,   0},   // 0: fallback (Blue)
                {0,   0},   // 1 (BT1): R=0,   G=0,   B=255 (Blue)
                {0,   255}, // 2 (BT2): R=0,   G=255, B=255 (Cyan)
                {255, 0}    // 3 (BT3): R=255, G=0,   B=255 (Magenta)
            };

            uint8_t prf = (uint8_t)ch582f_get_bt_profile();
            if (prf > 3) prf = 1;

            uint8_t r = bt_colors[prf][0];
            uint8_t g = bt_colors[prf][1];

            if (ch582f_is_pairing()) {
                // Nháy nhanh ~256ms (Dịch 8 bit thay vì chia 200) -> 1 cycle CPU
                bool on = (now >> 8) & 1;
                rgb_matrix_set_color(87, on ? r : 0, on ? g : 0, on ? 255 : 0);
            } else {
                uint32_t conn_time = ch582f_get_connected_time();
                uint32_t elapsed   = now - conn_time;

                if (!ch582f_is_connected()) {
                    if (elapsed < 5000) {
                        // Nháy chậm ~512ms (Dịch 9 bit thay vì chia 800) -> 1 cycle CPU
                        bool on = (now >> 9) & 1;
                        rgb_matrix_set_color(87, on ? r : 0, on ? g : 0, on ? 255 : 0);
                    } else {
                        rgb_matrix_set_color(87, 0, 0, 0);
                    }
                } else {
                    if (elapsed < 2000) {
                        rgb_matrix_set_color(87, r, g, 255);
                    } else {
                        rgb_matrix_set_color(87, 0, 0, 0);
                    }
                }
            }
        } else {
            rgb_matrix_set_color(87, 0, 0, 0);
        }
    }

    return true;
}
#endif
