// Copyright 2026 HuuTinh (@qpneruy)
// SPDX-License-Identifier: GPL-2.0-or-later

#include "xingmeng_m87.h"

bool sidelight_enabled = true;
bool lightbar_enabled  = true;
bool keylight_enabled  = true;
static uint32_t battery_show_timer = 0;
#define WAVE_DURATION_OUTWARD 1350
#define WAVE_DURATION_CREST   180
#define WAVE_DURATION_REWIND  1350
#define WAVE_DURATION_BURST   300
#define WAVE_DURATION_TOTAL   (WAVE_DURATION_OUTWARD + WAVE_DURATION_CREST + WAVE_DURATION_REWIND + WAVE_DURATION_BURST)

static uint32_t wave_start_time = 0;
static bool     wave_active     = false;

/* Precomputed distance table from F8 (Index 8) for all 88 LEDs (scaled 0..240) */
static const uint8_t led_dist_f8[88] = {
    182, 159, 137, 114,  91,  68,  46,  23,   0,  23,  46,  68,  91, 137, 159, 182,
    185, 163, 140, 118,  96,  75,  56,  39,  32,  39,  56,  75,  96, 118, 140, 163, 185,
    193, 172, 151, 130, 111,  93,  78,  67,  63,  64,  78,  93, 111, 130, 151, 172, 193,
    204, 184, 165, 147, 130, 115, 103,  95,  93,  95, 103, 115, 147,
    210, 185, 169, 154, 142, 133, 127, 124, 127, 133, 142, 169, 202,
    240, 223, 208, 163, 163, 170, 181, 193, 208, 223, 240,
    185
};

/* Cosine bell curve table: cos^2(x * pi / 2) for x in 0..1 (32 entries, values 255..0) */
static const uint8_t cos2_lut[32] = {
    255, 254, 252, 249, 245, 239, 232, 224,
    215, 206, 195, 184, 172, 159, 147, 134,
    121, 108,  96,  83,  71,  60,  49,  40,
     31,  23,  16,  10,   6,   3,   1,   0
};

/* Trailing persistence decay table: exp(-i / 8) * 0.35 * 255 (32 entries, values 89..2) */
static const uint8_t trail_lut[32] = {
    89, 79, 70, 61, 54, 48, 42, 37,
    33, 29, 26, 23, 20, 18, 16, 14,
    12, 11,  9,  8,  7,  6,  6,  5,
     4,  4,  3,  3,  3,  2,  2,  2
};

void power_on_wave_start(void) {
    wave_start_time = timer_read32();
    wave_active     = true;
}

bool power_on_wave_is_active(void) {
    if (!wave_active) {
        return false;
    }
    if (timer_elapsed32(wave_start_time) >= WAVE_DURATION_TOTAL) {
        wave_active = false;
        return false;
    }
    return true;
}

static void power_on_wave_render(uint8_t led_min, uint8_t led_max) {
    uint32_t elapsed = timer_elapsed32(wave_start_time);
    if (elapsed >= WAVE_DURATION_TOTAL) {
        wave_active = false;
        return;
    }

    uint8_t radius = 0;
    uint8_t phase  = 0; /* 0 = outward, 1 = crest, 2 = rewind, 3 = burst */
    uint8_t burst_intensity = 0;

    if (elapsed < WAVE_DURATION_OUTWARD) {
        phase  = 0;
        radius = (uint8_t)(((uint32_t)elapsed * 240) / WAVE_DURATION_OUTWARD);
    } else if (elapsed < (WAVE_DURATION_OUTWARD + WAVE_DURATION_CREST)) {
        phase  = 1;
        radius = 240;
    } else if (elapsed < (WAVE_DURATION_OUTWARD + WAVE_DURATION_CREST + WAVE_DURATION_REWIND)) {
        phase  = 2;
        uint32_t rewind_elapsed = elapsed - (WAVE_DURATION_OUTWARD + WAVE_DURATION_CREST);
        radius = (uint8_t)(240 - ((rewind_elapsed * 240) / WAVE_DURATION_REWIND));
    } else {
        phase = 3;
        uint32_t burst_elapsed = elapsed - (WAVE_DURATION_OUTWARD + WAVE_DURATION_CREST + WAVE_DURATION_REWIND);
        burst_intensity = (uint8_t)(255 - ((burst_elapsed * 255) / WAVE_DURATION_BURST));
    }

    const uint8_t wave_thickness = 45;

    for (uint8_t i = led_min; i < led_max; i++) {
        if (phase == 3) {
            /* Focal absorption burst at F8 (Index 8) */
            if (i == 8) {
                rgb_matrix_set_color(8, 0, burst_intensity, (uint8_t)(((uint16_t)burst_intensity * 136) >> 8));
            } else {
                rgb_matrix_set_color(i, 0, 0, 0);
            }
            continue;
        }

        uint8_t d     = led_dist_f8[i];
        uint8_t delta = (d > radius) ? (d - radius) : (radius - d);
        uint8_t intensity = 0;

        if (delta < wave_thickness) {
            uint8_t idx = (uint8_t)(((uint16_t)delta * 31) / wave_thickness);
            intensity   = cos2_lut[idx];
        }

        /* Trailing persistence */
        if (phase == 0 && radius > d) {
            uint8_t trail_d = (radius - d) / 3;
            if (trail_d < 32 && trail_lut[trail_d] > intensity) {
                intensity = trail_lut[trail_d];
            }
        } else if (phase == 2 && radius < d) {
            uint8_t trail_d = (d - radius) / 3;
            if (trail_d < 32 && trail_lut[trail_d] > intensity) {
                intensity = trail_lut[trail_d];
            }
        }

        if (intensity > 4) {
            /* Emerald neon: R = 0, G = 255, B = 136 */
            uint8_t g = (uint8_t)(((uint16_t)intensity * 255) >> 8);
            uint8_t b = (uint8_t)(((uint16_t)intensity * 136) >> 8);
            rgb_matrix_set_color(i, 0, g, b);
        } else {
            rgb_matrix_set_color(i, 0, 0, 0);
        }
    }
}

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
    power_on_wave_start();
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
        if (get_highest_layer(layer_state) == 1 && keycode == KC_F8) {
            power_on_wave_start();
            return false;
        }
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

    if (power_on_wave_is_active()) {
        power_on_wave_render(led_min, led_max);
        return true;
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
