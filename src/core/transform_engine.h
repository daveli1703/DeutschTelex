#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

namespace deutschtelex::core {

enum class ActionKind {
    // Allow the current input character to pass through unchanged.
    Pass,

    // Consume the current input character, remove the preceding character,
    // and insert replacement.
    ReplacePrevious,

    // Consume the current input character, remove the preceding converted
    // character, and insert replacement. This is the third-character escape.
    ReplaceConverted,
};

struct Action {
    ActionKind kind{ActionKind::Pass};

    // UTF-32 code points to insert. The view remains valid for the lifetime of
    // the program and never depends on a console or system code page.
    std::u32string_view replacement{};

    [[nodiscard]] static constexpr Action Pass() noexcept {
        return {};
    }
};

class TransformEngine {
public:
    class Snapshot {
    public:
        Snapshot() = default;

    private:
        constexpr Snapshot(std::uint8_t stage, std::uint8_t rule) noexcept
            : stage_(stage), rule_(rule) {}

        std::uint8_t stage_{};
        std::uint8_t rule_{};

        friend class TransformEngine;
    };

    // Processes exactly one Unicode code point and returns the semantic edit
    // that the platform adapter must apply for that input.
    [[nodiscard]] Action Process(char32_t input) noexcept;

    // Captures/restores only the finite-state-machine state. A Snapshot holds
    // no input text and is cheap to copy.
    [[nodiscard]] Snapshot Capture() const noexcept;
    void Restore(const Snapshot& snapshot) noexcept;

    // True only when the next matching suffix could produce a transformation.
    [[nodiscard]] bool HasPendingPrefix() const noexcept;

    // Discards any pending prefix or completed-conversion escape state.
    void Reset() noexcept;

private:
    enum class Stage : std::uint8_t {
        Idle,
        Prefix,
        Converted,
    };

    enum class RuleId : std::uint8_t {
        None,
        LowerA,
        LowerO,
        LowerU,
        UpperA,
        UpperO,
        UpperU,
        SharpS,
    };

    Stage stage_{Stage::Idle};
    RuleId rule_{RuleId::None};
};

// Supports exactly one safe correction: a pending prefix, one printable typo,
// and one immediate physical Backspace. It is deliberately not an undo stack.
class SingleTypoCheckpoint {
public:
    void BeginPrintable(const TransformEngine& engine) noexcept;
    void CompletePrintable(const Action& action) noexcept;
    [[nodiscard]] bool RestoreAfterBackspace(TransformEngine& engine) noexcept;
    void Invalidate() noexcept;

private:
    std::optional<TransformEngine::Snapshot> snapshot_;
};

}  // namespace deutschtelex::core
