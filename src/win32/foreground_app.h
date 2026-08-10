#pragma once

#include <string_view>

#include <windows.h>

namespace deutschtelex::win32 {

enum class ForegroundAppIdentity {
    Unknown,
    VisualStudioCode,
    Other,
};

[[nodiscard]] bool IsVisualStudioCodeExecutable(std::wstring_view executable_name) noexcept;
[[nodiscard]] ForegroundAppIdentity IdentifyForegroundApp(HWND window) noexcept;

[[nodiscard]] constexpr bool ShouldBypassInput(
    const bool globally_enabled, const bool disable_in_vscode,
    const ForegroundAppIdentity identity) noexcept {
    if (!globally_enabled) {
        return true;
    }
    return disable_in_vscode &&
           (identity == ForegroundAppIdentity::VisualStudioCode ||
            identity == ForegroundAppIdentity::Unknown);
}

// Holds only the current HWND and its coarse identity. It deliberately has no
// history and no executable-path storage.
class ForegroundContextCache {
public:
    [[nodiscard]] HWND Window() const noexcept;
    [[nodiscard]] ForegroundAppIdentity Identity() const noexcept;
    [[nodiscard]] bool Update(HWND window, ForegroundAppIdentity identity) noexcept;
    void Clear() noexcept;

private:
    HWND window_{};
    ForegroundAppIdentity identity_{ForegroundAppIdentity::Unknown};
};

}  // namespace deutschtelex::win32
