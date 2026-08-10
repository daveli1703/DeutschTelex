#pragma once

namespace deutschtelex::config {

struct AppSettings {
    bool start_with_windows{false};
    bool show_toggle_notifications{true};
    bool enable_eszett{true};
    bool disable_in_vscode{false};

    bool operator==(const AppSettings&) const = default;
};

[[nodiscard]] constexpr AppSettings DefaultSettings() noexcept {
    return {};
}

[[nodiscard]] constexpr bool ShouldShowToggleNotification(
    const AppSettings& settings, const bool notification_requested) noexcept {
    return notification_requested && settings.show_toggle_notifications;
}

}  // namespace deutschtelex::config
