#include "app/tray_app.h"

#include <string_view>

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR command_line, int) {
    const std::wstring_view arguments{command_line == nullptr ? L"" : command_line};
    const bool smoke_test = arguments == L"--hook-smoke-test";
    deutschtelex::app::TrayApp application{instance};
    if (!application.Initialize(smoke_test)) {
        return 1;
    }

    if (smoke_test) {
        application.Shutdown();
        return 0;
    }

    const int exit_code = application.Run();
    application.Shutdown();
    return exit_code;
}
