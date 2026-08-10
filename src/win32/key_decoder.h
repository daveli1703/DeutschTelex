#pragma once

#include <optional>

#include <windows.h>

namespace deutschtelex::win32 {

enum class ModifierEvent {
    NotModifier,
    Shift,
    Shortcut,
    CapsLock,
};

class ModifierState {
public:
    explicit ModifierState(bool caps_lock_enabled = false) noexcept;

    // Called once outside the low-level hook callback. Subsequent state is
    // maintained from hook events rather than GetAsyncKeyState timing.
    void InitializeFromSystem() noexcept;
    [[nodiscard]] ModifierEvent Update(DWORD virtual_key, bool key_down) noexcept;
    [[nodiscard]] bool ShortcutActive() const noexcept;
    [[nodiscard]] std::optional<char32_t> DecodeLatinLetter(DWORD virtual_key) const noexcept;

private:
    bool left_shift_{};
    bool right_shift_{};
    bool generic_shift_{};
    bool left_control_{};
    bool right_control_{};
    bool generic_control_{};
    bool left_alt_{};
    bool right_alt_{};
    bool generic_alt_{};
    bool left_windows_{};
    bool right_windows_{};
    bool caps_lock_enabled_{};
    bool caps_lock_key_down_{};
};

[[nodiscard]] bool IsResetKey(DWORD virtual_key) noexcept;

}  // namespace deutschtelex::win32
