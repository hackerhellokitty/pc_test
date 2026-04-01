# Notebook Inspector (NBI) — Skill & Knowledge Log

บันทึกความรู้และทักษะที่ได้จากการพัฒนาโครงการนี้

---

## WMI Queries (Windows Management Instrumentation)

- ใช้ `IWbemLocator` + `IWbemServices` เพื่อ query WMI — ต้อง `CoInitializeEx` + `CoInitializeSecurity` ก่อนเสมอ
- **Battery:** `SELECT DesignCapacity, FullChargeCapacity, CycleCount FROM BatteryFullCapacityTermulated` — ให้ข้อมูลละเอียดกว่า `GetSystemPowerStatus`
- **Thermal:** `SELECT CurrentTemperature FROM MSAcpi_ThermalZoneTemperature` — unit เป็น tenths of Kelvin → Celsius = `(val - 2732) / 10`
- **Storage (fallback):** `SELECT * FROM MSFT_Disk` → ดู `OperationalStatus`, `HealthStatus`
- บาง OEM ไม่ expose thermal sensor ผ่าน WMI — ต้อง detect และ skip gracefully ไม่หักคะแนน
- WMI query บางตัว (thermal, SMART) ต้องการ Administrator privilege — ต้อง handle กรณีที่ access denied

## Storage S.M.A.R.T.

- **Method หลัก (SATA):** `SMART_RCV_DRIVE_DATA` ioctl → parse `SmartDataBlock` 30 attributes
- **Method หลัก (NVMe):** `IOCTL_STORAGE_QUERY_PROPERTY` + `StorageDeviceProtocolSpecificProperty` → NVMe Log Page 0x02
- **WMI fallback ที่ใช้งานได้จริง:** `root\wmi` namespace — `MSStorageDriver_FailurePredictStatus` (PredictFailure flag) + `MSStorageDriver_ATAPISmartData` (VendorSpecific = raw 512-byte SMART block)
- `Win32_DiskDrive.Status` **ไม่น่าเชื่อถือ** — คืน "OK" เสมอแม้ disk จะพัง ห้ามใช้เป็น health indicator
- `GetPhysicalDisk.HealthStatus` ก็ไม่น่าเชื่อถือเช่นกัน ต้องอ่าน SMART raw value โดยตรง
- Attributes ที่สำคัญ: Reallocated (05), Pending (C5), Uncorrectable (C6) — ไม่ศูนย์ = flag ทันที
- NVMe กับ SATA ใช้ command set ต่างกัน — detect `StorageAdapterProperty` (BusType) ก่อน
- เปิด handle disk ด้วย `\\.\PhysicalDrive0` — ต้องการ Admin
- **Model/Serial:** ใช้ WMI `Win32_DiskDrive` (ROOT\CIMV2) — `STORAGE_DEVICE_DESCRIPTOR.ProductIdOffset` ไม่น่าเชื่อถือบน NVMe/modern driver
- **WMI session:** ต้อง `CoInitializeEx` + `CoInitializeSecurity` ก่อนทุกครั้ง — ถ้า session เดียวกัน query หลาย namespace ได้โดยเรียก `ConnectServer` ซ้ำบน locator เดิม
- **WMI COM threading:** อย่า call `CoUninitialize` ระหว่าง scan แล้ว init ใหม่ใน call ถัดไป — ทำให้ instance ถัดไป fail; ควรรวมเป็น single WMI session ต่อ scan cycle
- **InstanceName matching:** `MSStorageDriver_*` ใช้ InstanceName รูป `SCSI\Disk&Ven_X&Prod_<model>\...` — suffix `_0` คือ LUN ไม่ใช่ disk index; match ด้วย `Prod_<fragment>` เทียบกับ `Win32_DiskDrive.Model` แทน
- **VendorSpecific layout (ATAPISmartData):** SAFEARRAY of UINT8, offset 2 = attribute entries, แต่ละ entry 12 bytes: [0]=id, [1-2]=flags, [3]=current, [4]=worst, [5-10]=raw (LE), [11]=reserved

## Win32 Monitor & Brightness

- ใช้ `EnumDisplayMonitors` + `GetPhysicalMonitorsFromHMONITOR` เพื่อ handle multi-monitor
- `SetMonitorBrightness` / `GetMonitorBrightness` ใช้ได้เฉพาะ monitor ที่ support DDC/CI
- ต้อง `GetMonitorCapabilities` ก่อน — ถ้าไม่ support ให้ fallback หรือ disable slider
- **สำคัญ:** ต้อง `DestroyPhysicalMonitors` ทุกครั้งหลังใช้งาน ไม่งั้น handle leak

## USB / Device Watcher (WM_DEVICECHANGE)

- Register `DEV_BROADCAST_DEVICEINTERFACE` กับ `RegisterDeviceNotification`
- Filter เฉพาะ `GUID_DEVINTERFACE_USB_DEVICE` และ `GUID_DEVINTERFACE_DISK` — ไม่งั้น USB Hub trigger ทำให้ event ซ้ำซ้อน
- `DBT_DEVICEARRIVAL` / `DBT_DEVICEREMOVECOMPLETE` — parse `DEV_BROADCAST_HDR` เพื่อดู device path
- ใช้ `IOCTL_STORAGE_GET_DEVICE_NUMBER` แปลง device path เป็น PhysicalDriveN (เหมือน FlashVerify)

## Keyboard Scan Codes (Qt)

- `QKeyEvent::nativeScanCode()` ให้ scan code จริงจาก hardware — แยกปุ่มซ้าย/ขวาได้ (Shift, Ctrl, Alt มี scan code ต่างกัน)
- `QKeyEvent::key()` ให้ logical key — ไม่แยกซ้าย/ขวา ไม่เพียงพอ
- Keyboard layout ต่างรุ่นอาจมี scan code ต่างกัน (Fn row, Numpad, OEM keys) — เก็บ layout map เป็น JSON แยกไฟล์
- ปุ่มที่ไม่อยู่ใน layout map ให้แสดงเป็น hex scan code เพื่อ debug

## Qt Audio (QAudioSource / QAudioSink)

- `QMediaDevices::audioInputs()` / `audioOutputs()` enumerate devices
- `QAudioSource` + `QIODevice` → read raw PCM → คำนวณ RMS สำหรับ waveform display
- Loopback test: route `QAudioSource` output ไปที่ `QAudioSink` โดยตรงผ่าน `QBuffer`
- ต้อง handle กรณี device ไม่มี (no mic) — module skip ไม่ใช่ crash
- Sample rate แนะนำ 44100 Hz, 16-bit, stereo สำหรับ loopback

## Qt Painter — Touch Canvas

- `QWidget::mouseMoveEvent` + `Qt::LeftButton` สำหรับ mouse/touchpad
- `QTouchEvent` สำหรับ touchscreen จริง — ต้อง `setAttribute(Qt::WA_AcceptTouchEvents)`
- เก็บ stroke เป็น `QVector<QVector<QPointF>>` — วาดซ้ำทุกครั้งใน `paintEvent`
- ตรวจ dead zone: แบ่ง canvas เป็น grid → นับ cell ที่มี stroke ผ่าน → คิดเป็น % coverage

## IP Helper API (Connectivity)

- `GetAdaptersInfo` หรือ `GetAdaptersAddresses` enumerate network adapters
- ถ้า `adapters.empty()` หรือ no active adapter → status `NoInterface` → UI badge "ข้าม" สีเทา ไม่ใช่ error
- Ping: `IcmpCreateFile` + `IcmpSendEcho` — ง่ายกว่า raw socket และไม่ต้องการ Admin
- Timeout ควรสั้น (500ms) เพราะ test นี้ไม่ใช่ priority หลัก

## Admin Elevation (Qt App)

- ฝัง manifest ใน `.rc` file: `requestedExecutionLevel level="requireAdministrator"`
- หรือ detect at runtime: `OpenProcessToken` + `GetTokenInformation(TokenElevation)` → ถ้าไม่ elevated → `ShellExecuteW(L"runas", ...)`
- SMART query และ WMI thermal ต้องการ Admin — แนะนำ require ตั้งแต่ต้น

## CMake + Qt 6 + MSVC

- ใช้ `find_package(Qt6 REQUIRED COMPONENTS Widgets Multimedia)` — ต้องตั้ง `CMAKE_PREFIX_PATH` ชี้ไป Qt install dir
- Static build: ใช้ Qt static libraries + `/MT` runtime — ต้องตั้ง `set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")`
- `qt_add_resources` สำหรับฝัง `.qrc` (icons, keyboard layouts, checklist images)
- `windeployqt --static` หลัง build เพื่อ collect dependencies ที่เหลือ
- ระวัง: static Qt build ใหญ่กว่า dynamic มาก — target < 30 MB ต้องเลือก Qt modules ให้พอดี

## PDF / PNG Report Export (Qt)

- **PNG:** `QScreen::grabWindow(winId())` + `QPixmap::save()` — เร็วแต่ได้แค่ screenshot
- **PDF:** `QPrinter(QPrinter::PdfFormat)` + `QPainter` — layout ได้อิสระกว่า
- Dead pixel coordinates: overlay วงกลมบน screenshot ด้วย `QPainter::drawEllipse` ก่อน save
- ใส่ timestamp ด้วย `QDateTime::currentDateTime().toString(Qt::ISODate)`
- Serial number: `Win32_BIOS` WMI query → `SerialNumber`

---

## Stack สรุป

| Layer | Technology |
|---|---|
| Language | C++20 |
| GUI | Qt 6.7 (Widgets, Multimedia) |
| Build | MSVC + CMake + Ninja |
| Hardware Access | Win32 API, WMI, DeviceIoControl |
| Report Export | Qt QPrinter (PDF), QPixmap (PNG) |
| Platform | Windows 10 / 11 x64 |

---

## ความรู้ที่ยังใช้ได้จาก FlashVerify

- `IOCTL_STORAGE_GET_DEVICE_NUMBER` — แปลง device path เป็น PhysicalDriveN (ใช้ใน device watcher)
- `IOCTL_STORAGE_QUERY_PROPERTY` — base ของ SMART query
- Admin elevation pattern ผ่าน `ShellExecuteW("runas")`
- Filter `WM_DEVICECHANGE` event เฉพาะ device GUID ที่ต้องการ