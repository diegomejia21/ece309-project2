// src/sentinel_scanner.cpp
// Author: Diego Mejia

#include "core/sentinel_scanner.h"
#include <algorithm>
#include <utility>

SentinelScanner::SentinelScanner(std::string sentinel)
    : sentinel_(std::move(sentinel)) {}

SentinelScanner::Out SentinelScanner::feed(std::string_view chunk) {
    // Search the held-back tail plus the new chunk, so a sentinel split
    // across chunks is seen whole.
    std::string text = pending_;
    text += chunk;

    std::size_t pos = text.find(sentinel_);
    if (pos != std::string::npos) {
        pending_.clear();
        return {text.substr(0, pos), true};  // text after the sentinel is dropped
    }

    // Not found: the last sentinel_.size() - 1 characters could still be
    // the start of the sentinel, so hold them back and emit the rest.
    std::size_t keep = std::min(text.size(), sentinel_.size() - 1);
    pending_ = text.substr(text.size() - keep);
    return {text.substr(0, text.size() - keep), false};
}

SentinelScanner::Out SentinelScanner::flush() {
    Out out{pending_, false};
    pending_.clear();
    return out;
}
