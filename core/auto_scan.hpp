#pragma once

// ---------------------------------------------------------------------------
// core/auto_scan.hpp
// Synchronous WMI-based hardware scan.
// ---------------------------------------------------------------------------

#include "common/types.hpp"

namespace nbi {

// ---------------------------------------------------------------------------
// AutoScan
//
// Performs a one-shot synchronous WMI query to populate a DeviceInfo struct.
// Must be called from a thread that is either COM-uninitialised or initialised
// with COINIT_APARTMENTTHREADED / COINIT_MULTITHREADED — the implementation
// calls CoInitializeEx itself and CoUninitializes before returning.
//
// Never throws; individual WMI failures set the affected field to "N/A" / 0.
// ---------------------------------------------------------------------------
class AutoScan {
public:
    AutoScan()  = delete;
    ~AutoScan() = delete;

    /// Run all WMI queries synchronously and return the populated DeviceInfo.
    static DeviceInfo scan();
};

} // namespace nbi
