#pragma once

// ---------------------------------------------------------------------------
// common/types.hpp
// Shared data types used across NBI modules.
// ---------------------------------------------------------------------------

#include <cstdint>

#include <QString>
#include <QStringList>

namespace nbi {

// ---------------------------------------------------------------------------
// TestStatus — lifecycle state of a diagnostic module
// ---------------------------------------------------------------------------
enum class TestStatus : uint8_t {
    NotRun  = 0,  ///< Module has not been started yet
    Running = 1,  ///< Module is currently executing
    Pass    = 2,  ///< Module completed without issues
    Fail    = 3,  ///< Module detected one or more failures
    Skipped = 4,  ///< Module was explicitly skipped
};

// ---------------------------------------------------------------------------
// DeviceInfo — hardware/OS summary collected during initial scan
// ---------------------------------------------------------------------------
struct DeviceInfo {
    QString  cpu_name;       ///< e.g. "Intel(R) Core(TM) i7-1260P"
    QString  gpu_name;       ///< e.g. "Intel(R) Iris(R) Xe Graphics"
    QString  os_version;     ///< e.g. "Microsoft Windows 11 Pro"
    QString  serial_number;  ///< BIOS serial number
    uint32_t ram_gb{0};      ///< Total physical RAM rounded to GB
};

// ---------------------------------------------------------------------------
// ModuleResult — outcome reported by a single diagnostic module
// ---------------------------------------------------------------------------
struct ModuleResult {
    TestStatus  status{TestStatus::NotRun};
    QString     label;    ///< Display name, e.g. "Screen"
    QString     summary;  ///< One-line human-readable result
    QStringList issues;   ///< Detailed problem descriptions (empty == none)
};

} // namespace nbi
