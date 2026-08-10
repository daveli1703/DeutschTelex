#include "core/transform_engine.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using deutschtelex::core::Action;
using deutschtelex::core::ActionKind;
using deutschtelex::core::SingleTypoCheckpoint;
using deutschtelex::core::TransformEngine;

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

std::string Describe(const std::u32string_view text) {
    std::ostringstream output;
    output << std::uppercase << std::hex << std::setfill('0');
    for (const char32_t character : text) {
        output << "U+" << std::setw(4) << static_cast<std::uint32_t>(character) << ' ';
    }
    return output.str();
}

void Apply(std::u32string& output, const char32_t input, const Action& action) {
    switch (action.kind) {
    case ActionKind::Pass:
        output.push_back(input);
        break;
    case ActionKind::ReplacePrevious:
    case ActionKind::ReplaceConverted:
        if (output.empty()) {
            throw std::logic_error("replacement action without preceding output");
        }
        output.pop_back();
        output.append(action.replacement);
        break;
    }
}

std::u32string Transform(const std::u32string_view input) {
    TransformEngine engine;
    std::u32string output;
    for (const char32_t character : input) {
        Apply(output, character, engine.Process(character));
    }
    return output;
}

void CheckTransform(TestRunner& runner, const std::string_view name,
                    const std::u32string_view input,
                    const std::u32string_view expected) {
    const std::u32string actual = Transform(input);
    runner.Check(actual == expected, name,
                 "expected " + Describe(expected) + "but got " + Describe(actual));
}

struct ActionSnapshot {
    ActionKind kind;
    std::u32string replacement;

    bool operator==(const ActionSnapshot&) const = default;
};

std::vector<ActionSnapshot> CaptureActions(const std::u32string_view input) {
    TransformEngine engine;
    std::vector<ActionSnapshot> actions;
    actions.reserve(input.size());
    for (const char32_t character : input) {
        const Action action = engine.Process(character);
        actions.push_back({action.kind, std::u32string{action.replacement}});
    }
    return actions;
}

void CheckResetAfterPrefix(TestRunner& runner, const char32_t prefix,
                           const char32_t suffix, const std::string_view name) {
    TransformEngine engine;
    std::u32string output;
    Apply(output, prefix, engine.Process(prefix));
    engine.Reset();
    const Action action_after_reset = engine.Process(suffix);
    Apply(output, suffix, action_after_reset);

    std::u32string expected;
    expected.push_back(prefix);
    expected.push_back(suffix);
    runner.Check(action_after_reset.kind == ActionKind::Pass && output == expected, name,
                 "expected " + Describe(expected) + "but got " + Describe(output));
}

void CheckResetAfterConversion(TestRunner& runner, const std::u32string_view input,
                               const char32_t suffix,
                               const std::u32string_view expected,
                               const std::string_view name) {
    TransformEngine engine;
    std::u32string output;
    for (const char32_t character : input) {
        Apply(output, character, engine.Process(character));
    }
    engine.Reset();
    const Action action_after_reset = engine.Process(suffix);
    Apply(output, suffix, action_after_reset);
    runner.Check(action_after_reset.kind == ActionKind::Pass && output == expected, name,
                 "expected " + Describe(expected) + "but got " + Describe(output));
}

void ProcessPrintable(TransformEngine& engine, SingleTypoCheckpoint& checkpoint,
                      std::u32string& output, const char32_t input) {
    checkpoint.BeginPrintable(engine);
    const Action action = engine.Process(input);
    checkpoint.CompletePrintable(action);
    Apply(output, input, action);
}

bool ProcessPhysicalBackspace(TransformEngine& engine, SingleTypoCheckpoint& checkpoint,
                              std::u32string& output) {
    if (output.empty()) {
        throw std::logic_error("Backspace without preceding output");
    }
    output.pop_back();
    if (checkpoint.RestoreAfterBackspace(engine)) {
        return true;
    }
    engine.Reset();
    return false;
}

void CheckSingleTypoCorrection(TestRunner& runner, const char32_t prefix,
                               const char32_t suffix,
                               const std::u32string_view expected,
                               const std::string_view name) {
    TransformEngine engine;
    SingleTypoCheckpoint checkpoint;
    std::u32string output;
    ProcessPrintable(engine, checkpoint, output, prefix);
    ProcessPrintable(engine, checkpoint, output, U'w');
    const bool restored = ProcessPhysicalBackspace(engine, checkpoint, output);
    ProcessPrintable(engine, checkpoint, output, suffix);
    runner.Check(restored && output == expected, name,
                 "expected " + Describe(expected) + "but got " + Describe(output));
}

}  // namespace

int main() {
    TestRunner runner;

    const std::array basic_cases{
        std::pair{U"ae", U"\u00E4"},
        std::pair{U"aee", U"ae"},
        std::pair{U"oe", U"\u00F6"},
        std::pair{U"oee", U"oe"},
        std::pair{U"ue", U"\u00FC"},
        std::pair{U"uee", U"ue"},
        std::pair{U"Ae", U"\u00C4"},
        std::pair{U"Aee", U"Ae"},
        std::pair{U"Oe", U"\u00D6"},
        std::pair{U"Oee", U"Oe"},
        std::pair{U"Ue", U"\u00DC"},
        std::pair{U"Uee", U"Ue"},
        std::pair{U"sz", U"\u00DF"},
        std::pair{U"szz", U"sz"},
        std::pair{U"ss", U"ss"},
    };
    for (std::size_t index = 0; index < basic_cases.size(); ++index) {
        CheckTransform(runner, "basic mapping " + std::to_string(index + 1),
                       basic_cases[index].first, basic_cases[index].second);
    }

    // After conversion, the third suffix escapes to the literal pair. Further
    // suffixes pass normally because the escape returns the engine to Idle.
    const std::array repeated_cases{
        std::pair{U"aeee", U"aee"},
        std::pair{U"aeeee", U"aeee"},
        std::pair{U"oeee", U"oee"},
        std::pair{U"oeeee", U"oeee"},
        std::pair{U"ueee", U"uee"},
        std::pair{U"ueeee", U"ueee"},
        std::pair{U"szzz", U"szz"},
        std::pair{U"szzzz", U"szzz"},
    };
    for (std::size_t index = 0; index < repeated_cases.size(); ++index) {
        CheckTransform(runner, "repeated sequence " + std::to_string(index + 1),
                       repeated_cases[index].first, repeated_cases[index].second);
    }

    const std::array overlap_cases{
        std::pair{U"aae", U"a\u00E4"},
        std::pair{U"aaee", U"aae"},
        std::pair{U"ooe", U"o\u00F6"},
        std::pair{U"ooee", U"ooe"},
        std::pair{U"uue", U"u\u00FC"},
        std::pair{U"uuee", U"uue"},
        std::pair{U"ssz", U"s\u00DF"},
        std::pair{U"sszz", U"ssz"},
        std::pair{U"asasze", U"asa\u00DFe"},
        std::pair{U"aoe", U"a\u00F6"},
        std::pair{U"aue", U"a\u00FC"},
        std::pair{U"oue", U"o\u00FC"},
    };
    for (std::size_t index = 0; index < overlap_cases.size(); ++index) {
        CheckTransform(runner, "prefix overlap " + std::to_string(index + 1),
                       overlap_cases[index].first, overlap_cases[index].second);
    }

    const std::array word_cases{
        std::pair{U"Maedchen", U"M\u00E4dchen"},
        std::pair{U"schoen", U"sch\u00F6n"},
        std::pair{U"fuer", U"f\u00FCr"},
        std::pair{U"Haeuser", U"H\u00E4user"},
        std::pair{U"Koeln", U"K\u00F6ln"},
        std::pair{U"Mueller", U"M\u00FCller"},
        std::pair{U"dass", U"dass"},
        std::pair{U"wissen", U"wissen"},
        std::pair{U"mussen", U"mussen"},
    };
    for (std::size_t index = 0; index < word_cases.size(); ++index) {
        CheckTransform(runner, "German-like text " + std::to_string(index + 1),
                       word_cases[index].first, word_cases[index].second);
    }

    const std::array unsupported_cases{
        U"AE", U"OE", U"UE", U"aE", U"oE", U"uE", U"SZ", U"Sz",
        U"xAEy", U"preOEpost", U"UEber", U"xaEy", U"xoEy", U"xuEy",
        U"xSZy", U"xSzy",
    };
    for (const std::u32string_view input : unsupported_cases) {
        CheckTransform(runner, "unsupported capitalization " + Describe(input), input, input);
    }

    constexpr std::array boundaries{
        U' ', U'\t', U'\n', U'.', U',', U'!', U'?', U':', U';', U'-', U'_',
        U'(', U')', U'[', U']', U'{', U'}', U'"', U'\'',
    };
    for (const char32_t boundary : boundaries) {
        std::u32string after_input{U"ae"};
        after_input.push_back(boundary);
        std::u32string after_expected{U"\u00E4"};
        after_expected.push_back(boundary);
        CheckTransform(runner, "boundary after ae " + Describe({&boundary, 1}),
                       after_input, after_expected);

        std::u32string before_input(1, boundary);
        before_input.append(U"ae");
        std::u32string before_expected(1, boundary);
        before_expected.append(U"\u00E4");
        CheckTransform(runner, "boundary before ae " + Describe({&boundary, 1}),
                       before_input, before_expected);
    }
    CheckTransform(runner, "parenthesized ae", U"(ae)", U"(\u00E4)");
    CheckTransform(runner, "ae before hyphen", U"ae-test", U"\u00E4-test");
    CheckTransform(runner, "ae after hyphen", U"test-ae", U"test-\u00E4");

    const std::array mixed_cases{
        std::pair{U"123456", U"123456"},
        std::pair{U"a1e", U"a1e"},
        std::pair{U"a2e", U"a2e"},
        std::pair{U"2026ae", U"2026\u00E4"},
        std::pair{U"ae2026", U"\u00E42026"},
        std::pair{U"123sz456", U"123\u00DF456"},
    };
    for (std::size_t index = 0; index < mixed_cases.size(); ++index) {
        CheckTransform(runner, "number/mixed content " + std::to_string(index + 1),
                       mixed_cases[index].first, mixed_cases[index].second);
    }

    {
        TransformEngine engine;
        engine.Reset();
        runner.Check(engine.Process(U'e').kind == ActionKind::Pass,
                     "Reset while Idle is harmless");
    }
    CheckResetAfterPrefix(runner, U'a', U'e', "Reset after a");
    CheckResetAfterPrefix(runner, U'A', U'e', "Reset after A");
    CheckResetAfterPrefix(runner, U'o', U'e', "Reset after o");
    CheckResetAfterPrefix(runner, U'O', U'e', "Reset after O");
    CheckResetAfterPrefix(runner, U'u', U'e', "Reset after u");
    CheckResetAfterPrefix(runner, U'U', U'e', "Reset after U");
    CheckResetAfterPrefix(runner, U's', U'z', "Reset after s");

    CheckResetAfterConversion(runner, U"ae", U'e', U"\u00E4e", "Reset after ae");
    CheckResetAfterConversion(runner, U"oe", U'e', U"\u00F6e", "Reset after oe");
    CheckResetAfterConversion(runner, U"ue", U'e', U"\u00FCe", "Reset after ue");
    CheckResetAfterConversion(runner, U"Ae", U'e', U"\u00C4e", "Reset after Ae");
    CheckResetAfterConversion(runner, U"Oe", U'e', U"\u00D6e", "Reset after Oe");
    CheckResetAfterConversion(runner, U"Ue", U'e', U"\u00DCe", "Reset after Ue");
    CheckResetAfterConversion(runner, U"sz", U'z', U"\u00DFz", "Reset after sz");

    CheckSingleTypoCorrection(runner, U'a', U'e', U"\u00E4",
                              "single typo restores lowercase a prefix");
    CheckSingleTypoCorrection(runner, U'o', U'e', U"\u00F6",
                              "single typo restores lowercase o prefix");
    CheckSingleTypoCorrection(runner, U'u', U'e', U"\u00FC",
                              "single typo restores lowercase u prefix");
    CheckSingleTypoCorrection(runner, U's', U'z', U"\u00DF",
                              "single typo restores sharp-s prefix");
    CheckSingleTypoCorrection(runner, U'A', U'e', U"\u00C4",
                              "single typo restores uppercase A prefix");
    CheckSingleTypoCorrection(runner, U'O', U'e', U"\u00D6",
                              "single typo restores uppercase O prefix");
    CheckSingleTypoCorrection(runner, U'U', U'e', U"\u00DC",
                              "single typo restores uppercase U prefix");

    {
        TransformEngine engine;
        SingleTypoCheckpoint checkpoint;
        std::u32string output;
        ProcessPrintable(engine, checkpoint, output, U'a');
        ProcessPrintable(engine, checkpoint, output, U'w');
        ProcessPrintable(engine, checkpoint, output, U'x');
        const bool restored = ProcessPhysicalBackspace(engine, checkpoint, output);
        ProcessPrintable(engine, checkpoint, output, U'e');
        runner.Check(!restored && output == U"awe",
                     "second printable invalidates old typo checkpoint",
                     "expected " + Describe(U"awe") + "but got " + Describe(output));
    }

    for (const std::string_view context_change :
         {"Left", "Enter", "Ctrl+C", "foreground change", "foreign injection"}) {
        TransformEngine engine;
        SingleTypoCheckpoint checkpoint;
        std::u32string output;
        ProcessPrintable(engine, checkpoint, output, U'a');
        ProcessPrintable(engine, checkpoint, output, U'w');
        checkpoint.Invalidate();
        engine.Reset();
        const bool restored = checkpoint.RestoreAfterBackspace(engine);
        const Action action = engine.Process(U'e');
        runner.Check(!restored && action.kind == ActionKind::Pass,
                     "context change invalidates checkpoint: " + std::string(context_change));
    }

    {
        TransformEngine engine;
        SingleTypoCheckpoint checkpoint;
        std::u32string output;
        ProcessPrintable(engine, checkpoint, output, U'a');
        ProcessPrintable(engine, checkpoint, output, U'e');
        ProcessPrintable(engine, checkpoint, output, U'w');
        const bool restored = ProcessPhysicalBackspace(engine, checkpoint, output);
        const std::u32string after_backspace = output;
        ProcessPrintable(engine, checkpoint, output, U'e');
        runner.Check(!restored && after_backspace == U"\u00E4" && output == U"\u00E4e",
                     "typo after converted character never restores pre-conversion state",
                     "expected " + Describe(U"\u00E4e") + "but got " + Describe(output));
    }

    {
        TransformEngine engine;
        engine.Reset();
        engine.Reset();
        engine.Reset();
        std::u32string output;
        Apply(output, U'a', engine.Process(U'a'));
        Apply(output, U'e', engine.Process(U'e'));
        runner.Check(output == U"\u00E4", "repeated Reset is harmless");
    }

    {
        constexpr std::u32string_view input = U"Maedchen 2026: schoen, fuer; szz AE!";
        const std::u32string expected_output = Transform(input);
        const std::vector<ActionSnapshot> expected_actions = CaptureActions(input);
        for (int repetition = 1; repetition <= 10; ++repetition) {
            runner.Check(Transform(input) == expected_output &&
                             CaptureActions(input) == expected_actions,
                         "deterministic run " + std::to_string(repetition));
        }
    }

    {
        TransformEngine engine_a;
        TransformEngine engine_b;
        std::u32string output_a;
        std::u32string output_b;
        Apply(output_a, U'a', engine_a.Process(U'a'));
        Apply(output_b, U'o', engine_b.Process(U'o'));
        Apply(output_a, U'e', engine_a.Process(U'e'));
        Apply(output_b, U'e', engine_b.Process(U'e'));
        runner.Check(output_a == U"\u00E4" && output_b == U"\u00F6",
                     "interleaved engine instances are independent",
                     "A=" + Describe(output_a) + "B=" + Describe(output_b));
    }

    {
        TransformEngine engine;
        const Action first = engine.Process(U'a');
        const Action second = engine.Process(U'e');
        const Action third = engine.Process(U'e');
        runner.Check(first.kind == ActionKind::Pass && first.replacement.empty(),
                     "first prefix action is Pass");
        runner.Check(second.kind == ActionKind::ReplacePrevious &&
                         second.replacement == U"\u00E4",
                     "second character action is ReplacePrevious");
        runner.Check(third.kind == ActionKind::ReplaceConverted &&
                         third.replacement == U"ae",
                     "third character action is ReplaceConverted");
    }

    std::cout << runner.passed << " tests passed; " << runner.failed
              << " tests failed.\n";
    return runner.failed == 0 ? 0 : 1;
}
