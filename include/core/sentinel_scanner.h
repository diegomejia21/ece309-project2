// include/core/sentinel_scanner.h
// Author: Diego Mejia
//
// Detects a multi-character sentinel in a stream that arrives in arbitrary
// chunks. Holds back at most sentinel.size() - 1 characters between calls.

#pragma once
#include <cstddef>
#include <string>
#include <string_view>

class SentinelScanner {
public:
    explicit SentinelScanner(std::string sentinel);

    struct Out { std::string safe_text; bool sentinel_found; };

    // Feed the next chunk. Returns text guaranteed NOT to be part of
    // the sentinel (safe to print immediately) and whether the
    // sentinel has now been fully seen.
    Out feed(std::string_view chunk);

    // Call once, after the stream ends, to release any text still
    // being held back.
    Out flush();

    // Number of characters currently held back (used by the tests).
    std::size_t pending_size() const noexcept { return pending_.size(); }

private:
    std::string sentinel_;
    std::string pending_;   // holds back at most sentinel_.size() - 1
                             // trailing characters that could still
                             // become the start of the sentinel
};
