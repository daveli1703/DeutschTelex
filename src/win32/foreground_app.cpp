#include "win32/foreground_app.h"

#include <array>
#include <cwchar>
#include <string_view>

namespace deutschtelex::win32 {
namespace {

wchar_t LowerAscii(const wchar_t character) noexcept {
    return character >= L'A' && character <= L'Z'
               ? static_cast<wchar_t>(character - L'A' + L'a')
               : character;
}

}  // namespace

bool IsVisualStudioCodeExecutable(const std::wstring_view executable_name) noexcept {
    constexpr std::wstring_view expected = L"code.exe";
    if (executable_name.size() != expected.size()) {
        return false;
    }
    for (std::size_t index = 0; index < expected.size(); ++index) {
        if (LowerAscii(executable_name[index]) != expected[index]) {
            return false;
        }
    }
    return true;
}

ForegroundAppIdentity IdentifyForegroundApp(const HWND window) noexcept {
    if (window == nullptr) {
        return ForegroundAppIdentity::Unknown;
    }

    DWORD process_id{};
    if (GetWindowThreadProcessId(window, &process_id) == 0 || process_id == 0) {
        return ForegroundAppIdentity::Unknown;
    }

    const HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, process_id);
    if (process == nullptr) {
        return ForegroundAppIdentity::Unknown;
    }

    std::array<wchar_t, 32768> path{};
    DWORD path_length = static_cast<DWORD>(path.size());
    const BOOL queried = QueryFullProcessImageNameW(process, 0, path.data(), &path_length);
    CloseHandle(process);
    if (queried == FALSE || path_length == 0) {
        return ForegroundAppIdentity::Unknown;
    }

    const std::wstring_view full_path{path.data(), path_length};
    const std::size_t separator = full_path.find_last_of(L"\\/");
    const std::wstring_view executable_name =
        separator == std::wstring_view::npos ? full_path : full_path.substr(separator + 1U);
    return IsVisualStudioCodeExecutable(executable_name)
               ? ForegroundAppIdentity::VisualStudioCode
               : ForegroundAppIdentity::Other;
}

HWND ForegroundContextCache::Window() const noexcept {
    return window_;
}

ForegroundAppIdentity ForegroundContextCache::Identity() const noexcept {
    return identity_;
}

bool ForegroundContextCache::Update(const HWND window,
                                    const ForegroundAppIdentity identity) noexcept {
    const bool changed = window != window_ || identity != identity_;
    window_ = window;
    identity_ = identity;
    return changed;
}

void ForegroundContextCache::Clear() noexcept {
    window_ = nullptr;
    identity_ = ForegroundAppIdentity::Unknown;
}

}  // namespace deutschtelex::win32
