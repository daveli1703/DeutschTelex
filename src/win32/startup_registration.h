#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace deutschtelex::win32 {

struct StartupChangePlan {
    bool change_required{};
    bool rollback_enabled{};
};

[[nodiscard]] constexpr StartupChangePlan PlanStartupChange(
    const bool desired, const bool persisted,
    const std::optional<bool> actual) noexcept {
    const bool current = actual.value_or(persisted);
    return {desired != current, current};
}

[[nodiscard]] std::wstring QuoteStartupCommand(std::wstring_view executable_path);
[[nodiscard]] std::optional<std::wstring> CurrentExecutablePath() noexcept;

class StartupRegistration {
public:
    explicit StartupRegistration(std::wstring executable_path);

    // nullopt means the registry could not be queried reliably.
    [[nodiscard]] std::optional<bool> IsEnabled() const noexcept;
    [[nodiscard]] bool SetEnabled(bool enabled) const noexcept;

private:
    std::wstring executable_path_;
};

}  // namespace deutschtelex::win32
