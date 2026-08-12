#pragma once

#include "core/transform_engine.h"
#include "win32/foreground_app.h"
#include "win32/input_injector.h"
#include "win32/key_decoder.h"

#include <bitset>

#include <windows.h>

namespace deutschtelex::win32 {

enum class InjectionOrigin {
    Physical,
    DeutschTelex,
    Foreign,
};

class SuppressedKeyUps {
public:
    void Suppress(DWORD virtual_key) noexcept;
    void PassedKeyDown(DWORD virtual_key) noexcept;
    [[nodiscard]] bool ConsumeKeyUp(DWORD virtual_key) noexcept;
    void Clear() noexcept;
    [[nodiscard]] bool Contains(DWORD virtual_key) const noexcept;

private:
    std::bitset<256> keys_;
};

[[nodiscard]] InjectionOrigin ClassifyInjection(
    const KBDLLHOOKSTRUCT& event) noexcept;

class KeyboardHook {
public:
    KeyboardHook() = default;
    ~KeyboardHook();

    KeyboardHook(const KeyboardHook&) = delete;
    KeyboardHook& operator=(const KeyboardHook&) = delete;

    [[nodiscard]] bool Install() noexcept;
    [[nodiscard]] bool Uninstall() noexcept;
    [[nodiscard]] DWORD LastErrorCode() const noexcept;
    void SetEnabled(bool enabled) noexcept;
    [[nodiscard]] bool IsEnabled() const noexcept;
    void SetTransformConfig(core::TransformConfig config) noexcept;
    void SetDisableInVisualStudioCode(bool disabled) noexcept;

    // Invalidates only text-context state after a same-window caret gesture.
    // Suppressed physical key-up bookkeeping remains intact.
    void ResetTextContext() noexcept;

private:
    static LRESULT CALLBACK HookProcedure(int code, WPARAM message, LPARAM data) noexcept;
    [[nodiscard]] LRESULT Handle(WPARAM message, const KBDLLHOOKSTRUCT& event) noexcept;
    [[nodiscard]] LRESULT PassThrough(int code, WPARAM message, LPARAM data) const noexcept;
    void ResetEngineAndInvalidateCheckpoint() noexcept;
    void RefreshForegroundContext() noexcept;

    static KeyboardHook* active_instance_;

    HHOOK hook_{};
    DWORD last_error_{};
    bool enabled_{true};
    bool disable_in_vscode_{};
    ForegroundContextCache foreground_context_;
    core::TransformEngine engine_;
    core::SingleTypoCheckpoint typo_checkpoint_;
    ModifierState modifiers_;
    InputInjector injector_;

    // A bit is set only when a physical key-down was suppressed. Its matching
    // key-up is suppressed too. If an auto-repeat key-down is later passed,
    // the bit is cleared so the destination receives the eventual key-up.
    SuppressedKeyUps suppressed_key_ups_;
};

}  // namespace deutschtelex::win32
