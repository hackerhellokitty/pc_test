// ---------------------------------------------------------------------------
// core/network_manager.cpp
// ---------------------------------------------------------------------------

// Windows headers — must come before Qt
#ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <icmpapi.h>
#include <wlanapi.h>
#include <windows.h>

#include "core/network_manager.hpp"

#include <QString>
#include <QStringList>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

namespace {

QString ifTypeToString(DWORD type)
{
    switch (type) {
        case IF_TYPE_ETHERNET_CSMACD:   return QStringLiteral("Ethernet");
        case IF_TYPE_IEEE80211:         return QStringLiteral("Wi-Fi");
        case IF_TYPE_SOFTWARE_LOOPBACK: return QStringLiteral("Loopback");
        default:                        return QStringLiteral("Other");
    }
}

QString firstIpv4(PIP_ADAPTER_UNICAST_ADDRESS ua)
{
    for (auto* p = ua; p; p = p->Next) {
        if (p->Address.lpSockaddr &&
            p->Address.lpSockaddr->sa_family == AF_INET)
        {
            char buf[INET_ADDRSTRLEN] = {};
            auto* sin = reinterpret_cast<SOCKADDR_IN*>(p->Address.lpSockaddr);
            if (InetNtopA(AF_INET, &sin->sin_addr, buf, sizeof(buf)))
                return QString::fromLatin1(buf);
        }
    }
    return QStringLiteral("—");
}

bool hasIpv4(PIP_ADAPTER_UNICAST_ADDRESS ua)
{
    for (auto* p = ua; p; p = p->Next) {
        if (p->Address.lpSockaddr &&
            p->Address.lpSockaddr->sa_family == AF_INET)
            return true;
    }
    return false;
}

QString defaultGateway(PIP_ADAPTER_ADDRESSES adapters)
{
    for (auto* a = adapters; a; a = a->Next) {
        for (auto* gw = a->FirstGatewayAddress; gw; gw = gw->Next) {
            if (gw->Address.lpSockaddr &&
                gw->Address.lpSockaddr->sa_family == AF_INET)
            {
                char buf[INET_ADDRSTRLEN] = {};
                auto* sin = reinterpret_cast<SOCKADDR_IN*>(gw->Address.lpSockaddr);
                if (InetNtopA(AF_INET, &sin->sin_addr, buf, sizeof(buf)))
                    return QString::fromLatin1(buf);
            }
        }
    }
    return {};
}

} // namespace

namespace nbi {

// ---------------------------------------------------------------------------
// enumerateAdapters
// ---------------------------------------------------------------------------
std::vector<AdapterInfo> NetworkManager::enumerateAdapters()
{
    std::vector<AdapterInfo> result;

    ULONG bufLen = 0;
    const ULONG flags = GAA_FLAG_INCLUDE_GATEWAYS | GAA_FLAG_SKIP_MULTICAST |
                        GAA_FLAG_SKIP_DNS_SERVER;
    ::GetAdaptersAddresses(AF_INET, flags, nullptr, nullptr, &bufLen);
    if (bufLen == 0) return result;

    std::vector<BYTE> buf(bufLen);
    auto* addrs = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buf.data());

    if (::GetAdaptersAddresses(AF_INET, flags, nullptr, addrs, &bufLen) != ERROR_SUCCESS)
        return result;

    for (auto* a = addrs; a; a = a->Next) {
        if (a->IfType == IF_TYPE_SOFTWARE_LOOPBACK) continue;
        if (a->IfType == IF_TYPE_TUNNEL)            continue;
        if (!hasIpv4(a->FirstUnicastAddress))        continue;

        AdapterInfo ai;
        ai.name       = QString::fromWCharArray(a->FriendlyName);
        ai.type       = ifTypeToString(a->IfType);
        ai.ip_address = firstIpv4(a->FirstUnicastAddress);
        ai.connected  = true;
        ai.is_wifi    = (a->IfType == IF_TYPE_IEEE80211);
        result.push_back(std::move(ai));
    }

    return result;
}

// ---------------------------------------------------------------------------
// scanWifi  — Native Wi-Fi API
// ---------------------------------------------------------------------------
WifiInfo NetworkManager::scanWifi()
{
    WifiInfo info;

    HANDLE hWlan = nullptr;
    DWORD  negotiated = 0;
    if (::WlanOpenHandle(2, nullptr, &negotiated, &hWlan) != ERROR_SUCCESS)
        return info;   // WLAN service not available

    // Enumerate interfaces
    PWLAN_INTERFACE_INFO_LIST pIfList = nullptr;
    if (::WlanEnumInterfaces(hWlan, nullptr, &pIfList) != ERROR_SUCCESS || !pIfList) {
        ::WlanCloseHandle(hWlan, nullptr);
        return info;
    }

    if (pIfList->dwNumberOfItems == 0) {
        ::WlanFreeMemory(pIfList);
        ::WlanCloseHandle(hWlan, nullptr);
        return info;
    }

    info.available = true;

    // Use the first Wi-Fi interface
    const WLAN_INTERFACE_INFO& iface = pIfList->InterfaceInfo[0];
    info.adapter_name = QString::fromWCharArray(iface.strInterfaceDescription);
    info.radio_on     = (iface.isState != wlan_interface_state_disconnected &&
                         iface.isState != wlan_interface_state_not_ready);

    // Get currently connected SSID via connection attributes
    if (iface.isState == wlan_interface_state_connected) {
        PWLAN_CONNECTION_ATTRIBUTES pConn = nullptr;
        DWORD dataSize = 0;
        WLAN_OPCODE_VALUE_TYPE opcode;
        if (::WlanQueryInterface(hWlan, &iface.InterfaceGuid,
                                 wlan_intf_opcode_current_connection,
                                 nullptr, &dataSize,
                                 reinterpret_cast<PVOID*>(&pConn), &opcode) == ERROR_SUCCESS
            && pConn)
        {
            const auto& ssidRaw = pConn->wlanAssociationAttributes.dot11Ssid;
            info.connected_ssid = QString::fromUtf8(
                reinterpret_cast<const char*>(ssidRaw.ucSSID),
                static_cast<qsizetype>(ssidRaw.uSSIDLength));
            ::WlanFreeMemory(pConn);
        }
    }

    // Get available network list
    PWLAN_AVAILABLE_NETWORK_LIST pNetList = nullptr;
    if (::WlanGetAvailableNetworkList(hWlan, &iface.InterfaceGuid,
                                      WLAN_AVAILABLE_NETWORK_INCLUDE_ALL_ADHOC_PROFILES |
                                      WLAN_AVAILABLE_NETWORK_INCLUDE_ALL_MANUAL_HIDDEN_PROFILES,
                                      nullptr, &pNetList) == ERROR_SUCCESS
        && pNetList)
    {
        for (DWORD i = 0; i < pNetList->dwNumberOfItems; ++i) {
            const WLAN_AVAILABLE_NETWORK& n = pNetList->Network[i];

            // Skip hidden (empty SSID)
            if (n.dot11Ssid.uSSIDLength == 0) continue;

            WifiNetwork wn;
            wn.ssid = QString::fromUtf8(
                reinterpret_cast<const char*>(n.dot11Ssid.ucSSID),
                static_cast<qsizetype>(n.dot11Ssid.uSSIDLength));
            wn.signal_quality = static_cast<int>(n.wlanSignalQuality);
            wn.connected = (n.dwFlags & WLAN_AVAILABLE_NETWORK_CONNECTED) != 0;
            wn.secured   = (n.dot11DefaultAuthAlgorithm != DOT11_AUTH_ALGO_80211_OPEN);

            // Deduplicate by SSID
            bool found = false;
            for (auto& existing : info.networks) {
                if (existing.ssid == wn.ssid) {
                    // Keep strongest signal
                    if (wn.signal_quality > existing.signal_quality)
                        existing = wn;
                    found = true;
                    break;
                }
            }
            if (!found)
                info.networks.push_back(std::move(wn));
        }
        ::WlanFreeMemory(pNetList);

        // Sort: connected first, then by signal descending
        std::sort(info.networks.begin(), info.networks.end(),
            [](const WifiNetwork& a, const WifiNetwork& b) {
                if (a.connected != b.connected) return a.connected > b.connected;
                return a.signal_quality > b.signal_quality;
            });
    }

    ::WlanFreeMemory(pIfList);
    ::WlanCloseHandle(hWlan, nullptr);
    return info;
}

// ---------------------------------------------------------------------------
// ping
// ---------------------------------------------------------------------------
PingResult NetworkManager::ping(const QString& host, uint32_t timeout_ms)
{
    PingResult pr;
    pr.host = host;

    const std::string host_a = host.toStdString();
    struct addrinfo hints{}, *res = nullptr;
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_RAW;
    if (::getaddrinfo(host_a.c_str(), nullptr, &hints, &res) != 0 || !res)
        return pr;

    auto* sin = reinterpret_cast<SOCKADDR_IN*>(res->ai_addr);
    const IPAddr dest = sin->sin_addr.s_addr;
    ::freeaddrinfo(res);

    HANDLE hIcmp = ::IcmpCreateFile();
    if (hIcmp == INVALID_HANDLE_VALUE) return pr;

    constexpr WORD data_size = 32;
    char send_data[data_size] = {};
    const DWORD reply_size = sizeof(ICMP_ECHO_REPLY) + data_size + 8;
    std::vector<BYTE> reply_buf(reply_size);

    const DWORD sent = ::IcmpSendEcho(
        hIcmp, dest, send_data, data_size, nullptr,
        reply_buf.data(), reply_size, timeout_ms);
    ::IcmpCloseHandle(hIcmp);

    if (sent > 0) {
        auto* reply = reinterpret_cast<PICMP_ECHO_REPLY>(reply_buf.data());
        if (reply->Status == IP_SUCCESS) {
            pr.reachable = true;
            pr.rtt_ms    = reply->RoundTripTime;
        }
    }
    return pr;
}

// ---------------------------------------------------------------------------
// scan
// ---------------------------------------------------------------------------
NetworkResult NetworkManager::scan()
{
    NetworkResult r;

    WSADATA wsa{};
    ::WSAStartup(MAKEWORD(2, 2), &wsa);

    r.adapters = enumerateAdapters();
    r.wifi     = scanWifi();

    if (r.adapters.empty()) {
        r.no_interface = true;
        ::WSACleanup();
        return r;
    }

    // Default gateway for ping
    ULONG bufLen = 0;
    const ULONG flags = GAA_FLAG_INCLUDE_GATEWAYS | GAA_FLAG_SKIP_MULTICAST |
                        GAA_FLAG_SKIP_DNS_SERVER;
    ::GetAdaptersAddresses(AF_INET, flags, nullptr, nullptr, &bufLen);
    std::vector<BYTE> buf(bufLen > 0 ? bufLen : 1);
    auto* addrs = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buf.data());
    const QString gw = (bufLen > 0 &&
        ::GetAdaptersAddresses(AF_INET, flags, nullptr, addrs, &bufLen) == ERROR_SUCCESS)
        ? defaultGateway(addrs) : QString{};

    if (!gw.isEmpty())
        r.pings.push_back(ping(gw, 500));
    r.pings.push_back(ping(QStringLiteral("8.8.8.8"), 500));

    ::WSACleanup();
    return r;
}

// ---------------------------------------------------------------------------
// evaluate
// ---------------------------------------------------------------------------
ModuleResult NetworkManager::evaluate(const NetworkResult& r)
{
    ModuleResult m;
    m.label = QStringLiteral("Network");

    if (r.no_interface) {
        m.status  = TestStatus::Skipped;
        m.summary = QStringLiteral("ไม่พบ NIC");
        return m;
    }

    bool internet_ok = false;
    for (const auto& p : r.pings) {
        if (p.host == QStringLiteral("8.8.8.8") && p.reachable) {
            internet_ok = true;
            break;
        }
    }

    bool gateway_ok = false;
    for (const auto& p : r.pings) {
        if (p.host != QStringLiteral("8.8.8.8") && p.reachable) {
            gateway_ok = true;
            break;
        }
    }

    if (internet_ok) {
        m.status  = TestStatus::Pass;
        m.summary = QStringLiteral("ออนไลน์");
    } else if (gateway_ok) {
        m.status  = TestStatus::Pass;
        m.summary = QStringLiteral("LAN only");
    } else {
        m.status  = TestStatus::Fail;
        m.summary = QStringLiteral("ออฟไลน์");
        m.issues.append(QStringLiteral("ไม่สามารถ ping gateway หรือ 8.8.8.8 ได้"));
    }

    // Wi-Fi extra issue: adapter found but can't scan
    if (r.wifi.available && !r.wifi.radio_on)
        m.issues.append(QStringLiteral("Wi-Fi radio ปิดอยู่"));

    return m;
}

} // namespace nbi
