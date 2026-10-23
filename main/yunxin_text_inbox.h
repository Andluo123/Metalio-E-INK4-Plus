#pragma once

#include "chat_memory.h"

#include <atomic>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <utility>

// Receivers only update this cache. UI snapshots are queued under the same lock
// as boundaries, so timer updates cannot overtake stop/new-turn events.
class YunxinTextInbox {
public:
    struct Snapshot {
        ChatString text;
        uint32_t generation;
        bool finish;
        uint32_t sequence = 0;
    };

    template<class Queue>
    void Push(const char* text, Queue&& queue) {
        if (text == nullptr || text[0] == '\0') { return; }
        std::lock_guard<std::mutex> lock(mutex_);
        if (latest_ == text) { return; }
        const size_t length = std::strlen(text);
        if (!latest_.empty() && length >= latest_.size() &&
            std::memcmp(text, latest_.data(), latest_.size()) == 0) {
            latest_.append(text + latest_.size(), length - latest_.size());
        } else {
            if (!latest_.empty()) {
                auto snapshot = Snapshot{std::move(latest_), generation_.load(), true, ++sequence_};
                pending_boundaries_.push_back(snapshot);
                queue(std::move(snapshot));
                latest_.clear();
            }
            latest_.assign(text, length);
        }
        dirty_ = true;
    }

    template<class Queue>
    void Update(Queue&& queue) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!dirty_) { return; }
        queue(Snapshot{latest_, generation_.load(), false});
        dirty_ = false;
    }

    template<class Queue>
    void Finish(Queue&& queue) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto snapshot = Snapshot{std::move(latest_), generation_.load(), true, ++sequence_};
        pending_boundaries_.push_back(snapshot);
        queue(std::move(snapshot));
        latest_.clear();
        dirty_ = false;
    }

    ChatVector<Snapshot> Cancel() {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto generation = generation_.fetch_add(1) + 1;
        auto result = std::move(pending_boundaries_);
        for (auto& snapshot : result) snapshot.generation = generation;
        if (!latest_.empty()) {
            try { result.push_back(Snapshot{std::move(latest_), generation, true}); }
            catch (const std::bad_alloc&) { /* Preserve already queued turns on OOM. */ }
        }
        latest_.clear();
        dirty_ = false;
        return result;
    }

    bool IsCurrent(const Snapshot& snapshot) const {
        return snapshot.generation == generation_.load();
    }

    void Applied(const Snapshot& snapshot) {
        if (!snapshot.finish || snapshot.sequence == 0) { return; }
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto it = pending_boundaries_.begin(); it != pending_boundaries_.end(); ++it) {
            if (it->sequence == snapshot.sequence) { pending_boundaries_.erase(it); break; }
        }
    }

private:
    std::mutex mutex_;
    ChatString latest_;
    bool dirty_ = false;
    uint32_t sequence_ = 0;
    ChatVector<Snapshot> pending_boundaries_;
    std::atomic<uint32_t> generation_{0};
};
