#include "metalio_touch_upgrade.h"
#include "ft6336u_upgrade_protocol.h"
#include "metalio_i2c1.h"
#include "epd_board_metalio_eink4_plus.h"
#include "config.h"
#include "sdkconfig.h"
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#ifdef CONFIG_METALIO_TOUCH_UPGRADE_ON_BOOT
extern const uint8_t firmware_start[] asm("_binary_ft6336u_v03_app_bin_start");
extern const uint8_t firmware_end[] asm("_binary_ft6336u_v03_app_bin_end");
namespace {
struct Transport {
    bool Write(const uint8_t* bytes, size_t size) {
        return metalio_i2c1_transmit_locked(I2C_NUM_1, TOUCH_I2C_ADDR, bytes, size, 500) == ESP_OK;
    }
    bool Read(const uint8_t* command, size_t command_size, uint8_t* output, size_t size) {
        return metalio_i2c1_transmit_receive_locked(I2C_NUM_1, TOUCH_I2C_ADDR, command, command_size,
                                                    output, size, 500) == ESP_OK;
    }
    void Delay(int ms) { vTaskDelay(std::max<TickType_t>(1, pdMS_TO_TICKS(ms))); }
};
}
#endif

bool MetalioTouch_UpgradeOnBoot() {
#ifdef CONFIG_METALIO_TOUCH_UPGRADE_ON_BOOT
    constexpr const char* tag = "touch_upgrade";
    epd_board_metalio_eink4_plus_tp_reset(); // Ensure active mode before probing IDs.
    if (!metalio_i2c1_lock(1000)) {
        ESP_LOGE(tag, "I2C lock timeout; upgrade not started");
        return false;
    }
    Transport io;
    ESP_LOGI(tag, "checking FT6336U for panel LT0470GF0430M, target V03");
    const auto result = Ft6336uUpgrade::Run(io, firmware_start, firmware_end - firmware_start);
    metalio_i2c1_unlock();
    const bool ok = result == Ft6336uUpgrade::Result::Current || result == Ft6336uUpgrade::Result::Updated;
    if (ok) ESP_LOGI(tag, "touch firmware %s", result == Ft6336uUpgrade::Result::Updated ? "updated and verified V03" : "already current; skipped");
    else ESP_LOGE(tag, "upgrade failed result=%d; no automatic retry this boot", static_cast<int>(result));
    return ok;
#else
    return true;
#endif
}
