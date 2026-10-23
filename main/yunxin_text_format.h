#pragma once

#include "chat_memory.h"

#include <string>
#include <string_view>

// Append-only formatting keeps displayed prefixes stable for incremental layout.
class YunxinTextFormatter {
public:
    void Append(std::string_view text) {
        for (size_t i = 0; i < text.size();) {
            if (pending_break_) {
                if (text[i] == '\r' || text[i] == '\n' || text[i] == ' ' || text[i] == '\t') {
                    ++i;
                    continue;
                }
                if (text.substr(i, 3) == "　") { i += 3; continue; }
                display_ += "\n　　";
                pending_break_ = false;
            }
            if (text.substr(i, 3) == "。") {
                display_ += "。";
                i += 3;
                if (++periods_ == 4) { periods_ = 0; pending_break_ = true; }
            } else {
                display_ += text[i++];
            }
        }
    }

    const ChatString& Text() const { return display_; }
    void Reset() { display_.clear(); periods_ = 0; pending_break_ = false; }

private:
    ChatString display_;
    unsigned periods_ = 0;
    bool pending_break_ = false;
};
