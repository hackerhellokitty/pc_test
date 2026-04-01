#pragma once

// ---------------------------------------------------------------------------
// core/network_manager.hpp
// Network connectivity check: adapter enumeration + Wi-Fi scan + ICMP ping.
// ---------------------------------------------------------------------------

#include <cstdint>
#include <vector>

#include <QString>

#include "common/types.hpp"

namespace nbi {

// ---------------------------------------------------------------------------
// AdapterInfo — one active network adapter
// ---------------------------------------------------------------------------
struct AdapterInfo {
    QString  name;              ///< Friendly name, e.g. "Intel Wi-Fi 6E"
    QString  type;              ///< "Wi-Fi" | "Ethernet" | "Other"
    QString  ip_address;        ///< First IPv4 address (or "—")
    bool     connected{false};  ///< Has at least one unicast IPv4 address
    bool     is_wifi{false};
};

// ---------------------------------------------------------------------------
// WifiNetwork — one visible SSID from WlanGetAvailableNetworkList
// ---------------------------------------------------------------------------
struct WifiNetwork {
    QString  ssid;
    int      signal_quality{0};  ///< 0–100 (Windows WLAN_SIGNAL_QUALITY)
    bool     connected{false};   ///< Currently associated
    bool     secured{true};
};

// ---------------------------------------------------------------------------
// WifiInfo — Wi-Fi adapter state
// ---------------------------------------------------------------------------
struct WifiInfo {
    bool                    available{false};   ///< Has a Wi-Fi adapter
    bool                    radio_on{false};    ///< Radio is enabled
    QString                 adapter_name;
    QString                 connected_ssid;     ///< Empty if not associated
    std::vector<WifiNetwork> networks;          ///< Visible SSIDs
};

// ---------------------------------------------------------------------------
// PingResult
// ---------------------------------------------------------------------------
struct PingResult {
    QString  host;
    bool     reachable{false};
    uint32_t rtt_ms{0};
};

// ---------------------------------------------------------------------------
// NetworkResult — full outcome of the network module
// ---------------------------------------------------------------------------
struct NetworkResult {
    bool                     no_interface{false};
    std::vector<AdapterInfo> adapters;
    WifiInfo                 wifi;
    std::vector<PingResult>  pings;
};

// ---------------------------------------------------------------------------
// NetworkManager
// ---------------------------------------------------------------------------
class NetworkManager {
public:
    static std::vector<AdapterInfo> enumerateAdapters();
    static WifiInfo                 scanWifi();
    static PingResult               ping(const QString& host, uint32_t timeout_ms = 500);
    static NetworkResult            scan();
    static ModuleResult             evaluate(const NetworkResult& r);
};

} // namespace nbi
