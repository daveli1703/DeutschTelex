#pragma once

#include "win32/keyboard_hook.h"

#include <string_view>

#include <windows.h>

namespace deutschtelex::app {

inline constexpr UINT kTrayIconId = 1;
inline constexpr UINT kTrayCallbackMessage = WM_APP + 1;
inline constexpr UINT kHotkeyId = 1;
inline constexpr UINT kCommandToggle = 1001;
inline constexpr UINT kCommandAbout = 1002;
inline constexpr UINT kCommandExit = 1003;

enum class Command {
    None,
    Toggle,
    About,
    Exit,
};

[[nodiscard]] constexpr Command CommandFromId(const UINT command_id) noexcept {
    switch (command_id) {
    case kCommandToggle:
        return Command::Toggle;
    case kCommandAbout:
        return Command::About;
    case kCommandExit:
        return Command::Exit;
    default:
        return Command::None;
    }
}

class EnabledState {
public:
    [[nodiscard]] bool IsEnabled() const noexcept;
    [[nodiscard]] bool Toggle() noexcept;

private:
    bool enabled_{true};
};

[[nodiscard]] std::wstring_view TooltipFor(bool enabled) noexcept;

class TrayApp {
public:
    explicit TrayApp(HINSTANCE instance) noexcept;
    ~TrayApp();

    TrayApp(const TrayApp&) = delete;
    TrayApp& operator=(const TrayApp&) = delete;

    [[nodiscard]] bool Initialize() noexcept;
    [[nodiscard]] int Run() noexcept;
    void Shutdown() noexcept;

private:
    static LRESULT CALLBACK WindowProcedure(HWND window, UINT message,
                                            WPARAM w_param, LPARAM l_param) noexcept;
    [[nodiscard]] LRESULT HandleWindowMessage(UINT message, WPARAM w_param,
                                               LPARAM l_param) noexcept;
    [[nodiscard]] bool CreateCoordinatorWindow() noexcept;
    [[nodiscard]] bool AddTrayIcon() noexcept;
    void RemoveTrayIcon() noexcept;
    void UpdateTrayIcon() noexcept;
    void ShowTrayMenu() noexcept;
    void ToggleEnabled(bool notify) noexcept;
    void ShowToggleNotification() noexcept;
    void ShowHotkeyUnavailableNotification() noexcept;
    void ShowAbout() noexcept;
    void HandleCommand(Command command) noexcept;

    HINSTANCE instance_{};
    HWND window_{};
    HANDLE instance_mutex_{};
    win32::KeyboardHook hook_;
    EnabledState enabled_state_;
    bool tray_added_{};
    bool hotkey_registered_{};
    bool shutting_down_{};
};

}  // namespace deutschtelex::app
