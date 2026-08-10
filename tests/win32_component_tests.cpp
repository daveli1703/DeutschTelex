#include "app/tray_app.h"
#include "core/transform_engine.h"
#include "win32/foreground_app.h"
#include "win32/input_injector.h"
#include "win32/key_decoder.h"
#include "win32/keyboard_hook.h"
#include "win32/startup_registration.h"

#include <array>
#include <iostream>
#include <string_view>
#include <utility>

namespace {

using deutschtelex::core::Action;
using deutschtelex::core::ActionKind;
using deutschtelex::app::Command;
using deutschtelex::app::CommandFromId;
using deutschtelex::app::EnabledState;
using deutschtelex::app::TooltipFor;
using deutschtelex::win32::BuildReplacementBatch;
using deutschtelex::win32::ForegroundAppIdentity;
using deutschtelex::win32::ForegroundContextCache;
using deutschtelex::win32::InjectionBatch;
using deutschtelex::win32::InjectionOrigin;
using deutschtelex::win32::IsResetKey;
using deutschtelex::win32::ModifierEvent;
using deutschtelex::win32::ModifierState;

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

bool AllEventsTagged(const InjectionBatch& batch, const ULONG_PTR marker) {
    for (UINT index = 0; index < batch.count; ++index) {
        if (batch.events[index].ki.dwExtraInfo != marker) {
            return false;
        }
    }
    return true;
}

}  // namespace

int main() {
    TestRunner runner;

    const std::array vscode_name_cases{
        std::pair{L"Code.exe", true},
        std::pair{L"code.exe", true},
        std::pair{L"CODE.EXE", true},
        std::pair{L"Notepad.exe", false},
        std::pair{L"chrome.exe", false},
        std::pair{L"devenv.exe", false},
        std::pair{L"WindowsTerminal.exe", false},
        std::pair{L"MyCode.exe", false},
        std::pair{L"CodeHelper.exe", false},
        std::pair{L"code.exe.backup", false},
    };
    for (const auto& [name, expected] : vscode_name_cases) {
        runner.Check(deutschtelex::win32::IsVisualStudioCodeExecutable(name) == expected,
                     "VS Code executable name matches exactly and case-insensitively");
    }

    runner.Check(!deutschtelex::win32::ShouldBypassInput(
                     true, false, ForegroundAppIdentity::VisualStudioCode),
                 "global ON with VS Code exclusion OFF allows transformation");
    runner.Check(deutschtelex::win32::ShouldBypassInput(
                     true, true, ForegroundAppIdentity::VisualStudioCode),
                 "global ON with VS Code exclusion ON bypasses VS Code");
    runner.Check(!deutschtelex::win32::ShouldBypassInput(
                     true, true, ForegroundAppIdentity::Other),
                 "global ON with VS Code exclusion ON allows other applications");
    runner.Check(deutschtelex::win32::ShouldBypassInput(
                     false, false, ForegroundAppIdentity::Other),
                 "global OFF bypasses a normal application");
    runner.Check(deutschtelex::win32::ShouldBypassInput(
                     false, true, ForegroundAppIdentity::VisualStudioCode),
                 "global OFF bypasses excluded VS Code");
    runner.Check(deutschtelex::win32::ShouldBypassInput(
                     true, true, ForegroundAppIdentity::Unknown),
                 "unresolved identity conservatively bypasses when exclusion is enabled");

    const HWND normal_window = reinterpret_cast<HWND>(static_cast<ULONG_PTR>(1));
    const HWND vscode_window = reinterpret_cast<HWND>(static_cast<ULONG_PTR>(2));
    {
        ForegroundContextCache context;
        runner.Check(context.Update(normal_window, ForegroundAppIdentity::Other),
                     "first foreground context is a transition");
        runner.Check(!context.Update(normal_window, ForegroundAppIdentity::Other),
                     "unchanged foreground context reuses cache");
        runner.Check(context.Update(vscode_window, ForegroundAppIdentity::VisualStudioCode),
                     "normal to VS Code is a context transition");
        runner.Check(context.Identity() == ForegroundAppIdentity::VisualStudioCode,
                     "foreground cache retains only current coarse identity");
        context.Clear();
        runner.Check(context.Window() == nullptr &&
                         context.Identity() == ForegroundAppIdentity::Unknown,
                     "clearing foreground cache removes current identity");
    }

    {
        deutschtelex::core::TransformEngine engine;
        ForegroundContextCache context;
        static_cast<void>(context.Update(normal_window, ForegroundAppIdentity::Other));
        static_cast<void>(engine.Process(U'a'));
        if (context.Update(vscode_window, ForegroundAppIdentity::VisualStudioCode)) {
            engine.Reset();
        }
        runner.Check(engine.Process(U'e').kind == ActionKind::Pass,
                     "normal to excluded transition resets pending prefix");
    }

    {
        deutschtelex::core::TransformEngine engine;
        ForegroundContextCache context;
        static_cast<void>(context.Update(vscode_window,
                                         ForegroundAppIdentity::VisualStudioCode));
        if (context.Update(normal_window, ForegroundAppIdentity::Other)) {
            engine.Reset();
        }
        runner.Check(engine.Process(U'e').kind == ActionKind::Pass,
                     "excluded to normal transition starts with clean engine state");
    }

    {
        EnabledState enabled_state;
        runner.Check(enabled_state.IsEnabled(), "enabled state defaults to ON");
        runner.Check(!enabled_state.Toggle(), "first toggle changes enabled state to OFF");
        runner.Check(!enabled_state.IsEnabled(), "enabled state reports OFF after toggle");
        runner.Check(enabled_state.Toggle(), "second toggle changes enabled state to ON");
        runner.Check(TooltipFor(true) == L"DeutschTelex \u2014 ON", "ON tooltip is accurate");
        runner.Check(TooltipFor(false) == L"DeutschTelex \u2014 OFF", "OFF tooltip is accurate");
        runner.Check(CommandFromId(deutschtelex::app::kCommandToggle) == Command::Toggle,
                     "toggle command ID maps to Toggle");
        runner.Check(CommandFromId(deutschtelex::app::kCommandSettings) == Command::Settings,
                     "settings command ID maps to Settings");
        runner.Check(CommandFromId(deutschtelex::app::kCommandAbout) == Command::About,
                     "about command ID maps to About");
        runner.Check(CommandFromId(deutschtelex::app::kCommandExit) == Command::Exit,
                     "exit command ID maps to Exit");
        runner.Check(CommandFromId(0) == Command::None,
                     "unknown command ID maps to None");
    }

    runner.Check(deutschtelex::win32::QuoteStartupCommand(
                     L"C:\\Program Files\\DeutschTelex\\DeutschTelex.exe") ==
                     L"\"C:\\Program Files\\DeutschTelex\\DeutschTelex.exe\"",
                 "startup command safely quotes a path containing spaces");
    runner.Check(deutschtelex::win32::QuoteStartupCommand(L"C:\\DeutschTelex.exe") ==
                     L"\"C:\\DeutschTelex.exe\"",
                 "startup command consistently quotes a simple path");
    runner.Check(deutschtelex::win32::PlanStartupChange(true, false, false)
                         .change_required,
                 "startup plan enables a missing registration");
    runner.Check(!deutschtelex::win32::PlanStartupChange(true, false, true)
                          .change_required,
                 "startup plan preserves an already enabled registration");
    const auto rollback_plan =
        deutschtelex::win32::PlanStartupChange(false, false, true);
    runner.Check(rollback_plan.change_required && rollback_plan.rollback_enabled,
                 "startup plan remembers actual state for rollback");
    runner.Check(!deutschtelex::win32::PlanStartupChange(false, false, std::nullopt)
                          .change_required,
                 "startup plan safely falls back to persisted state after query failure");

    constexpr std::array reset_keys{
        VK_BACK, VK_DELETE, VK_RETURN, VK_TAB, VK_ESCAPE, VK_LEFT, VK_RIGHT,
        VK_UP, VK_DOWN, VK_HOME, VK_END, VK_PRIOR, VK_NEXT,
    };
    for (const DWORD key : reset_keys) {
        runner.Check(IsResetKey(key), "required reset key is classified");
    }
    runner.Check(!IsResetKey('A'), "letter is not classified as reset key");

    {
        ModifierState modifiers;
        runner.Check(modifiers.DecodeLatinLetter('A') == U'a', "plain letter is lowercase");
        runner.Check(modifiers.Update(VK_LSHIFT, true) == ModifierEvent::Shift,
                     "left Shift is tracked");
        runner.Check(modifiers.DecodeLatinLetter('A') == U'A', "Shift makes uppercase");
        static_cast<void>(modifiers.Update(VK_LSHIFT, false));
        runner.Check(modifiers.DecodeLatinLetter('A') == U'a', "Shift release restores lowercase");
        runner.Check(!modifiers.DecodeLatinLetter(VK_OEM_PERIOD).has_value(),
                     "non-letter has no QWERTY letter decoding");
    }

    {
        ModifierState modifiers{true};
        runner.Check(modifiers.DecodeLatinLetter('A') == U'A', "Caps Lock makes uppercase");
        static_cast<void>(modifiers.Update(VK_RSHIFT, true));
        runner.Check(modifiers.DecodeLatinLetter('A') == U'a',
                     "Shift and Caps Lock produce lowercase");
    }

    {
        ModifierState modifiers;
        static_cast<void>(modifiers.Update(VK_CAPITAL, true));
        static_cast<void>(modifiers.Update(VK_CAPITAL, true));
        runner.Check(modifiers.DecodeLatinLetter('A') == U'A',
                     "Caps Lock repeat toggles only once");
        static_cast<void>(modifiers.Update(VK_CAPITAL, false));
        static_cast<void>(modifiers.Update(VK_CAPITAL, true));
        runner.Check(modifiers.DecodeLatinLetter('A') == U'a',
                     "next Caps Lock press toggles again");
    }

    {
        ModifierState modifiers;
        static_cast<void>(modifiers.Update(VK_LCONTROL, true));
        runner.Check(modifiers.ShortcutActive(), "Control activates shortcut state");
        static_cast<void>(modifiers.Update(VK_LCONTROL, false));
        runner.Check(!modifiers.ShortcutActive(), "Control release clears shortcut state");
        static_cast<void>(modifiers.Update(VK_RMENU, true));
        runner.Check(modifiers.ShortcutActive(), "AltGr/right Alt activates shortcut state");
        static_cast<void>(modifiers.Update(VK_RMENU, false));
        static_cast<void>(modifiers.Update(VK_LWIN, true));
        runner.Check(modifiers.ShortcutActive(), "Windows key activates shortcut state");
    }

    constexpr ULONG_PTR marker = static_cast<ULONG_PTR>(0x12345678UL);
    {
        const auto batch = BuildReplacementBatch(
            Action{ActionKind::ReplacePrevious, U"\u00E4"}, marker);
        runner.Check(batch.has_value(), "BMP replacement batch is built");
        if (batch.has_value()) {
            runner.Check(batch->count == 4, "BMP replacement has four events");
            runner.Check(batch->events[0].ki.wVk == VK_BACK &&
                             batch->events[0].ki.dwFlags == 0,
                         "batch begins with Backspace down");
            runner.Check(batch->events[1].ki.wVk == VK_BACK &&
                             batch->events[1].ki.dwFlags == KEYEVENTF_KEYUP,
                         "Backspace up follows Backspace down");
            runner.Check(batch->events[2].ki.wScan == 0x00E4 &&
                             batch->events[2].ki.dwFlags == KEYEVENTF_UNICODE,
                         "umlaut is a Unicode key-down");
            runner.Check(batch->events[3].ki.wScan == 0x00E4 &&
                             batch->events[3].ki.dwFlags ==
                                 (KEYEVENTF_UNICODE | KEYEVENTF_KEYUP),
                         "umlaut is followed by Unicode key-up");
            runner.Check(AllEventsTagged(*batch, marker), "every BMP event is tagged");
        }
    }

    {
        const auto batch = BuildReplacementBatch(
            Action{ActionKind::ReplaceConverted, U"ae"}, marker);
        runner.Check(batch.has_value() && batch->count == 6,
                     "escape replacement has six events");
        if (batch.has_value()) {
            runner.Check(batch->events[2].ki.wScan == U'a' &&
                             batch->events[4].ki.wScan == U'e',
                         "escape replacement preserves literal order");
            runner.Check(AllEventsTagged(*batch, marker), "every escape event is tagged");
        }
    }

    {
        const auto batch = BuildReplacementBatch(
            Action{ActionKind::ReplacePrevious, U"\U0001F600"}, marker);
        runner.Check(batch.has_value() && batch->count == 6,
                     "supplementary code point becomes a surrogate pair");
        if (batch.has_value()) {
            runner.Check(batch->events[2].ki.wScan == 0xD83D &&
                             batch->events[4].ki.wScan == 0xDE00,
                         "surrogate pair values are correct");
        }
    }

    runner.Check(!BuildReplacementBatch(Action::Pass(), marker).has_value(),
                 "Pass action is not injectable");
    constexpr char32_t isolated_surrogate[] = {static_cast<char32_t>(0xD800U)};
    runner.Check(!BuildReplacementBatch(
                      Action{ActionKind::ReplacePrevious,
                             std::u32string_view{isolated_surrogate, 1}}, marker)
                      .has_value(),
                 "isolated surrogate is rejected");
    constexpr char32_t invalid_code_point[] = {static_cast<char32_t>(0x110000U)};
    runner.Check(!BuildReplacementBatch(
                      Action{ActionKind::ReplacePrevious,
                             std::u32string_view{invalid_code_point, 1}}, marker)
                      .has_value(),
                 "out-of-range code point is rejected");

    {
        KBDLLHOOKSTRUCT event{};
        runner.Check(deutschtelex::win32::ClassifyInjection(event) ==
                         InjectionOrigin::Physical,
                     "untagged physical event is classified as physical");
        event.flags = LLKHF_INJECTED;
        runner.Check(deutschtelex::win32::ClassifyInjection(event) ==
                         InjectionOrigin::Foreign,
                     "foreign injected event is classified as foreign");
        event.dwExtraInfo = deutschtelex::win32::kDeutschTelexInjectionMarker;
        runner.Check(deutschtelex::win32::ClassifyInjection(event) ==
                         InjectionOrigin::DeutschTelex,
                     "own marker takes priority over injected flag");
    }

    std::cout << runner.passed << " tests passed; " << runner.failed
              << " tests failed.\n";
    return runner.failed == 0 ? 0 : 1;
}
