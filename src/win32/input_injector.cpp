#include "win32/input_injector.h"

#include <cstdint>

namespace deutschtelex::win32 {
namespace {

bool AppendVirtualKey(InjectionBatch& batch, const WORD virtual_key,
                      const DWORD flags, const ULONG_PTR marker) noexcept {
    if (batch.count >= batch.events.size()) {
        return false;
    }

    INPUT& input = batch.events[batch.count++];
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = virtual_key;
    input.ki.dwFlags = flags;
    input.ki.dwExtraInfo = marker;
    return true;
}

bool AppendUnicodeUnit(InjectionBatch& batch, const char16_t code_unit,
                       const ULONG_PTR marker) noexcept {
    return AppendVirtualKey(batch, 0, KEYEVENTF_UNICODE, marker) &&
           AppendVirtualKey(batch, 0, KEYEVENTF_UNICODE | KEYEVENTF_KEYUP, marker) &&
           ((batch.events[batch.count - 2].ki.wScan = static_cast<WORD>(code_unit)),
            (batch.events[batch.count - 1].ki.wScan = static_cast<WORD>(code_unit)), true);
}

bool AppendCodePoint(InjectionBatch& batch, const char32_t code_point,
                     const ULONG_PTR marker) noexcept {
    const auto value = static_cast<std::uint32_t>(code_point);
    if (value <= 0xD7FFU || (value >= 0xE000U && value <= 0xFFFFU)) {
        return AppendUnicodeUnit(batch, static_cast<char16_t>(value), marker);
    }
    if (value < 0x10000U || value > 0x10FFFFU) {
        return false;
    }

    const std::uint32_t adjusted = value - 0x10000U;
    const auto high = static_cast<char16_t>(0xD800U + (adjusted >> 10U));
    const auto low = static_cast<char16_t>(0xDC00U + (adjusted & 0x3FFU));
    return AppendUnicodeUnit(batch, high, marker) && AppendUnicodeUnit(batch, low, marker);
}

}  // namespace

std::optional<InjectionBatch> BuildReplacementBatch(const core::Action& action,
                                                     const ULONG_PTR marker) noexcept {
    if (action.kind == core::ActionKind::Pass || action.replacement.empty()) {
        return std::nullopt;
    }

    InjectionBatch batch;
    if (!AppendVirtualKey(batch, VK_BACK, 0, marker) ||
        !AppendVirtualKey(batch, VK_BACK, KEYEVENTF_KEYUP, marker)) {
        return std::nullopt;
    }

    for (const char32_t code_point : action.replacement) {
        if (!AppendCodePoint(batch, code_point, marker)) {
            return std::nullopt;
        }
    }
    return batch;
}

bool InputInjector::Inject(const core::Action& action) const noexcept {
    std::optional<InjectionBatch> batch = BuildReplacementBatch(action);
    if (!batch.has_value()) {
        return false;
    }

    const UINT inserted = SendInput(batch->count, batch->events.data(), sizeof(INPUT));
    return inserted == batch->count;
}

}  // namespace deutschtelex::win32
