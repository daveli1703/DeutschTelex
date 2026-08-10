#include "win32/startup_registration.h"

#include <utility>
#include <vector>

#include <windows.h>

namespace deutschtelex::win32 {
namespace {

constexpr wchar_t kRunKey[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr wchar_t kValueName[] = L"DeutschTelex";

}  // namespace

std::wstring QuoteStartupCommand(const std::wstring_view executable_path) {
    std::wstring command;
    command.reserve(executable_path.size() + 2U);
    command.push_back(L'"');
    command.append(executable_path);
    command.push_back(L'"');
    return command;
}

std::optional<std::wstring> CurrentExecutablePath() noexcept {
    try {
        std::vector<wchar_t> buffer(260U);
        while (buffer.size() <= 32768U) {
            SetLastError(ERROR_SUCCESS);
            const DWORD length = GetModuleFileNameW(nullptr, buffer.data(),
                                                    static_cast<DWORD>(buffer.size()));
            if (length == 0) {
                return std::nullopt;
            }
            if (length < buffer.size()) {
                return std::wstring{buffer.data(), length};
            }
            buffer.resize(buffer.size() * 2U);
        }
    } catch (...) {
    }
    return std::nullopt;
}

StartupRegistration::StartupRegistration(std::wstring executable_path)
    : executable_path_(std::move(executable_path)) {}

std::optional<bool> StartupRegistration::IsEnabled() const noexcept {
    HKEY key{};
    const LSTATUS open_status = RegOpenKeyExW(HKEY_CURRENT_USER, kRunKey, 0, KEY_QUERY_VALUE,
                                              &key);
    if (open_status == ERROR_FILE_NOT_FOUND) {
        return false;
    }
    if (open_status != ERROR_SUCCESS) {
        return std::nullopt;
    }

    DWORD type{};
    DWORD size{};
    const LSTATUS query_status = RegQueryValueExW(key, kValueName, nullptr, &type, nullptr,
                                                  &size);
    RegCloseKey(key);
    if (query_status == ERROR_FILE_NOT_FOUND) {
        return false;
    }
    if (query_status != ERROR_SUCCESS || type != REG_SZ || size < sizeof(wchar_t)) {
        return std::nullopt;
    }
    return true;
}

bool StartupRegistration::SetEnabled(const bool enabled) const noexcept {
    HKEY key{};
    if (enabled) {
        const LSTATUS create_status = RegCreateKeyExW(
            HKEY_CURRENT_USER, kRunKey, 0, nullptr, REG_OPTION_NON_VOLATILE,
            KEY_SET_VALUE, nullptr, &key, nullptr);
        if (create_status != ERROR_SUCCESS) {
            return false;
        }
        const std::wstring command = QuoteStartupCommand(executable_path_);
        const DWORD bytes = static_cast<DWORD>((command.size() + 1U) * sizeof(wchar_t));
        const LSTATUS set_status = RegSetValueExW(
            key, kValueName, 0, REG_SZ,
            reinterpret_cast<const BYTE*>(command.c_str()), bytes);
        RegCloseKey(key);
        return set_status == ERROR_SUCCESS;
    }

    const LSTATUS open_status = RegOpenKeyExW(HKEY_CURRENT_USER, kRunKey, 0,
                                              KEY_SET_VALUE, &key);
    if (open_status == ERROR_FILE_NOT_FOUND) {
        return true;
    }
    if (open_status != ERROR_SUCCESS) {
        return false;
    }
    const LSTATUS delete_status = RegDeleteValueW(key, kValueName);
    RegCloseKey(key);
    return delete_status == ERROR_SUCCESS || delete_status == ERROR_FILE_NOT_FOUND;
}

}  // namespace deutschtelex::win32
