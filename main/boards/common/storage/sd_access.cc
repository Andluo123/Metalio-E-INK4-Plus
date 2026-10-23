#include "sd_access.h"

#include <mutex>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <ff.h>

namespace {
std::mutex s_mu;
bool s_quiescing = false;
unsigned s_pending = 0;
struct PermitSlot { TaskHandle_t task = nullptr; unsigned depth = 0; };
PermitSlot s_permits[8];

bool Admit() {
    std::lock_guard<std::mutex> lock(s_mu);
    bool permitted = !s_quiescing;
    for (const auto& slot : s_permits) {
        permitted |= slot.task == xTaskGetCurrentTaskHandle() && slot.depth != 0;
    }
    if (permitted) ++s_pending;
    return permitted;
}
void Finish() {
    std::lock_guard<std::mutex> lock(s_mu);
    configASSERT(s_pending != 0);
    --s_pending;
}
void AddPermit() {
    std::lock_guard<std::mutex> lock(s_mu);
    auto task = xTaskGetCurrentTaskHandle();
    for (auto& slot : s_permits) {
        if (slot.task == task && slot.depth) { ++slot.depth; return; }
    }
    for (auto& slot : s_permits) {
        if (slot.depth == 0) { slot.task = task; slot.depth = 1; return; }
    }
    configASSERT(false);
}
void RemovePermit() {
    std::lock_guard<std::mutex> lock(s_mu);
    for (auto& slot : s_permits) {
        if (slot.task == xTaskGetCurrentTaskHandle() && slot.depth) {
            if (--slot.depth == 0) slot.task = nullptr;
            return;
        }
    }
    configASSERT(false);
}
}

namespace sd_access {
void Quiesce() { std::lock_guard<std::mutex> lock(s_mu); s_quiescing = true; }
void Resume() { std::lock_guard<std::mutex> lock(s_mu); s_quiescing = false; }
bool IsQuiescing() { std::lock_guard<std::mutex> lock(s_mu); return s_quiescing; }
bool IsIdle() { std::lock_guard<std::mutex> lock(s_mu); return s_pending == 0; }
unsigned Pending() { std::lock_guard<std::mutex> lock(s_mu); return s_pending; }
Permit::Permit() { AddPermit(); }
Permit::~Permit() { RemovePermit(); }
Transaction::Transaction() : admitted_(Admit()) { if (admitted_) AddPermit(); }
Transaction::~Transaction() { if (admitted_) { RemovePermit(); Finish(); } }
}

// Linker wraps keep the drain count for the entire lifetime of every FIL/FF_DIR,
// not merely the current disk transaction: unmount cannot invalidate open files.
extern "C" {
FRESULT __real_f_open(FIL*, const TCHAR*, BYTE);
FRESULT __real_f_close(FIL*);
FRESULT __real_f_opendir(FF_DIR*, const TCHAR*);
FRESULT __real_f_closedir(FF_DIR*);
FRESULT __wrap_f_open(FIL* file, const TCHAR* path, BYTE mode) {
    if (!Admit()) return FR_NOT_READY;
    FRESULT result = __real_f_open(file, path, mode);
    if (result != FR_OK) Finish();
    return result;
}
FRESULT __wrap_f_close(FIL* file) {
    FRESULT result = __real_f_close(file);
    if (result == FR_OK) Finish();
    return result;
}
FRESULT __wrap_f_opendir(FF_DIR* dir, const TCHAR* path) {
    if (!Admit()) return FR_NOT_READY;
    FRESULT result = __real_f_opendir(dir, path);
    if (result != FR_OK) Finish();
    return result;
}
FRESULT __wrap_f_closedir(FF_DIR* dir) {
    FRESULT result = __real_f_closedir(dir);
    if (result == FR_OK) Finish();
    return result;
}

#define SD_PATH_OPERATION(name, params, args) \
    FRESULT __real_##name params; \
    FRESULT __wrap_##name params { \
        if (!Admit()) return FR_NOT_READY; \
        FRESULT result = __real_##name args; \
        Finish(); \
        return result; \
    }
SD_PATH_OPERATION(f_stat, (const TCHAR* path, FILINFO* info), (path, info))
SD_PATH_OPERATION(f_mkdir, (const TCHAR* path), (path))
SD_PATH_OPERATION(f_unlink, (const TCHAR* path), (path))
SD_PATH_OPERATION(f_rename, (const TCHAR* from, const TCHAR* to), (from, to))
SD_PATH_OPERATION(f_utime, (const TCHAR* path, const FILINFO* info), (path, info))
SD_PATH_OPERATION(f_getfree, (const TCHAR* path, DWORD* count, FATFS** fs), (path, count, fs))
#undef SD_PATH_OPERATION
}
