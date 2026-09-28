// include/core/message.h
// Author: Diego Mejia
//
// A single conversation turn: who said it (Role) and what was said.
// Spec §3.1.

#pragma once
#include <string>
#include <utility>

enum class Role { System, User, Assistant };

class Message {
public:
    // Default-constructs an empty System message with empty content.
    // Needed so Conversation can allocate raw array slots before
    // append() fills them in.
    Message() : role_(Role::System) {}

    Message(Role role, std::string content)
        : role_(role), content_(std::move(content)) {}

    Role               role()    const noexcept { return role_; }     // Who sent this message.
    const std::string& content() const noexcept { return content_; }  // The message text.

private:
    Role        role_;
    std::string content_;
};
