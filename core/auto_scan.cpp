// ---------------------------------------------------------------------------
// core/auto_scan.cpp
// WMI-based hardware information scan.
//
// COM/WMI usage pattern:
//   1. CoInitializeEx          — initialise COM for this thread
//   2. CoInitializeSecurity    — set default security for WMI
//   3. CoCreateInstance        — obtain IWbemLocator
//   4. IWbemLocator::ConnectServer — open ROOT\CIMV2 namespace
//   5. CoSetProxyBlanket       — fix security on the IWbemServices proxy
//   6. Execute WQL queries     — via IWbemServices::ExecQuery
//   7. CoUninitialize          — release COM before returning
// ---------------------------------------------------------------------------

#include <windows.h>

// WMI headers — order matters
#include <comdef.h>
#include <wbemidl.h>

#include <cstdint>
#include <string>
#include <vector>

#include <QString>

#include "core/auto_scan.hpp"

// wbemuuid.lib supplies the WMI CLSID/IID symbols; linked via CMakeLists.txt

namespace nbi {

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------
namespace {

// RAII wrapper that calls CoUninitialize when it goes out of scope.
struct ComGuard {
    bool initialised{false};
    explicit ComGuard(HRESULT hr) : initialised(SUCCEEDED(hr)) {}
    ~ComGuard() { if (initialised) CoUninitialize(); }
    ComGuard(const ComGuard&) = delete;
    ComGuard& operator=(const ComGuard&) = delete;
};

// Simple smart wrapper for IUnknown-derived COM pointers.
template<typename T>
struct ComPtr {
    T* p{nullptr};
    ComPtr() = default;
    explicit ComPtr(T* raw) : p(raw) {}
    ~ComPtr() { if (p) { p->Release(); p = nullptr; } }
    ComPtr(const ComPtr&) = delete;
    ComPtr& operator=(const ComPtr&) = delete;
    T** operator&() { return &p; }
    T*  operator->() const { return p; }
    explicit operator bool() const { return p != nullptr; }
};

// Convert a BSTR or wstring to QString.
inline QString wstr_to_qstring(const std::wstring& ws) {
    return QString::fromStdWString(ws);
}

// ---------------------------------------------------------------------------
// wmi_query
//
// Executes a single WQL SELECT query against the given IWbemServices and
// returns the string value of `prop` from the first result row.
// Returns an empty wstring on any failure.
// ---------------------------------------------------------------------------
std::wstring wmi_query(IWbemServices* svc,
                       const std::wstring& query,
                       const std::wstring& prop)
{
    if (!svc) return {};

    _bstr_t bquery(query.c_str());
    _bstr_t bWQL(L"WQL");

    ComPtr<IEnumWbemClassObject> enumerator;
    HRESULT hr = svc->ExecQuery(
        bWQL,
        bquery,
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
        nullptr,
        &enumerator.p
    );
    if (FAILED(hr) || !enumerator) return {};

    ULONG returned = 0;
    ComPtr<IWbemClassObject> obj;
    hr = enumerator->Next(WBEM_INFINITE, 1, &obj.p, &returned);
    if (FAILED(hr) || returned == 0 || !obj) return {};

    VARIANT var;
    VariantInit(&var);
    hr = obj->Get(prop.c_str(), 0, &var, nullptr, nullptr);
    if (FAILED(hr)) return {};

    std::wstring result;
    if (var.vt == VT_BSTR && var.bstrVal) {
        result = var.bstrVal;
    }
    VariantClear(&var);
    return result;
}

// ---------------------------------------------------------------------------
// wmi_query_all
//
// Like wmi_query but collects the value of `prop` from EVERY result row.
// Used to sum all RAM DIMMs.
// ---------------------------------------------------------------------------
std::vector<std::wstring> wmi_query_all(IWbemServices* svc,
                                        const std::wstring& query,
                                        const std::wstring& prop)
{
    std::vector<std::wstring> results;
    if (!svc) return results;

    _bstr_t bquery(query.c_str());
    _bstr_t bWQL(L"WQL");

    ComPtr<IEnumWbemClassObject> enumerator;
    HRESULT hr = svc->ExecQuery(
        bWQL,
        bquery,
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
        nullptr,
        &enumerator.p
    );
    if (FAILED(hr) || !enumerator) return results;

    while (true) {
        ULONG returned = 0;
        ComPtr<IWbemClassObject> obj;
        hr = enumerator->Next(WBEM_INFINITE, 1, &obj.p, &returned);
        if (FAILED(hr) || returned == 0 || !obj) break;

        VARIANT var;
        VariantInit(&var);
        hr = obj->Get(prop.c_str(), 0, &var, nullptr, nullptr);
        if (SUCCEEDED(hr)) {
            if (var.vt == VT_BSTR && var.bstrVal) {
                results.emplace_back(var.bstrVal);
            }
        }
        VariantClear(&var);
    }
    return results;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// AutoScan::scan
// ---------------------------------------------------------------------------
DeviceInfo AutoScan::scan()
{
    DeviceInfo info;
    info.cpu_name      = QStringLiteral("N/A");
    info.gpu_name      = QStringLiteral("N/A");
    info.os_version    = QStringLiteral("N/A");
    info.serial_number = QStringLiteral("N/A");
    info.ram_gb        = 0;

    // ------------------------------------------------------------------
    // Step 1: Initialise COM
    // ------------------------------------------------------------------
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    // RPC_E_CHANGED_MODE means COM is already initialised with a different
    // threading model — that is acceptable; we will still use it.
    bool com_we_own = SUCCEEDED(hr);
    // Treat RPC_E_CHANGED_MODE as "usable but not ours to uninitialize"
    if (hr == RPC_E_CHANGED_MODE) {
        com_we_own = false;
        hr = S_OK;
    }
    if (FAILED(hr)) {
        return info;  // Cannot proceed without COM
    }

    // ------------------------------------------------------------------
    // Step 2: Set default COM security (must be called once per process,
    //         before any proxy is created).  Ignore failure if already set.
    // ------------------------------------------------------------------
    CoInitializeSecurity(
        nullptr,                        // Security descriptor
        -1,                             // COM authentication services
        nullptr,                        // Authentication services list
        nullptr,                        // Reserved
        RPC_C_AUTHN_LEVEL_DEFAULT,      // Default authentication
        RPC_C_IMP_LEVEL_IMPERSONATE,    // Default impersonation
        nullptr,                        // Authentication info
        EOAC_NONE,                      // Additional capabilities
        nullptr                         // Reserved
    );
    // Return value intentionally ignored — it fails with E_TOO_LATE if
    // another part of the process already called it, which is fine.

    // ------------------------------------------------------------------
    // Step 3: Obtain IWbemLocator
    // ------------------------------------------------------------------
    ComPtr<IWbemLocator> locator;
    hr = CoCreateInstance(
        CLSID_WbemLocator,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_IWbemLocator,
        reinterpret_cast<void**>(&locator.p)
    );
    if (FAILED(hr) || !locator) {
        if (com_we_own) CoUninitialize();
        return info;
    }

    // ------------------------------------------------------------------
    // Step 4: Connect to ROOT\CIMV2
    // ------------------------------------------------------------------
    ComPtr<IWbemServices> svc;
    hr = locator->ConnectServer(
        _bstr_t(L"ROOT\\CIMV2"),  // WMI namespace
        nullptr,                   // Username (null = current user)
        nullptr,                   // Password
        nullptr,                   // Locale
        0,                         // Security flags
        nullptr,                   // Authority
        nullptr,                   // Context object
        &svc.p
    );
    if (FAILED(hr) || !svc) {
        if (com_we_own) CoUninitialize();
        return info;
    }

    // ------------------------------------------------------------------
    // Step 5: Set security on the WbemServices proxy
    // ------------------------------------------------------------------
    hr = CoSetProxyBlanket(
        svc.p,
        RPC_C_AUTHN_WINNT,          // Authentication service
        RPC_C_AUTHZ_NONE,           // Authorization service
        nullptr,                    // Server principal name
        RPC_C_AUTHN_LEVEL_CALL,     // Authentication level
        RPC_C_IMP_LEVEL_IMPERSONATE,// Impersonation level
        nullptr,                    // Client identity
        EOAC_NONE                   // Proxy capabilities
    );
    if (FAILED(hr)) {
        if (com_we_own) CoUninitialize();
        return info;
    }

    // ------------------------------------------------------------------
    // Step 6: Execute WMI queries — failures are silent; field stays N/A
    // ------------------------------------------------------------------

    // --- CPU name ---
    {
        std::wstring val = wmi_query(svc.p,
            L"SELECT Name FROM Win32_Processor",
            L"Name");
        if (!val.empty()) {
            info.cpu_name = wstr_to_qstring(val).trimmed();
        }
    }

    // --- RAM (sum all DIMMs) ---
    {
        auto rows = wmi_query_all(svc.p,
            L"SELECT Capacity FROM Win32_PhysicalMemory",
            L"Capacity");
        uint64_t total_bytes = 0;
        for (const auto& row : rows) {
            try {
                total_bytes += std::stoull(row);
            } catch (...) {
                // Non-numeric value from WMI — skip this DIMM
            }
        }
        if (total_bytes > 0) {
            // Convert bytes -> GB (1 GB = 1073741824 bytes)
            info.ram_gb = static_cast<uint32_t>(total_bytes / 1073741824ULL);
        }
    }

    // --- GPU name (first adapter) ---
    {
        std::wstring val = wmi_query(svc.p,
            L"SELECT Name FROM Win32_VideoController",
            L"Name");
        if (!val.empty()) {
            info.gpu_name = wstr_to_qstring(val).trimmed();
        }
    }

    // --- BIOS serial number ---
    {
        std::wstring val = wmi_query(svc.p,
            L"SELECT SerialNumber FROM Win32_BIOS",
            L"SerialNumber");
        if (!val.empty()) {
            info.serial_number = wstr_to_qstring(val).trimmed();
        }
    }

    // --- OS caption ---
    {
        std::wstring val = wmi_query(svc.p,
            L"SELECT Caption FROM Win32_OperatingSystem",
            L"Caption");
        if (!val.empty()) {
            info.os_version = wstr_to_qstring(val).trimmed();
        }
    }

    // ------------------------------------------------------------------
    // Step 7: Clean up COM
    // IWbemServices and IWbemLocator must be fully released BEFORE
    // CoUninitialize is called.  We force this by explicitly nulling the
    // raw pointers inside the ComPtr wrappers (their destructors check for
    // null, so no double-release occurs) and then calling Release manually.
    // ------------------------------------------------------------------
    if (svc.p)     { svc.p->Release();     svc.p     = nullptr; }
    if (locator.p) { locator.p->Release(); locator.p = nullptr; }

    if (com_we_own) CoUninitialize();

    return info;
}

} // namespace nbi
