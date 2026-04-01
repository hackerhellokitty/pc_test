// ---------------------------------------------------------------------------
// core/scorer.cpp
// ---------------------------------------------------------------------------
#include "core/scorer.hpp"

namespace nbi {

ScoreResult Scorer::compute(const std::array<ModuleResult, 10>& results)
{
    // --- Pass/Fail per slot (Skipped/NotRun excluded from denominator) ---
    int earned = 0;
    int possible = 0;

    for (int i = 0; i < 10; ++i) {
        const int w = kWeights[static_cast<std::size_t>(i)];
        if (w == 0) continue;
        const auto& r = results[static_cast<std::size_t>(i)];
        if (r.status == TestStatus::Skipped || r.status == TestStatus::NotRun) continue;
        possible += w;
        if (r.status == TestStatus::Pass) earned += w;
    }

    int raw = (possible > 0) ? (earned * 100 / possible) : 100;

    // --- Cap rule ---
    bool capped = false;
    QString cap_reason;

    // Battery index = 3: Fail + wear message contains "< 60%"
    const auto& bat = results[3];
    if (bat.status == TestStatus::Fail) {
        capped = true;
        cap_reason = QStringLiteral("Battery Wear ต่ำกว่า 60%");
    }

    // SMART index = 4: Fail
    const auto& smart = results[4];
    if (smart.status == TestStatus::Fail) {
        capped = true;
        if (!cap_reason.isEmpty()) cap_reason += QStringLiteral(" + ");
        cap_reason += QStringLiteral("S.M.A.R.T. พบปัญหา");
    }

    int final_score = raw;
    if (capped && raw >= 60) final_score = 59;

    // --- Verdict ---
    QString verdict;
    QString verdict_color;
    if (final_score >= 80) {
        verdict       = QStringLiteral("ผ่าน");
        verdict_color = QStringLiteral("#4caf50");
    } else if (final_score >= 60) {
        verdict       = QStringLiteral("ปานกลาง");
        verdict_color = QStringLiteral("#f0c040");
    } else {
        verdict       = QStringLiteral("ไม่ผ่าน");
        verdict_color = QStringLiteral("#f44336");
    }

    ScoreResult sr;
    sr.raw_score    = raw;
    sr.final_score  = final_score;
    sr.capped       = capped;
    sr.cap_reason   = cap_reason;
    sr.verdict      = verdict;
    sr.verdict_color = verdict_color;
    return sr;
}

} // namespace nbi
