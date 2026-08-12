#pragma once

#include "app/settings_window.h"
#include "config/app_settings.h"
#include "config/settings_store.h"
#include "win32/keyboard_hook.h"
#include "win32/mouse_reset_hook.h"
#include "win32/startup_registration.h"

#include <filesystem>
#include <optional>
#include <string_view>

#include <windows.h>

namespace deutschtelex::app {

inline constexpr UINT kTrayIconId = 1;
inline constexpr UINT kTrayCallbackMessage = WM_APP + 1;
inline constexpr UINT kHotkeyId = 1;
inline constexpr UINT kCommandToggle = 1001;
inline constexpr UINT kCommandSettings = 1002;
inline constexpr UINT kCommandAbout = 1003;
inline constexpr UINT kCommandExit = 1004;

enum class Command {
    None,
    Toggle,
    Settings,
    About,
    Exit,
};

[[nodiscard]] constexpr Command CommandFromId(const UINT command_id) noexcept {
    switch (command_id) {
    case kCommandToggle:
        return Command::Toggle;
    case kCommandSettings:
        return Command::Settings;
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

class TrayApp : public SettingsWindowDelegate {
public:
    explicit TrayApp(HINSTANCE instance) noexcept;
    ~TrayApp();

    TrayApp(const TrayApp&) = delete;
    TrayApp& operator=(const TrayApp&) = delete;

    [[nodiscard]] bool Initialize(bool smoke_test = false) noexcept;
    [[nodiscard]] int Run() noexcept;
    void Shutdown() noexcept;

    [[nodiscard]] bool SaveSettings(const config::AppSettings& settings,
                                    HWND dialog_owner) noexcept override;

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
    void OpenSettings() noexcept;
    void ToggleEnabled(bool notify) noexcept;
    void ShowToggleNotification() noexcept;
    void ShowHotkeyUnavailableNotification() noexcept;
    void ShowAbout() noexcept;
    void HandleCommand(Command command) noexcept;
    void LoadSettings() noexcept;
    [[nodiscard]] static std::optional<std::filesystem::path>
    DefaultSettingsPath() noexcept;

    HINSTANCE instance_{};
    HWND window_{};
    HANDLE instance_mutex_{};
    HICON tray_icon_{};
    win32::KeyboardHook hook_;
    win32::MouseResetHook mouse_reset_hook_;
    EnabledState enabled_state_;
    config::AppSettings settings_{};
    std::optional<config::SettingsStore> settings_store_;
    std::optional<win32::StartupRegistration> startup_registration_;
    std::optional<SettingsWindow> settings_window_;
    bool tray_added_{};
    bool hotkey_registered_{};
    bool shutting_down_{};
};

}  // namespace deutschtelex::app
