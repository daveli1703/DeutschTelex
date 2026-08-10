#pragma once

#include "config/app_settings.h"

#include <windows.h>

namespace deutschtelex::app {

class SettingsWindowDelegate {
public:
    virtual ~SettingsWindowDelegate() = default;
    [[nodiscard]] virtual bool SaveSettings(const config::AppSettings& settings,
                                            HWND dialog_owner) noexcept = 0;
};

class SettingsWindow {
public:
    SettingsWindow(HINSTANCE instance, HWND owner, SettingsWindowDelegate& delegate) noexcept;
    ~SettingsWindow();

    SettingsWindow(const SettingsWindow&) = delete;
    SettingsWindow& operator=(const SettingsWindow&) = delete;

    [[nodiscard]] bool Open(const config::AppSettings& settings) noexcept;
    void Close() noexcept;
    [[nodiscard]] bool IsOpen() const noexcept;
    [[nodiscard]] bool PreTranslateMessage(MSG& message) const noexcept;

private:
    static LRESULT CALLBACK WindowProcedure(HWND window, UINT message,
                                            WPARAM w_param, LPARAM l_param) noexcept;
    [[nodiscard]] LRESULT HandleMessage(UINT message, WPARAM w_param,
                                        LPARAM l_param) noexcept;
    [[nodiscard]] bool CreateControls() noexcept;
    [[nodiscard]] HWND CreateControl(const wchar_t* class_name, const wchar_t* text,
                                     DWORD style, int x, int y, int width, int height,
                                     int identifier) noexcept;
    void PopulateControls(const config::AppSettings& settings) noexcept;
    [[nodiscard]] config::AppSettings ReadControls() const noexcept;
    void SaveAndClose() noexcept;

    HINSTANCE instance_{};
    HWND owner_{};
    SettingsWindowDelegate& delegate_;
    HWND window_{};
    HWND start_with_windows_{};
    HWND show_notifications_{};
    HWND enable_eszett_{};
    HWND disable_in_vscode_{};
    config::AppSettings opening_settings_{};
};

}  // namespace deutschtelex::app
