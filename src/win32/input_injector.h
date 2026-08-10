#pragma once

#include "core/transform_engine.h"

#include <array>
#include <optional>

#include <windows.h>

namespace deutschtelex::win32 {

inline constexpr ULONG_PTR kDeutschTelexInjectionMarker =
    static_cast<ULONG_PTR>(0x44545832UL);  // "DTX2"

struct InjectionBatch {
    // Backspace plus up to eight UTF-16 code units, each represented by a
    // key-down/key-up pair. Current core actions need at most two code units.
    std::array<INPUT, 18> events{};
    UINT count{};
};

// Converts core UTF-32 code points to tagged Win32 UTF-16 keyboard events.
// Invalid Unicode scalar values or an oversized replacement are rejected.
[[nodiscard]] std::optional<InjectionBatch> BuildReplacementBatch(
    const core::Action& action,
    ULONG_PTR marker = kDeutschTelexInjectionMarker) noexcept;

class InputInjector {
public:
    [[nodiscard]] bool Inject(const core::Action& action) const noexcept;
};

}  // namespace deutschtelex::win32
