#pragma once

// ESP-IDF 6.x 起 esp_sleep_get_wakeup_cause() 已弃用，替代接口
// esp_sleep_get_wakeup_causes() 返回“唤醒源位图”而不是单个枚举。
// 三处睡眠代码都需要同一套位判断，统一放在这里，避免各写一遍 BIT(cause)
// 而产生语义漂移。

#include <cstdint>

#include <esp_sleep.h>

inline bool PowerWakeCauseMatches(uint32_t causes, esp_sleep_wakeup_cause_t cause) {
    if (cause == ESP_SLEEP_WAKEUP_UNDEFINED || cause == ESP_SLEEP_WAKEUP_ALL) {
        return false;
    }
    return (causes & (uint32_t(1) << static_cast<unsigned>(cause))) != 0;
}

// 位图可能同时含多个源；取最低位作为“主因”，与旧接口的单值语义最接近，
// 仅用于日志和状态判断的兜底路径。
inline esp_sleep_wakeup_cause_t PowerWakePrimaryCause(uint32_t causes) {
    for (unsigned bit = 0; bit < 32; ++bit) {
        if ((causes & (uint32_t(1) << bit)) != 0) {
            return static_cast<esp_sleep_wakeup_cause_t>(bit);
        }
    }
    return ESP_SLEEP_WAKEUP_UNDEFINED;
}
