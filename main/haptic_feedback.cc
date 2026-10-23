#include "haptic_feedback.h"

#include "board.h"
#include "settings.h"

#include <cstdint>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>
#include <freertos/task.h>

namespace {

constexpr const char* TAG = "Haptic";
constexpr const char* kNvsNs = "display";
constexpr const char* kNvsKey = "haptic";
constexpr const char* kNvsIntensityKey = "haptic_lvl";
constexpr bool kDefaultEnabled = true;
constexpr int kIntensityWeak = 0; // 弱
constexpr int kIntensityMedium = 1; // 中
constexpr int kIntensityStrong = 2; // 强
constexpr int kDefaultIntensity = kIntensityWeak;
constexpr int kPulseMsWeak = 35; // 弱脉宽 ms
constexpr int kPulseMsMedium = 50; // 中脉宽 ms
constexpr int kPulseMsStrong = 70; // 强脉宽 ms

// LVGL USER_1：标记已挂震动回调，避免重复 Attach 导致连震。
constexpr lv_obj_flag_t kHapticBoundFlag = LV_OBJ_FLAG_USER_1;
constexpr size_t kMaxZones = 96;

struct HapticZone {
    lv_obj_t* obj = nullptr;
    int16_t x1 = 0;
    int16_t y1 = 0;
    int16_t x2 = 0;
    int16_t y2 = 0;
};

bool s_enabled = kDefaultEnabled;
int s_intensity = kDefaultIntensity;
bool s_ready = false;
bool s_persist_busy = false;
portMUX_TYPE s_persist_mux = portMUX_INITIALIZER_UNLOCKED;
bool s_zone_early_pulse = true;  // 全屏遮罩时可关，避免下层热区误震
// touch_feed 早震与 LVGL PRESSED 去重
uint32_t s_press_seq = 0;
uint32_t s_pulsed_seq = 0;

HapticZone s_zones[kMaxZones]{};
size_t s_zone_count = 0;
portMUX_TYPE s_zone_mux = portMUX_INITIALIZER_UNLOCKED;

int ClampIntensity(int intensity) {
    if (intensity == kIntensityMedium || intensity == kIntensityStrong) {
        return intensity;
    }
    return kIntensityWeak;
}

int PulseMsForIntensity(int intensity) {
    switch (ClampIntensity(intensity)) {
        case kIntensityMedium:
            return kPulseMsMedium;
        case kIntensityStrong:
            return kPulseMsStrong;
        default:
            return kPulseMsWeak;
    }
}

void LoadFromNvs() {
    Settings settings(kNvsNs, false);
    s_enabled = settings.GetBool(kNvsKey, kDefaultEnabled);
    s_intensity = ClampIntensity(settings.GetInt(kNvsIntensityKey, kDefaultIntensity));
    s_ready = true;
    ESP_LOGI(TAG, "nvs load haptic=%d intensity=%d (%dms)", s_enabled ? 1 : 0, s_intensity,
             PulseMsForIntensity(s_intensity));
}

void EnsureReady() {
    if (!s_ready) {
        LoadFromNvs();
    }
}

void PersistTask(void* /*arg*/) {
    for (;;) {
        portENTER_CRITICAL(&s_persist_mux);
        const bool enabled = s_enabled;
        const int intensity = s_intensity;
        portEXIT_CRITICAL(&s_persist_mux);
        {
            Settings settings(kNvsNs, true);
            settings.SetBool(kNvsKey, enabled);
            settings.SetInt(kNvsIntensityKey, intensity);
        }
        portENTER_CRITICAL(&s_persist_mux);
        const bool done = enabled == s_enabled && intensity == s_intensity;
        if (done) s_persist_busy = false;
        portEXIT_CRITICAL(&s_persist_mux);
        if (done) break;
    }
    vTaskDelete(nullptr);
}

void SchedulePersist() {
    // NVS needs an internal stack; coalesce rapid changes into one worker.
    portENTER_CRITICAL(&s_persist_mux);
    const bool busy = s_persist_busy;
    s_persist_busy = true;
    portEXIT_CRITICAL(&s_persist_mux);
    if (busy) return;
    if (xTaskCreate(PersistTask, "haptic_nvs", 4096, nullptr, 5, nullptr) != pdPASS) {
        portENTER_CRITICAL(&s_persist_mux);
        s_persist_busy = false;
        portEXIT_CRITICAL(&s_persist_mux);
        ESP_LOGE(TAG, "haptic_nvs task failed; preference remains in RAM");
    }
}

void PulseNow() {
    Board::GetInstance().PulseVibration(PulseMsForIntensity(s_intensity));
}

void WriteZoneCoords(HapticZone* z, lv_obj_t* obj) {
    lv_area_t a{};
    lv_obj_get_coords(obj, &a);
    portENTER_CRITICAL(&s_zone_mux);
    z->obj = obj;
    z->x1 = a.x1;
    z->y1 = a.y1;
    z->x2 = a.x2;
    z->y2 = a.y2;
    portEXIT_CRITICAL(&s_zone_mux);
}

HapticZone* FindZoneByObj(const lv_obj_t* obj) {
    for (size_t i = 0; i < s_zone_count; ++i) {
        if (s_zones[i].obj == obj) {
            return &s_zones[i];
        }
    }
    return nullptr;
}

void RemoveZoneAt(size_t idx) {
    if (idx >= s_zone_count) {
        return;
    }
    portENTER_CRITICAL(&s_zone_mux);
    s_zones[idx] = s_zones[s_zone_count - 1];
    s_zones[s_zone_count - 1] = {};
    s_zone_count--;
    portEXIT_CRITICAL(&s_zone_mux);
}

bool RegisterZone(lv_obj_t* obj) {
    HapticZone* existing = FindZoneByObj(obj);
    if (existing != nullptr) {
        WriteZoneCoords(existing, obj);
        return true;
    }
    if (s_zone_count >= kMaxZones) {
        // Long font/TOC lists contain many off-screen buttons. Only retain
        // visible early-feedback zones; PRESSED feedback still works for all.
        for (size_t i = s_zone_count; i > 0; --i) {
            if (!lv_obj_is_visible(s_zones[i - 1].obj)) RemoveZoneAt(i - 1);
        }
        if (s_zone_count >= kMaxZones) {
            static bool warned = false;
            if (!warned) {
                ESP_LOGW(TAG, "visible zone table full (%u); use press feedback", static_cast<unsigned>(kMaxZones));
                warned = true;
            }
            return false;
        }
    }
    lv_area_t a{};
    lv_obj_get_coords(obj, &a);
    portENTER_CRITICAL(&s_zone_mux);
    HapticZone* z = &s_zones[s_zone_count];
    z->obj = obj;
    z->x1 = a.x1;
    z->y1 = a.y1;
    z->x2 = a.x2;
    z->y2 = a.y2;
    s_zone_count++;
    portEXIT_CRITICAL(&s_zone_mux);
    return true;
}

void OnGeomChanged(lv_event_t* e) {
    lv_obj_t* obj = static_cast<lv_obj_t*>(lv_event_get_target(e));
    HapticZone* z = FindZoneByObj(obj);
    if (z == nullptr) {
        if (lv_event_get_code(e) == LV_EVENT_DRAW_MAIN_BEGIN && lv_obj_is_visible(obj))
            RegisterZone(obj);
        return;
    }
    lv_area_t a{};
    lv_obj_get_coords(obj, &a);
    // 仅坐标真变才写：布局改的是位置，SIZE_CHANGED 往往不来，靠 DRAW 补
    if (z->x1 == a.x1 && z->y1 == a.y1 && z->x2 == a.x2 && z->y2 == a.y2) {
        return;
    }
    WriteZoneCoords(z, obj);
}

void OnObjDeleted(lv_event_t* e) {
    lv_obj_t* obj = static_cast<lv_obj_t*>(lv_event_get_target(e));
    for (size_t i = 0; i < s_zone_count; ++i) {
        if (s_zones[i].obj == obj) {
            RemoveZoneAt(i);
            return;
        }
    }
}

void OnPressFeedback(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_PRESSED) {
        return;
    }
    lv_obj_t* obj = static_cast<lv_obj_t*>(lv_event_get_target(e));
    HapticZone* z = FindZoneByObj(obj);
    if (z != nullptr) {
        WriteZoneCoords(z, obj);
    }
    if (s_press_seq != 0 && s_pulsed_seq == s_press_seq) {
        return;
    }
    // 占位，防止同一次按下多次 PRESSED / 抬手后晚到的 PRESSED 再震
    if (s_press_seq != 0) {
        s_pulsed_seq = s_press_seq;
    }
    HapticPulseIfEnabled();
}

}  // namespace

bool HapticIsEnabled(void) {
    EnsureReady();
    return s_enabled;
}

void HapticSetEnabled(bool enabled) {
    EnsureReady();
    portENTER_CRITICAL(&s_persist_mux);
    s_enabled = enabled;
    portEXIT_CRITICAL(&s_persist_mux);
    s_ready = true;
    SchedulePersist();
}

int HapticGetIntensity(void) {
    EnsureReady();
    return s_intensity;
}

void HapticSetIntensity(int intensity) {
    EnsureReady();
    const int next = ClampIntensity(intensity);
    if (s_intensity == next) {
        return;
    }
    portENTER_CRITICAL(&s_persist_mux);
    s_intensity = next;
    portEXIT_CRITICAL(&s_persist_mux);
    s_ready = true;
    SchedulePersist();
}

void HapticPulseIfEnabled(void) {
    if (!HapticIsEnabled()) {
        return;
    }
    PulseNow();
}

void HapticBeginPress(void) {
    s_press_seq++;
    s_pulsed_seq = 0;
}

void HapticPulseOnFingerDown(void) {
    if (s_press_seq == 0) {
        s_press_seq = 1;
    }
    s_pulsed_seq = s_press_seq;
    if (!HapticIsEnabled()) {
        return;
    }
    PulseNow();
}

bool HapticTryPulseAtUiPoint(int16_t x, int16_t y) {
    if (!HapticIsEnabled() || !s_zone_early_pulse) {
        return false;
    }
    bool hit = false;
    portENTER_CRITICAL(&s_zone_mux);
    for (size_t i = 0; i < s_zone_count; ++i) {
        const HapticZone& z = s_zones[i];
        if (z.obj == nullptr) {
            continue;
        }
        if (x >= z.x1 && x <= z.x2 && y >= z.y1 && y <= z.y2) {
            hit = true;
            break;
        }
    }
    portEXIT_CRITICAL(&s_zone_mux);
    if (!hit) {
        return false;
    }
    HapticPulseOnFingerDown();
    return true;
}

void HapticSetZoneEarlyPulseEnabled(bool enabled) {
    s_zone_early_pulse = enabled;
}

void HapticAttachClick(lv_obj_t* obj) {
    if (obj == nullptr) {
        return;
    }
    if (lv_obj_has_flag(obj, kHapticBoundFlag)) {
        return;
    }
    lv_obj_add_flag(obj, kHapticBoundFlag);
    lv_obj_add_event_cb(obj, OnPressFeedback, LV_EVENT_PRESSED, nullptr);
    lv_obj_add_event_cb(obj, OnGeomChanged, LV_EVENT_SIZE_CHANGED, nullptr);
    // 布局常改位置不改尺寸：SIZE_CHANGED 不来，上屏绘制时刷新热区坐标
    lv_obj_add_event_cb(obj, OnGeomChanged, LV_EVENT_DRAW_MAIN_BEGIN, nullptr);
    lv_obj_add_event_cb(obj, OnObjDeleted, LV_EVENT_DELETE, nullptr);
    // Register after layout, when DRAW supplies the actual visible coordinates.
}

void HapticDetachClick(lv_obj_t* obj) {
    if (obj == nullptr || !lv_obj_has_flag(obj, kHapticBoundFlag)) {
        return;
    }
    lv_obj_clear_flag(obj, kHapticBoundFlag);
    lv_obj_remove_event_cb(obj, OnPressFeedback);
    lv_obj_remove_event_cb(obj, OnGeomChanged);
    lv_obj_remove_event_cb(obj, OnObjDeleted);
    for (size_t i = 0; i < s_zone_count; ++i) {
        if (s_zones[i].obj == obj) {
            RemoveZoneAt(i);
            break;
        }
    }
}
