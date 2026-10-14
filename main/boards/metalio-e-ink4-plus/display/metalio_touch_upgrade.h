#pragma once
#include <stdbool.h>

// Boot-only: after EPD/I2C initialization, before touch feed, LVGL and monitors.
// Returns true when disabled, already current, or successfully updated.
bool MetalioTouch_UpgradeOnBoot();
