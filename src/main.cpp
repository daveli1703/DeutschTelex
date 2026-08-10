#include "app/tray_app.h"

#include <string_view>

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR command_line, int) {
    deutschtelex::app::TrayApp application{instance};
    if (!application.Initialize()) {
        return 1;
    }

    const std::wstring_view arguments{command_line == nullptr ? L"" : command_line};
    if (arguments == L"--hook-smoke-test") {
        application.Shutdown();
        return 0;
    }

    const int exit_code = application.Run();
    application.Shutdown();
    return exit_code;
}
