// include/core/conversation.h
// Author: Diego Mejia
//
// Growable array of Messages, oldest first. The only class in the project
// that uses raw new/delete.
//
// - Capacity starts at 4 on the first append and doubles when full.
// - A System message may only be appended to an empty conversation, so it
//   is always at index 0 (append throws std::invalid_argument otherwise).
// - at(i) throws std::out_of_range when i >= size().

#pragma once
#include "core/message.h"
#include <cstddef>

class Conversation {
public:
    static constexpr std::size_t kInitialCapacity = 4;
    static constexpr std::size_t kGrowthFactor    = 2;

    // Empty conversation: size() == 0, no allocation yet.
    Conversation() = default;

    // Releases all owned Message storage. No effect if already empty
    // (e.g. moved-from).
    ~Conversation();

    // Deep copy: allocates its own buffer and copies every Message.
    // this->begin() must differ from other.begin() afterward.
    Conversation(const Conversation& other);
    Conversation& operator=(const Conversation& other);

    // Steals other's buffer — no per-element copying. Afterward, other
    // must be left valid and empty (safe to destroy or reassign).
    Conversation(Conversation&& other) noexcept;
    Conversation& operator=(Conversation&& other) noexcept;

    // Appends m, growing the backing array if needed. Amortized O(1).
    void append(Message m);

    // Number of messages currently stored.
    std::size_t size() const noexcept;

    // Number of allocated slots (used by the tests to check growth).
    std::size_t capacity() const noexcept;

    // Bounds-checked access. Throws std::out_of_range on i >= size().
    const Message& at(std::size_t i) const;

    // Range-for iteration, oldest message first. begin() == end() when
    // size() == 0.
    const Message* begin() const noexcept;
    const Message* end()   const noexcept;

private:
    void swap(Conversation& other) noexcept;

    Message*    data_ = nullptr;
    std::size_t size_ = 0;
    std::size_t capacity_ = 0;
};
