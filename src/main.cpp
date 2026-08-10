#include "app/tray_app.h"

#include <cwchar>

#include <shellapi.h>

namespace {

bool IsHookSmokeTestInvocation() noexcept {
    int argument_count{};
    PWSTR* const arguments = CommandLineToArgvW(GetCommandLineW(), &argument_count);
    if (arguments == nullptr) {
        return false;
    }
    const bool smoke_test = argument_count == 2 &&
                            std::wcscmp(arguments[1], L"--hook-smoke-test") == 0;
    LocalFree(arguments);
    return smoke_test;
}

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    const bool smoke_test = IsHookSmokeTestInvocation();
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
