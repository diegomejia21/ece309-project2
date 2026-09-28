// src/conversation.cpp
// Author: Diego Mejia

#include "core/conversation.h"
#include <stdexcept>
#include <utility>

Conversation::~Conversation() {
    delete[] data_;  // no-op on nullptr (empty or moved-from)
}

// Always allocates, even when other is empty, so the copy never shares a
// begin() pointer with other.
Conversation::Conversation(const Conversation& other) {
    std::size_t cap = other.capacity_ != 0 ? other.capacity_ : kInitialCapacity;
    Message* buf = new Message[cap];
    try {
        for (std::size_t i = 0; i < other.size_; ++i) buf[i] = other.data_[i];
    } catch (...) {
        delete[] buf;  // a string copy threw; don't leak the new buffer
        throw;
    }
    data_ = buf;
    size_ = other.size_;
    capacity_ = cap;
}

// Copy-and-swap: safe on self-assignment, and *this is unchanged if the
// copy throws.
Conversation& Conversation::operator=(const Conversation& other) {
    Conversation tmp(other);
    swap(tmp);
    return *this;  // tmp's destructor frees the old buffer
}

Conversation::Conversation(Conversation&& other) noexcept
    : data_(other.data_), size_(other.size_), capacity_(other.capacity_) {
    other.data_ = nullptr;
    other.size_ = 0;
    other.capacity_ = 0;
}

Conversation& Conversation::operator=(Conversation&& other) noexcept {
    if (this != &other) {
        delete[] data_;
        data_ = other.data_;
        size_ = other.size_;
        capacity_ = other.capacity_;
        other.data_ = nullptr;
        other.size_ = 0;
        other.capacity_ = 0;
    }
    return *this;
}

void Conversation::append(Message m) {
    if (m.role() == Role::System && size_ != 0) {
        throw std::invalid_argument("a System message must be the first message");
    }

    if (size_ == capacity_) {
        // Allocate the bigger buffer first so a failed new leaves us unchanged.
        std::size_t new_cap = capacity_ == 0 ? kInitialCapacity : capacity_ * kGrowthFactor;
        Message* buf = new Message[new_cap];
        for (std::size_t i = 0; i < size_; ++i) buf[i] = std::move(data_[i]);
        delete[] data_;
        data_ = buf;
        capacity_ = new_cap;
    }

    data_[size_] = std::move(m);
    ++size_;
}

std::size_t Conversation::size() const noexcept { return size_; }

std::size_t Conversation::capacity() const noexcept { return capacity_; }

const Message& Conversation::at(std::size_t i) const {
    if (i >= size_) throw std::out_of_range("Conversation::at: index out of range");
    return data_[i];
}

const Message* Conversation::begin() const noexcept { return data_; }

const Message* Conversation::end() const noexcept { return data_ + size_; }

void Conversation::swap(Conversation& other) noexcept {
    std::swap(data_, other.data_);
    std::swap(size_, other.size_);
    std::swap(capacity_, other.capacity_);
}
