#include "app/settings_window.h"

#include "resources/resource.h"

#include <uxtheme.h>

#include <algorithm>
#include <cstring>
#include <iterator>

namespace deutschtelex::app {
namespace {

constexpr wchar_t kWindowClassName[] = L"DeutschTelexSettingsWindow";
constexpr wchar_t kWindowTitle[] = L"DeutschTelex Settings";
constexpr int kDefaultsButton = 2001;
constexpr int kStartWithWindowsCheckbox = 2101;
constexpr int kShowNotificationsCheckbox = 2102;
constexpr int kEnableEszettCheckbox = 2103;
constexpr int kDisableInVSCodeCheckbox = 2104;
constexpr int kClientWidth = 560;
constexpr int kClientHeight = 620;
constexpr UINT kDefaultDpi = 96;

constexpr COLORREF kWindowColor = RGB(255, 255, 255);
constexpr COLORREF kCardColor = RGB(247, 248, 250);
constexpr COLORREF kBorderColor = RGB(224, 227, 232);
constexpr COLORREF kAccentColor = RGB(24, 90, 180);
constexpr COLORREF kPrimaryTextColor = RGB(32, 33, 36);
constexpr COLORREF kSecondaryTextColor = RGB(95, 99, 104);

int ScaleForDpi(const int value, const UINT dpi) noexcept {
    return MulDiv(value, static_cast<int>(dpi == 0 ? kDefaultDpi : dpi),
                  static_cast<int>(kDefaultDpi));
}

template <typename Function>
Function LoadUser32Function(const char* const name) noexcept {
    Function function{};
    const HMODULE user32 = GetModuleHandleW(L"user32.dll");
    const FARPROC address = user32 == nullptr ? nullptr : GetProcAddress(user32, name);
    static_assert(sizeof(function) == sizeof(address));
    std::memcpy(&function, &address, sizeof(function));
    return function;
}

UINT QueryWindowDpi(const HWND window) noexcept {
    using GetDpiForWindowFunction = UINT(WINAPI*)(HWND);
    const auto get_dpi_for_window =
        LoadUser32Function<GetDpiForWindowFunction>("GetDpiForWindow");
    if (get_dpi_for_window != nullptr && window != nullptr) {
        const UINT dpi = get_dpi_for_window(window);
        if (dpi != 0) {
            return dpi;
        }
    }

    const HDC device_context = GetDC(window);
    if (device_context == nullptr) {
        return kDefaultDpi;
    }
    const int dpi = GetDeviceCaps(device_context, LOGPIXELSX);
    ReleaseDC(window, device_context);
    return dpi > 0 ? static_cast<UINT>(dpi) : kDefaultDpi;
}

bool AdjustBoundsForDpi(RECT& bounds, const DWORD style, const DWORD extended_style,
                        const UINT dpi) noexcept {
    using AdjustWindowRectExForDpiFunction = BOOL(WINAPI*)(LPRECT, DWORD, BOOL, DWORD, UINT);
    const auto adjust_for_dpi =
        LoadUser32Function<AdjustWindowRectExForDpiFunction>("AdjustWindowRectExForDpi");
    if (adjust_for_dpi != nullptr) {
        return adjust_for_dpi(&bounds, style, FALSE, extended_style, dpi) != FALSE;
    }
    return AdjustWindowRectEx(&bounds, style, FALSE, extended_style) != FALSE;
}

void SetChecked(const HWND control, const bool checked) noexcept {
    SendMessageW(control, BM_SETCHECK, checked ? BST_CHECKED : BST_UNCHECKED, 0);
}

bool IsChecked(const HWND control) noexcept {
    return SendMessageW(control, BM_GETCHECK, 0, 0) == BST_CHECKED;
}

HFONT CreateUiFont(const UINT dpi, const int point_size, const int weight) noexcept {
    LOGFONTW font{};
    font.lfHeight = -MulDiv(point_size, static_cast<int>(dpi), 72);
    font.lfWeight = weight;
    font.lfQuality = CLEARTYPE_QUALITY;
    constexpr wchar_t typeface[] = L"Segoe UI";
    std::copy_n(typeface, std::size(typeface), font.lfFaceName);
    return CreateFontIndirectW(&font);
}

void SetControlBounds(const HWND control, const UINT dpi, const int x, const int y,
                      const int width, const int height) noexcept {
    if (control != nullptr) {
        SetWindowPos(control, nullptr, ScaleForDpi(x, dpi), ScaleForDpi(y, dpi),
                     ScaleForDpi(width, dpi), ScaleForDpi(height, dpi),
                     SWP_NOACTIVATE | SWP_NOZORDER);
    }
}

void ApplyFont(const HWND control, const HFONT preferred_font) noexcept {
    if (control == nullptr) {
        return;
    }
    const HFONT font = preferred_font != nullptr
                           ? preferred_font
                           : static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
    SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
}

void DrawCard(const HDC device_context, const UINT dpi, const int x, const int y,
              const int width, const int height) noexcept {
    const HGDIOBJ old_brush = SelectObject(device_context, GetStockObject(DC_BRUSH));
    const HGDIOBJ old_pen = SelectObject(device_context, GetStockObject(DC_PEN));
    SetDCBrushColor(device_context, kCardColor);
    SetDCPenColor(device_context, kBorderColor);
    RoundRect(device_context, ScaleForDpi(x, dpi), ScaleForDpi(y, dpi),
              ScaleForDpi(x + width, dpi), ScaleForDpi(y + height, dpi),
              ScaleForDpi(10, dpi), ScaleForDpi(10, dpi));
    SelectObject(device_context, old_pen);
    SelectObject(device_context, old_brush);
}

}  // namespace

SettingsWindow::SettingsWindow(const HINSTANCE instance, const HWND owner,
                               SettingsWindowDelegate& delegate) noexcept
    : instance_(instance), owner_(owner), delegate_(delegate) {}

SettingsWindow::~SettingsWindow() {
    Close();
    DestroyFonts();
    DestroyBrushes();
    if (header_icon_image_ != nullptr) {
        DestroyIcon(header_icon_image_);
    }
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
    window_class.hIcon = LoadIconW(instance_, MAKEINTRESOURCEW(IDI_DEUTSCHTELEX));
    window_class.hIconSm = window_class.hIcon;
    window_class.hbrBackground = static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH));
    window_class.lpszClassName = kWindowClassName;
    if (RegisterClassExW(&window_class) == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return false;
    }

    dpi_ = QueryWindowDpi(owner_);

    RECT bounds{0, 0, ScaleForDpi(kClientWidth, dpi_), ScaleForDpi(kClientHeight, dpi_)};
    const DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU;
    const DWORD extended_style = WS_EX_DLGMODALFRAME | WS_EX_TOOLWINDOW;
    if (!AdjustBoundsForDpi(bounds, style, extended_style, dpi_)) {
        return false;
    }
    const int width = bounds.right - bounds.left;
    const int height = bounds.bottom - bounds.top;

    MONITORINFO monitor_info{};
    monitor_info.cbSize = sizeof(monitor_info);
    const HMONITOR monitor = MonitorFromWindow(owner_, MONITOR_DEFAULTTONEAREST);
    if (GetMonitorInfoW(monitor, &monitor_info) == FALSE) {
        monitor_info.rcWork = {0, 0, GetSystemMetrics(SM_CXSCREEN),
                               GetSystemMetrics(SM_CYSCREEN)};
    }
    const int x = static_cast<int>(monitor_info.rcWork.left +
                  std::max(0L, (monitor_info.rcWork.right - monitor_info.rcWork.left - width) / 2L));
    const int y = static_cast<int>(monitor_info.rcWork.top +
                  std::max(0L, (monitor_info.rcWork.bottom - monitor_info.rcWork.top - height) / 2L));

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
        dpi_ = QueryWindowDpi(window_);
        CreateBrushes();
        CreateFonts();
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
    case WM_DPICHANGED: {
        const auto* suggested_bounds = reinterpret_cast<const RECT*>(l_param);
        dpi_ = LOWORD(w_param);
        SetWindowPos(window_, nullptr, suggested_bounds->left, suggested_bounds->top,
                     suggested_bounds->right - suggested_bounds->left,
                     suggested_bounds->bottom - suggested_bounds->top,
                     SWP_NOACTIVATE | SWP_NOZORDER);
        CreateFonts();
        ApplyFonts();
        UpdateHeaderIcon();
        LayoutControls();
        InvalidateRect(window_, nullptr, TRUE);
        return 0;
    }
    case WM_ERASEBKGND: {
        RECT bounds{};
        GetClientRect(window_, &bounds);
        FillRect(reinterpret_cast<HDC>(w_param), &bounds,
                 window_brush_ != nullptr ? window_brush_
                                          : static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));
        return 1;
    }
    case WM_PAINT:
        PaintWindow();
        return 0;
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN: {
        const LRESULT color = ControlColor(reinterpret_cast<HDC>(w_param),
                                           reinterpret_cast<HWND>(l_param));
        return color != 0 ? color : DefWindowProcW(window_, message, w_param, l_param);
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
    const DWORD checkbox_style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX;
    const DWORD static_style = WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX;
    const DWORD button_style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON;

    header_icon_ = CreateControl(L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_ICON,
                                 0, normal_font_);
    header_title_ = CreateControl(L"STATIC", L"DeutschTelex", static_style,
                                  0, heading_font_);
    header_subtitle_ = CreateControl(
        L"STATIC", L"Fast, private German input for a QWERTY keyboard",
        static_style, 0, small_font_);

    general_title_ = CreateControl(L"STATIC", L"GENERAL", static_style,
                                   0, section_font_);
    start_with_windows_ = CreateControl(
        L"BUTTON", L"&Start DeutschTelex with Windows", checkbox_style,
        kStartWithWindowsCheckbox, normal_font_);
    show_notifications_ = CreateControl(
        L"BUTTON", L"Show &ON/OFF notifications", checkbox_style,
        kShowNotificationsCheckbox, normal_font_);

    input_title_ = CreateControl(L"STATIC", L"INPUT", static_style,
                                 0, section_font_);
    mapping_text_ = CreateControl(
        L"STATIC", L"ae  \u2192  \u00E4       oe  \u2192  \u00F6       ue  \u2192  \u00FC",
        static_style | SS_CENTERIMAGE, 0, semibold_font_);
    enable_eszett_ = CreateControl(L"BUTTON", L"Enable &sz  \u2192  \u00DF",
                                   checkbox_style, kEnableEszettCheckbox, normal_font_);

    applications_title_ = CreateControl(L"STATIC", L"APPLICATIONS", static_style,
                                         0, section_font_);
    disable_in_vscode_ = CreateControl(
        L"BUTTON", L"&Disable DeutschTelex in Visual Studio Code", checkbox_style,
        kDisableInVSCodeCheckbox, normal_font_);

    shortcut_title_ = CreateControl(L"STATIC", L"KEYBOARD SHORTCUT", static_style,
                                    0, section_font_);
    shortcut_label_ = CreateControl(L"STATIC", L"Toggle DeutschTelex",
                                    static_style, 0, normal_font_);
    shortcut_value_ = CreateControl(L"STATIC", L"Ctrl + Alt + G",
                                    static_style | SS_RIGHT, 0, semibold_font_);
    privacy_note_ = CreateControl(L"STATIC", L"Settings are stored locally.",
                                  static_style, 0, small_font_);

    defaults_button_ = CreateControl(L"BUTTON", L"&Defaults", button_style,
                                     kDefaultsButton, normal_font_);
    cancel_button_ = CreateControl(L"BUTTON", L"Cancel", button_style,
                                   IDCANCEL, normal_font_);
    save_button_ = CreateControl(L"BUTTON", L"&Save", button_style | BS_DEFPUSHBUTTON,
                                 IDOK, semibold_font_);

    if (header_icon_ == nullptr || header_title_ == nullptr || header_subtitle_ == nullptr ||
        general_title_ == nullptr || start_with_windows_ == nullptr ||
        show_notifications_ == nullptr || input_title_ == nullptr || mapping_text_ == nullptr ||
        enable_eszett_ == nullptr || applications_title_ == nullptr ||
        disable_in_vscode_ == nullptr || shortcut_title_ == nullptr ||
        shortcut_label_ == nullptr || shortcut_value_ == nullptr || privacy_note_ == nullptr ||
        defaults_button_ == nullptr || cancel_button_ == nullptr || save_button_ == nullptr) {
        return false;
    }

    SetWindowTheme(start_with_windows_, L"Explorer", nullptr);
    SetWindowTheme(show_notifications_, L"Explorer", nullptr);
    SetWindowTheme(enable_eszett_, L"Explorer", nullptr);
    SetWindowTheme(disable_in_vscode_, L"Explorer", nullptr);
    SetWindowTheme(defaults_button_, L"Explorer", nullptr);
    SetWindowTheme(cancel_button_, L"Explorer", nullptr);
    SetWindowTheme(save_button_, L"Explorer", nullptr);

    UpdateHeaderIcon();
    LayoutControls();
    PopulateControls(opening_settings_);
    return true;
}

HWND SettingsWindow::CreateControl(const wchar_t* const class_name,
                                   const wchar_t* const text, const DWORD style,
                                   const int identifier, const HFONT font) noexcept {
    const HWND control = CreateWindowExW(
        0, class_name, text, style, 0, 0, 0, 0, window_,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(identifier)), instance_, nullptr);
    ApplyFont(control, font);
    return control;
}

void SettingsWindow::CreateFonts() noexcept {
    DestroyFonts();
    normal_font_ = CreateUiFont(dpi_, 10, FW_NORMAL);
    small_font_ = CreateUiFont(dpi_, 9, FW_NORMAL);
    semibold_font_ = CreateUiFont(dpi_, 10, FW_SEMIBOLD);
    heading_font_ = CreateUiFont(dpi_, 19, FW_SEMIBOLD);
    section_font_ = CreateUiFont(dpi_, 9, FW_SEMIBOLD);
}

void SettingsWindow::DestroyFonts() noexcept {
    for (HFONT* const font : {&normal_font_, &small_font_, &semibold_font_,
                              &heading_font_, &section_font_}) {
        if (*font != nullptr) {
            DeleteObject(*font);
            *font = nullptr;
        }
    }
}

void SettingsWindow::CreateBrushes() noexcept {
    DestroyBrushes();
    window_brush_ = CreateSolidBrush(kWindowColor);
    card_brush_ = CreateSolidBrush(kCardColor);
}

void SettingsWindow::DestroyBrushes() noexcept {
    if (window_brush_ != nullptr) {
        DeleteObject(window_brush_);
        window_brush_ = nullptr;
    }
    if (card_brush_ != nullptr) {
        DeleteObject(card_brush_);
        card_brush_ = nullptr;
    }
}

void SettingsWindow::ApplyFonts() noexcept {
    ApplyFont(header_icon_, normal_font_);
    ApplyFont(header_title_, heading_font_);
    ApplyFont(header_subtitle_, small_font_);
    ApplyFont(general_title_, section_font_);
    ApplyFont(input_title_, section_font_);
    ApplyFont(applications_title_, section_font_);
    ApplyFont(shortcut_title_, section_font_);
    ApplyFont(mapping_text_, semibold_font_);
    ApplyFont(shortcut_label_, normal_font_);
    ApplyFont(shortcut_value_, semibold_font_);
    ApplyFont(privacy_note_, small_font_);
    ApplyFont(start_with_windows_, normal_font_);
    ApplyFont(show_notifications_, normal_font_);
    ApplyFont(enable_eszett_, normal_font_);
    ApplyFont(disable_in_vscode_, normal_font_);
    ApplyFont(defaults_button_, normal_font_);
    ApplyFont(cancel_button_, normal_font_);
    ApplyFont(save_button_, semibold_font_);
}

void SettingsWindow::UpdateHeaderIcon() noexcept {
    if (header_icon_image_ != nullptr) {
        DestroyIcon(header_icon_image_);
        header_icon_image_ = nullptr;
    }
    header_icon_image_ = static_cast<HICON>(LoadImageW(
        instance_, MAKEINTRESOURCEW(IDI_DEUTSCHTELEX), IMAGE_ICON,
        ScaleForDpi(40, dpi_), ScaleForDpi(40, dpi_), LR_DEFAULTCOLOR));
    if (header_icon_ != nullptr) {
        SendMessageW(header_icon_, STM_SETICON,
                     reinterpret_cast<WPARAM>(header_icon_image_), 0);
    }
}

void SettingsWindow::LayoutControls() noexcept {
    SetControlBounds(header_icon_, dpi_, 24, 24, 40, 40);
    SetControlBounds(header_title_, dpi_, 80, 18, 456, 32);
    SetControlBounds(header_subtitle_, dpi_, 80, 52, 456, 22);

    SetControlBounds(general_title_, dpi_, 40, 118, 460, 20);
    SetControlBounds(start_with_windows_, dpi_, 40, 147, 460, 24);
    SetControlBounds(show_notifications_, dpi_, 40, 184, 460, 24);

    SetControlBounds(input_title_, dpi_, 40, 254, 460, 20);
    SetControlBounds(mapping_text_, dpi_, 40, 282, 460, 24);
    SetControlBounds(enable_eszett_, dpi_, 40, 321, 460, 24);

    SetControlBounds(applications_title_, dpi_, 40, 388, 460, 20);
    SetControlBounds(disable_in_vscode_, dpi_, 40, 419, 460, 24);

    SetControlBounds(shortcut_title_, dpi_, 40, 488, 460, 20);
    SetControlBounds(shortcut_label_, dpi_, 40, 517, 260, 22);
    SetControlBounds(shortcut_value_, dpi_, 340, 517, 160, 22);

    SetControlBounds(privacy_note_, dpi_, 24, 576, 210, 22);
    SetControlBounds(defaults_button_, dpi_, 252, 566, 88, 34);
    SetControlBounds(cancel_button_, dpi_, 348, 566, 88, 34);
    SetControlBounds(save_button_, dpi_, 444, 566, 96, 34);
}

void SettingsWindow::PaintWindow() noexcept {
    PAINTSTRUCT paint{};
    const HDC device_context = BeginPaint(window_, &paint);
    RECT client_bounds{};
    GetClientRect(window_, &client_bounds);
    FillRect(device_context, &client_bounds,
             window_brush_ != nullptr ? window_brush_
                                      : static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));

    const HGDIOBJ old_pen = SelectObject(device_context, GetStockObject(DC_PEN));
    SetDCPenColor(device_context, kBorderColor);
    MoveToEx(device_context, ScaleForDpi(20, dpi_), ScaleForDpi(88, dpi_), nullptr);
    LineTo(device_context, ScaleForDpi(540, dpi_), ScaleForDpi(88, dpi_));
    SelectObject(device_context, old_pen);

    DrawCard(device_context, dpi_, 20, 104, 520, 124);
    DrawCard(device_context, dpi_, 20, 240, 520, 122);
    DrawCard(device_context, dpi_, 20, 374, 520, 88);
    DrawCard(device_context, dpi_, 20, 474, 520, 72);
    EndPaint(window_, &paint);
}

LRESULT SettingsWindow::ControlColor(const HDC device_context, const HWND control) const noexcept {
    const bool push_button = control == defaults_button_ || control == cancel_button_ ||
                             control == save_button_;
    if (push_button) {
        return 0;
    }

    SetBkMode(device_context, TRANSPARENT);
    if (control == general_title_ || control == input_title_ ||
        control == applications_title_ || control == shortcut_title_) {
        SetTextColor(device_context, kAccentColor);
    } else if (control == header_subtitle_ || control == privacy_note_ ||
               control == shortcut_label_) {
        SetTextColor(device_context, kSecondaryTextColor);
    } else {
        SetTextColor(device_context, kPrimaryTextColor);
    }

    const bool window_background = control == header_icon_ || control == header_title_ ||
                                   control == header_subtitle_ || control == privacy_note_;
    const HBRUSH brush = window_background
                             ? window_brush_
                             : card_brush_;
    return reinterpret_cast<LRESULT>(
        brush != nullptr ? brush : static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));
}

void SettingsWindow::PopulateControls(const config::AppSettings& settings) noexcept {
    SetChecked(start_with_windows_, settings.start_with_windows);
    SetChecked(show_notifications_, settings.show_toggle_notifications);
    SetChecked(enable_eszett_, settings.enable_eszett);
    SetChecked(disable_in_vscode_, settings.disable_in_vscode);
}

config::AppSettings SettingsWindow::ReadControls() const noexcept {
    return {
        IsChecked(start_with_windows_),
        IsChecked(show_notifications_),
        IsChecked(enable_eszett_),
        IsChecked(disable_in_vscode_),
    };
}

void SettingsWindow::SaveAndClose() noexcept {
    if (delegate_.SaveSettings(ReadControls(), window_)) {
        Close();
    }
}

}  // namespace deutschtelex::app
