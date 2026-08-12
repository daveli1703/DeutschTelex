#pragma once

#include <windows.h>

namespace deutschtelex::win32 {

class KeyboardHook;

// True for physical editing gestures that can move the caret or change the
// visible context without changing the foreground HWND.
[[nodiscard]] bool IsMouseContextResetMessage(WPARAM message) noexcept;

// Observes mouse editing gestures only to invalidate keyboard transformation
// state. It retains no coordinates, buttons, targets, or event history.
class MouseResetHook {
public:
    MouseResetHook() = default;
    ~MouseResetHook();

    MouseResetHook(const MouseResetHook&) = delete;
    MouseResetHook& operator=(const MouseResetHook&) = delete;

    [[nodiscard]] bool Install(KeyboardHook& keyboard_hook) noexcept;
    [[nodiscard]] bool Uninstall() noexcept;
    [[nodiscard]] DWORD LastErrorCode() const noexcept;

private:
    static LRESULT CALLBACK HookProcedure(int code, WPARAM message,
                                          LPARAM data) noexcept;

    static MouseResetHook* active_instance_;

    HHOOK hook_{};
    KeyboardHook* keyboard_hook_{};
    DWORD last_error_{};
};

}  // namespace deutschtelex::win32
