#pragma once

// ---------------------------------------------------------------------------
// core/battery_manager.hpp
// Battery health via WMI BatteryFullChargeCapacity + BatteryStaticData.
// ---------------------------------------------------------------------------

#include <cstdint>
#include <vector>
#include <QString>
#include "common/types.hpp"

namespace nbi {

struct BatteryInfo {
    QString  name;                  ///< e.g. "DELL BVBEL5" or "Battery 0"
    uint32_t design_capacity{0};    ///< mWh — from BatteryStaticData
    uint32_t full_charge_capacity{0}; ///< mWh — from BatteryFullChargeCapacity
    uint32_t cycle_count{0};        ///< from BatteryStaticData (0 = unknown)
    int      wear_level{-1};        ///< 0–100 %, -1 = unknown
    bool     flagged{false};        ///< wear < 60 %
    QString  error_detail;
};

struct BatteryResult {
    bool                    no_battery{false};
    std::vector<BatteryInfo> batteries;
};

class BatteryManager {
public:
    static BatteryResult scan();
    static ModuleResult  evaluate(const BatteryResult& r);
};

} // namespace nbi
