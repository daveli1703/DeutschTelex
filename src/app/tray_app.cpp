#include "app/tray_app.h"
#include "resources/resource.h"

#include <shellapi.h>
#include <shlobj.h>
#include <strsafe.h>

#include <iterator>

namespace deutschtelex::app {
namespace {

constexpr wchar_t kWindowClassName[] = L"DeutschTelexTrayCoordinator";
constexpr wchar_t kWindowTitle[] = L"DeutschTelex";
constexpr wchar_t kMutexName[] = L"Local\\DeutschTelex.Phase2.Singleton";
constexpr wchar_t kSmokeTestMutexName[] = L"Local\\DeutschTelex.HookSmokeTest.Singleton";

void CopyText(wchar_t* destination, const std::size_t destination_count,
              const std::wstring_view source) noexcept {
    static_cast<void>(StringCchCopyNW(destination, destination_count, source.data(),
                                      source.size()));
}

}  // namespace

bool EnabledState::IsEnabled() const noexcept {
    return enabled_;
}

bool EnabledState::Toggle() noexcept {
    enabled_ = !enabled_;
    return enabled_;
}

std::wstring_view TooltipFor(const bool enabled) noexcept {
    return enabled ? L"DeutschTelex \u2014 ON" : L"DeutschTelex \u2014 OFF";
}

TrayApp::TrayApp(const HINSTANCE instance) noexcept : instance_(instance) {}

TrayApp::~TrayApp() {
    Shutdown();
}

bool TrayApp::Initialize(const bool smoke_test) noexcept {
    instance_mutex_ = CreateMutexW(nullptr, FALSE,
                                   smoke_test ? kSmokeTestMutexName : kMutexName);
    const DWORD mutex_error = GetLastError();
    if (instance_mutex_ == nullptr) {
        MessageBoxW(nullptr, L"DeutschTelex could not create its single-instance lock.",
                    kWindowTitle, MB_OK | MB_ICONERROR);
        return false;
    }
    if (mutex_error == ERROR_ALREADY_EXISTS) {
        CloseHandle(instance_mutex_);
        instance_mutex_ = nullptr;
        return false;
    }

    LoadSettings();

    if (!CreateCoordinatorWindow()) {
        MessageBoxW(nullptr, L"DeutschTelex could not create its coordinator window.",
                    kWindowTitle, MB_OK | MB_ICONERROR);
        Shutdown();
        return false;
    }
    settings_window_.emplace(instance_, window_, *this);
    hook_.SetTransformConfig({settings_.enable_eszett});
    hook_.SetDisableInVisualStudioCode(settings_.disable_in_vscode);
    if (!hook_.Install()) {
        MessageBoxW(window_, L"DeutschTelex could not install the keyboard hook.",
                    kWindowTitle, MB_OK | MB_ICONERROR);
        Shutdown();
        return false;
    }
    if (!AddTrayIcon()) {
        MessageBoxW(window_, L"DeutschTelex could not create its tray icon.",
                    kWindowTitle, MB_OK | MB_ICONERROR);
        Shutdown();
        return false;
    }

    if (RegisterHotKey(window_, kHotkeyId, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, L'G') !=
        FALSE) {
        hotkey_registered_ = true;
    } else {
        ShowHotkeyUnavailableNotification();
    }
    return true;
}

int TrayApp::Run() noexcept {
    MSG message{};
    while (true) {
        const BOOL result = GetMessageW(&message, nullptr, 0, 0);
        if (result == 0) {
            return 0;
        }
        if (result == -1) {
            return 1;
        }
        if (settings_window_.has_value() &&
            settings_window_->PreTranslateMessage(message)) {
            continue;
        }
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
}

void TrayApp::Shutdown() noexcept {
    if (shutting_down_) {
        return;
    }
    shutting_down_ = true;

    hook_.SetEnabled(false);
    static_cast<void>(hook_.Uninstall());
    if (hotkey_registered_) {
        UnregisterHotKey(window_, kHotkeyId);
        hotkey_registered_ = false;
    }
    RemoveTrayIcon();
    if (settings_window_.has_value()) {
        settings_window_->Close();
        settings_window_.reset();
    }
    if (window_ != nullptr) {
        DestroyWindow(window_);
        window_ = nullptr;
    }
    if (instance_mutex_ != nullptr) {
        CloseHandle(instance_mutex_);
        instance_mutex_ = nullptr;
    }
}

bool TrayApp::SaveSettings(const config::AppSettings& settings,
                           const HWND dialog_owner) noexcept {
    const config::AppSettings previous = settings_;
    std::optional<bool> actual_startup;
    if (startup_registration_.has_value()) {
        actual_startup = startup_registration_->IsEnabled();
    }
    const win32::StartupChangePlan startup_plan = win32::PlanStartupChange(
        settings.start_with_windows, previous.start_with_windows, actual_startup);

    if (startup_plan.change_required) {
        if (!startup_registration_.has_value() ||
            !startup_registration_->SetEnabled(settings.start_with_windows)) {
            MessageBoxW(dialog_owner,
                        L"DeutschTelex could not update the per-user Start with Windows entry.\n\n"
                        L"No settings were changed.",
                        kWindowTitle, MB_OK | MB_ICONERROR);
            return false;
        }
    }

    if (!settings_store_.has_value() || !settings_store_->Save(settings)) {
        bool rollback_succeeded = true;
        if (startup_plan.change_required) {
            rollback_succeeded = startup_registration_.has_value() &&
                                 startup_registration_->SetEnabled(
                                     startup_plan.rollback_enabled);
        }
        MessageBoxW(
            dialog_owner,
            rollback_succeeded
                ? L"DeutschTelex could not save settings. The previous settings remain active."
                : L"DeutschTelex could not save settings and could not fully restore the previous "
                  L"startup registration. Please review Start with Windows before trying again.",
            kWindowTitle, MB_OK | MB_ICONERROR);
        return false;
    }

    settings_ = settings;
    if (settings_.enable_eszett != previous.enable_eszett) {
        hook_.SetTransformConfig({settings_.enable_eszett});
    }
    if (settings_.disable_in_vscode != previous.disable_in_vscode) {
        hook_.SetDisableInVisualStudioCode(settings_.disable_in_vscode);
    }
    return true;
}

LRESULT CALLBACK TrayApp::WindowProcedure(const HWND window, const UINT message,
                                          const WPARAM w_param,
                                          const LPARAM l_param) noexcept {
    TrayApp* application = nullptr;
    if (message == WM_NCCREATE) {
        const auto* create = reinterpret_cast<const CREATESTRUCTW*>(l_param);
        application = static_cast<TrayApp*>(create->lpCreateParams);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(application));
        application->window_ = window;
    } else {
        application = reinterpret_cast<TrayApp*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    }
    return application == nullptr ? DefWindowProcW(window, message, w_param, l_param)
                                  : application->HandleWindowMessage(message, w_param, l_param);
}

LRESULT TrayApp::HandleWindowMessage(const UINT message, const WPARAM w_param,
                                     const LPARAM l_param) noexcept {
    switch (message) {
    case WM_COMMAND:
        HandleCommand(CommandFromId(LOWORD(w_param)));
        return 0;
    case WM_HOTKEY:
        if (w_param == kHotkeyId) {
            ToggleEnabled(true);
        }
        return 0;
    case kTrayCallbackMessage: {
        // NOTIFYICON_VERSION_4 stores the notification event in LOWORD(l_param).
        // Older notification-area implementations also work because their event
        // value is already contained in that low word.
        const UINT tray_event = LOWORD(l_param);
        if (tray_event == NIN_SELECT || tray_event == NIN_KEYSELECT ||
            tray_event == WM_LBUTTONUP) {
            OpenSettings();
        } else if (tray_event == WM_RBUTTONUP || tray_event == WM_CONTEXTMENU) {
            ShowTrayMenu();
        }
        return 0;
    }
    case WM_QUERYENDSESSION:
        return TRUE;
    case WM_ENDSESSION:
        if (w_param != FALSE) {
            Shutdown();
        }
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(window_, message, w_param, l_param);
    }
}

bool TrayApp::CreateCoordinatorWindow() noexcept {
    WNDCLASSEXW window_class{};
    window_class.cbSize = sizeof(window_class);
    window_class.hInstance = instance_;
    window_class.lpfnWndProc = WindowProcedure;
    window_class.lpszClassName = kWindowClassName;
    if (RegisterClassExW(&window_class) == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return false;
    }

    window_ = CreateWindowExW(0, kWindowClassName, kWindowTitle, WS_OVERLAPPED,
                              0, 0, 0, 0, nullptr, nullptr, instance_, this);
    return window_ != nullptr;
}

bool TrayApp::AddTrayIcon() noexcept {
    tray_icon_ = static_cast<HICON>(LoadImageW(
        instance_, MAKEINTRESOURCEW(IDI_DEUTSCHTELEX), IMAGE_ICON,
        GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR));
    if (tray_icon_ == nullptr) {
        return false;
    }

    NOTIFYICONDATAW notification{};
    notification.cbSize = sizeof(notification);
    notification.hWnd = window_;
    notification.uID = kTrayIconId;
    notification.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    notification.uCallbackMessage = kTrayCallbackMessage;
    notification.hIcon = tray_icon_;
    CopyText(notification.szTip, std::size(notification.szTip), TooltipFor(enabled_state_.IsEnabled()));
    if (Shell_NotifyIconW(NIM_ADD, &notification) == FALSE) {
        DestroyIcon(tray_icon_);
        tray_icon_ = nullptr;
        return false;
    }
    tray_added_ = true;

    notification.uVersion = NOTIFYICON_VERSION_4;
    static_cast<void>(Shell_NotifyIconW(NIM_SETVERSION, &notification));
    return true;
}

void TrayApp::RemoveTrayIcon() noexcept {
    if (!tray_added_) {
        return;
    }
    NOTIFYICONDATAW notification{};
    notification.cbSize = sizeof(notification);
    notification.hWnd = window_;
    notification.uID = kTrayIconId;
    static_cast<void>(Shell_NotifyIconW(NIM_DELETE, &notification));
    tray_added_ = false;
    if (tray_icon_ != nullptr) {
        DestroyIcon(tray_icon_);
        tray_icon_ = nullptr;
    }
}

void TrayApp::UpdateTrayIcon() noexcept {
    if (!tray_added_) {
        return;
    }
    NOTIFYICONDATAW notification{};
    notification.cbSize = sizeof(notification);
    notification.hWnd = window_;
    notification.uID = kTrayIconId;
    notification.uFlags = NIF_TIP;
    CopyText(notification.szTip, std::size(notification.szTip), TooltipFor(enabled_state_.IsEnabled()));
    static_cast<void>(Shell_NotifyIconW(NIM_MODIFY, &notification));
}

void TrayApp::ShowTrayMenu() noexcept {
    const HMENU menu = CreatePopupMenu();
    if (menu == nullptr) {
        return;
    }
    const wchar_t* toggle_text = enabled_state_.IsEnabled() ? L"DeutschTelex: ON" :
                                                            L"DeutschTelex: OFF";
    AppendMenuW(menu, MF_STRING, kCommandToggle, toggle_text);
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, kCommandSettings, L"Settings");
    AppendMenuW(menu, MF_STRING, kCommandAbout, L"About");
    AppendMenuW(menu, MF_STRING, kCommandExit, L"Exit");

    POINT cursor{};
    GetCursorPos(&cursor);
    SetForegroundWindow(window_);
    TrackPopupMenu(menu, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN | TPM_LEFTALIGN,
                   cursor.x, cursor.y, 0, window_, nullptr);
    DestroyMenu(menu);
    PostMessageW(window_, WM_NULL, 0, 0);
}

void TrayApp::OpenSettings() noexcept {
    if (!settings_window_.has_value() || !settings_window_->Open(settings_)) {
        MessageBoxW(window_, L"DeutschTelex could not open its Settings window.",
                    kWindowTitle, MB_OK | MB_ICONERROR);
    }
}

void TrayApp::ToggleEnabled(const bool notify) noexcept {
    static_cast<void>(enabled_state_.Toggle());
    hook_.SetEnabled(enabled_state_.IsEnabled());
    UpdateTrayIcon();
    if (config::ShouldShowToggleNotification(settings_, notify)) {
        ShowToggleNotification();
    }
}

void TrayApp::ShowToggleNotification() noexcept {
    if (!tray_added_) {
        return;
    }
    NOTIFYICONDATAW notification{};
    notification.cbSize = sizeof(notification);
    notification.hWnd = window_;
    notification.uID = kTrayIconId;
    notification.uFlags = NIF_INFO;
    notification.dwInfoFlags = NIIF_INFO;
    CopyText(notification.szInfoTitle, std::size(notification.szInfoTitle), L"DeutschTelex");
    CopyText(notification.szInfo, std::size(notification.szInfo),
             enabled_state_.IsEnabled() ? L"ON" : L"OFF");
    static_cast<void>(Shell_NotifyIconW(NIM_MODIFY, &notification));
}

void TrayApp::ShowHotkeyUnavailableNotification() noexcept {
    if (!tray_added_) {
        return;
    }
    NOTIFYICONDATAW notification{};
    notification.cbSize = sizeof(notification);
    notification.hWnd = window_;
    notification.uID = kTrayIconId;
    notification.uFlags = NIF_INFO;
    notification.dwInfoFlags = NIIF_WARNING;
    CopyText(notification.szInfoTitle, std::size(notification.szInfoTitle), L"DeutschTelex");
    CopyText(notification.szInfo, std::size(notification.szInfo),
             L"Ctrl+Alt+G is unavailable. Use the tray menu to toggle.");
    static_cast<void>(Shell_NotifyIconW(NIM_MODIFY, &notification));
}

void TrayApp::ShowAbout() noexcept {
    MessageBoxW(window_,
                L"DeutschTelex\n\n"
                L"A lightweight UniKey-inspired German Telex input method.\n\n"
                L"Version 0.5 development build\n\n"
                L"ae -> \u00E4\noe -> \u00F6\nue -> \u00FC\nsz -> \u00DF\n\n"
                L"No telemetry.\nNo typed-text logging.",
                kWindowTitle, MB_OK | MB_ICONINFORMATION);
}

void TrayApp::HandleCommand(const Command command) noexcept {
    switch (command) {
    case Command::Toggle:
        ToggleEnabled(true);
        break;
    case Command::Settings:
        OpenSettings();
        break;
    case Command::About:
        ShowAbout();
        break;
    case Command::Exit:
        Shutdown();
        break;
    case Command::None:
        break;
    }
}

void TrayApp::LoadSettings() noexcept {
    if (const std::optional<std::filesystem::path> path = DefaultSettingsPath();
        path.has_value()) {
        settings_store_.emplace(*path);
        settings_ = settings_store_->Load();
    }

    if (const std::optional<std::wstring> executable =
            win32::CurrentExecutablePath(); executable.has_value()) {
        startup_registration_.emplace(*executable);
        if (const std::optional<bool> actual = startup_registration_->IsEnabled();
            actual.has_value()) {
            settings_.start_with_windows = *actual;
        }
    }
}

std::optional<std::filesystem::path> TrayApp::DefaultSettingsPath() noexcept {
    PWSTR local_app_data{};
    if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_DEFAULT,
                                    nullptr, &local_app_data))) {
        return std::nullopt;
    }
    try {
        std::filesystem::path path{local_app_data};
        CoTaskMemFree(local_app_data);
        return path / L"DeutschTelex" / L"settings.ini";
    } catch (...) {
        CoTaskMemFree(local_app_data);
        return std::nullopt;
    }
}

}  // namespace deutschtelex::app
