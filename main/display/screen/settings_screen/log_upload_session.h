#pragma once

#include <atomic>
#include <memory>
#include <mutex>

enum class LogUploadResult { Pending, Ok, Failed, Cancelled, NoFile, NetFailed, StorageUnavailable, NoMemory };
enum class LogUploadPhase { Snapshot, WaitingNetwork, Sending };

struct LogUploadSession {
    std::atomic<unsigned> references{0};
    std::atomic<bool> cancel{false};
    std::atomic<bool> running{true};
    std::atomic<int> percent{0};
    // One atomic snapshot: total (bits 0-7), completed (8-15), file id (16-23).
    std::atomic<unsigned> jobs{0};
    std::atomic<LogUploadPhase> phase{LogUploadPhase::Snapshot};
    std::atomic<LogUploadResult> result{LogUploadResult::Pending};
    // Worker calls this only after file/HTTP/power/activation guards have left scope.
    void Finish(LogUploadResult value) {
        result.store(cancel.load() ? LogUploadResult::Cancelled : value);
        running.store(false, std::memory_order_release);
    }
};

// Intrusive lifetime: one nothrow allocation, no hidden shared_ptr control-block
// allocation that could abort with exceptions disabled on a low-memory device.
class LogUploadSessionPtr {
public:
    LogUploadSessionPtr() = default;
    explicit LogUploadSessionPtr(LogUploadSession* p) : ptr_(p) { Retain(); }
    LogUploadSessionPtr(const LogUploadSessionPtr& other) : ptr_(other.ptr_) { Retain(); }
    LogUploadSessionPtr(LogUploadSessionPtr&& other) noexcept : ptr_(other.ptr_) { other.ptr_ = nullptr; }
    ~LogUploadSessionPtr() { Release(); }
    LogUploadSessionPtr& operator=(LogUploadSessionPtr other) noexcept {
        auto* old = ptr_; ptr_ = other.ptr_; other.ptr_ = old;
        return *this;
    }
    LogUploadSession* operator->() const { return ptr_; }
    LogUploadSession& operator*() const { return *ptr_; }
    explicit operator bool() const { return ptr_ != nullptr; }
private:
    void Retain() { if (ptr_) ptr_->references.fetch_add(1, std::memory_order_relaxed); }
    void Release() {
        if (ptr_ && ptr_->references.fetch_sub(1, std::memory_order_acq_rel) == 1) delete ptr_;
    }
    LogUploadSession* ptr_ = nullptr;
};

// Serializes Begin versus cancellation. Each session has independent lifetime/state.
// The registry never owns HTTP, FILE, UI objects or blocking cleanup operations.
class LogUploadSessions {
public:
    LogUploadSessionPtr Current() {
        std::lock_guard<std::mutex> lock(mu_);
        return current_;
    }
    bool Begin(const LogUploadSessionPtr& next) {
        std::lock_guard<std::mutex> lock(mu_);
        if (!next) return false;
        if (current_ && current_->running.load(std::memory_order_acquire)) return false;
        current_ = next;
        return true;
    }
    void Cancel() {
        std::lock_guard<std::mutex> lock(mu_);
        if (current_ && current_->running.load()) current_->cancel.store(true);
    }
private:
    std::mutex mu_;
    LogUploadSessionPtr current_;
};
