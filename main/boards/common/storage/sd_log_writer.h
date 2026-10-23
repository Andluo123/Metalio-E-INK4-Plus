#pragma once

#include <cstddef>
#include <cstdint>

// Permanent log hook; only the dedicated INTERNAL-stack worker touches FatFS/NVS.
void SdLogWriter_Start();
bool SdLogWriter_IsEnabled();
void SdLogWriter_SetEnabled(bool enabled);
void SdLogWriter_OnSdMounted();
void SdLogWriter_OnSdUnmounting();
// INTERNAL-stack caller only. Truncate TXT and remove exported SD crash files;
// preserve the enabled preference and Flash coredump. Serialized with SD I/O.
bool SdLogWriter_Clear();

enum class SdLogSnapshotResult { Ok, NoFile, Unavailable, NoMemory, IoError };
// Called only by a background INTERNAL-stack task. Owns data until destruction.
// No SD handle survives this call: USB may export the card during HTTP upload.
struct SdLogSnapshot {
    uint8_t* data = nullptr;
    size_t size = 0;
    ~SdLogSnapshot();
    SdLogSnapshot() = default;
    SdLogSnapshot(const SdLogSnapshot&) = delete;
    SdLogSnapshot& operator=(const SdLogSnapshot&) = delete;
};
SdLogSnapshotResult SdLogWriter_Snapshot(SdLogSnapshot& out);
SdLogSnapshotResult SdLogWriter_CrashSnapshot(SdLogSnapshot& out);

// Serialize logger Flash/NVS/SD operations against time-critical physical EPD refresh.
void SdLogWriter_LockForDisplayRefresh();
void SdLogWriter_UnlockForDisplayRefresh();
