#pragma once

#include <mutex>

// A complete panel waveform needs uninterrupted cache/PSRAM access. OTA flash
// operations must wait for it, and a waveform must wait for an ongoing write.
// Never hold this gate while waiting for LVGL callbacks or network data.
inline std::mutex& EpdFlashMutex() {
    static std::mutex mutex;
    return mutex;
}

class EpdFlashGuard {
public:
    EpdFlashGuard() : lock_(EpdFlashMutex()) {}

private:
    std::lock_guard<std::mutex> lock_;
};
