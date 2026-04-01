// ---------------------------------------------------------------------------
// core/thermal_manager.cpp
// CPU temperature via WMI MSAcpi_ThermalZoneTemperature (ROOT\WMI).
// If no zone >= 30°C found → sensor_missing = true → Skipped (no score penalty).
// ---------------------------------------------------------------------------

#ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <comdef.h>
#include <wbemidl.h>

#include "core/thermal_manager.hpp"

namespace {

uint32_t wmiUint(IWbemClassObject* obj, const wchar_t* prop)
{
    VARIANT v{};
    if (FAILED(obj->Get(prop, 0, &v, nullptr, nullptr))) return 0;
    uint32_t val = 0;
    if (v.vt == VT_I4)  val = static_cast<uint32_t>(v.lVal);
    if (v.vt == VT_UI4) val = v.uintVal;
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

int kelvinToCelsius(uint32_t tenths_kelvin)
{
    if (tenths_kelvin < 2732) return -1;
    return static_cast<int>((tenths_kelvin - 2732) / 10);
}

} // namespace

namespace nbi {

ThermalResult ThermalManager::scan()
{
    ThermalResult result;

    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const bool com_inited = SUCCEEDED(hr);
    CoInitializeSecurity(nullptr, -1, nullptr, nullptr,
        RPC_C_AUTHN_LEVEL_DEFAULT, RPC_C_IMP_LEVEL_IMPERSONATE,
        nullptr, EOAC_NONE, nullptr);

    IWbemLocator* pLoc = nullptr;
    hr = CoCreateInstance(CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER,
                          IID_IWbemLocator, reinterpret_cast<void**>(&pLoc));
    if (FAILED(hr)) {
        if (com_inited) CoUninitialize();
        result.sensor_missing = true;
        return result;
    }

    IWbemServices* pSvc = nullptr;
    _bstr_t ns(L"ROOT\\WMI");
    hr = pLoc->ConnectServer(ns, nullptr, nullptr, nullptr, 0, nullptr, nullptr, &pSvc);
    if (SUCCEEDED(hr) && pSvc) {
        CoSetProxyBlanket(pSvc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr,
            RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE);

        IEnumWbemClassObject* pEnum = nullptr;
        _bstr_t q(L"SELECT InstanceName, CurrentTemperature FROM MSAcpi_ThermalZoneTemperature");
        _bstr_t wql(L"WQL");
        if (SUCCEEDED(pSvc->ExecQuery(wql, q,
                WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
                nullptr, &pEnum)) && pEnum)
        {
            IWbemClassObject* pObj = nullptr;
            ULONG ret = 0;
            while (pEnum->Next(WBEM_INFINITE, 1, &pObj, &ret) == S_OK) {
                const uint32_t raw  = wmiUint(pObj, L"CurrentTemperature");
                const int      temp = kelvinToCelsius(raw);
                QString        name = wmiStr(pObj, L"InstanceName");

                const int slash = name.lastIndexOf(QLatin1Char('\\'));
                if (slash >= 0) name = name.mid(slash + 1);
                if (name.contains(QLatin1Char('_')))
                    name = name.section(QLatin1Char('_'), 0, -2);

                // Only include zones that look like actual CPU/thermal sensors
                // Ambient/chassis zones typically report < 30°C
                if (temp >= 30) {
                    ThermalZone z;
                    z.name   = name.isEmpty() ? QStringLiteral("Zone") : name;
                    z.temp_c = temp;
                    result.zones.push_back(z);
                }
                pObj->Release();
            }
            pEnum->Release();
        }
        pSvc->Release();
    }

    pLoc->Release();
    if (com_inited) CoUninitialize();

    if (result.zones.empty()) {
        result.sensor_missing = true;
        return result;
    }

    int max_temp = -1;
    for (const auto& z : result.zones)
        if (z.temp_c > max_temp) max_temp = z.temp_c;

    result.cpu_temp_c = max_temp;
    result.flagged    = (max_temp >= 95);

    return result;
}

ModuleResult ThermalManager::evaluate(const ThermalResult& r)
{
    ModuleResult m;
    m.label = QStringLiteral("Temperature");

    if (r.sensor_missing) {
        m.status  = TestStatus::Skipped;
        m.summary = QStringLiteral("ไม่รองรับ sensor");
        return m;
    }

    if (r.flagged) {
        m.status  = TestStatus::Fail;
        m.summary = QStringLiteral("CPU ร้อนเกิน 95°C (%1°C)").arg(r.cpu_temp_c);
        m.issues.append(m.summary);
    } else {
        m.status  = TestStatus::Pass;
        m.summary = QStringLiteral("CPU %1°C").arg(r.cpu_temp_c);
    }

    return m;
}

} // namespace nbi
