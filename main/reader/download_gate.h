#pragma once
#include "http.h"
#include <atomic>
#include <mutex>

// Worker retains ownership; gate only borrows while Binding is alive.
struct DownloadGate {
    std::atomic<bool> cancel{false};
    std::mutex mu;
    Http* http = nullptr;
    void Reset() {
        std::lock_guard<std::mutex> lock(mu);
        if (!http) cancel.store(false);
    }
    void RequestCancel() { cancel.store(true); }
    void AbortBoundHttp() {
        std::lock_guard<std::mutex> lock(mu);
        if (cancel.load() && http) http->Cancel();
    }
    void RequestStop() { RequestCancel(); AbortBoundHttp(); }
    bool BindHttp(Http* h) {
        std::lock_guard<std::mutex> lock(mu);
        if (!h || http || cancel.load()) return false;
        http = h;
        return true;
    }
    void UnbindHttp() {
        std::lock_guard<std::mutex> lock(mu);
        http = nullptr;
    }
    // Only called by the worker after its HTTP operation returns.
    void CloseHttp() {
        Http* bound;
        {
            std::lock_guard<std::mutex> lock(mu);
            bound = http;
            http = nullptr;
        }
        if (bound) bound->Close();
    }
    bool IsCancelled() const { return cancel.load(); }
    // Declare after unique_ptr: detach before the worker destroys Http.
    struct Binding {
        DownloadGate* gate = nullptr;
        Binding() = default;
        ~Binding() { if (gate) gate->UnbindHttp(); }
        Binding(const Binding&) = delete;
        Binding& operator=(const Binding&) = delete;
    };
};
