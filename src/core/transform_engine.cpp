#include "core/transform_engine.h"

#include <array>

namespace deutschtelex::core {
namespace {

struct Rule {
    char32_t first;
    char32_t second;
    std::u32string_view converted;
    std::u32string_view escaped;
};

constexpr std::array<Rule, 7> kRules{{
    {U'a', U'e', U"\u00E4", U"ae"},
    {U'o', U'e', U"\u00F6", U"oe"},
    {U'u', U'e', U"\u00FC", U"ue"},
    {U'A', U'e', U"\u00C4", U"Ae"},
    {U'O', U'e', U"\u00D6", U"Oe"},
    {U'U', U'e', U"\u00DC", U"Ue"},
    {U's', U'z', U"\u00DF", U"sz"},
}};

}  // namespace

Action TransformEngine::Process(const char32_t input) noexcept {
    const auto rule_index = [](const RuleId id) noexcept -> std::size_t {
        return static_cast<std::size_t>(id) - 1U;
    };

    const auto rule_for_prefix = [this](const char32_t character) noexcept -> RuleId {
        for (std::size_t index = 0; index < kRules.size(); ++index) {
            if (index == kRules.size() - 1U && !config_.enable_eszett) {
                continue;
            }
            if (kRules[index].first == character) {
                return static_cast<RuleId>(index + 1U);
            }
        }
        return RuleId::None;
    };

    if (stage_ == Stage::Converted) {
        const Rule& active_rule = kRules[rule_index(rule_)];
        if (input == active_rule.second) {
            const std::u32string_view escaped = active_rule.escaped;
            Reset();
            return {ActionKind::ReplaceConverted, escaped};
        }

        rule_ = rule_for_prefix(input);
        stage_ = rule_ == RuleId::None ? Stage::Idle : Stage::Prefix;
        return Action::Pass();
    }

    if (stage_ == Stage::Prefix) {
        const Rule& active_rule = kRules[rule_index(rule_)];
        if (input == active_rule.second) {
            stage_ = Stage::Converted;
            return {ActionKind::ReplacePrevious, active_rule.converted};
        }
    }

    rule_ = rule_for_prefix(input);
    stage_ = rule_ == RuleId::None ? Stage::Idle : Stage::Prefix;
    return Action::Pass();
}

void TransformEngine::SetConfig(const TransformConfig config) noexcept {
    config_ = config;
    Reset();
}

TransformConfig TransformEngine::Config() const noexcept {
    return config_;
}

TransformEngine::Snapshot TransformEngine::Capture() const noexcept {
    return Snapshot{static_cast<std::uint8_t>(stage_), static_cast<std::uint8_t>(rule_)};
}

void TransformEngine::Restore(const Snapshot& snapshot) noexcept {
    stage_ = static_cast<Stage>(snapshot.stage_);
    rule_ = static_cast<RuleId>(snapshot.rule_);
}

bool TransformEngine::HasPendingPrefix() const noexcept {
    return stage_ == Stage::Prefix;
}

void TransformEngine::Reset() noexcept {
    stage_ = Stage::Idle;
    rule_ = RuleId::None;
}

void SingleTypoCheckpoint::BeginPrintable(const TransformEngine& engine) noexcept {
    snapshot_.reset();
    if (engine.HasPendingPrefix()) {
        snapshot_ = engine.Capture();
    }
}

void SingleTypoCheckpoint::CompletePrintable(const Action& action) noexcept {
    if (action.kind != ActionKind::Pass) {
        snapshot_.reset();
    }
}

bool SingleTypoCheckpoint::RestoreAfterBackspace(TransformEngine& engine) noexcept {
    if (!snapshot_.has_value()) {
        return false;
    }

    engine.Restore(*snapshot_);
    snapshot_.reset();
    return true;
}

void SingleTypoCheckpoint::Invalidate() noexcept {
    snapshot_.reset();
}

}  // namespace deutschtelex::core
