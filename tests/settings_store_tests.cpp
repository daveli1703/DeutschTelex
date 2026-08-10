#include "config/app_settings.h"
#include "config/settings_store.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string_view>

namespace {

using deutschtelex::config::AppSettings;
using deutschtelex::config::DefaultSettings;
using deutschtelex::config::SettingsStore;
using deutschtelex::config::ShouldShowToggleNotification;

struct TestRunner {
    int passed{};
    int failed{};

    void Check(const bool condition, const std::string_view name) {
        if (condition) {
            ++passed;
        } else {
            ++failed;
            std::cerr << "FAILED: " << name << '\n';
        }
    }
};

class TemporaryDirectory {
public:
    TemporaryDirectory() {
        const auto suffix = std::chrono::steady_clock::now().time_since_epoch().count();
        path_ = std::filesystem::temp_directory_path() /
                ("deutschtelex-settings-tests-" + std::to_string(suffix));
        std::filesystem::create_directories(path_);
    }

    ~TemporaryDirectory() {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
    }

    [[nodiscard]] const std::filesystem::path& Path() const noexcept {
        return path_;
    }

private:
    std::filesystem::path path_;
};

void WriteText(const std::filesystem::path& path, const std::string_view text) {
    std::ofstream output{path, std::ios::binary | std::ios::trunc};
    output << text;
}

}  // namespace

int main() {
    TestRunner runner;
    TemporaryDirectory temporary;
    const std::filesystem::path path = temporary.Path() / "settings.ini";
    const SettingsStore store{path};

    const AppSettings defaults = DefaultSettings();
    runner.Check(!defaults.start_with_windows, "Start with Windows defaults to false");
    runner.Check(defaults.show_toggle_notifications, "toggle notifications default to true");
    runner.Check(defaults.enable_eszett, "eszett mapping defaults to true");
    runner.Check(!defaults.disable_in_vscode, "VS Code exclusion defaults to false");
    runner.Check(store.Load() == defaults, "missing settings file loads defaults");

    const SettingsStore nested_store{temporary.Path() / "missing" / "settings.ini"};
    runner.Check(nested_store.Load() == defaults,
                 "missing settings directory loads defaults");
    runner.Check(nested_store.Save(AppSettings{true, false, true}),
                 "save creates a missing settings directory");
    runner.Check(nested_store.Load() == AppSettings{true, false, true},
                 "settings round-trip through a newly created directory");

    const AppSettings changed{true, false, false, false};
    runner.Check(store.Save(changed), "settings save succeeds");
    runner.Check(store.Load() == changed, "saved settings round-trip exactly");
    const AppSettings vscode_excluded{false, true, true, true};
    runner.Check(store.Save(vscode_excluded), "VS Code exclusion saves as true");
    runner.Check(store.Load() == vscode_excluded, "VS Code exclusion true round-trips");
    runner.Check(!std::filesystem::exists(path.wstring() + L".tmp"),
                 "successful save leaves no temporary file");

    WriteText(path, "[General]\nShowNotifications=false\n");
    runner.Check(store.Load() == AppSettings{false, false, true},
                 "missing keys retain individual defaults");

    WriteText(path,
              "[General]\nStartWithWindows=potato\nShowNotifications=false\n"
              "UnknownSetting=true\n\n[Input]\nEnableEszett=false\n"
              "\n[Applications]\nDisableInVSCode=true\n"
              "[Future]\nSomething=true\n");
    runner.Check(store.Load() == AppSettings{false, false, false, true},
                 "partially valid file applies only valid known values");

    WriteText(path,
              "[General]\nStartWithWindows=potato\nShowNotifications=perhaps\n"
              "[Input]\nEnableEszett=potato\n"
              "[Applications]\nDisableInVSCode=potato\n");
    runner.Check(store.Load() == defaults,
                 "invalid Boolean values safely use defaults");

    WriteText(path, "[Unknown]\nFutureOption=false\n");
    runner.Check(store.Load() == defaults, "unknown section and key are ignored");

    WriteText(path, "");
    runner.Check(store.Load() == defaults, "empty settings file loads defaults");

    runner.Check(ShouldShowToggleNotification(defaults, true),
                 "enabled preference requests normal toggle notification");
    AppSettings quiet = defaults;
    quiet.show_toggle_notifications = false;
    runner.Check(!ShouldShowToggleNotification(quiet, true),
                 "disabled preference suppresses normal toggle notification");
    runner.Check(!ShouldShowToggleNotification(defaults, false),
                 "unrequested status notification stays suppressed");

    std::cout << runner.passed << " tests passed; " << runner.failed
              << " tests failed.\n";
    return runner.failed == 0 ? 0 : 1;
}
