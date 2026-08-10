#include "win32/keyboard_hook.h"

#include <atomic>
#include <cstring>
#include <iostream>

#include <windows.h>

namespace {

std::atomic<DWORD> g_message_thread_id{};

BOOL WINAPI ConsoleControlHandler(const DWORD control_type) {
    switch (control_type) {
    case CTRL_C_EVENT:
    case CTRL_BREAK_EVENT:
    case CTRL_CLOSE_EVENT:
    case CTRL_LOGOFF_EVENT:
    case CTRL_SHUTDOWN_EVENT: {
        const DWORD thread_id = g_message_thread_id.load(std::memory_order_relaxed);
        return thread_id != 0 && PostThreadMessageW(thread_id, WM_QUIT, 0, 0) != FALSE;
    }
    default:
        return FALSE;
    }
}

}  // namespace

int main(const int argc, char* argv[]) {
    constexpr wchar_t kMutexName[] = L"Local\\DeutschTelex.Phase2.Singleton";
    const HANDLE instance_mutex = CreateMutexW(nullptr, FALSE, kMutexName);
    const DWORD mutex_error = GetLastError();
    if (instance_mutex == nullptr) {
        std::cerr << "DeutschTelex could not create its single-instance mutex. Error: "
                  << mutex_error << '\n';
        return 1;
    }
    if (mutex_error == ERROR_ALREADY_EXISTS) {
        std::cerr << "DeutschTelex is already running.\n";
        CloseHandle(instance_mutex);
        return 2;
    }

    MSG message{};
    PeekMessageW(&message, nullptr, WM_USER, WM_USER, PM_NOREMOVE);
    g_message_thread_id.store(GetCurrentThreadId(), std::memory_order_relaxed);
    if (SetConsoleCtrlHandler(ConsoleControlHandler, TRUE) == FALSE) {
        std::cerr << "DeutschTelex could not install its console shutdown handler. Error: "
                  << GetLastError() << '\n';
        CloseHandle(instance_mutex);
        return 1;
    }

    deutschtelex::win32::KeyboardHook hook;
    if (!hook.Install()) {
        std::cerr << "DeutschTelex could not install the keyboard hook. Error: "
                  << hook.LastErrorCode() << '\n';
        SetConsoleCtrlHandler(ConsoleControlHandler, FALSE);
        CloseHandle(instance_mutex);
        return 1;
    }

    std::cout << "DeutschTelex Phase 2\nKeyboard hook active.\n";
    const bool smoke_test = argc == 2 && std::strcmp(argv[1], "--hook-smoke-test") == 0;
    if (!smoke_test) {
        std::cout << "Press Ctrl+C to exit.\n";
    }

    int exit_code = 0;
    if (!smoke_test) {
        BOOL message_result{};
        while ((message_result = GetMessageW(&message, nullptr, 0, 0)) > 0) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        if (message_result == -1) {
            std::cerr << "DeutschTelex message loop failed. Error: " << GetLastError() << '\n';
            exit_code = 1;
        }
    }

    if (!hook.Uninstall()) {
        std::cerr << "DeutschTelex could not remove the keyboard hook cleanly. Error: "
                  << hook.LastErrorCode() << '\n';
        exit_code = 1;
    }
    SetConsoleCtrlHandler(ConsoleControlHandler, FALSE);
    g_message_thread_id.store(0, std::memory_order_relaxed);
    CloseHandle(instance_mutex);
    std::cout << "DeutschTelex stopped.\n";
    return exit_code;
}
