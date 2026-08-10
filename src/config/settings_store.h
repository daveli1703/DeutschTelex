#pragma once

#include "config/app_settings.h"

#include <filesystem>

namespace deutschtelex::config {

class SettingsStore {
public:
    explicit SettingsStore(std::filesystem::path path);

    [[nodiscard]] AppSettings Load() const noexcept;
    [[nodiscard]] bool Save(const AppSettings& settings) const noexcept;
    [[nodiscard]] const std::filesystem::path& Path() const noexcept;

private:
    std::filesystem::path path_;
};

}  // namespace deutschtelex::config
