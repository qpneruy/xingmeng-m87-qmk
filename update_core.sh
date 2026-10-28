#!/usr/bin/env bash
# ==============================================================================
# Update QMK Core & Sonix ChibiOS HAL from upstream SonixQMK
# Excludes keyboards/ so your standalone Xingmeng M87 tree is never conflicted.
# ==============================================================================
set -e

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$REPO_ROOT"

echo "==> Fetching latest changes from SonixQMK upstream (sn32_develop)..."
git fetch https://github.com/SonixQMK/qmk_firmware.git sn32_develop

echo "==> Synchronizing core engine directories..."
git checkout FETCH_HEAD -- \
    quantum \
    platforms \
    drivers/eeprom \
    drivers/sensors \
    builddefs \
    data \
    Makefile \
    configure.mk

echo "==> Checking LED driver patches..."
if [ -f patches/drivers_led_sn32f2xx.c.patch ]; then
    patch -p1 -N < patches/drivers_led_sn32f2xx.c.patch || true
fi

echo "==> Updating hardware submodules (ChibiOS / ChibiOS-Contrib / LUFA)..."
git submodule update --init lib/chibios lib/chibios-contrib lib/lufa lib/printf

echo "==> Success! QMK Core updated cleanly without touching keyboards/xingmeng_m87."
