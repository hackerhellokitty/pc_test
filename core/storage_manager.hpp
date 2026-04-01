#pragma once

// ---------------------------------------------------------------------------
// core/storage_manager.hpp
// S.M.A.R.T. storage health checker.
//
// Strategy:
//   1. Enumerate physical drives \\.\PhysicalDrive0 … until open fails
//   2. Per drive: detect bus type (SATA vs NVMe) via StorageAdapterProperty
//   3. SATA: read SMART attributes via IOCTL_SMART_SEND_DRIVE_COMMAND
//   4. NVMe: read via IOCTL_STORAGE_QUERY_PROPERTY (NVMe SMART log)
//            fallback: WMI MSFT_StorageReliabilityCounter / MSFT_Disk HealthStatus
//   5. Flag immediately if Reallocated(05) / Pending(C5) / Uncorrectable(C6) ≠ 0
// ---------------------------------------------------------------------------

#include <cstdint>
#include <vector>
#include <QString>

#include "common/types.hpp"

namespace nbi {

// ---------------------------------------------------------------------------
// BusType
// ---------------------------------------------------------------------------
enum class BusType {
    Unknown,
    Sata,
    NVMe,
    Other,
};

// ---------------------------------------------------------------------------
// SmartAttr — one raw SMART attribute (SATA)
// ---------------------------------------------------------------------------
struct SmartAttr {
    uint8_t  id{0};
    QString  name;
    uint32_t raw_value{0};   // little-endian 48-bit raw, stored as 32-bit (sufficient for critical attrs)
};

// ---------------------------------------------------------------------------
// DriveInfo — result for one physical drive
// ---------------------------------------------------------------------------
struct DriveInfo {
    int     index{0};          ///< PhysicalDriveN index
    QString model;
    QString serial;
    uint64_t size_bytes{0};
    BusType  bus{BusType::Unknown};

    // SMART critical attributes
    uint32_t reallocated{0};   ///< Attr 05 raw
    uint32_t pending{0};       ///< Attr C5 raw
    uint32_t uncorrectable{0}; ///< Attr C6 raw

    // NVMe-specific (from SMART log or WMI)
    uint32_t media_errors{0};
    uint32_t percent_used{0};  ///< NVMe % lifetime used (0–255, 255=worn out)

    bool smart_available{false};
    bool flagged{false};       ///< true if any critical attr ≠ 0

    QString  health_status;    ///< WMI fallback: "Healthy" / "Warning" / "Unhealthy"
    QString  error_detail;     ///< human-readable reason for flagged

    std::vector<SmartAttr> attrs;  ///< all SATA attrs (for display)
};

// ---------------------------------------------------------------------------
// StorageManager
// ---------------------------------------------------------------------------
class StorageManager {
public:
    /// Enumerate all physical drives and read SMART data.
    /// Blocking — call from a worker thread or accept brief UI freeze.
    static std::vector<DriveInfo> scan();

    /// Build a ModuleResult from scan results (for dashboard card).
    static ModuleResult evaluate(const std::vector<DriveInfo>& drives);

private:
    static DriveInfo  probeDrive(int index);
    static BusType    detectBusType(void* handle);
    static bool       readSmartSata(void* handle, DriveInfo& out);
    static bool       readSmartNvme(void* handle, DriveInfo& out);
    static bool       readSmartWmiFallback(int index, DriveInfo& out);
    static bool       readModelSerial(void* handle, DriveInfo& out);
};

} // namespace nbi
