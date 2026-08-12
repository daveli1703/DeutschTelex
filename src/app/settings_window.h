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
                                     DWORD style, int identifier, HFONT font) noexcept;
    void CreateFonts() noexcept;
    void DestroyFonts() noexcept;
    void CreateBrushes() noexcept;
    void DestroyBrushes() noexcept;
    void ApplyFonts() noexcept;
    void UpdateHeaderIcon() noexcept;
    void LayoutControls() noexcept;
    void PaintWindow() noexcept;
    [[nodiscard]] LRESULT ControlColor(HDC device_context, HWND control) const noexcept;
    void PopulateControls(const config::AppSettings& settings) noexcept;
    [[nodiscard]] config::AppSettings ReadControls() const noexcept;
    void SaveAndClose() noexcept;

    HINSTANCE instance_{};
    HWND owner_{};
    SettingsWindowDelegate& delegate_;
    HWND window_{};
    UINT dpi_{96};

    HWND header_icon_{};
    HWND header_title_{};
    HWND header_subtitle_{};
    HWND general_title_{};
    HWND input_title_{};
    HWND applications_title_{};
    HWND shortcut_title_{};
    HWND mapping_text_{};
    HWND shortcut_label_{};
    HWND shortcut_value_{};
    HWND privacy_note_{};
    HWND defaults_button_{};
    HWND cancel_button_{};
    HWND save_button_{};
    HWND start_with_windows_{};
    HWND show_notifications_{};
    HWND enable_eszett_{};
    HWND disable_in_vscode_{};

    HFONT normal_font_{};
    HFONT small_font_{};
    HFONT semibold_font_{};
    HFONT heading_font_{};
    HFONT section_font_{};
    HBRUSH window_brush_{};
    HBRUSH card_brush_{};
    HICON header_icon_image_{};
    config::AppSettings opening_settings_{};
};

}  // namespace deutschtelex::app
