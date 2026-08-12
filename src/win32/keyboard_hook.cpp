#include "win32/keyboard_hook.h"

namespace deutschtelex::win32 {

KeyboardHook* KeyboardHook::active_instance_ = nullptr;

void SuppressedKeyUps::Suppress(const DWORD virtual_key) noexcept {
    if (virtual_key < keys_.size()) {
        keys_.set(virtual_key);
    }
}

void SuppressedKeyUps::PassedKeyDown(const DWORD virtual_key) noexcept {
    if (virtual_key < keys_.size()) {
        keys_.reset(virtual_key);
    }
}

bool SuppressedKeyUps::ConsumeKeyUp(const DWORD virtual_key) noexcept {
    if (virtual_key >= keys_.size() || !keys_.test(virtual_key)) {
        return false;
    }
    keys_.reset(virtual_key);
    return true;
}

void SuppressedKeyUps::Clear() noexcept {
    keys_.reset();
}

bool SuppressedKeyUps::Contains(const DWORD virtual_key) const noexcept {
    return virtual_key < keys_.size() && keys_.test(virtual_key);
}

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
    suppressed_key_ups_.Clear();
    RefreshForegroundContext();
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
    suppressed_key_ups_.Clear();
    foreground_context_.Clear();
    return succeeded;
}

DWORD KeyboardHook::LastErrorCode() const noexcept {
    return last_error_;
}

void KeyboardHook::SetEnabled(const bool enabled) noexcept {
    enabled_ = enabled;
    ResetEngineAndInvalidateCheckpoint();
    suppressed_key_ups_.Clear();
    RefreshForegroundContext();
    if (enabled_) {
        modifiers_.InitializeFromSystem();
    }
}

bool KeyboardHook::IsEnabled() const noexcept {
    return enabled_;
}

void KeyboardHook::SetTransformConfig(const core::TransformConfig config) noexcept {
    engine_.SetConfig(config);
    typo_checkpoint_.Invalidate();
    suppressed_key_ups_.Clear();
    RefreshForegroundContext();
}

void KeyboardHook::SetDisableInVisualStudioCode(const bool disabled) noexcept {
    disable_in_vscode_ = disabled;
    ResetEngineAndInvalidateCheckpoint();
    suppressed_key_ups_.Clear();
    RefreshForegroundContext();
    modifiers_.InitializeFromSystem();
}

void KeyboardHook::ResetTextContext() noexcept {
    ResetEngineAndInvalidateCheckpoint();
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

    if (!enabled_) {
        return PassThrough(HC_ACTION, message, event_data);
    }

    if (origin == InjectionOrigin::Foreign) {
        ResetEngineAndInvalidateCheckpoint();
        static_cast<void>(modifiers_.Update(event.vkCode, key_down));
        return PassThrough(HC_ACTION, message, event_data);
    }

    const HWND current_foreground = GetForegroundWindow();
    if (current_foreground != foreground_context_.Window()) {
        const ForegroundAppIdentity identity = disable_in_vscode_
                                                   ? IdentifyForegroundApp(current_foreground)
                                                   : ForegroundAppIdentity::Other;
        static_cast<void>(foreground_context_.Update(current_foreground, identity));
        ResetEngineAndInvalidateCheckpoint();
    }

    if (ShouldBypassInput(enabled_, disable_in_vscode_, foreground_context_.Identity())) {
        static_cast<void>(modifiers_.Update(event.vkCode, key_down));
        if (event.vkCode < 256U) {
            if (key_up && suppressed_key_ups_.ConsumeKeyUp(event.vkCode)) {
                return 1;
            }
            if (key_down) {
                suppressed_key_ups_.PassedKeyDown(event.vkCode);
            }
        }
        return PassThrough(HC_ACTION, message, event_data);
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
        if (suppressed_key_ups_.ConsumeKeyUp(event.vkCode)) {
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
        suppressed_key_ups_.PassedKeyDown(event.vkCode);
        return PassThrough(HC_ACTION, message, event_data);
    }

    if (!injector_.Inject(action)) {
        ResetEngineAndInvalidateCheckpoint();
        suppressed_key_ups_.PassedKeyDown(event.vkCode);
        return PassThrough(HC_ACTION, message, event_data);
    }

    suppressed_key_ups_.Suppress(event.vkCode);
    return 1;
}

void KeyboardHook::ResetEngineAndInvalidateCheckpoint() noexcept {
    engine_.Reset();
    typo_checkpoint_.Invalidate();
}

void KeyboardHook::RefreshForegroundContext() noexcept {
    const HWND current_foreground = GetForegroundWindow();
    const ForegroundAppIdentity identity = disable_in_vscode_
                                               ? IdentifyForegroundApp(current_foreground)
                                               : ForegroundAppIdentity::Other;
    static_cast<void>(foreground_context_.Update(current_foreground, identity));
}

LRESULT KeyboardHook::PassThrough(const int code, const WPARAM message,
                                  const LPARAM data) const noexcept {
    return CallNextHookEx(hook_, code, message, data);
}

}  // namespace deutschtelex::win32
