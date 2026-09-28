# ECE 309 — Project 2: The Conversation Loop

**Author:** Diego Mejia

A C++ `miniharness` that alternates user and model turns, streams the
model's replies in chunks, and stops on the `<|end_conversation|>` sentinel,
a turn limit, or EOF.

## Files

Provided starter code (unmodified): `include/model/`, `include/harness/`,
`src/model_client.cpp`, `src/scripted_client.cpp`, `src/replay_client.cpp`,
`src/harness.cpp`, `src/main.cpp`, `CMakeLists.txt`.

My code:

- `include/core/message.h`: `Role` and `Message`
- `include/core/conversation.h`, `src/conversation.cpp`: growable array
  (starts at 4, doubles when full), Rule of Five
- `include/core/sentinel_scanner.h`, `src/sentinel_scanner.cpp`: finds the
  sentinel across chunk boundaries, holding back at most 19 characters
- `tests/p2/test_p2.cpp`: 16 assert-based tests
- `docs/design-log-p2.md`: design log

Behavior choices:

- `at(i)` throws `std::out_of_range` when `i >= size()`.
- Appending a System message to a non-empty conversation throws
  `std::invalid_argument`, so the system message is always first.
- `capacity()` and `pending_size()` were added so the tests can check
  growth and the pending-buffer bound.

## Build and run

Needs CMake and a compiler with AddressSanitizer (GCC or Clang on
Linux/macOS).

```bash
cmake -S . -B build
cmake --build build
./build/test_p2
./build/miniharness --script scripts/greeting.script --save transcript.txt
```

Press Ctrl-D to end the conversation early; the transcript is still saved.
