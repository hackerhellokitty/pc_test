#pragma once

// ---------------------------------------------------------------------------
// core/thermal_manager.hpp
// CPU temperature via WMI MSAcpi_ThermalZoneTemperature (ROOT\WMI).
// ---------------------------------------------------------------------------

#include <cstdint>
#include <vector>
#include <QString>
#include "common/types.hpp"

namespace nbi {

struct ThermalZone {
    QString name;           ///< e.g. "TZ00", "CPU0"
    int     temp_c{-1};     ///< current temperature °C, -1 = unknown
};

struct ThermalResult {
    std::vector<ThermalZone> zones;
    int  cpu_temp_c{-1};    ///< highest zone temp considered CPU, -1 = no sensor
    bool sensor_missing{false};
    bool flagged{false};    ///< any zone >= 95°C
};

class ThermalManager {
public:
    static ThermalResult scan();
    static ModuleResult  evaluate(const ThermalResult& r);
};

} // namespace nbi
