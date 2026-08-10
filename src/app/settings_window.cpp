#include "app/settings_window.h"

namespace deutschtelex::app {
namespace {

constexpr wchar_t kWindowClassName[] = L"DeutschTelexSettingsWindow";
constexpr wchar_t kWindowTitle[] = L"DeutschTelex Settings";
constexpr int kDefaultsButton = 2001;
constexpr int kStartWithWindowsCheckbox = 2101;
constexpr int kShowNotificationsCheckbox = 2102;
constexpr int kEnableEszettCheckbox = 2103;
constexpr int kClientWidth = 430;
constexpr int kClientHeight = 390;

void SetChecked(const HWND control, const bool checked) noexcept {
    SendMessageW(control, BM_SETCHECK, checked ? BST_CHECKED : BST_UNCHECKED, 0);
}

bool IsChecked(const HWND control) noexcept {
    return SendMessageW(control, BM_GETCHECK, 0, 0) == BST_CHECKED;
}

}  // namespace

SettingsWindow::SettingsWindow(const HINSTANCE instance, const HWND owner,
                               SettingsWindowDelegate& delegate) noexcept
    : instance_(instance), owner_(owner), delegate_(delegate) {}

SettingsWindow::~SettingsWindow() {
    Close();
}

bool SettingsWindow::Open(const config::AppSettings& settings) noexcept {
    if (window_ != nullptr) {
        ShowWindow(window_, SW_RESTORE);
        SetForegroundWindow(window_);
        return true;
    }

    WNDCLASSEXW window_class{};
    window_class.cbSize = sizeof(window_class);
    window_class.hInstance = instance_;
    window_class.lpfnWndProc = WindowProcedure;
    window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    window_class.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    window_class.lpszClassName = kWindowClassName;
    if (RegisterClassExW(&window_class) == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return false;
    }

    RECT bounds{0, 0, kClientWidth, kClientHeight};
    const DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU;
    const DWORD extended_style = WS_EX_DLGMODALFRAME | WS_EX_TOOLWINDOW;
    if (AdjustWindowRectEx(&bounds, style, FALSE, extended_style) == FALSE) {
        return false;
    }
    const int width = bounds.right - bounds.left;
    const int height = bounds.bottom - bounds.top;
    const int x = (GetSystemMetrics(SM_CXSCREEN) - width) / 2;
    const int y = (GetSystemMetrics(SM_CYSCREEN) - height) / 2;

    opening_settings_ = settings;
    window_ = CreateWindowExW(extended_style, kWindowClassName, kWindowTitle, style,
                              x, y, width, height, owner_, nullptr, instance_, this);
    if (window_ == nullptr) {
        return false;
    }
    ShowWindow(window_, SW_SHOWNORMAL);
    SetForegroundWindow(window_);
    return true;
}

void SettingsWindow::Close() noexcept {
    if (window_ != nullptr) {
        DestroyWindow(window_);
    }
}

bool SettingsWindow::IsOpen() const noexcept {
    return window_ != nullptr;
}

bool SettingsWindow::PreTranslateMessage(MSG& message) const noexcept {
    return window_ != nullptr && IsDialogMessageW(window_, &message) != FALSE;
}

LRESULT CALLBACK SettingsWindow::WindowProcedure(const HWND window, const UINT message,
                                                  const WPARAM w_param,
                                                  const LPARAM l_param) noexcept {
    SettingsWindow* settings_window = nullptr;
    if (message == WM_NCCREATE) {
        const auto* create = reinterpret_cast<const CREATESTRUCTW*>(l_param);
        settings_window = static_cast<SettingsWindow*>(create->lpCreateParams);
        SetWindowLongPtrW(window, GWLP_USERDATA,
                          reinterpret_cast<LONG_PTR>(settings_window));
        settings_window->window_ = window;
    } else {
        settings_window = reinterpret_cast<SettingsWindow*>(
            GetWindowLongPtrW(window, GWLP_USERDATA));
    }
    return settings_window == nullptr
               ? DefWindowProcW(window, message, w_param, l_param)
               : settings_window->HandleMessage(message, w_param, l_param);
}

LRESULT SettingsWindow::HandleMessage(const UINT message, const WPARAM w_param,
                                      const LPARAM l_param) noexcept {
    switch (message) {
    case WM_CREATE:
        return CreateControls() ? 0 : -1;
    case WM_COMMAND:
        switch (LOWORD(w_param)) {
        case kDefaultsButton:
            PopulateControls(config::DefaultSettings());
            return 0;
        case IDCANCEL:
            Close();
            return 0;
        case IDOK:
            SaveAndClose();
            return 0;
        default:
            return 0;
        }
    case WM_CLOSE:
        Close();
        return 0;
    case WM_NCDESTROY: {
        const HWND destroyed_window = window_;
        window_ = nullptr;
        SetWindowLongPtrW(destroyed_window, GWLP_USERDATA, 0);
        return DefWindowProcW(destroyed_window, message, w_param, l_param);
    }
    default:
        return DefWindowProcW(window_, message, w_param, l_param);
    }
}

bool SettingsWindow::CreateControls() noexcept {
    const DWORD group_style = WS_CHILD | WS_VISIBLE | BS_GROUPBOX;
    const DWORD checkbox_style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX;
    const DWORD static_style = WS_CHILD | WS_VISIBLE | SS_LEFT;
    const DWORD button_style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON;

    if (CreateControl(L"BUTTON", L"GENERAL", group_style, 15, 12, 400, 88, 0) == nullptr) {
        return false;
    }
    start_with_windows_ = CreateControl(
        L"BUTTON", L"Start DeutschTelex with Windows", checkbox_style,
        30, 38, 350, 22, kStartWithWindowsCheckbox);
    show_notifications_ = CreateControl(
        L"BUTTON", L"Show ON/OFF notifications", checkbox_style,
        30, 67, 350, 22, kShowNotificationsCheckbox);

    if (CreateControl(L"BUTTON", L"INPUT", group_style, 15, 110, 400, 112, 0) == nullptr ||
        CreateControl(L"STATIC", L"ae \u2192 \u00E4       oe \u2192 \u00F6       ue \u2192 \u00FC",
                      static_style, 30, 139, 360, 22, 0) == nullptr) {
        return false;
    }
    enable_eszett_ = CreateControl(L"BUTTON", L"Enable sz \u2192 \u00DF", checkbox_style,
                                   30, 181, 350, 22, kEnableEszettCheckbox);

    if (CreateControl(L"BUTTON", L"KEYBOARD SHORTCUT", group_style,
                      15, 232, 400, 70, 0) == nullptr ||
        CreateControl(L"STATIC", L"Toggle DeutschTelex:    Ctrl + Alt + G",
                      static_style, 30, 260, 360, 22, 0) == nullptr) {
        return false;
    }

    if (CreateControl(L"BUTTON", L"Defaults", button_style,
                      126, 330, 85, 28, kDefaultsButton) == nullptr ||
        CreateControl(L"BUTTON", L"Cancel", button_style,
                      218, 330, 85, 28, IDCANCEL) == nullptr ||
        CreateControl(L"BUTTON", L"Save", button_style | BS_DEFPUSHBUTTON,
                      310, 330, 85, 28, IDOK) == nullptr) {
        return false;
    }

    if (start_with_windows_ == nullptr || show_notifications_ == nullptr ||
        enable_eszett_ == nullptr) {
        return false;
    }
    PopulateControls(opening_settings_);
    return true;
}

HWND SettingsWindow::CreateControl(const wchar_t* const class_name,
                                   const wchar_t* const text, const DWORD style,
                                   const int x, const int y, const int width,
                                   const int height, const int identifier) noexcept {
    const HWND control = CreateWindowExW(
        0, class_name, text, style, x, y, width, height, window_,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(identifier)), instance_, nullptr);
    if (control != nullptr) {
        SendMessageW(control, WM_SETFONT,
                     reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);
    }
    return control;
}

void SettingsWindow::PopulateControls(const config::AppSettings& settings) noexcept {
    SetChecked(start_with_windows_, settings.start_with_windows);
    SetChecked(show_notifications_, settings.show_toggle_notifications);
    SetChecked(enable_eszett_, settings.enable_eszett);
}

config::AppSettings SettingsWindow::ReadControls() const noexcept {
    return {
        IsChecked(start_with_windows_),
        IsChecked(show_notifications_),
        IsChecked(enable_eszett_),
    };
}

void SettingsWindow::SaveAndClose() noexcept {
    if (delegate_.SaveSettings(ReadControls(), window_)) {
        Close();
    }
}

}  // namespace deutschtelex::app
