#pragma once

#include <esp_heap_caps.h>
#include <limits>
#include <new>
#include <string>
#include <vector>

// Speech snapshots and pagination caches must not consume the internal RAM
// needed by Wi-Fi, I2S DMA and display refresh. No internal fallback.
template<class T> struct ChatAllocator {
    using value_type = T;
    ChatAllocator() noexcept = default;
    template<class U> ChatAllocator(const ChatAllocator<U>&) noexcept {}
    T* allocate(size_t n) {
        if (n > std::numeric_limits<size_t>::max() / sizeof(T)) throw std::bad_alloc();
        auto* p = heap_caps_malloc(n * sizeof(T), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!p) throw std::bad_alloc();
        return static_cast<T*>(p);
    }
    void deallocate(T* p, size_t) noexcept { heap_caps_free(p); }
};
template<class T, class U> constexpr bool operator==(ChatAllocator<T>, ChatAllocator<U>) { return true; }
template<class T, class U> constexpr bool operator!=(ChatAllocator<T>, ChatAllocator<U>) { return false; }
using ChatString = std::basic_string<char, std::char_traits<char>, ChatAllocator<char>>;
template<class T> using ChatVector = std::vector<T, ChatAllocator<T>>;
