#include "config/settings_store.h"

#include <cctype>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

#ifdef _WIN32
#include <windows.h>
#endif

namespace deutschtelex::config {
namespace {

std::string_view Trim(const std::string_view value) noexcept {
    std::size_t first = 0;
    while (first < value.size() &&
           std::isspace(static_cast<unsigned char>(value[first])) != 0) {
        ++first;
    }
    std::size_t last = value.size();
    while (last > first &&
           std::isspace(static_cast<unsigned char>(value[last - 1U])) != 0) {
        --last;
    }
    return value.substr(first, last - first);
}

std::optional<bool> ParseBoolean(const std::string_view value) noexcept {
    if (value == "true") {
        return true;
    }
    if (value == "false") {
        return false;
    }
    return std::nullopt;
}

const char* BooleanText(const bool value) noexcept {
    return value ? "true" : "false";
}

}  // namespace

SettingsStore::SettingsStore(std::filesystem::path path) : path_(std::move(path)) {}

AppSettings SettingsStore::Load() const noexcept {
    AppSettings settings = DefaultSettings();
    try {
        std::ifstream input{path_};
        if (!input) {
            return settings;
        }

        std::string section;
        std::string line;
        while (std::getline(input, line)) {
            const std::string_view trimmed = Trim(line);
            if (trimmed.empty() || trimmed.front() == ';' || trimmed.front() == '#') {
                continue;
            }
            if (trimmed.size() >= 2U && trimmed.front() == '[' && trimmed.back() == ']') {
                section.assign(Trim(trimmed.substr(1U, trimmed.size() - 2U)));
                continue;
            }

            const std::size_t separator = trimmed.find('=');
            if (separator == std::string_view::npos) {
                continue;
            }
            const std::string_view key = Trim(trimmed.substr(0, separator));
            const std::optional<bool> value = ParseBoolean(Trim(trimmed.substr(separator + 1U)));
            if (!value.has_value()) {
                continue;
            }

            if (section == "General" && key == "StartWithWindows") {
                settings.start_with_windows = *value;
            } else if (section == "General" && key == "ShowNotifications") {
                settings.show_toggle_notifications = *value;
            } else if (section == "Input" && key == "EnableEszett") {
                settings.enable_eszett = *value;
            }
        }
    } catch (...) {
        return DefaultSettings();
    }
    return settings;
}

bool SettingsStore::Save(const AppSettings& settings) const noexcept {
    try {
        std::error_code error;
        const std::filesystem::path directory = path_.parent_path();
        if (!directory.empty()) {
            std::filesystem::create_directories(directory, error);
            if (error) {
                return false;
            }
        }

        std::filesystem::path temporary = path_;
        temporary += L".tmp";
        {
            std::ofstream output{temporary, std::ios::binary | std::ios::trunc};
            if (!output) {
                return false;
            }
            output << "[General]\n"
                   << "StartWithWindows=" << BooleanText(settings.start_with_windows) << '\n'
                   << "ShowNotifications=" << BooleanText(settings.show_toggle_notifications)
                   << "\n\n[Input]\n"
                   << "EnableEszett=" << BooleanText(settings.enable_eszett) << '\n';
            output.flush();
            if (!output) {
                output.close();
                std::filesystem::remove(temporary, error);
                return false;
            }
        }

#ifdef _WIN32
        if (MoveFileExW(temporary.c_str(), path_.c_str(),
                        MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) == FALSE) {
            std::filesystem::remove(temporary, error);
            return false;
        }
#else
        std::filesystem::rename(temporary, path_, error);
        if (error) {
            std::filesystem::remove(temporary, error);
            return false;
        }
#endif
        return true;
    } catch (...) {
        return false;
    }
}

const std::filesystem::path& SettingsStore::Path() const noexcept {
    return path_;
}

}  // namespace deutschtelex::config
