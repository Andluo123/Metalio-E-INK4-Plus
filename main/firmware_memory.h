#pragma once

#include <esp_heap_caps.h>
#include <esp_log.h>

// Keep exact bytes and the historical low water: recovered free space must not
// disguise a startup/audio/network peak below the project's 50 KiB floor.
inline void FirmwareMemoryCheckpoint(const char* phase) {
    constexpr uint32_t internal = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;
    constexpr uint32_t dma = MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA | MALLOC_CAP_8BIT;
    const auto free = heap_caps_get_free_size(internal);
    const auto minimum = heap_caps_get_minimum_free_size(internal);
    ESP_LOGI("MemBudget", "%s internal8 free=%u min=%u largest=%u floor=51200 %s",
             phase, unsigned(free), unsigned(minimum),
             unsigned(heap_caps_get_largest_free_block(internal)),
             minimum >= 51200 ? "within-floor" : "BELOW-FLOOR");
    ESP_LOGI("MemBudget", "%s internal-DMA free=%u min=%u largest=%u", phase,
             unsigned(heap_caps_get_free_size(dma)),
             unsigned(heap_caps_get_minimum_free_size(dma)),
             unsigned(heap_caps_get_largest_free_block(dma)));
}
