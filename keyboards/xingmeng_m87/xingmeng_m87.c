// Copyright 2026 HuuTinh (@qpneruy)
// SPDX-License-Identifier: GPL-2.0-or-later

#include "xingmeng_m87.h"

bool sidelight_enabled = true;
bool lightbar_enabled  = true;
bool keylight_enabled  = true;
static uint32_t battery_show_timer = 0;
// Thời lượng 3 pha của vụ nổ Supernova (Tổng: 2000ms)
#define WAVE_DURATION_CHARGE  300   // Nung nóng lõi tại F8
#define WAVE_DURATION_EXPAND  1000  // Sóng Siêu Tân Tinh bung ra toàn phím
#define WAVE_DURATION_REVEAL  700   // Viền lửa vàng quét mở khóa hiệu ứng chính
#define WAVE_DURATION_TOTAL   (WAVE_DURATION_CHARGE + WAVE_DURATION_EXPAND + WAVE_DURATION_REVEAL)

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
    if (!keylight_enabled) {
        wave_active = false;
        return;
    }

    uint32_t elapsed = timer_elapsed32(wave_start_time);
    if (elapsed >= WAVE_DURATION_TOTAL) {
        wave_active = false;
        return;
    }

    uint8_t max_val = rgb_matrix_get_val();
    if (max_val < 130) {
        max_val = 130;
    }

    uint8_t  phase         = 0;
    uint16_t radius        = 0;
    uint8_t  reveal_radius = 0;
    uint8_t  charge_val    = 0;

    if (elapsed < WAVE_DURATION_CHARGE) {
        // PHA 0: Tụ năng lượng tại F8 (0 -> 255)
        phase      = 0;
        charge_val = (uint8_t)((elapsed * 255) / WAVE_DURATION_CHARGE);
    } else if (elapsed < (WAVE_DURATION_CHARGE + WAVE_DURATION_EXPAND)) {
        // PHA 1: Bung sóng đa tầng từ 0 -> 250 (Ease-Out)
        phase                = 1;
        uint32_t exp_elapsed = elapsed - WAVE_DURATION_CHARGE;
        uint16_t t           = (uint16_t)((exp_elapsed << 8) / WAVE_DURATION_EXPAND);
        uint16_t inv_t       = 255 - t;
        uint16_t ease_out    = 255 - ((inv_t * inv_t) >> 8);
        radius               = (ease_out * 250) >> 8;
    } else {
        // PHA 2: Sóng nền vẫn trôi nhẹ ra ngoài (250 -> 310) trong khi Viền Vàng quét mở khóa
        phase                = 2;
        uint32_t rev_elapsed = elapsed - (WAVE_DURATION_CHARGE + WAVE_DURATION_EXPAND);
        uint16_t t           = (uint16_t)((rev_elapsed << 8) / WAVE_DURATION_REVEAL);
        uint16_t inv_t       = 255 - t;
        uint16_t ease_out    = 255 - ((inv_t * inv_t) >> 8);
        radius               = 250 + ((t * 60) >> 8);
        reveal_radius        = (uint8_t)((ease_out * 245) >> 8);
    }

    for (uint8_t i = led_min; i < led_max; i++) {
        uint8_t d = led_dist_f8[i];
        uint8_t r = 0, g = 0, b = 0;

        if (phase == 0) {
            // Pha 0: Nung lõi F8 từ Xanh Băng -> Trắng Vàng
            uint8_t intensity = 0;
            if (d == 0) {
                intensity = charge_val;
            } else if (d < 42) {
                intensity = (uint8_t)(((uint16_t)charge_val * (42 - d)) >> 6);
            }
            if (intensity == 0) {
                rgb_matrix_set_color(i, 0, 0, 0);
                continue;
            }
            r = intensity;
            g = intensity;
            b = (intensity > 128) ? 255 : (intensity << 1);
        } else {
            // Nếu ở Pha 2 và vòng mở khóa đã đi qua -> Nhường chỗ cho Hiệu ứng chính!
            if (phase == 2 && d < reveal_radius) {
                continue;
            }

            if (d > radius + 14) {
                // Sóng chưa chạm tới -> Tắt đen
                rgb_matrix_set_color(i, 0, 0, 0);
                continue;
            } else if (d > radius) {
                // Mép trước của sóng (Leading Edge): Lóe sáng Trắng-Vàng
                uint8_t ahead = (uint8_t)(d - radius); // 1..14
                uint8_t fade  = 255 - (ahead * 18);
                r = fade;
                g = (uint8_t)(((uint16_t)fade * 240) >> 8);
                b = (uint8_t)(((uint16_t)fade * 180) >> 8);
            } else {
                /* ----------------------------------------------------------------
                 * ĐUÔI SÓNG ĐA TẦNG (Dựa vào khoảng cách sau đỉnh sóng: dist_behind)
                 * Không còn bị đọng ở màu Tím Đỏ mà chuyển mượt qua 4 vòng năng lượng:
                 * - Vòng 1 (0..35)   : Trắng Vàng -> Vàng Mặt Trời (Solar Gold)
                 * - Vòng 2 (36..83)  : Cam Lửa -> Dải Hồng Neon mỏng (Cyber Coral)
                 * - Vòng 3 (84..147) : Tím Điện -> Xanh Dương Vũ Trụ (Electric Blue)
                 * - Vòng 4 (> 147)   : Xanh Băng / Ngọc Bích (Ice Cyan Core)
                 * ---------------------------------------------------------------- */
                uint16_t dist_behind = radius - d;

                if (dist_behind <= 35) {
                    // Vòng 1: Dải Trắng Vàng -> Vàng Cam dày và rực
                    uint8_t p = (uint8_t)dist_behind; // 0..35
                    r = 255;
                    g = 245 - (p * 3);                // 245 -> 140
                    b = (p < 32) ? (192 - (p * 6)) : 0;
                } else if (dist_behind <= 83) {
                    // Vòng 2: Cam Lửa lướt nhanh qua Hồng Neon
                    uint8_t p = (uint8_t)(dist_behind - 36); // 0..47
                    r = 255 - (p >> 1);               // 255 -> 232
                    g = (p < 46) ? (140 - (p * 3)) : 0;
                    b = p * 3;                        // 0 -> 141
                } else if (dist_behind <= 147) {
                    // Vòng 3: Chuyển từ Hồng Tím sang Xanh Dương Vũ Trụ (Cyber Blue)
                    uint8_t p = (uint8_t)(dist_behind - 84); // 0..63
                    r = (p < 58) ? (232 - (p << 2)) : 0;
                    g = p << 1;                       // 0 -> 126
                    b = 141 + p + (p >> 1);           // 141 -> 235
                } else {
                    // Vòng 4: Mặt hồ Xanh Băng (Ice Cyan) lan ra từ tâm F8
                    uint16_t p = dist_behind - 148;
                    if (p > 63) {
                        p = 63;
                    }
                    r = 0;
                    g = (uint8_t)(126 + p + (p >> 1)); // 126 -> 220 (Cyan sáng dịu)
                    b = (uint8_t)(235 - (p >> 1));     // 235 -> 204
                }
            }

            // PHA 2: Hòa trộn (Alpha-Blend) Viền Lửa Vàng (Solar Gold Rim) đè lên nền Xanh Băng
            if (phase == 2 && d < reveal_radius + 28) {
                uint8_t rim   = 28 - (d - reveal_radius); // 1..28 (Càng sát mép mở khóa càng rực)
                uint8_t alpha = rim * 9;                  // Trọng số hòa trộn: 9..252
                uint8_t inv_a = 255 - alpha;

                // Màu của Viền Lửa Mở Khóa: Vàng Cam rực + Lõi Trắng ở sát mép trong
                uint8_t rim_g = 195 + (rim << 1);
                uint8_t rim_b = (rim > 18) ? ((rim - 18) * 22) : 0;

                r = (uint8_t)(((uint16_t)r * inv_a + (uint16_t)255 * alpha) >> 8);
                g = (uint8_t)(((uint16_t)g * inv_a + (uint16_t)rim_g * alpha) >> 8);
                b = (uint8_t)(((uint16_t)b * inv_a + (uint16_t)rim_b * alpha) >> 8);
            }
        }

        // Scale độ sáng cuối cùng theo cấu hình bàn phím
        r = (uint8_t)(((uint16_t)r * max_val) >> 8);
        g = (uint8_t)(((uint16_t)g * max_val) >> 8);
        b = (uint8_t)(((uint16_t)b * max_val) >> 8);

        rgb_matrix_set_color(i, r, g, b);
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
    uint32_t now = timer_read32();

    // Nếu đang chạy hiệu ứng mở phím -> Vẽ sóng Supernova, tạm ẩn báo Pin
    if (power_on_wave_is_active()) {
        power_on_wave_render(led_min, led_max);
    }
    // -------------------------------------------------------------
    // 1. Battery Indicator on Number Row (Keys 1..0: LED indices 17..26)
    // -------------------------------------------------------------
    else if (battery_show_timer != 0) {
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
