#include "core/transform_engine.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace {

using deutschtelex::core::Action;
using deutschtelex::core::ActionKind;
using deutschtelex::core::TransformConfig;
using deutschtelex::core::TransformEngine;

constexpr std::uint64_t kSeed = 0xD3E075C1E7E10070ULL;

struct TestRunner {
    int passed{};
    int failed{};

    void Check(const bool condition, const std::string_view name,
               const std::string& detail = {}) {
        if (condition) {
            ++passed;
            return;
        }
        ++failed;
        std::cerr << "FAILED: " << name;
        if (!detail.empty()) {
            std::cerr << " (" << detail << ')';
        }
        std::cerr << '\n';
    }
};

class FixedRandom {
public:
    explicit FixedRandom(const std::uint64_t seed) noexcept : state_(seed) {}

    std::uint64_t Next() noexcept {
        state_ ^= state_ >> 12U;
        state_ ^= state_ << 25U;
        state_ ^= state_ >> 27U;
        return state_ * 0x2545F4914F6CDD1DULL;
    }

    std::size_t Index(const std::size_t size) noexcept {
        return static_cast<std::size_t>(Next() % size);
    }

private:
    std::uint64_t state_;
};

enum class EventKind : std::uint8_t {
    Character,
    Reset,
    Configure,
};

struct Event {
    EventKind kind{};
    char32_t character{};
    bool enable_eszett{};
};

struct ActionRecord {
    ActionKind kind{};
    std::u32string replacement;

    bool operator==(const ActionRecord&) const = default;
};

struct RunResult {
    std::u32string visible;
    std::vector<ActionRecord> actions;
    bool valid_actions{true};
    bool bounded_output{true};
    bool disabled_eszett_emitted{false};

    bool operator==(const RunResult&) const = default;
};

bool ValidReplacement(const Action& action) {
    switch (action.kind) {
    case ActionKind::Pass:
        return action.replacement.empty();
    case ActionKind::ReplacePrevious:
        return action.replacement == U"\u00E4" ||
               action.replacement == U"\u00F6" ||
               action.replacement == U"\u00FC" ||
               action.replacement == U"\u00C4" ||
               action.replacement == U"\u00D6" ||
               action.replacement == U"\u00DC" ||
               action.replacement == U"\u00DF";
    case ActionKind::ReplaceConverted:
        return action.replacement == U"ae" || action.replacement == U"oe" ||
               action.replacement == U"ue" || action.replacement == U"Ae" ||
               action.replacement == U"Oe" || action.replacement == U"Ue" ||
               action.replacement == U"sz";
    }
    return false;
}

RunResult Run(const std::vector<Event>& events) {
    TransformEngine engine;
    RunResult result;
    std::size_t processed_characters{};
    bool eszett_enabled = true;

    for (const Event& event : events) {
        if (event.kind == EventKind::Reset) {
            engine.Reset();
            continue;
        }
        if (event.kind == EventKind::Configure) {
            eszett_enabled = event.enable_eszett;
            engine.SetConfig({eszett_enabled});
            continue;
        }

        ++processed_characters;
        const Action action = engine.Process(event.character);
        result.valid_actions = result.valid_actions && ValidReplacement(action);
        result.actions.push_back({action.kind, std::u32string{action.replacement}});
        if (action.kind == ActionKind::Pass) {
            result.visible.push_back(event.character);
        } else if (result.visible.empty()) {
            result.valid_actions = false;
        } else {
            result.visible.pop_back();
            result.visible.append(action.replacement);
        }
        if (!eszett_enabled && action.replacement.find(U'\u00DF') !=
                                    std::u32string_view::npos) {
            result.disabled_eszett_emitted = true;
        }
        result.bounded_output = result.bounded_output &&
                                result.visible.size() <= processed_characters;
    }
    return result;
}

std::vector<Event> GenerateEvents(FixedRandom& random, const std::size_t count) {
    constexpr std::u32string_view alphabet =
        U"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
        U" \t\n.,!?;:-_()[]{}'\"/\\@#$%^&*+=|~`";
    std::vector<Event> events;
    events.reserve(count);
    for (std::size_t index = 0; index < count; ++index) {
        const std::uint64_t choice = random.Next() % 100U;
        if (choice < 4U) {
            events.push_back({EventKind::Reset});
        } else if (choice < 8U) {
            events.push_back({EventKind::Configure, U'\0',
                              (random.Next() & 1U) != 0U});
        } else {
            events.push_back({EventKind::Character,
                              alphabet[random.Index(alphabet.size())]});
        }
    }
    return events;
}

std::u32string TransformRepeated(const std::u32string_view pattern,
                                 const std::size_t repetitions) {
    TransformEngine engine;
    std::u32string output;
    output.reserve(pattern.size() * repetitions);
    for (std::size_t repetition = 0; repetition < repetitions; ++repetition) {
        for (const char32_t character : pattern) {
            const Action action = engine.Process(character);
            if (action.kind == ActionKind::Pass) {
                output.push_back(character);
            } else {
                if (output.empty()) {
                    return {};
                }
                output.pop_back();
                output.append(action.replacement);
            }
        }
    }
    return output;
}

}  // namespace

int main() {
    static_assert(std::is_trivially_copyable_v<TransformEngine::Snapshot>);
    static_assert(sizeof(TransformEngine::Snapshot) <= 2U);
    static_assert(sizeof(TransformEngine) <= 8U);

    TestRunner runner;
    FixedRandom random{kSeed};

    bool deterministic = true;
    bool valid_actions = true;
    bool bounded_output = true;
    bool disabled_eszett_safe = true;
    for (std::size_t sequence = 0; sequence < 256U; ++sequence) {
        const std::vector<Event> events = GenerateEvents(random, 4096U);
        const RunResult first = Run(events);
        const RunResult second = Run(events);
        deterministic = deterministic && first == second;
        valid_actions = valid_actions && first.valid_actions;
        bounded_output = bounded_output && first.bounded_output;
        disabled_eszett_safe = disabled_eszett_safe &&
                               !first.disabled_eszett_emitted;
        if (!(first == second && first.valid_actions && first.bounded_output &&
              !first.disabled_eszett_emitted)) {
            std::cerr << "Reproduce with seed 0x" << std::hex << kSeed << std::dec
                      << ", sequence " << sequence << ", 4096 events.\n";
            break;
        }
    }
    runner.Check(deterministic, "fixed-seed generated streams are deterministic");
    runner.Check(valid_actions, "generated streams emit only valid semantic actions");
    runner.Check(bounded_output, "generated streams never produce more output than input");
    runner.Check(disabled_eszett_safe, "disabled eszett never emits sharp-s");

    {
        TransformEngine first;
        TransformEngine second;
        static_cast<void>(first.Process(U'a'));
        static_cast<void>(second.Process(U'o'));
        const Action first_result = first.Process(U'e');
        const Action second_result = second.Process(U'e');
        runner.Check(first_result.replacement == U"\u00E4" &&
                         second_result.replacement == U"\u00F6",
                     "independent engines retain independent state");
    }

    {
        TransformEngine engine;
        static_cast<void>(engine.Process(U'a'));
        engine.Reset();
        const Action e = engine.Process(U'e');
        engine.Reset();
        static_cast<void>(engine.Process(U's'));
        engine.SetConfig(TransformConfig{false});
        const Action z = engine.Process(U'z');
        runner.Check(e.kind == ActionKind::Pass && z.kind == ActionKind::Pass,
                     "Reset and configuration changes clear semantic state");
    }

    constexpr std::size_t kLongRepetitions = 100000U;
    runner.Check(TransformRepeated(U"ae", kLongRepetitions) ==
                     std::u32string(kLongRepetitions, U'\u00E4'),
                 "long repeated ae stream remains stable");
    const auto check_repeated_literal = [&runner](const std::u32string_view pattern,
                                                   const std::u32string_view unit,
                                                   const std::string_view name) {
        constexpr std::size_t repetitions = 50000U;
        std::u32string expected;
        expected.reserve(unit.size() * repetitions);
        for (std::size_t index = 0; index < repetitions; ++index) {
            expected.append(unit);
        }
        runner.Check(TransformRepeated(pattern, repetitions) == expected, name);
    };
    check_repeated_literal(U"aee", U"ae", "long repeated aee escape stream remains stable");
    check_repeated_literal(U"oee", U"oe", "long repeated oee escape stream remains stable");
    check_repeated_literal(U"uee", U"ue", "long repeated uee escape stream remains stable");
    check_repeated_literal(U"szz", U"sz", "long repeated szz escape stream remains stable");
    runner.Check(TransformRepeated(U"sz", kLongRepetitions) ==
                     std::u32string(kLongRepetitions, U'\u00DF'),
                 "long repeated sz stream remains stable");

    std::cout << runner.passed << " tests passed; " << runner.failed
              << " tests failed. Fixed seed: 0x" << std::hex << kSeed << std::dec
              << ".\n";
    return runner.failed == 0 ? 0 : 1;
}
