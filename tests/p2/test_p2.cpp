// tests/p2/test_p2.cpp
// Author: Diego Mejia
//
// Assert-based tests for Project 2 (spec §5). Build with the provided
// CMakeLists.txt so AddressSanitizer checks every test for leaks and
// memory errors.

#undef NDEBUG  // keep asserts on even in a Release build
#include "core/conversation.h"
#include "core/message.h"
#include "core/sentinel_scanner.h"
#include "harness/harness.h"
#include "model/replay_client.h"
#include "model/scripted_client.h"

#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#define TEST(name) static void name()
#define RUN(name) do { name(); std::printf("[PASS] %s\n", #name); ++passed; } while (0)

const std::string SENTINEL = "<|end_conversation|>";

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Returns an InputSource that gives back the lines in order, then EOF.
class LinesInput : public InputSource {
public:
    explicit LinesInput(std::vector<std::string> lines) : lines_(std::move(lines)) {}
    std::string read_line() override {
        if (next_ == lines_.size()) { eof_ = true; return ""; }
        return lines_[next_++];
    }
    bool is_eof() const override { return eof_; }
    std::size_t lines_read() const { return next_; }

private:
    std::vector<std::string> lines_;
    std::size_t next_ = 0;
    bool eof_ = false;
};

// Records everything the harness prints.
class StringOutput : public OutputSink {
public:
    void write(std::string_view text) override { all += text; }
    std::string all;
};

// Test files go in the temp directory so the tests run from any folder.
std::string temp_file(const std::string& name, const std::string& contents) {
    std::string path = (std::filesystem::temp_directory_path() / name).string();
    std::ofstream(path) << contents;
    return path;
}

// Feeds text one character at a time and returns everything emitted.
std::string feed_one_at_a_time(SentinelScanner& s, const std::string& text, bool& found) {
    std::string out;
    found = false;
    for (char c : text) {
        auto r = s.feed(std::string(1, c));
        out += r.safe_text;
        if (r.sentinel_found) { found = true; return out; }
    }
    return out + s.flush().safe_text;
}

// Same format as save_transcript() in main.cpp (which tests can't link to).
void save_transcript(const Conversation& conv, const std::string& path) {
    const char* names[] = {"system", "user", "assistant"};
    std::ofstream f(path);
    for (const Message& m : conv) {
        if (&m != conv.begin()) f << "---\n";
        f << "role: " << names[static_cast<int>(m.role())] << "\n" << m.content() << "\n";
    }
}

// ---------------------------------------------------------------------------
// Conversation
// ---------------------------------------------------------------------------

TEST(MessageDefaultIsEmptySystem) {
    Message m;
    assert(m.role() == Role::System && m.content().empty());
    Message u(Role::User, "hi");
    assert(u.role() == Role::User && u.content() == "hi");
}

// 1. Empty conversation bounds
TEST(EmptyConversationBounds) {
    Conversation c;
    assert(c.size() == 0 && c.capacity() == 0);
    assert(c.begin() == c.end());

    bool threw = false;
    try { c.at(0); } catch (const std::out_of_range&) { threw = true; }
    assert(threw);

    c.append(Message(Role::User, "one"));
    assert(c.at(0).content() == "one");
    threw = false;
    try { c.at(1); } catch (const std::out_of_range&) { threw = true; }
    assert(threw);
}

// 2. System message ordering
TEST(SystemMessageStaysFirst) {
    Conversation c;
    c.append(Message(Role::System, "Be concise."));
    for (int i = 0; i < 50; ++i) c.append(Message(Role::User, "msg"));  // several regrowths
    assert(c.at(0).role() == Role::System && c.at(0).content() == "Be concise.");

    bool threw = false;
    try { c.append(Message(Role::System, "late")); } catch (const std::invalid_argument&) { threw = true; }
    assert(threw && c.size() == 51);
}

// 3. Rule of Five: copy
TEST(CopyIsDeep) {
    Conversation a;
    a.append(Message(Role::User, "hello"));
    a.append(Message(Role::Assistant, "hi"));

    Conversation b(a);
    assert(b.begin() != a.begin());
    assert(b.size() == 2 && b.at(1).content() == "hi");

    a.append(Message(Role::User, "only in a"));
    assert(a.size() == 3 && b.size() == 2);

    Conversation c;
    c.append(Message(Role::User, "overwritten"));
    c = a;
    assert(c.begin() != a.begin() && c.size() == 3);
    Conversation& same = c;
    c = same;  // self-assignment
    assert(c.size() == 3 && c.at(0).content() == "hello");

    Conversation empty;
    Conversation empty_copy(empty);
    assert(empty_copy.begin() != empty.begin() && empty_copy.size() == 0);
}

// 4. Rule of Five: move
TEST(MoveStealsPointer) {
    Conversation a;
    a.append(Message(Role::User, "hello"));
    const Message* data = a.begin();

    Conversation b(std::move(a));
    assert(b.begin() == data && b.size() == 1);
    assert(a.begin() == nullptr && a.size() == 0 && a.capacity() == 0);

    Conversation c;
    c.append(Message(Role::User, "freed by the move"));
    c = std::move(b);
    assert(c.begin() == data && c.at(0).content() == "hello");
    assert(b.begin() == nullptr && b.size() == 0);

    a.append(Message(Role::User, "moved-from is still usable"));
    assert(a.size() == 1);
}

// 5. Growth behavior
TEST(CapacityDoubles) {
    Conversation c;
    std::size_t expected = 0;
    for (std::size_t n = 0; n < 1000; ++n) {
        if (n == expected) expected = (expected == 0) ? 4 : expected * 2;
        c.append(Message(Role::User, std::to_string(n)));
        assert(c.size() == n + 1);
        assert(c.capacity() == expected);
    }
    for (std::size_t i = 0; i < c.size(); ++i) assert(c.at(i).content() == std::to_string(i));
}

// ---------------------------------------------------------------------------
// SentinelScanner
// ---------------------------------------------------------------------------

// 6. Clean text
TEST(ScannerCleanText) {
    const std::string text = "Hello, world. There is no stop marker in this reply.";
    SentinelScanner s(SENTINEL);
    auto r = s.feed(text);
    auto f = s.flush();
    assert(!r.sentinel_found && !f.sentinel_found);
    assert(r.safe_text + f.safe_text == text);
}

TEST(ScannerWholeSentinelInOneChunk) {
    SentinelScanner s(SENTINEL);
    auto r = s.feed("Goodbye." + SENTINEL + "ignored");
    assert(r.sentinel_found && r.safe_text == "Goodbye.");
}

// 7. Split sentinel (the spec's sample test)
TEST(ScannerCatchesSentinelAtEveryBoundary) {
    const std::string text = "Goodbye." + SENTINEL;
    for (std::size_t split = 0; split <= text.size(); ++split) {
        SentinelScanner scanner(SENTINEL);
        auto out1 = scanner.feed(text.substr(0, split));
        auto out2 = scanner.feed(text.substr(split));
        assert((out1.sentinel_found || out2.sentinel_found) &&
               "sentinel must be caught regardless of split point");
        assert(out1.safe_text + out2.safe_text == "Goodbye.");
    }
}

TEST(ScannerOneCharacterAtATime) {
    SentinelScanner s(SENTINEL);
    bool found = false;
    assert(feed_one_at_a_time(s, "See you!" + SENTINEL, found) == "See you!");
    assert(found);
}

// 8. False alarms
TEST(ScannerIgnoresPartialMatches) {
    const std::string texts[] = {"<|end_world|>", "<|end_conversation|", "<|END_CONVERSATION|>"};
    for (const std::string& t : texts) {
        SentinelScanner s(SENTINEL);
        bool found = true;
        assert(feed_one_at_a_time(s, t, found) == t);
        assert(!found);
    }
}

// 9. Bounded memory
TEST(ScannerPendingStaysBounded) {
    SentinelScanner s(SENTINEL);
    std::size_t fed = 0, emitted = 0;
    while (fed < 4 * 1024 * 1024) {  // 4 MB of near-misses, one byte at a time
        for (char c : std::string("<|end_conversation|")) {
            auto r = s.feed(std::string(1, c));
            assert(!r.sentinel_found);
            emitted += r.safe_text.size();
            ++fed;
            assert(s.pending_size() <= SENTINEL.size() - 1);
        }
    }
    assert(emitted + s.pending_size() == fed);  // nothing lost
}

// ---------------------------------------------------------------------------
// Provided Harness and ModelClient
// ---------------------------------------------------------------------------

// 10. Turn limit
TEST(HarnessStopsAtTurnLimit) {
    std::string path = temp_file("p2_turns.script",
        "role: assistant\none\n---\nrole: assistant\ntwo\n---\nrole: assistant\nthree\n");
    HarnessConfig cfg;
    cfg.max_turns = 2;
    Harness h(std::make_unique<ScriptedModelClient>(path), cfg);
    LinesInput in({"a", "b", "c"});
    StringOutput out;

    StopReason r = h.run(in, out);
    assert(r.kind == StopReason::Kind::TurnLimit);
    assert(h.conversation().size() == 4);  // 2 user + 2 assistant
    assert(in.lines_read() == 2);
}

// 11. Sentinel halt
TEST(HarnessHaltsAtSentinel) {
    std::string path = temp_file("p2_sentinel.script",
        "chunk: 3\nrole: assistant\nGoodbye.<|end_conversation|>\n---\n"
        "role: assistant\nnever used\n");
    Harness h(std::make_unique<ScriptedModelClient>(path), HarnessConfig{});
    LinesInput in({"bye", "still there?"});
    StringOutput out;

    StopReason r = h.run(in, out);
    assert(r.kind == StopReason::Kind::Sentinel);
    assert(in.lines_read() == 1);
    assert(out.all.find("Goodbye.") != std::string::npos);
    assert(out.all.find("<|") == std::string::npos);  // sentinel never printed
    assert(h.conversation().at(1).content() == "Goodbye." + SENTINEL);  // but stored
}

TEST(HarnessStopsOnEof) {
    std::string path = temp_file("p2_eof.script", "role: assistant\nhi\n");
    Harness h(std::make_unique<ScriptedModelClient>(path), HarnessConfig{});
    LinesInput in(std::vector<std::string>{});  // immediate EOF
    StringOutput out;
    assert(h.run(in, out).kind == StopReason::Kind::UserExit);
    assert(h.conversation().size() == 0);
}

// 12. Transcript round trip
TEST(TranscriptRoundTrip) {
    std::string script = temp_file("p2_roundtrip.script",
        "role: system\nBe concise.\n---\n"
        "chunk: 4\nrole: assistant\nHi! What can I do for you today?\n---\n"
        "chunk: 5\nrole: assistant\nGoodbye.<|end_conversation|>\n");
    auto scripted = std::make_unique<ScriptedModelClient>(script);
    HarnessConfig cfg1;
    cfg1.system_message = scripted->system_message();
    Harness first(std::move(scripted), cfg1);
    LinesInput in1({"hello", "bye"});
    StringOutput out1;
    first.run(in1, out1);

    std::string transcript = (std::filesystem::temp_directory_path() / "p2_transcript.txt").string();
    save_transcript(first.conversation(), transcript);

    auto replay = std::make_unique<ReplayModelClient>(transcript);
    HarnessConfig cfg2;
    cfg2.system_message = replay->system_message();
    Harness second(std::move(replay), cfg2);
    LinesInput in2({"hello", "bye"});
    StringOutput out2;
    second.run(in2, out2);

    assert(out2.all == out1.all);
    const Conversation& a = first.conversation();
    const Conversation& b = second.conversation();
    assert(a.size() == 5 && b.size() == 5);
    for (std::size_t i = 0; i < a.size(); ++i) {
        assert(a.at(i).role() == b.at(i).role());
        assert(a.at(i).content() == b.at(i).content());
    }
}

int main() {
    int passed = 0;
    RUN(MessageDefaultIsEmptySystem);
    RUN(EmptyConversationBounds);
    RUN(SystemMessageStaysFirst);
    RUN(CopyIsDeep);
    RUN(MoveStealsPointer);
    RUN(CapacityDoubles);
    RUN(ScannerCleanText);
    RUN(ScannerWholeSentinelInOneChunk);
    RUN(ScannerCatchesSentinelAtEveryBoundary);
    RUN(ScannerOneCharacterAtATime);
    RUN(ScannerIgnoresPartialMatches);
    RUN(ScannerPendingStaysBounded);
    RUN(HarnessStopsAtTurnLimit);
    RUN(HarnessHaltsAtSentinel);
    RUN(HarnessStopsOnEof);
    RUN(TranscriptRoundTrip);
    std::printf("\n%d tests passed\n", passed);
    return 0;
}
