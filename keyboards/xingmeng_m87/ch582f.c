// Copyright 2026 HuuTinh (@qpneruy)
// SPDX-License-Identifier: GPL-2.0-or-later
// Hardware-accelerated CH582F wireless bridge for SN32F240B (Cortex-M0)

#include "ch582f.h"
#include <string.h>
#include "host.h"
#include "host_driver.h"
#include "SN32F240B.h"
#include "wait.h"
#include "eeconfig.h"

// Hardware Pin Bitmasks (Port 0)
// P0.2 = VBUS, P0.3 = CapsLock, P0.4 = WinLock, P0.6 = 2.4G, P0.7 = BT
#define MASK_PIN_SWITCH_24G  (1u << 6)
#define MASK_PIN_SWITCH_BT   (1u << 7)
#define MASK_PIN_VBUS        (1u << 2)
#define MASK_LED_CAPS        (1u << 3)
#define MASK_LED_WINLOCK     (1u << 4)

// State variables
static m87_mode_t       current_mode        = M87_MODE_WIRED;
static m87_bt_profile_t current_bt_profile  = M87_BT_PRF_1;
static uint8_t          battery_percent     = 100;
static bool             is_connected        = false;
static bool             is_pairing          = false;
static uint32_t         pairing_start_time  = 0;
static uint32_t         connected_time      = 0;
static uint8_t          host_led_state      = 0;
static host_driver_t   *default_host_driver = NULL;
static uint8_t          switch_debounce_counter = 0;
static uint8_t          last_switch_raw     = 0xFF;
static uint16_t         held_wireless_key   = 0;
static uint32_t         wireless_key_timer  = 0;
static uint8_t          boot_init_state     = 0;
static uint32_t         boot_time           = 0;
static uint8_t          last_led_bits       = 0xFF;

// Driver callbacks
static uint8_t ch582f_driver_leds(void);
static void ch582f_driver_send_keyboard(report_keyboard_t *report);
static void ch582f_driver_send_nkro(report_nkro_t *report);
static void ch582f_driver_send_mouse(report_mouse_t *report);
static void ch582f_driver_send_extra(report_extra_t *report);
#ifdef RAW_ENABLE
static void ch582f_driver_send_raw_hid(uint8_t *data, uint8_t length);
#endif

static host_driver_t ch582f_host_driver = {
    ch582f_driver_leds,
    ch582f_driver_send_keyboard,
    ch582f_driver_send_nkro,
    ch582f_driver_send_mouse,
    ch582f_driver_send_extra
#ifdef RAW_ENABLE
    , ch582f_driver_send_raw_hid
#endif
};

// -------------------------------------------------------------
// Low-Level Fast UART Helpers
// -------------------------------------------------------------
static inline void uart2_write_byte_fast(uint8_t b) {
    uint32_t timeout = 50000;
    while (!(SN_UART2->LS & (1 << 5)) && --timeout);
    SN_UART2->TH = b;
}

static inline void uart2_write_buf_fast(const uint8_t *data, uint8_t len) {
    for (uint8_t i = 0; i < len; i++) {
        uart2_write_byte_fast(data[i]);
    }
}

static inline bool uart2_read_byte_fast(uint8_t *b) {
    if (SN_UART2->LS & 1) {
        *b = (uint8_t)(SN_UART2->RB & 0xFF);
        return true;
    }
    return false;
}

static void uart2_init(void) {
    *(volatile uint32_t *)0x40042000 = 0x009FFFBA;
    SN_SYS0->SWDCTRL |= 1;
    SN_SYS1->AHBCLKEN |= (1 << 18);
    SN_GPIO0->BSET = 3u;

    SN_UART2->LC = 0x83;
    SN_UART2->FD = 0;
    SN_UART2->DLM = 0;
    SN_UART2->DLL = 26; // 48MHz / (16 * 26) = 115200 bps
    SN_UART2->LC = 0x03;
    SN_UART2->FIFOCTRL = 0x07;
    SN_UART2->CTRL = 0xC1;

    SN_GPIO0->MODE &= ~0x00000003;
    SN_GPIO0->CFG &= ~0x0000000F;
    wait_ms(10);
}

// -------------------------------------------------------------
// Core Public APIs
// -------------------------------------------------------------
void ch582f_init(void) {
    default_host_driver = host_get_driver();

    // P0.2, P0.6, P0.7 = Inputs
    // P0.6 & P0.7 = Pull-up enabled (00b), P0.2 (VBUS) = Floating input without Pull-up (10b)
    SN_GPIO0->MODE &= ~(MASK_PIN_SWITCH_BT | MASK_PIN_SWITCH_24G | MASK_PIN_VBUS);
    SN_GPIO0->CFG   = (SN_GPIO0->CFG & ~((3u << 14) | (3u << 12) | (3u << 4))) | (2u << 4);

    // P0.3, P0.4 = Output Push-Pull, initial Low
    SN_GPIO0->BCLR  = (MASK_LED_CAPS | MASK_LED_WINLOCK);
    SN_GPIO0->MODE |= (MASK_LED_CAPS | MASK_LED_WINLOCK);
    SN_GPIO0->CFG  &= ~((3u << 6) | (3u << 8));

    keymap_config.swap_lalt_lgui = false;
    keymap_config.swap_ralt_rgui = false;

    uart2_init();

    if (eeconfig_is_enabled()) {
        uint8_t saved_prf = eeconfig_read_kb() & 0x03;
        if (saved_prf >= M87_BT_PRF_1 && saved_prf <= M87_BT_PRF_3) {
            current_bt_profile = (m87_bt_profile_t)saved_prf;
        }
    }

    boot_time = timer_read32();
    boot_init_state = 0;

    // Single-cycle Port0 read for initial switch state
    uint32_t p0 = SN_GPIO0->DATA;
    if (!(p0 & MASK_PIN_SWITCH_BT)) {
        ch582f_set_mode(M87_MODE_BT);
    } else if (!(p0 & MASK_PIN_SWITCH_24G)) {
        ch582f_set_mode(M87_MODE_24G);
    } else {
        ch582f_set_mode(M87_MODE_WIRED);
    }
}

void ch582f_set_mode(m87_mode_t mode) {
    current_mode = mode;
    is_pairing = false;
    is_connected = false;

    uint8_t pkt[3];
    pkt[0] = 0xA6;

    if (mode == M87_MODE_WIRED) {
        keymap_config.nkro = true;
        if (default_host_driver) {
            host_set_driver(default_host_driver);
        }
        pkt[1] = 0x11;
        pkt[2] = (uint8_t)(0xA6 + 0x11);
    } else {
        keymap_config.nkro = false;
        host_set_driver(&ch582f_host_driver);
        pkt[1] = (mode == M87_MODE_24G) ? 0x30 : (0x30 + (uint8_t)current_bt_profile);
        pkt[2] = (uint8_t)(0xA6 + pkt[1]);
    }
    uart2_write_buf_fast(pkt, 3);
}

m87_mode_t ch582f_get_mode(void) {
    return current_mode;
}

void ch582f_set_bt_profile(m87_bt_profile_t profile) {
    current_bt_profile = profile;
    is_pairing = false;
    is_connected = false;

    if (eeconfig_is_enabled()) {
        uint32_t cfg = (eeconfig_read_kb() & ~0x03) | (profile & 0x03);
        eeconfig_update_kb(cfg);
    }

    if (current_mode == M87_MODE_BT) {
        uint8_t pkt[3] = {0xA6, (uint8_t)(0x30 + profile), (uint8_t)(0xA6 + 0x30 + profile)};
        uart2_write_buf_fast(pkt, 3);
    }
}

m87_bt_profile_t ch582f_get_bt_profile(void) {
    return current_bt_profile;
}

void ch582f_start_pairing(void) {
    is_pairing = true;
    is_connected = false;
    pairing_start_time = timer_read32();

    static const uint8_t vpid_pkt[6]  = {0xAD, 0x45, 0x0C, 0x06, 0x80, 0x84};
    static const uint8_t name_pkt[15] = {0xA9, 12, 'M', '8', '7', ' ', 'K', 'e', 'y', 'b', 'o', 'a', 'r', 'd', 0xC2};
    static const uint8_t win_pkt[3]   = {0xA5, 0x10, 0xB5};
    static const uint8_t pair_pkt[3]  = {0xA6, 0x51, 0xF7};

    uart2_write_buf_fast(vpid_pkt, 6);
    wait_ms(10);
    uart2_write_buf_fast(name_pkt, 15);
    wait_ms(10);

    uint8_t prof_pkt[3] = {0xA6, (uint8_t)(0x30 + current_bt_profile), (uint8_t)(0xA6 + 0x30 + current_bt_profile)};
    uart2_write_buf_fast(prof_pkt, 3);
    wait_ms(20);
    uart2_write_buf_fast(win_pkt, 3);
    wait_ms(10);

    for (uint8_t i = 0; i < 3; i++) {
        uart2_write_buf_fast(pair_pkt, 3);
        wait_ms(20);
    }
}

bool ch582f_is_pairing(void) {
    return is_pairing;
}

uint8_t ch582f_get_battery_level(void) {
    return battery_percent;
}

void ch582f_poll_battery(void) {
    if (current_mode == M87_MODE_WIRED) return;
    static const uint8_t pkt[3] = {0xA6, 0x53, 0xF9};
    uart2_write_buf_fast(pkt, 3);
}

bool ch582f_is_vbus_connected(void) {
    return (SN_GPIO0->DATA & MASK_PIN_VBUS) != 0;
}

bool ch582f_is_connected(void) {
    return (current_mode == M87_MODE_WIRED) ? true : is_connected;
}

uint32_t ch582f_get_connected_time(void) {
    return connected_time;
}

void ch582f_set_os_mode(bool is_windows) {
    if (current_mode == M87_MODE_WIRED) return;
    uint8_t mode = is_windows ? 0x10 : 0x00;
    uint8_t pkt[3] = {0xA5, mode, (uint8_t)(0xA5 + mode)};
    uart2_write_buf_fast(pkt, 3);
}

// -------------------------------------------------------------
// Ultra-Fast HID Reports
// -------------------------------------------------------------
void ch582f_send_keyboard(report_keyboard_t *report) {
    if (current_mode == M87_MODE_WIRED) return;

    // Fast 32-bit dword diffing without calling memcmp()
    static uint32_t last_rep[2] = {0xFFFFFFFF, 0xFFFFFFFF};
    uint32_t cur_rep[2];
    __builtin_memcpy(cur_rep, report, 8);

    if (cur_rep[0] == last_rep[0] && cur_rep[1] == last_rep[1]) {
        return;
    }
    last_rep[0] = cur_rep[0];
    last_rep[1] = cur_rep[1];

    uint8_t *k = report->keys;
    uint8_t sum = 0xA1 + report->mods + k[0] + k[1] + k[2] + k[3] + k[4] + k[5];

    uart2_write_byte_fast(0xA1);
    uart2_write_byte_fast(report->mods);
    uart2_write_byte_fast(0x00);
    uart2_write_byte_fast(k[0]);
    uart2_write_byte_fast(k[1]);
    uart2_write_byte_fast(k[2]);
    uart2_write_byte_fast(k[3]);
    uart2_write_byte_fast(k[4]);
    uart2_write_byte_fast(k[5]);
    uart2_write_byte_fast(sum);
}

void ch582f_send_nkro(report_nkro_t *report) {
    if (current_mode == M87_MODE_WIRED) return;

    uint8_t sum = 0xA2;
    uart2_write_byte_fast(0xA2);
    for (uint8_t i = 0; i < 14; i++) {
        uint8_t b = report->bits[i];
        sum += b;
        uart2_write_byte_fast(b);
    }
    uart2_write_byte_fast(sum);
}

void ch582f_send_consumer(uint16_t usage) {
    if (current_mode == M87_MODE_WIRED) return;

    uint8_t lo = (uint8_t)usage;
    uint8_t hi = (uint8_t)(usage >> 8);
    uart2_write_byte_fast(0xA3);
    uart2_write_byte_fast(lo);
    uart2_write_byte_fast(hi);
    uart2_write_byte_fast((uint8_t)(0xA3 + lo + hi));
}

// -------------------------------------------------------------
// Host Callbacks
// -------------------------------------------------------------
static uint8_t ch582f_driver_leds(void) {
    return host_led_state;
}

static void ch582f_driver_send_keyboard(report_keyboard_t *report) {
    if (report->keys[0] != 0 || report->mods != 0) {
        if (!is_connected) {
            is_connected = true;
            connected_time = timer_read32();
        }
        is_pairing = false;
    }
    ch582f_send_keyboard(report);
}

static void ch582f_driver_send_nkro(report_nkro_t *report) {
    if (!is_connected) {
        is_connected = true;
        connected_time = timer_read32();
    }
    is_pairing = false;

    report_keyboard_t k_report;
    k_report.mods = report->mods;
    k_report.reserved = 0;

    // 8-byte De Bruijn table for branchless bit indexing on Cortex-M0
    static const uint8_t ctz_lut[8] = {0, 1, 6, 2, 7, 5, 4, 3};

    uint8_t count = 0;
    for (uint8_t i = 0; i < 30 && count < 6; i++) {
        uint8_t b = report->bits[i];
        while (b && count < 6) {
            uint8_t isolated = b & -b;
            uint8_t bit = ctz_lut[((uint8_t)(isolated * 29) >> 5) & 7];
            k_report.keys[count++] = (i << 3) + bit;
            b &= (b - 1); // Clear lowest bit
        }
    }
    while (count < 6) k_report.keys[count++] = 0;

    ch582f_send_keyboard(&k_report);
}

static void ch582f_driver_send_mouse(report_mouse_t *report) {
    (void)report;
}

static void ch582f_driver_send_extra(report_extra_t *report) {
    if (report->report_id == REPORT_ID_CONSUMER) {
        ch582f_send_consumer(report->usage);
    }
}

#ifdef RAW_ENABLE
static void ch582f_driver_send_raw_hid(uint8_t *data, uint8_t length) {
    if (default_host_driver && default_host_driver->send_raw_hid) {
        default_host_driver->send_raw_hid(data, length);
    }
}
#endif

// -------------------------------------------------------------
// Fast Packet RX
// -------------------------------------------------------------
static inline void parse_rx_byte(uint8_t b) {
    static uint8_t rx_buf[3];
    static uint8_t rx_idx = 0;

    if (rx_idx == 0) {
        if (b == 0x5B || b == 0x5A || b == 0x5C || b == 0x61) {
            rx_buf[0] = b;
            rx_idx = 1;
        }
        return;
    }

    rx_buf[rx_idx++] = b;

    if (rx_idx >= 3) {
        uint8_t header = rx_buf[0];
        uint8_t param  = rx_buf[1];
        uint8_t cksum  = rx_buf[2];

        if (header == 0x61 && param == '\r' && cksum == '\n') {
            rx_idx = 0;
            return;
        }

        if (cksum == (uint8_t)(header + param)) {
            if (!(header == 0x5B && (param == 0x42 || param == 0x43))) {
                uart2_write_byte_fast('a');
                uart2_write_byte_fast('\r');
                uart2_write_byte_fast('\n');
            }

            if (header == 0x5B) {
                if (param == 0x32 || param == 0x43) {
                    if (!is_connected) {
                        is_connected = true;
                        connected_time = timer_read32();
                    }
                    is_pairing = false;
                } else if (param == 0x31) {
                    is_pairing = true;
                    is_connected = false;
                } else if (param == 0x33 || param == 0x42) {
                    is_connected = false;
                    is_pairing = false;
                }
            } else if (header == 0x5A) {
                if (param < 8) {
                    host_led_state = param;
                    if (!is_connected) {
                        is_connected = true;
                        connected_time = timer_read32();
                    }
                    is_pairing = false;
                }
            } else if (header == 0x5C) {
                battery_percent = param;
            }
        }
        rx_idx = 0;
    }
}

// -------------------------------------------------------------
// Non-Blocking Periodic Background Task
// -------------------------------------------------------------
void ch582f_task(void) {
    // 0. Synchronize QMK host driver pointer (captures USB driver after protocol_init)
    host_driver_t *cur_drv = host_get_driver();
    if (default_host_driver == NULL && cur_drv != &ch582f_host_driver) {
        default_host_driver = cur_drv;
    }
    host_driver_t *target_drv = (current_mode == M87_MODE_WIRED) ? default_host_driver : &ch582f_host_driver;
    if (target_drv && cur_drv != target_drv) {
        host_set_driver(target_drv);
    }

    // 1. Process UART RX
    uint8_t rx_b;
    while (uart2_read_byte_fast(&rx_b)) {
        parse_rx_byte(rx_b);
    }

    // 2. Non-blocking Boot Initialization (Zero wait_ms blocking)
    if (boot_init_state < 4) {
        uint32_t elapsed = timer_elapsed32(boot_time);
        if (boot_init_state == 0 && elapsed >= 400) {
            static const uint8_t vpid_pkt[6] = {0xAD, 0x45, 0x0C, 0x06, 0x80, 0x84};
            uart2_write_buf_fast(vpid_pkt, 6);
            boot_init_state = 1;
        } else if (boot_init_state == 1 && elapsed >= 410) {
            static const uint8_t name_pkt[15] = {0xA9, 12, 'M', '8', '7', ' ', 'K', 'e', 'y', 'b', 'o', 'a', 'r', 'd', 0xC2};
            uart2_write_buf_fast(name_pkt, 15);
            boot_init_state = 2;
        } else if (boot_init_state == 2 && elapsed >= 420) {
            static const uint8_t win_pkt[3] = {0xA5, 0x10, 0xB5};
            uart2_write_buf_fast(win_pkt, 3);
            boot_init_state = 3;
        } else if (boot_init_state == 3 && elapsed >= 430) {
            if (current_mode != M87_MODE_WIRED) {
                uint8_t code = (current_mode == M87_MODE_24G) ? 0x30 : (0x30 + (uint8_t)current_bt_profile);
                uint8_t pkt[3] = {0xA6, code, (uint8_t)(0xA6 + code)};
                uart2_write_buf_fast(pkt, 3);
            }
            boot_init_state = 4;
        }
    }

    // 3. Port0 direct switch debouncer
    uint32_t p0 = SN_GPIO0->DATA;
    uint8_t current_raw = ((!(p0 & MASK_PIN_SWITCH_BT)) ? 1 : 0) | ((!(p0 & MASK_PIN_SWITCH_24G)) ? 2 : 0);

    if (current_raw != last_switch_raw) {
        switch_debounce_counter = 20;
        last_switch_raw = current_raw;
    } else if (switch_debounce_counter > 0) {
        if (--switch_debounce_counter == 0) {
            m87_mode_t new_mode = (current_raw & 1) ? M87_MODE_BT : ((current_raw & 2) ? M87_MODE_24G : M87_MODE_WIRED);
            if (new_mode != current_mode) {
                ch582f_set_mode(new_mode);
            }
        }
    }

    // 4. Cache-Checked LED writes via atomic Set/Clear registers
    uint8_t cur_leds = (host_keyboard_led_state().caps_lock ? 1 : 0) | (keymap_config.no_gui ? 2 : 0);
    if (cur_leds != last_led_bits) {
        last_led_bits = cur_leds;
        if (cur_leds & 1) SN_GPIO0->BSET = MASK_LED_CAPS;
        else             SN_GPIO0->BCLR = MASK_LED_CAPS;

        if (cur_leds & 2) SN_GPIO0->BSET = MASK_LED_WINLOCK;
        else             SN_GPIO0->BCLR = MASK_LED_WINLOCK;
    }

    // 5. Long-press Wireless Key handler
    if (held_wireless_key != 0) {
        if (timer_elapsed32(wireless_key_timer) >= 3000) {
            if (held_wireless_key == M87_24G) {
                ch582f_set_mode(M87_MODE_24G);
            } else {
                current_mode = M87_MODE_BT;
                keymap_config.nkro = false;
                host_set_driver(&ch582f_host_driver);
                ch582f_set_bt_profile((m87_bt_profile_t)(held_wireless_key - M87_BT1 + 1));
            }
            ch582f_start_pairing();
            held_wireless_key = 0;
        }
    }
}

// -------------------------------------------------------------
// Key Process Handler
// -------------------------------------------------------------
bool ch582f_process_record(uint16_t keycode, keyrecord_t *record) {
    if (record->event.pressed && current_mode != M87_MODE_WIRED) {
        if (!is_connected) {
            is_connected = true;
            connected_time = timer_read32();
        }
        is_pairing = false;
    }

    switch (keycode) {
        case M87_BT1:
        case M87_BT2:
        case M87_BT3:
        case M87_24G:
            if (record->event.pressed) {
                held_wireless_key = keycode;
                wireless_key_timer = timer_read32();
            } else {
                if (held_wireless_key != 0) {
                    if (keycode == M87_24G) {
                        ch582f_set_mode(M87_MODE_24G);
                    } else {
                        current_mode = M87_MODE_BT;
                        keymap_config.nkro = false;
                        host_set_driver(&ch582f_host_driver);
                        ch582f_set_bt_profile((m87_bt_profile_t)(keycode - M87_BT1 + 1));
                    }
                    held_wireless_key = 0;
                }
            }
            return false;
        case M87_BAT:
            return false;
    }
    return true;
}
