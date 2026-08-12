#include "win32/mouse_reset_hook.h"

#include "win32/keyboard_hook.h"

namespace deutschtelex::win32 {

MouseResetHook* MouseResetHook::active_instance_ = nullptr;

bool IsMouseContextResetMessage(const WPARAM message) noexcept {
    switch (message) {
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_MBUTTONDOWN:
    case WM_XBUTTONDOWN:
    case WM_MOUSEWHEEL:
    case WM_MOUSEHWHEEL:
        return true;
    default:
        return false;
    }
}

MouseResetHook::~MouseResetHook() {
    static_cast<void>(Uninstall());
}

bool MouseResetHook::Install(KeyboardHook& keyboard_hook) noexcept {
    if (hook_ != nullptr || active_instance_ != nullptr) {
        last_error_ = ERROR_ALREADY_EXISTS;
        return false;
    }

    keyboard_hook_ = &keyboard_hook;
    hook_ = SetWindowsHookExW(WH_MOUSE_LL, HookProcedure,
                              GetModuleHandleW(nullptr), 0);
    if (hook_ == nullptr) {
        keyboard_hook_ = nullptr;
        last_error_ = GetLastError();
        return false;
    }

    active_instance_ = this;
    last_error_ = ERROR_SUCCESS;
    return true;
}

bool MouseResetHook::Uninstall() noexcept {
    bool succeeded = true;
    if (hook_ != nullptr) {
        if (UnhookWindowsHookEx(hook_) == FALSE) {
            last_error_ = GetLastError();
            succeeded = false;
        }
        hook_ = nullptr;
    }
    if (active_instance_ == this) {
        active_instance_ = nullptr;
    }
    keyboard_hook_ = nullptr;
    return succeeded;
}

DWORD MouseResetHook::LastErrorCode() const noexcept {
    return last_error_;
}

LRESULT CALLBACK MouseResetHook::HookProcedure(const int code,
                                                const WPARAM message,
                                                const LPARAM data) noexcept {
    if (code >= 0 && active_instance_ != nullptr &&
        active_instance_->keyboard_hook_ != nullptr &&
        IsMouseContextResetMessage(message)) {
        active_instance_->keyboard_hook_->ResetTextContext();
    }
    return CallNextHookEx(active_instance_ == nullptr
                              ? nullptr
                              : active_instance_->hook_,
                          code, message, data);
}

}  // namespace deutschtelex::win32
