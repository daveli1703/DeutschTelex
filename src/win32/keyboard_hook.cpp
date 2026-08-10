#include "win32/keyboard_hook.h"

namespace deutschtelex::win32 {

KeyboardHook* KeyboardHook::active_instance_ = nullptr;

InjectionOrigin ClassifyInjection(const KBDLLHOOKSTRUCT& event) noexcept {
    if (event.dwExtraInfo == kDeutschTelexInjectionMarker) {
        return InjectionOrigin::DeutschTelex;
    }
    return (event.flags & LLKHF_INJECTED) != 0 ? InjectionOrigin::Foreign
                                               : InjectionOrigin::Physical;
}

KeyboardHook::~KeyboardHook() {
    static_cast<void>(Uninstall());
}

bool KeyboardHook::Install() noexcept {
    if (hook_ != nullptr || active_instance_ != nullptr) {
        last_error_ = ERROR_ALREADY_EXISTS;
        return false;
    }

    ResetEngineAndInvalidateCheckpoint();
    suppressed_key_ups_.reset();
    foreground_window_ = GetForegroundWindow();
    modifiers_.InitializeFromSystem();

    hook_ = SetWindowsHookExW(WH_KEYBOARD_LL, HookProcedure, GetModuleHandleW(nullptr), 0);
    if (hook_ == nullptr) {
        last_error_ = GetLastError();
        return false;
    }

    active_instance_ = this;
    last_error_ = ERROR_SUCCESS;
    return true;
}

bool KeyboardHook::Uninstall() noexcept {
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
    ResetEngineAndInvalidateCheckpoint();
    suppressed_key_ups_.reset();
    return succeeded;
}

DWORD KeyboardHook::LastErrorCode() const noexcept {
    return last_error_;
}

LRESULT CALLBACK KeyboardHook::HookProcedure(const int code, const WPARAM message,
                                             const LPARAM data) noexcept {
    if (code < 0 || active_instance_ == nullptr) {
        return CallNextHookEx(nullptr, code, message, data);
    }

    const auto* event = reinterpret_cast<const KBDLLHOOKSTRUCT*>(data);
    return active_instance_->Handle(message, *event);
}

LRESULT KeyboardHook::Handle(const WPARAM message, const KBDLLHOOKSTRUCT& event) noexcept {
    const LPARAM event_data = reinterpret_cast<LPARAM>(&event);

    const InjectionOrigin origin = ClassifyInjection(event);
    if (origin == InjectionOrigin::DeutschTelex) {
        return PassThrough(HC_ACTION, message, event_data);
    }

    const bool key_down = message == WM_KEYDOWN || message == WM_SYSKEYDOWN;
    const bool key_up = message == WM_KEYUP || message == WM_SYSKEYUP;
    if (!key_down && !key_up) {
        return PassThrough(HC_ACTION, message, event_data);
    }

    if (origin == InjectionOrigin::Foreign) {
        ResetEngineAndInvalidateCheckpoint();
        static_cast<void>(modifiers_.Update(event.vkCode, key_down));
        return PassThrough(HC_ACTION, message, event_data);
    }

    const HWND current_foreground = GetForegroundWindow();
    if (current_foreground != foreground_window_) {
        foreground_window_ = current_foreground;
        ResetEngineAndInvalidateCheckpoint();
    }

    const ModifierEvent modifier_event = modifiers_.Update(event.vkCode, key_down);
    if (modifier_event != ModifierEvent::NotModifier) {
        if (modifier_event == ModifierEvent::Shortcut ||
            modifier_event == ModifierEvent::CapsLock) {
            ResetEngineAndInvalidateCheckpoint();
        }
        return PassThrough(HC_ACTION, message, event_data);
    }

    if (key_up) {
        if (event.vkCode < suppressed_key_ups_.size() &&
            suppressed_key_ups_.test(event.vkCode)) {
            suppressed_key_ups_.reset(event.vkCode);
            return 1;
        }
        return PassThrough(HC_ACTION, message, event_data);
    }

    if (modifiers_.ShortcutActive()) {
        ResetEngineAndInvalidateCheckpoint();
        return PassThrough(HC_ACTION, message, event_data);
    }

    if (IsResetKey(event.vkCode)) {
        if (event.vkCode != VK_BACK || !typo_checkpoint_.RestoreAfterBackspace(engine_)) {
            ResetEngineAndInvalidateCheckpoint();
        }
        return PassThrough(HC_ACTION, message, event_data);
    }

    const std::optional<char32_t> character = modifiers_.DecodeLatinLetter(event.vkCode);
    if (!character.has_value()) {
        ResetEngineAndInvalidateCheckpoint();
        return PassThrough(HC_ACTION, message, event_data);
    }

    typo_checkpoint_.BeginPrintable(engine_);
    const core::Action action = engine_.Process(*character);
    typo_checkpoint_.CompletePrintable(action);
    if (action.kind == core::ActionKind::Pass) {
        if (event.vkCode < suppressed_key_ups_.size()) {
            suppressed_key_ups_.reset(event.vkCode);
        }
        return PassThrough(HC_ACTION, message, event_data);
    }

    if (!injector_.Inject(action)) {
        ResetEngineAndInvalidateCheckpoint();
        if (event.vkCode < suppressed_key_ups_.size()) {
            suppressed_key_ups_.reset(event.vkCode);
        }
        return PassThrough(HC_ACTION, message, event_data);
    }

    if (event.vkCode < suppressed_key_ups_.size()) {
        suppressed_key_ups_.set(event.vkCode);
    }
    return 1;
}

void KeyboardHook::ResetEngineAndInvalidateCheckpoint() noexcept {
    engine_.Reset();
    typo_checkpoint_.Invalidate();
}

LRESULT KeyboardHook::PassThrough(const int code, const WPARAM message,
                                  const LPARAM data) const noexcept {
    return CallNextHookEx(hook_, code, message, data);
}

}  // namespace deutschtelex::win32
