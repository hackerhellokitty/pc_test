#pragma once
// ---------------------------------------------------------------------------
// core/scorer.hpp
// Scoring engine: weight per module, cap rule, verdict.
// ---------------------------------------------------------------------------
#include <array>
#include <QString>
#include "common/types.hpp"

namespace nbi {

struct ScoreResult {
    int     raw_score{0};      ///< 0-100 before cap
    int     final_score{0};    ///< 0-100 after cap
    bool    capped{false};     ///< true if cap rule triggered
    QString cap_reason;        ///< human-readable reason for cap
    QString verdict;           ///< "ผ่าน" / "ปานกลาง" / "ไม่ผ่าน"
    QString verdict_color;     ///< CSS hex colour
};

class Scorer {
public:
    // results must be exactly 10 elements ordered 0-9 as per kModuleNames
    static ScoreResult compute(const std::array<ModuleResult, 10>& results);

private:
    static constexpr std::array<int, 10> kWeights = {
        15, // 0 Screen
        15, // 1 Keyboard
         5, // 2 Touchpad
        20, // 3 Battery
        20, // 4 SMART
         5, // 5 Thermal
         5, // 6 Ports
         5, // 7 Audio
         0, // 8 Network  (informational)
         0, // 9 Physical (informational)
    };
};

} // namespace nbi
