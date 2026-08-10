#include "win32/key_decoder.h"

namespace deutschtelex::win32 {

ModifierState::ModifierState(const bool caps_lock_enabled) noexcept
    : caps_lock_enabled_(caps_lock_enabled) {}

void ModifierState::InitializeFromSystem() noexcept {
    const auto is_down = [](const int virtual_key) noexcept {
        return (GetAsyncKeyState(virtual_key) & 0x8000) != 0;
    };

    left_shift_ = is_down(VK_LSHIFT);
    right_shift_ = is_down(VK_RSHIFT);
    generic_shift_ = false;
    left_control_ = is_down(VK_LCONTROL);
    right_control_ = is_down(VK_RCONTROL);
    generic_control_ = false;
    left_alt_ = is_down(VK_LMENU);
    right_alt_ = is_down(VK_RMENU);
    generic_alt_ = false;
    left_windows_ = is_down(VK_LWIN);
    right_windows_ = is_down(VK_RWIN);
    caps_lock_enabled_ = (GetKeyState(VK_CAPITAL) & 1) != 0;
    caps_lock_key_down_ = is_down(VK_CAPITAL);
}

ModifierEvent ModifierState::Update(const DWORD virtual_key, const bool key_down) noexcept {
    switch (virtual_key) {
    case VK_LSHIFT:
        left_shift_ = key_down;
        return ModifierEvent::Shift;
    case VK_RSHIFT:
        right_shift_ = key_down;
        return ModifierEvent::Shift;
    case VK_SHIFT:
        generic_shift_ = key_down;
        return ModifierEvent::Shift;
    case VK_LCONTROL:
        left_control_ = key_down;
        return ModifierEvent::Shortcut;
    case VK_RCONTROL:
        right_control_ = key_down;
        return ModifierEvent::Shortcut;
    case VK_CONTROL:
        generic_control_ = key_down;
        return ModifierEvent::Shortcut;
    case VK_LMENU:
        left_alt_ = key_down;
        return ModifierEvent::Shortcut;
    case VK_RMENU:
        right_alt_ = key_down;
        return ModifierEvent::Shortcut;
    case VK_MENU:
        generic_alt_ = key_down;
        return ModifierEvent::Shortcut;
    case VK_LWIN:
        left_windows_ = key_down;
        return ModifierEvent::Shortcut;
    case VK_RWIN:
        right_windows_ = key_down;
        return ModifierEvent::Shortcut;
    case VK_CAPITAL:
        if (key_down && !caps_lock_key_down_) {
            caps_lock_enabled_ = !caps_lock_enabled_;
        }
        caps_lock_key_down_ = key_down;
        return ModifierEvent::CapsLock;
    default:
        return ModifierEvent::NotModifier;
    }
}

bool ModifierState::ShortcutActive() const noexcept {
    return left_control_ || right_control_ || generic_control_ || left_alt_ ||
           right_alt_ || generic_alt_ || left_windows_ || right_windows_;
}

std::optional<char32_t> ModifierState::DecodeLatinLetter(
    const DWORD virtual_key) const noexcept {
    if (virtual_key < static_cast<DWORD>('A') || virtual_key > static_cast<DWORD>('Z')) {
        return std::nullopt;
    }

    const bool shift = left_shift_ || right_shift_ || generic_shift_;
    const bool uppercase = shift != caps_lock_enabled_;
    const char32_t base = static_cast<char32_t>(virtual_key);
    return uppercase ? base : base + (U'a' - U'A');
}

bool IsResetKey(const DWORD virtual_key) noexcept {
    switch (virtual_key) {
    case VK_BACK:
    case VK_DELETE:
    case VK_RETURN:
    case VK_TAB:
    case VK_ESCAPE:
    case VK_LEFT:
    case VK_RIGHT:
    case VK_UP:
    case VK_DOWN:
    case VK_HOME:
    case VK_END:
    case VK_PRIOR:
    case VK_NEXT:
        return true;
    default:
        return false;
    }
}

}  // namespace deutschtelex::win32
