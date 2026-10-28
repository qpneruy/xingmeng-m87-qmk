// Copyright 2026 HuuTinh (@qpneruy)
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "quantum.h"
#include "ch582f.h"

extern bool sidelight_enabled;
extern bool lightbar_enabled;
extern bool keylight_enabled;

void load_led_strip_settings(void);
void save_led_strip_settings(void);
