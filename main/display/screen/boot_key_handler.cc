/**
 * @brief BOOT/电源键分发
 */
#include "boot_key_handler.h"

#include "cloud_screen/cloud_screen.h"
#include "ota_confirm_dialog/ota_confirm_dialog.h"
#include "ota_upgrade_screen/ota_upgrade_screen.h"
#include "power_policy.h"
#include "settings_screen/settings_log_upload.h"
#include "screen_common.h"
#include "standby_screen/standby_screen.h"
#include "vk_key_handler.h"
#include "usb_virtual_disk.h"

#include <atomic>
#include <esp_log.h>

#define TAG "BootKey"

static std::atomic<bool> s_boot_held{false};
static std::atomic<bool> s_long_press_fired{false};

static bool DispatchBootAction(BootKeyAction action, const char* kind, const char* screen) {
    if (action == nullptr) {
        ESP_LOGI(TAG, "%s no-op (screen=%s)", kind, screen);
        return false;
    }
    if (!action()) {
        ESP_LOGI(TAG, "%s ignored by screen=%s", kind, screen);
        return false;
    }
    ESP_LOGI(TAG, "%s handled by screen=%s", kind, screen);
    return true;
}

static bool OtaBlocks() {
    return OtaUpgradeScreen::IsActive() || OtaConfirmDialog::IsActive();
}

static void EnterStandbyAsync(void* /*arg*/) {
    SettingsLogUpload_RequestAbort("power_key");
    CloudScreen::StopTransfer();
    CloudScreen::ParkApForStandby();
    StandbyScreen::Show();
}

void BootKey_OnPressDown() {
    if (SettingsLogUpload_IsActive() || OtaBlocks()) {
        return;
    }
    s_boot_held.store(true);
    s_long_press_fired.store(false);
    const char* screen = VkKey_ActiveScreen();
    DispatchBootAction(VkKey_GetBootPressDown(screen), "press-down", screen);
}

void BootKey_OnPressUp() {
    if (SettingsLogUpload_IsActive() || OtaBlocks()) {
        s_boot_held.store(false);
        return;
    }
    s_boot_held.store(false);
    const char* screen = VkKey_ActiveScreen();
    DispatchBootAction(VkKey_GetBootPressUp(screen), "press-up", screen);
}

void BootKey_OnClick() {
    if (SettingsLogUpload_IsActive() || OtaBlocks() || s_long_press_fired.load()) {
        return;
    }
    const char* screen = VkKey_ActiveScreen();
    DispatchBootAction(VkKey_GetBootClick(screen), "short-press", screen);
}

void BootKey_OnDoubleClick() {
    if (SettingsLogUpload_IsActive() || OtaBlocks() || s_long_press_fired.load()) {
        return;
    }
    const char* screen = VkKey_ActiveScreen();
    DispatchBootAction(VkKey_GetBootDoubleClick(screen), "double-click", screen);
}

void BootKey_OnLongPress() {
    if (SettingsLogUpload_IsActive() || OtaBlocks()) {
        s_long_press_fired.store(true);
        return;
    }
    s_long_press_fired.store(true);
    const char* screen = VkKey_ActiveScreen();
    if (StandbyScreen::IsActive()) {
        if (StandbyScreen::HandleBootLongPress()) {
            ESP_LOGI(TAG, "long-press handled by standby");
            return;
        }
    }
    DispatchBootAction(VkKey_GetBootLongPress(screen), "long-press", screen);
}

void PowerKey_OnClick() {
    if (OtaBlocks()) {
        return;
    }
    if (StandbyScreen::IsActive()) {
        StandbyScreen::RequestWake();
        return;
    }
    auto& usb = UsbVirtualDisk::GetInstance();
    if (usb.IsGadgetActive() || usb.IsBusy()) {
        ESP_LOGI(TAG, "power standby deferred: USB owns SD");
        return;
    }
    ESP_LOGI(TAG, "power short -> standby");
    PowerPolicy::GetInstance().PreparePowerKeyStandby();
    if (!ScreenLvAsyncUrgent(EnterStandbyAsync)) {
        ScreenLvAsync(EnterStandbyAsync);
    }
}

bool BootKey_IsHeld() {
    return s_boot_held.load();
}

bool BootKey_DidLongPress() {
    return s_long_press_fired.load();
}
