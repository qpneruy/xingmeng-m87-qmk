// Copyright 2026 HuuTinh (@qpneruy)
// SPDX-License-Identifier: GPL-2.0-or-later

#include "xingmeng_m87.h"

bool sidelight_enabled = true;
bool lightbar_enabled  = true;
bool keylight_enabled  = true;
static uint32_t battery_show_timer = 0;
// Thời lượng chuẩn hóa theo lũy thừa nhị phân để triệt tiêu 100% phép chia (Tổng: 2048ms)
#define WAVE_DURATION_CHARGE  256   // 2^8 ms  -> Copy trực tiếp byte thấp
#define WAVE_DURATION_EXPAND  1024  // 2^10 ms -> Dịch phải 2 bit (>> 2)
#define WAVE_DURATION_REVEAL  768   // 3 * 256 ms -> Nhân nghịch đảo (* 85 >> 8)
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

static void power_on_wave_render(uint8_t led_min, uint8_t led_max, uint32_t now) {
    if (!keylight_enabled) {
        wave_active = false;
        return;
    }

    uint32_t elapsed = now - wave_start_time;
    if (elapsed >= WAVE_DURATION_TOTAL) {
        wave_active = false;
        return;
    }

    uint8_t max_val = rgb_matrix_get_val();
    if (max_val < 130) {
        max_val = 130;
    }

    // -------------------------------------------------------------------------
    // NHÁNH RIÊNG CHO PHA 0 (0..255ms): Không bắt 88 LED phải check if (phase == 0)
    // -------------------------------------------------------------------------
    if (elapsed < WAVE_DURATION_CHARGE) {
        uint8_t charge_val = (uint8_t)elapsed; // 0..255 (0 CPU cycle)
        for (uint8_t i = led_min; i < led_max; i++) {
            uint8_t d = led_dist_f8[i];
            if (d >= 42) {
                rgb_matrix_set_color(i, 0, 0, 0);
                continue;
            }
            uint8_t intensity = (d == 0) ? charge_val : (uint8_t)(((uint16_t)charge_val * (42 - d)) >> 6);
            uint8_t r         = (uint8_t)(((uint16_t)intensity * max_val) >> 8);
            uint8_t b_raw     = (intensity > 127) ? 255 : (intensity << 1);
            uint8_t b         = (uint8_t)(((uint16_t)b_raw * max_val) >> 8);
            rgb_matrix_set_color(i, r, r, b);
        }
        return;
    }

    // -------------------------------------------------------------------------
    // TÍNH THÔNG SỐ CHO PHA 1 & PHA 2 (0% Phép chia)
    // -------------------------------------------------------------------------
    bool     is_phase2     = false;
    uint16_t radius        = 0;
    uint8_t  reveal_radius = 0;

    if (elapsed < (WAVE_DURATION_CHARGE + WAVE_DURATION_EXPAND)) {
        // PHA 1: exp_elapsed chạy 0..1023 -> dịch phải 2 bit ra 0..255
        uint16_t t        = (uint16_t)((elapsed - WAVE_DURATION_CHARGE) >> 2);
        uint16_t inv_t    = 255 - t;
        uint16_t ease_out = 255 - ((inv_t * inv_t) >> 8);
        radius            = (ease_out * 250) >> 8;
    } else {
        // PHA 2: rev_elapsed chạy 0..767 -> nhân 85 >> 8 tương đương chia 3 (ra 0..254)
        is_phase2            = true;
        uint32_t rev_elapsed = elapsed - (WAVE_DURATION_CHARGE + WAVE_DURATION_EXPAND);
        uint16_t t           = (uint16_t)((rev_elapsed * 85) >> 8);
        uint16_t inv_t       = 255 - t;
        uint16_t ease_out    = 255 - ((inv_t * inv_t) >> 8);
        radius               = 250 + ((t * 60) >> 8);
        reveal_radius        = (uint8_t)((ease_out * 245) >> 8);
    }

    for (uint8_t i = led_min; i < led_max; i++) {
        uint8_t d = led_dist_f8[i];

        // Pha 2: Vùng đã mở khóa -> Bỏ qua ngay lập tức (nhường cho hiệu ứng chính)
        if (is_phase2 && d < reveal_radius) {
            continue;
        }

        // Vùng sóng chưa chạm tới -> Tắt đen (Độ rộng mép trước = 16 để dùng << 4)
        if (d >= radius + 16) {
            rgb_matrix_set_color(i, 0, 0, 0);
            continue;
        }

        uint8_t r, g, b;

        if (d > radius) {
            // Mép trước của sóng (ahead: 1..15) -> Dịch bit thuần túy, không nhân
            uint8_t fade = (uint8_t)((16 - (d - radius)) << 4); // 16..240
            r            = fade;
            g            = fade - (fade >> 4); // Xấp xỉ fade * 240 / 256
            b            = fade - (fade >> 2); // Xấp xỉ fade * 192 / 256
        } else {
            /* ----------------------------------------------------------------
             * ĐUÔI SÓNG 4 VÒNG CHUẨN HÓA LŨY THỪA 2 (32, 64, 64, 64)
             * Liên tục 100% tại mọi điểm nối, 0 lệnh rẽ nhánh kẹp biên (clamp)
             * ---------------------------------------------------------------- */
            uint16_t dist_behind = radius - d;

            if (dist_behind < 32) {
                // Vòng 1 (0..31): Trắng Vàng (252,252,186) -> Vàng Cam (252,128,0)
                uint8_t p = (uint8_t)dist_behind;
                r         = 252;
                g         = 252 - (p << 2);
                b         = 186 - (p << 2) - (p << 1); // 186 - p * 6
            } else if (dist_behind < 96) {
                // Vòng 2 (32..95): Vàng Cam (252,126,0) -> Hồng Neon (252,0,126)
                uint8_t p = (uint8_t)(dist_behind - 32); // 0..63
                r         = 252;
                g         = 126 - (p << 1);
                b         = p << 1;
            } else if (dist_behind < 160) {
                // Vòng 3 (96..159): Hồng Neon (252,0,126) -> Xanh Vũ Trụ (0,126,252)
                uint8_t p = (uint8_t)(dist_behind - 96); // 0..63
                r         = (63 - p) << 2;
                g         = p << 1;
                b         = 126 + (p << 1);
            } else {
                // Vòng 4 (>= 160): Xanh Vũ Trụ (0,126,252) -> Xanh Băng (0,220,221)
                uint16_t p = dist_behind - 160;
                if (p > 63) {
                    p = 63;
                }
                r = 0;
                g = (uint8_t)(126 + p + (p >> 1));
                b = (uint8_t)(252 - (p >> 1));
            }
        }

        // PHA 2: Hòa trộn Viền Lửa Vàng (độ rộng 32 = 2^5) bằng công thức Lerp >> 5
        if (is_phase2 && d < reveal_radius + 32) {
            uint8_t rim   = 32 - (d - reveal_radius); // 1..32
            uint8_t rim_g = 190 + (rim << 1);         // 192..254
            uint8_t rim_b = (rim > 16) ? ((rim - 16) << 4) : 0;

            r += (uint8_t)(((255 - r) * rim) >> 5);
            g += (uint8_t)(((int16_t)(rim_g - g) * rim) >> 5);
            b += (uint8_t)(((int16_t)(rim_b - b) * rim) >> 5);
        }

        // Scale độ sáng tổng
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

    if (wave_active) {
        power_on_wave_render(led_min, led_max, now);
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
