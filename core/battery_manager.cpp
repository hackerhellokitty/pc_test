// ---------------------------------------------------------------------------
// core/battery_manager.cpp
// Query battery health via WMI root\wmi namespace:
//   BatteryStaticData   -> DesignedCapacity, CycleCount, DeviceName
//   BatteryFullChargeCapacity -> FullChargedCapacity
// ---------------------------------------------------------------------------

#ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

// COM / WMI
#include <comdef.h>
#include <wbemidl.h>

#include "core/battery_manager.hpp"

#include <QStringList>

namespace {

// Helper: read a UINT32 property from a WMI object, returns 0 on failure
uint32_t wmiUint(IWbemClassObject* obj, const wchar_t* prop)
{
    VARIANT v{};
    if (FAILED(obj->Get(prop, 0, &v, nullptr, nullptr))) return 0;
    const uint32_t val = (v.vt == VT_I4 || v.vt == VT_UI4)
        ? static_cast<uint32_t>(v.uintVal) : 0;
    VariantClear(&v);
    return val;
}

QString wmiStr(IWbemClassObject* obj, const wchar_t* prop)
{
    VARIANT v{};
    if (FAILED(obj->Get(prop, 0, &v, nullptr, nullptr))) return {};
    QString s;
    if (v.vt == VT_BSTR && v.bstrVal)
        s = QString::fromWCharArray(v.bstrVal);
    VariantClear(&v);
    return s;
}

} // namespace

namespace nbi {

BatteryResult BatteryManager::scan()
{
    BatteryResult result;

    // --- COM init ---
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const bool com_inited = SUCCEEDED(hr);

    hr = CoInitializeSecurity(nullptr, -1, nullptr, nullptr,
        RPC_C_AUTHN_LEVEL_DEFAULT, RPC_C_IMP_LEVEL_IMPERSONATE,
        nullptr, EOAC_NONE, nullptr);
    // Ignore RPC_E_TOO_LATE — already initialized elsewhere

    IWbemLocator* pLoc = nullptr;
    hr = CoCreateInstance(CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER,
                          IID_IWbemLocator, reinterpret_cast<void**>(&pLoc));
    if (FAILED(hr)) {
        if (com_inited) CoUninitialize();
        result.no_battery = true;
        return result;
    }

    IWbemServices* pSvc = nullptr;
    _bstr_t ns(L"ROOT\\WMI");
    hr = pLoc->ConnectServer(ns, nullptr, nullptr, nullptr, 0, nullptr, nullptr, &pSvc);
    if (FAILED(hr)) {
        pLoc->Release();
        if (com_inited) CoUninitialize();
        result.no_battery = true;
        return result;
    }

    CoSetProxyBlanket(pSvc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr,
        RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE);

    // -----------------------------------------------------------------------
    // 1. BatteryStaticData — DesignedCapacity, CycleCount, DeviceName
    // -----------------------------------------------------------------------
    struct StaticEntry {
        QString  instance_name;
        QString  device_name;
        uint32_t designed_capacity{0};
        uint32_t cycle_count{0};
    };
    std::vector<StaticEntry> statics;

    {
        IEnumWbemClassObject* pEnum = nullptr;
        _bstr_t query(L"SELECT * FROM BatteryStaticData");
        _bstr_t wql(L"WQL");
        if (SUCCEEDED(pSvc->ExecQuery(wql, query,
                WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
                nullptr, &pEnum)) && pEnum)
        {
            IWbemClassObject* pObj = nullptr;
            ULONG ret = 0;
            while (pEnum->Next(WBEM_INFINITE, 1, &pObj, &ret) == S_OK) {
                StaticEntry e;
                e.instance_name    = wmiStr(pObj, L"InstanceName");
                e.device_name      = wmiStr(pObj, L"DeviceName").trimmed();
                e.designed_capacity = wmiUint(pObj, L"DesignedCapacity");
                e.cycle_count      = wmiUint(pObj, L"CycleCount");
                if (e.designed_capacity > 0)
                    statics.push_back(std::move(e));
                pObj->Release();
            }
            pEnum->Release();
        }
    }

    // -----------------------------------------------------------------------
    // 2. BatteryFullChargeCapacity — FullChargedCapacity
    // -----------------------------------------------------------------------
    struct FullEntry {
        QString  instance_name;
        uint32_t full_charge{0};
    };
    std::vector<FullEntry> fulls;

    {
        IEnumWbemClassObject* pEnum = nullptr;
        _bstr_t query(L"SELECT * FROM BatteryFullChargeCapacity");
        _bstr_t wql(L"WQL");
        if (SUCCEEDED(pSvc->ExecQuery(wql, query,
                WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
                nullptr, &pEnum)) && pEnum)
        {
            IWbemClassObject* pObj = nullptr;
            ULONG ret = 0;
            while (pEnum->Next(WBEM_INFINITE, 1, &pObj, &ret) == S_OK) {
                FullEntry e;
                e.instance_name = wmiStr(pObj, L"InstanceName");
                e.full_charge   = wmiUint(pObj, L"FullChargedCapacity");
                if (e.full_charge > 0)
                    fulls.push_back(std::move(e));
                pObj->Release();
            }
            pEnum->Release();
        }
    }

    pSvc->Release();
    pLoc->Release();
    if (com_inited) CoUninitialize();

    // -----------------------------------------------------------------------
    // 3. Match static + full entries by InstanceName prefix
    // -----------------------------------------------------------------------
    for (const auto& s : statics) {
        BatteryInfo bi;
        bi.name             = s.device_name.isEmpty()
                              ? QStringLiteral("Battery") : s.device_name;
        bi.design_capacity  = s.designed_capacity;
        bi.cycle_count      = s.cycle_count;

        // Find matching full-charge entry (same InstanceName base)
        for (const auto& f : fulls) {
            // InstanceName suffix "_0" may differ — compare up to first '_'
            const QString s_base = s.instance_name.section(QLatin1Char('_'), 0, -2);
            const QString f_base = f.instance_name.section(QLatin1Char('_'), 0, -2);
            if (s_base == f_base || s.instance_name == f.instance_name) {
                bi.full_charge_capacity = f.full_charge;
                break;
            }
        }

        // Wear level
        if (bi.design_capacity > 0 && bi.full_charge_capacity > 0) {
            bi.wear_level = static_cast<int>(
                bi.full_charge_capacity * 100 / bi.design_capacity);
            if (bi.wear_level > 100) bi.wear_level = 100;
            bi.flagged = (bi.wear_level < 60);
        } else if (bi.full_charge_capacity == 0) {
            bi.error_detail = QStringLiteral("ไม่พบ FullChargeCapacity");
        }

        result.batteries.push_back(std::move(bi));
    }

    if (result.batteries.empty())
        result.no_battery = true;

    return result;
}

ModuleResult BatteryManager::evaluate(const BatteryResult& r)
{
    ModuleResult m;
    m.label = QStringLiteral("Battery");

    if (r.no_battery) {
        m.status  = TestStatus::Skipped;
        m.summary = QStringLiteral("ไม่พบแบตเตอรี่");
        return m;
    }

    int flagged = 0;
    for (const auto& b : r.batteries) {
        if (b.flagged) {
            ++flagged;
            m.issues.append(
                QStringLiteral("%1: Wear %2% (< 60%)")
                    .arg(b.name).arg(b.wear_level));
        }
    }

    if (flagged > 0) {
        m.status  = TestStatus::Fail;
        m.summary = QStringLiteral("แบตเสื่อม %1 ก้อน").arg(flagged);
    } else {
        m.status  = TestStatus::Pass;
        m.summary = QStringLiteral("แบตปกติ");
    }

    return m;
}

} // namespace nbi
