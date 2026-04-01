// ---------------------------------------------------------------------------
// core/storage_manager.cpp
// ---------------------------------------------------------------------------

#include "core/storage_manager.hpp"

#include <windows.h>
#include <winioctl.h>
#include <ntddscsi.h>   // ATA_PASS_THROUGH_EX, SENDCMDINPARAMS
#include <ntddstor.h>

// WMI
#include <comdef.h>
#include <wbemidl.h>
#pragma comment(lib, "wbemuuid.lib")

#include <string>
#include <vector>

#include <QString>
#include <QStringList>

// ---------------------------------------------------------------------------
// SMART IOCTL definitions not always present in older SDKs
// ---------------------------------------------------------------------------
#ifndef SMART_GET_VERSION
#  define SMART_GET_VERSION   CTL_CODE(IOCTL_DISK_BASE, 0x0020, METHOD_BUFFERED, FILE_READ_ACCESS)
#endif
#ifndef SMART_SEND_DRIVE_CMD
#  define SMART_SEND_DRIVE_CMD CTL_CODE(IOCTL_DISK_BASE, 0x0021, METHOD_BUFFERED, FILE_READ_WRITE_ACCESS)
#endif
#ifndef SMART_RCV_DRIVE_DATA
#  define SMART_RCV_DRIVE_DATA CTL_CODE(IOCTL_DISK_BASE, 0x0022, METHOD_BUFFERED, FILE_READ_WRITE_ACCESS)
#endif

#define ATAID_SMART         0xB0
#define SMART_CYL_LOW       0x4F
#define SMART_CYL_HI        0xC2
#define SMART_READ_DATA     0xD0
#define SMART_READ_ATTRIBS  0xD0

// SMART attribute IDs we care about
static constexpr uint8_t kAttrReallocated    = 0x05;
static constexpr uint8_t kAttrPending        = 0xC5;
static constexpr uint8_t kAttrUncorrectable  = 0xC6;

// ---------------------------------------------------------------------------
// Internal SMART structures
// ---------------------------------------------------------------------------
#pragma pack(push, 1)
struct SmartAttributeEntry {
    uint8_t  id;
    uint16_t flags;
    uint8_t  current;
    uint8_t  worst;
    uint8_t  raw[6];
    uint8_t  reserved;
};

struct SmartDataBlock {
    uint16_t             revision;
    SmartAttributeEntry  attrs[30];
    uint8_t              offline_status;
    uint8_t              self_test_status;
    uint16_t             offline_secs;
    uint8_t              vendor1;
    uint8_t              offline_caps;
    uint16_t             smart_caps;
    uint8_t              error_log_caps;
    uint8_t              vendor2;
    uint8_t              short_test_interval;
    uint8_t              ext_test_interval;
    uint8_t              reserved[12];
    uint8_t              vendor3[125];
    uint8_t              checksum;
};
#pragma pack(pop)

namespace nbi {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static QString attrName(uint8_t id)
{
    switch (id) {
        case 0x01: return QStringLiteral("Read Error Rate");
        case 0x03: return QStringLiteral("Spin-Up Time");
        case 0x04: return QStringLiteral("Start/Stop Count");
        case 0x05: return QStringLiteral("Reallocated Sectors");
        case 0x07: return QStringLiteral("Seek Error Rate");
        case 0x09: return QStringLiteral("Power-On Hours");
        case 0x0A: return QStringLiteral("Spin Retry Count");
        case 0x0C: return QStringLiteral("Power Cycle Count");
        case 0xAA: return QStringLiteral("Available Reserved Space");
        case 0xAB: return QStringLiteral("Program Fail Count");
        case 0xAC: return QStringLiteral("Erase Fail Count");
        case 0xB1: return QStringLiteral("Wear Range Delta");
        case 0xB4: return QStringLiteral("Unused Reserved Blocks");
        case 0xBB: return QStringLiteral("Uncorrectable Errors");
        case 0xBC: return QStringLiteral("Command Timeout");
        case 0xBD: return QStringLiteral("High Fly Writes");
        case 0xBE: return QStringLiteral("Airflow Temperature");
        case 0xBF: return QStringLiteral("G-Sense Error Rate");
        case 0xC0: return QStringLiteral("Power-Off Retract");
        case 0xC1: return QStringLiteral("Load/Unload Cycle Count");
        case 0xC2: return QStringLiteral("HDA Temperature");
        case 0xC4: return QStringLiteral("Reallocation Event Count");
        case 0xC5: return QStringLiteral("Current Pending Sectors");
        case 0xC6: return QStringLiteral("Uncorrectable Sector Count");
        case 0xC7: return QStringLiteral("UltraDMA CRC Error Count");
        case 0xC8: return QStringLiteral("Write Error Rate");
        case 0xCA: return QStringLiteral("Data Address Mark Errors");
        case 0xE8: return QStringLiteral("Available Reserved Space");
        case 0xE9: return QStringLiteral("Media Wearout Indicator");
        case 0xF1: return QStringLiteral("Total LBAs Written");
        case 0xF2: return QStringLiteral("Total LBAs Read");
        default:   return QStringLiteral("Attr 0x%1").arg(id, 2, 16, QChar('0')).toUpper();
    }
}

static uint32_t rawToU32(const uint8_t raw[6])
{
    // SMART raw is little-endian 48-bit; we take the lower 32 bits
    return static_cast<uint32_t>(raw[0])
         | (static_cast<uint32_t>(raw[1]) << 8)
         | (static_cast<uint32_t>(raw[2]) << 16)
         | (static_cast<uint32_t>(raw[3]) << 24);
}

// ---------------------------------------------------------------------------
// readModelSerial — size via IOCTL only; model/serial filled later by queryWmiDiskDrives
// ---------------------------------------------------------------------------
bool StorageManager::readModelSerial(void* h, DriveInfo& out)
{
    DISK_GEOMETRY_EX geom{};
    DWORD geom_ret = 0;
    if (DeviceIoControl(h, IOCTL_DISK_GET_DRIVE_GEOMETRY_EX,
                        nullptr, 0, &geom, sizeof(geom), &geom_ret, nullptr))
        out.size_bytes = static_cast<uint64_t>(geom.DiskSize.QuadPart);
    return true;
}

// ---------------------------------------------------------------------------
// detectBusType
// ---------------------------------------------------------------------------
BusType StorageManager::detectBusType(void* h)
{
    STORAGE_PROPERTY_QUERY query{};
    query.PropertyId = StorageAdapterProperty;
    query.QueryType  = PropertyStandardQuery;

    uint8_t buf[256]{};
    DWORD returned = 0;
    if (!DeviceIoControl(h, IOCTL_STORAGE_QUERY_PROPERTY,
                         &query, sizeof(query),
                         buf, sizeof(buf), &returned, nullptr))
        return BusType::Unknown;

    auto* desc = reinterpret_cast<STORAGE_ADAPTER_DESCRIPTOR*>(buf);
    switch (desc->BusType) {
        case BusTypeAta:
        case BusTypeSata:    return BusType::Sata;
        case BusTypeNvme:    return BusType::NVMe;
        default:             return BusType::Other;
    }
}

// ---------------------------------------------------------------------------
// readSmartSata — ATA SMART READ DATA via SMART_RCV_DRIVE_DATA
// ---------------------------------------------------------------------------
bool StorageManager::readSmartSata(void* h, DriveInfo& out)
{
    // SMART_RCV_DRIVE_DATA: output is SENDCMDOUTPARAMS where bBuffer[0] is the
    // start of the 512-byte SMART data sector. Allocate header + 512 bytes flat.
    const DWORD kOutSize = sizeof(SENDCMDOUTPARAMS) - 1 + 512;

    SENDCMDINPARAMS cmd{};
    cmd.cBufferSize               = 512;
    cmd.irDriveRegs.bFeaturesReg  = SMART_READ_DATA;
    cmd.irDriveRegs.bSectorCountReg  = 1;
    cmd.irDriveRegs.bSectorNumberReg = 1;
    cmd.irDriveRegs.bCylLowReg    = SMART_CYL_LOW;
    cmd.irDriveRegs.bCylHighReg   = SMART_CYL_HI;
    cmd.irDriveRegs.bDriveHeadReg = 0xA0;
    cmd.irDriveRegs.bCommandReg   = ATAID_SMART;
    cmd.bDriveNumber              = 0;

    std::vector<uint8_t> out_buf(kOutSize, 0);
    DWORD returned = 0;
    if (!DeviceIoControl(h, SMART_RCV_DRIVE_DATA,
                         &cmd, sizeof(cmd),
                         out_buf.data(), kOutSize, &returned, nullptr))
        return false;

    // bBuffer[0] of SENDCMDOUTPARAMS is the start of the 512-byte data sector
    const uint8_t* sector = out_buf.data() + offsetof(SENDCMDOUTPARAMS, bBuffer);
    const auto& blk = *reinterpret_cast<const SmartDataBlock*>(sector);

    for (const SmartAttributeEntry& e : blk.attrs) {
        if (e.id == 0) continue;

        SmartAttr attr;
        attr.id        = e.id;
        attr.name      = attrName(e.id);
        attr.raw_value = rawToU32(e.raw);
        out.attrs.push_back(attr);

        switch (e.id) {
            case kAttrReallocated:   out.reallocated   = attr.raw_value; break;
            case kAttrPending:       out.pending        = attr.raw_value; break;
            case kAttrUncorrectable: out.uncorrectable  = attr.raw_value; break;
            default: break;
        }
    }

    out.smart_available = true;
    return true;
}

// ---------------------------------------------------------------------------
// readSmartNvme — NVMe SMART via IOCTL_STORAGE_QUERY_PROPERTY (StorageDeviceProtocolSpecificProperty)
// ---------------------------------------------------------------------------
bool StorageManager::readSmartNvme(void* h, DriveInfo& out)
{
    // NVMe SMART/Health Information Log — Log Page 0x02
    // Single shared buffer for in+out (same pattern as WD/Samsung tools).
    // STORAGE_PROTOCOL_SPECIFIC_DATA overlaps AdditionalParameters[0]
    // by positioning it at (buf + sizeof(STORAGE_PROPERTY_QUERY) - sizeof(DWORD)).
    const DWORD kBufSize = sizeof(STORAGE_PROPERTY_QUERY)
                         + sizeof(STORAGE_PROTOCOL_SPECIFIC_DATA)
                         + 512;  // 512-byte NVMe log page

    std::vector<uint8_t> buf(kBufSize, 0);

    auto* query       = reinterpret_cast<STORAGE_PROPERTY_QUERY*>(buf.data());
    query->PropertyId = StorageDeviceProtocolSpecificProperty;
    query->QueryType  = PropertyStandardQuery;

    // Overlap: psd starts at AdditionalParameters (last DWORD of STORAGE_PROPERTY_QUERY)
    auto* psd = reinterpret_cast<STORAGE_PROTOCOL_SPECIFIC_DATA*>(
        buf.data() + sizeof(STORAGE_PROPERTY_QUERY) - sizeof(DWORD));
    psd->ProtocolType                = ProtocolTypeNvme;
    psd->DataType                    = NVMeDataTypeLogPage;
    psd->ProtocolDataRequestValue    = 0x02;  // SMART/Health Information Log
    psd->ProtocolDataRequestSubValue = 0;
    psd->ProtocolDataOffset          = sizeof(STORAGE_PROTOCOL_SPECIFIC_DATA);
    psd->ProtocolDataLength          = 512;

    DWORD returned = 0;
    if (!DeviceIoControl(h, IOCTL_STORAGE_QUERY_PROPERTY,
                         buf.data(), kBufSize,
                         buf.data(), kBufSize, &returned, nullptr))
        return false;

    if (returned < sizeof(STORAGE_PROPERTY_QUERY) + sizeof(DWORD))
        return false;

    // Log data starts at psd + psd->ProtocolDataOffset
    const uint8_t* d = reinterpret_cast<const uint8_t*>(psd) + psd->ProtocolDataOffset;

    // NVMe spec byte layout:
    // byte 0     : critical warning
    // bytes 1-2  : composite temperature (Kelvin LE)
    // byte 3     : available spare %
    // byte 4     : available spare threshold %
    // byte 5     : percent used (0-255, typically 0-100)
    out.percent_used = d[5];

    // bytes 160-175 : media and data integrity errors (128-bit LE)
    out.media_errors = static_cast<uint32_t>(d[160])
                     | (static_cast<uint32_t>(d[161]) << 8)
                     | (static_cast<uint32_t>(d[162]) << 16)
                     | (static_cast<uint32_t>(d[163]) << 24);
    if (out.media_errors) out.uncorrectable = out.media_errors;

    out.smart_available = true;
    return true;
}

// ---------------------------------------------------------------------------
// readSmartWmiFallback — WMI MSFT_Disk HealthStatus
// ---------------------------------------------------------------------------
bool StorageManager::readSmartWmiFallback(int /*index*/, DriveInfo& out)
{
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const bool com_owned = SUCCEEDED(hr) && hr != S_FALSE;
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) return false;
    CoInitializeSecurity(nullptr, -1, nullptr, nullptr,
                         RPC_C_AUTHN_LEVEL_DEFAULT, RPC_C_IMP_LEVEL_IMPERSONATE,
                         nullptr, EOAC_NONE, nullptr);

    IWbemLocator*  locator  = nullptr;
    IWbemServices* services = nullptr;

    auto cleanup = [&]() {
        if (services) { services->Release(); services = nullptr; }
        if (locator)  { locator->Release();  locator  = nullptr; }
        if (com_owned) CoUninitialize();
    };

    hr = CoCreateInstance(
        CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER,
        IID_IWbemLocator, reinterpret_cast<void**>(&locator)
    );
    if (FAILED(hr)) { if (com_owned) CoUninitialize(); return false; }

    // Use ROOT\Microsoft\Windows\Storage namespace for MSFT_Disk
    hr = locator->ConnectServer(
        _bstr_t(L"ROOT\\Microsoft\\Windows\\Storage"),
        nullptr, nullptr, nullptr, 0, nullptr, nullptr, &services
    );
    if (FAILED(hr)) { cleanup(); return false; }

    CoSetProxyBlanket(services, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE,
                      nullptr, RPC_C_AUTHN_LEVEL_CALL,
                      RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE);

    IEnumWbemClassObject* enumerator = nullptr;
    hr = services->ExecQuery(
        _bstr_t(L"WQL"),
        _bstr_t(L"SELECT Model, SerialNumber, HealthStatus, OperationalStatus FROM MSFT_Disk"),
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
        nullptr, &enumerator
    );
    if (FAILED(hr)) { cleanup(); return false; }

    IWbemClassObject* obj = nullptr;
    ULONG ret = 0;
    bool found = false;

    while (enumerator->Next(WBEM_INFINITE, 1, &obj, &ret) == WBEM_S_NO_ERROR) {
        VARIANT v;
        VariantInit(&v);

        if (out.model.isEmpty()) {
            if (SUCCEEDED(obj->Get(L"Model", 0, &v, nullptr, nullptr)) && v.vt == VT_BSTR)
                out.model = QString::fromWCharArray(v.bstrVal).trimmed();
            VariantClear(&v);
        }

        if (SUCCEEDED(obj->Get(L"HealthStatus", 0, &v, nullptr, nullptr)) && v.vt == VT_I4) {
            // 0=Healthy, 1=Warning, 2=Unhealthy, 5=Unknown
            switch (v.lVal) {
                case 0: out.health_status = QStringLiteral("Healthy");   break;
                case 1: out.health_status = QStringLiteral("Warning");
                        out.flagged = true;                               break;
                case 2: out.health_status = QStringLiteral("Unhealthy");
                        out.flagged = true;                               break;
                default: out.health_status = QStringLiteral("Unknown");  break;
            }
        }
        VariantClear(&v);

        obj->Release();
        found = true;
        break;  // use first disk (matching by index not reliable here)
    }

    if (enumerator) enumerator->Release();
    cleanup();
    out.smart_available = found;
    return found;
}

// ---------------------------------------------------------------------------
// probeDrive — open one PhysicalDriveN and gather all info
// ---------------------------------------------------------------------------
DriveInfo StorageManager::probeDrive(int index)
{
    DriveInfo info;
    info.index = index;

    const QString path = QStringLiteral("\\\\.\\PhysicalDrive%1").arg(index);
    HANDLE h = CreateFileW(
        reinterpret_cast<LPCWSTR>(path.utf16()),
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr, OPEN_EXISTING, 0, nullptr
    );

    if (h == INVALID_HANDLE_VALUE) {
        // Try read-only (some NVMe drives refuse write access)
        h = CreateFileW(
            reinterpret_cast<LPCWSTR>(path.utf16()),
            GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            nullptr, OPEN_EXISTING, 0, nullptr
        );
    }
    if (h == INVALID_HANDLE_VALUE) return info;

    readModelSerial(h, info);
    info.bus = detectBusType(h);

    bool smart_ok = false;
    if (info.bus == BusType::Sata) {
        smart_ok = readSmartSata(h, info);
    } else if (info.bus == BusType::NVMe) {
        smart_ok = readSmartNvme(h, info);
    }

    CloseHandle(h);

    if (!smart_ok) {
        // WMI fallback for any bus type
        readSmartWmiFallback(index, info);
    }

    // Determine flagged status
    if (info.reallocated != 0 || info.pending != 0 || info.uncorrectable != 0
        || info.media_errors != 0)
    {
        info.flagged = true;
        QStringList reasons;
        if (info.reallocated   != 0) reasons << QStringLiteral("Reallocated=%1").arg(info.reallocated);
        if (info.pending       != 0) reasons << QStringLiteral("Pending=%1").arg(info.pending);
        if (info.uncorrectable != 0) reasons << QStringLiteral("Uncorrectable=%1").arg(info.uncorrectable);
        if (info.media_errors  != 0) reasons << QStringLiteral("MediaErrors=%1").arg(info.media_errors);
        info.error_detail = reasons.join(QStringLiteral(", "));
    }

    return info;
}

// ---------------------------------------------------------------------------
// queryWmiDiskDrives — single WMI session, fill model/serial/size for all drives
// ---------------------------------------------------------------------------
static void queryWmiDiskDrives(std::vector<DriveInfo>& drives)
{
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const bool com_owned = (hr == S_OK);  // S_FALSE = already init, RPC_E_CHANGED_MODE = diff model
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE && hr != S_FALSE) return;

    CoInitializeSecurity(nullptr, -1, nullptr, nullptr,
                         RPC_C_AUTHN_LEVEL_DEFAULT, RPC_C_IMP_LEVEL_IMPERSONATE,
                         nullptr, EOAC_NONE, nullptr);

    IWbemLocator*  locator  = nullptr;
    IWbemServices* services = nullptr;

    hr = CoCreateInstance(CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER,
                          IID_IWbemLocator, reinterpret_cast<void**>(&locator));
    if (FAILED(hr)) { if (com_owned) CoUninitialize(); return; }

    hr = locator->ConnectServer(_bstr_t(L"ROOT\\CIMV2"), nullptr, nullptr,
                                nullptr, 0, nullptr, nullptr, &services);
    if (FAILED(hr)) { locator->Release(); if (com_owned) CoUninitialize(); return; }

    CoSetProxyBlanket(services, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr,
                      RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE,
                      nullptr, EOAC_NONE);

    // --- Query 1: Win32_DiskDrive (model / serial / size) -------------------
    IEnumWbemClassObject* enumerator = nullptr;
    hr = services->ExecQuery(
        _bstr_t(L"WQL"),
        _bstr_t(L"SELECT Index, Model, SerialNumber, Size, Status FROM Win32_DiskDrive"),
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
        nullptr, &enumerator
    );

    if (SUCCEEDED(hr) && enumerator) {
        IWbemClassObject* obj = nullptr;
        ULONG ret = 0;
        while (enumerator->Next(WBEM_INFINITE, 1, &obj, &ret) == WBEM_S_NO_ERROR) {
            VARIANT v;
            VariantInit(&v);

            int idx = -1;
            if (SUCCEEDED(obj->Get(L"Index", 0, &v, nullptr, nullptr)) && v.vt == VT_I4)
                idx = v.lVal;
            VariantClear(&v);

            if (idx < 0) { obj->Release(); continue; }

            DriveInfo* di = nullptr;
            for (auto& d : drives)
                if (d.index == idx) { di = &d; break; }
            if (!di) { obj->Release(); continue; }

            if (SUCCEEDED(obj->Get(L"Model", 0, &v, nullptr, nullptr)) && v.vt == VT_BSTR)
                di->model = QString::fromWCharArray(v.bstrVal).trimmed();
            VariantClear(&v);

            if (SUCCEEDED(obj->Get(L"SerialNumber", 0, &v, nullptr, nullptr)) && v.vt == VT_BSTR)
                di->serial = QString::fromWCharArray(v.bstrVal).trimmed();
            VariantClear(&v);

            if (di->size_bytes == 0
                && SUCCEEDED(obj->Get(L"Size", 0, &v, nullptr, nullptr)) && v.vt == VT_BSTR)
                di->size_bytes = QString::fromWCharArray(v.bstrVal).toULongLong();
            VariantClear(&v);

            obj->Release();
        }
        enumerator->Release();
    }

    // --- Query 2: MSStorageDriver_FailurePredictStatus (root\wmi) -----------
    // Requires Admin — available in our process via requireAdministrator manifest
    IWbemServices* wmi_svc = nullptr;
    hr = locator->ConnectServer(_bstr_t(L"ROOT\\WMI"), nullptr, nullptr,
                                nullptr, 0, nullptr, nullptr, &wmi_svc);
    if (SUCCEEDED(hr) && wmi_svc) {
        CoSetProxyBlanket(wmi_svc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr,
                          RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE,
                          nullptr, EOAC_NONE);

        IEnumWbemClassObject* enum2 = nullptr;
        hr = wmi_svc->ExecQuery(
            _bstr_t(L"WQL"),
            _bstr_t(L"SELECT InstanceName, PredictFailure, Reason "
                    L"FROM MSStorageDriver_FailurePredictStatus"),
            WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
            nullptr, &enum2
        );

        if (SUCCEEDED(hr) && enum2) {
            IWbemClassObject* obj2 = nullptr;
            ULONG ret2 = 0;
            while (enum2->Next(WBEM_INFINITE, 1, &obj2, &ret2) == WBEM_S_NO_ERROR) {
                VARIANT v;
                VariantInit(&v);

                // InstanceName looks like "SCSI\Disk&Ven_...&Prod_...\5&...&0&000000_0"
                // Last digit before final "_N" indicates drive number — simpler: match by order
                QString instance_name;
                if (SUCCEEDED(obj2->Get(L"InstanceName", 0, &v, nullptr, nullptr)) && v.vt == VT_BSTR)
                    instance_name = QString::fromWCharArray(v.bstrVal).trimmed();
                VariantClear(&v);

                bool predict_fail = false;
                if (SUCCEEDED(obj2->Get(L"PredictFailure", 0, &v, nullptr, nullptr)) && v.vt == VT_BOOL)
                    predict_fail = (v.boolVal != VARIANT_FALSE);
                VariantClear(&v);

                uint32_t reason = 0;
                if (SUCCEEDED(obj2->Get(L"Reason", 0, &v, nullptr, nullptr)) && v.vt == VT_I4)
                    reason = static_cast<uint32_t>(v.lVal);
                VariantClear(&v);

                // Extract model fragment from "Prod_<model>\" portion of InstanceName
                // e.g. "SCSI\Disk&Ven_&Prod_ST500DM002-1BD14\5&..." → "ST500DM002-1BD14"
                QString prod_fragment;
                {
                    const int prod_pos = instance_name.indexOf(QStringLiteral("Prod_"));
                    if (prod_pos >= 0) {
                        const int start = prod_pos + 5;
                        int end = instance_name.indexOf(QLatin1Char('\\'), start);
                        if (end < 0) end = instance_name.length();
                        prod_fragment = instance_name.mid(start, end - start).trimmed();
                    }
                }

                for (auto& d : drives) {
                    if (prod_fragment.isEmpty() || d.model.isEmpty()) continue;
                    if (d.smart_available) continue;  // already matched, skip duplicate
                    const QString& shorter = prod_fragment.length() <= d.model.length()
                        ? prod_fragment : d.model;
                    const QString& longer  = prod_fragment.length() <= d.model.length()
                        ? d.model : prod_fragment;
                    if (!longer.contains(shorter, Qt::CaseInsensitive)) continue;
                    d.smart_available = true;
                    if (predict_fail) {
                        d.flagged       = true;
                        d.health_status = QStringLiteral("Pred Fail");
                        d.error_detail  = reason != 0
                            ? QStringLiteral("SMART Reason=0x%1").arg(reason, 0, 16)
                            : QStringLiteral("SMART Predict Failure");
                    } else {
                        d.health_status = QStringLiteral("OK");
                    }
                    break;
                }

                obj2->Release();
            }
            enum2->Release();
        }
        // --- Query 3: MSStorageDriver_ATAPISmartData — raw SMART attributes ---
        IEnumWbemClassObject* enum3 = nullptr;
        hr = wmi_svc->ExecQuery(
            _bstr_t(L"WQL"),
            _bstr_t(L"SELECT InstanceName, VendorSpecific FROM MSStorageDriver_ATAPISmartData"),
            WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
            nullptr, &enum3
        );

        if (SUCCEEDED(hr) && enum3) {
            IWbemClassObject* obj3 = nullptr;
            ULONG ret3 = 0;
            while (enum3->Next(WBEM_INFINITE, 1, &obj3, &ret3) == WBEM_S_NO_ERROR) {
                VARIANT v;
                VariantInit(&v);

                QString instance_name;
                if (SUCCEEDED(obj3->Get(L"InstanceName", 0, &v, nullptr, nullptr)) && v.vt == VT_BSTR)
                    instance_name = QString::fromWCharArray(v.bstrVal).trimmed();
                VariantClear(&v);

                // VendorSpecific is a SAFEARRAY of bytes — 512 bytes SMART data block
                // Layout: 2 bytes revision + 30 × 12-byte attribute entries
                if (SUCCEEDED(obj3->Get(L"VendorSpecific", 0, &v, nullptr, nullptr))
                    && (v.vt & VT_ARRAY) && (v.vt & VT_UI1))
                {
                    SAFEARRAY* sa = v.parray;
                    LONG lb = 0, ub = 0;
                    SafeArrayGetLBound(sa, 1, &lb);
                    SafeArrayGetUBound(sa, 1, &ub);
                    const LONG len = ub - lb + 1;

                    uint8_t* data = nullptr;
                    if (SUCCEEDED(SafeArrayAccessData(sa, reinterpret_cast<void**>(&data))) && len >= 362) {
                        // Each attribute entry starts at offset 2, 12 bytes each:
                        // [0]=id [1-2]=flags [3]=current [4]=worst [5-10]=raw [11]=reserved
                        uint32_t reallocated = 0, pending = 0, uncorrectable = 0;
                        bool any_found = false;

                        for (int i = 0; i < 30; ++i) {
                            const uint8_t* e = data + 2 + i * 12;
                            const uint8_t id = e[0];
                            if (id == 0) continue;
                            any_found = true;
                            const uint32_t raw = static_cast<uint32_t>(e[5])
                                               | (static_cast<uint32_t>(e[6]) << 8)
                                               | (static_cast<uint32_t>(e[7]) << 16)
                                               | (static_cast<uint32_t>(e[8]) << 24);
                            if (id == 0x05) reallocated   = raw;
                            if (id == 0xC5) pending        = raw;
                            if (id == 0xC6) uncorrectable  = raw;
                        }

                        SafeArrayUnaccessData(sa);

                        if (any_found) {
                            // Match drive by InstanceName Prod_ fragment
                            QString prod_fragment;
                            const int prod_pos = instance_name.indexOf(QStringLiteral("Prod_"));
                            if (prod_pos >= 0) {
                                const int start = prod_pos + 5;
                                int end = instance_name.indexOf(QLatin1Char('\\'), start);
                                if (end < 0) end = instance_name.length();
                                prod_fragment = instance_name.mid(start, end - start).trimmed();
                            }

                            for (auto& d : drives) {
                                if (prod_fragment.isEmpty() || d.model.isEmpty()) continue;
                                const QString& shorter = prod_fragment.length() <= d.model.length()
                                    ? prod_fragment : d.model;
                                const QString& longer = prod_fragment.length() <= d.model.length()
                                    ? d.model : prod_fragment;
                                if (!longer.contains(shorter, Qt::CaseInsensitive)) continue;

                                // Override with real raw values
                                d.reallocated   = reallocated;
                                d.pending        = pending;
                                d.uncorrectable  = uncorrectable;

                                if (reallocated != 0 || pending != 0 || uncorrectable != 0) {
                                    d.flagged = true;
                                    QStringList reasons;
                                    if (reallocated  != 0) reasons << QStringLiteral("Reallocated=%1").arg(reallocated);
                                    if (pending      != 0) reasons << QStringLiteral("Pending=%1").arg(pending);
                                    if (uncorrectable!= 0) reasons << QStringLiteral("Uncorrectable=%1").arg(uncorrectable);
                                    d.error_detail  = reasons.join(QStringLiteral(", "));
                                    d.health_status = QStringLiteral("WARNING");
                                } else if (!d.flagged) {
                                    d.health_status = QStringLiteral("OK");
                                }
                                break;
                            }
                        }
                    }
                }
                VariantClear(&v);
                obj3->Release();
            }
            enum3->Release();
        }

        wmi_svc->Release();
    }

    // Mark any drive still without SMART data as unknown
    for (auto& d : drives) {
        if (!d.smart_available) {
            d.health_status   = QStringLiteral("No SMART data");
            d.smart_available = false;
        }
    }

    services->Release();
    locator->Release();
    if (com_owned) CoUninitialize();
}

// ---------------------------------------------------------------------------
// scan — enumerate PhysicalDrive0 … until open fails (max 16)
// ---------------------------------------------------------------------------
std::vector<DriveInfo> StorageManager::scan()
{
    std::vector<DriveInfo> result;
    for (int i = 0; i < 16; ++i) {
        const QString path = QStringLiteral("\\\\.\\PhysicalDrive%1").arg(i);
        HANDLE h = CreateFileW(
            reinterpret_cast<LPCWSTR>(path.utf16()),
            0,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            nullptr, OPEN_EXISTING, 0, nullptr
        );
        if (h == INVALID_HANDLE_VALUE) break;
        CloseHandle(h);

        result.push_back(probeDrive(i));
    }

    // Fill model/serial/size via single WMI session
    queryWmiDiskDrives(result);

    return result;
}

// ---------------------------------------------------------------------------
// evaluate
// ---------------------------------------------------------------------------
ModuleResult StorageManager::evaluate(const std::vector<DriveInfo>& drives)
{
    ModuleResult r;
    r.label = QStringLiteral("S.M.A.R.T.");

    if (drives.empty()) {
        r.status  = TestStatus::Skipped;
        r.summary = QStringLiteral("No drives found");
        return r;
    }

    bool any_flagged = false;
    for (const DriveInfo& d : drives) {
        if (d.flagged) {
            any_flagged = true;
            const QString drive_name = d.model.isEmpty()
                ? QStringLiteral("Drive %1").arg(d.index)
                : d.model;
            r.issues.append(
                QStringLiteral("%1: %2").arg(drive_name, d.error_detail.isEmpty()
                    ? d.health_status
                    : d.error_detail)
            );
        }
    }

    if (any_flagged) {
        r.status  = TestStatus::Fail;
        r.summary = QStringLiteral("%1 drive(s) flagged").arg(r.issues.size());
    } else {
        r.status  = TestStatus::Pass;
        r.summary = QStringLiteral("%1 drive(s) healthy").arg(drives.size());
    }
    return r;
}

} // namespace nbi
