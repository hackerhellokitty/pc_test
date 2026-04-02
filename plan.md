# Notebook Inspector (NBI) — Development Plan

> 4 phases แบ่งตาม "สิ่งที่ต้องพิสูจน์" ในแต่ละช่วง

---

## Overview

| Phase | ชื่อ | สิ่งที่พิสูจน์ | ประมาณเวลา |
|---|---|---|---|
| 1 | Foundation | Build pipeline + WMI ทำงานได้ | 1–2 สัปดาห์ |
| 2 | MVP | มีของใช้หน้างานได้จริง | 2–3 สัปดาห์ |
| 3 | Hardware Depth | Win32 API เชิงลึกครบทุกโมดูล | 3–4 สัปดาห์ |
| 4 | Report + Release | Export ได้, UX สมบูรณ์, พร้อม ship | 2 สัปดาห์ |

รวมประมาณ **8–11 สัปดาห์**

---

## Phase 1 — Foundation

**เป้าหมาย:** โปรแกรมเปิดได้ แสดงข้อมูลเครื่องบนหน้าแรก พิสูจน์ว่า WMI และ build pipeline ทำงานได้จริง

### Tasks

- [x] CMake project setup — Qt 6.11 + MSVC 2026 + dynamic build, `CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL"`
- [x] โครงสร้าง folder ตาม blueprint (`common/`, `core/`, `gui/`, `resources/`)
- [x] Admin elevation manifest — `.rc` + `requestedExecutionLevel = requireAdministrator`
- [x] Single instance check — `CreateMutex`
- [x] `common/` — define `TestStatus`, `DeviceInfo`, `ModuleResult`
- [x] `core/auto_scan.cpp` — WMI query: CPU/GPU/RAM spec, Serial Number (`Win32_BIOS`), OS version
- [x] `gui/main_dashboard.cpp` — หน้าแรก แสดง spec + traffic light placeholder ทุกโมดูล
- [x] Build ทดสอบบน Windows 10/11 — compile, รัน, ไม่ crash

### Deliverable

โปรแกรมเปิดขึ้น แสดง spec เครื่อง มี placeholder โมดูลทั้งหมดบนหน้าแรก

---

## Phase 2 — MVP

**เป้าหมาย:** 3 โมดูลที่ทดสอบด้วยตาและมือ ไม่ต้องการ Win32 API ซับซ้อน — มีของพอใช้งานได้จริงหน้างาน

### Tasks

**Screen Test**
- [x] `gui/screen_test_view.cpp` — full-screen color cycle (R→G→B→W→Black)
- [x] Brightness slider — `SetMonitorBrightness` + `GetMonitorCapabilities` fallback
- [x] Dead pixel pin — คลิกบนจอเพื่อบันทึกพิกัด + สีพื้นหลัง ณ ตอนปัก + `GetPhysicalMonitorsFromHMONITOR` multi-monitor
- [x] Touchscreen canvas — `QTouchEvent` stroke + grid coverage 20×15, manual toggle "มี Touchscreen" ที่ dashboard card

**Keyboard Test**
- [x] `gui/keyboard_view.cpp` — วาด keyboard map จาก JSON layout (`resources/keyboard_layouts/`)
- [x] `WH_KEYBOARD_LL` hook — state per key: untested → pressed → marked-broken (ใช้ low-level hook แทน nativeScanCode เพราะ IME ดัก ` และปุ่มอื่น)
- [x] "Mark ว่าเสีย" — คลิกขวาบนแผนผัง
- [x] Layout JSON: EN standard 104 keys + TKL 87 keys (ไม่มี numpad)
- [x] Layout selector dropdown ใน KeyboardWindow — เปลี่ยน layout ได้ระหว่างทดสอบ
- [x] Checkbox "ไม่มีคีย์บอร์ด" ที่ dashboard card — tablet → Skipped ทันที

**Touchpad Test**
- [x] `gui/touchpad_view.cpp` — QPainter canvas + stroke recording (reuse TouchCanvasView)
- [x] Coverage % (grid-based 20×15)
- [x] Gesture: single click, double click, right click, scroll — auto-detect
- [x] `mouseMoveEvent` สำหรับ touchpad
- [x] Checkbox "ไม่มี Touchpad" ที่ dashboard card → Skipped ทันที

### Deliverable

**MVP** — รันได้หน้างาน ทดสอบ screen/keyboard/touchpad ครบ ใช้ต่อราคาเบื้องต้นได้

---

## Phase 3 — Hardware Depth

**เป้าหมาย:** โมดูลที่ต้องการ Win32 API + WMI เชิงลึก รวมทั้ง peripheral detection ที่เขียนเร็วกว่า

### Tasks

**Battery Health**
- [x] `core/battery_manager.cpp` — WMI `BatteryFullChargeCapacity` + `BatteryStaticData`
- [x] Wear Level = `(FullChargeCapacity / DesignCapacity) × 100` + Cycle Count
- [x] Threshold: Wear Level < 60% = flag
- [x] `gui/battery_view.cpp` + `battery_window.cpp` — Wear Level bar, Design/Full/Cycle stats
- [x] Checkbox "เป็น PC / ไม่มีแบต" ที่ dashboard card → Skipped ทันที
- [x] UAC manifest embed ใน `.rc` + `/MANIFEST:NO` — requireAdministrator ทุกครั้ง
- [ ] รอ test จริง

**Storage S.M.A.R.T.**
- [x] `core/storage_manager.cpp` — SATA: `SMART_RCV_DRIVE_DATA`, NVMe: `StorageDeviceProtocolSpecificProperty` Log Page 0x02
- [x] Parse attributes: Reallocated (05), Pending (C5), Uncorrectable (C6) — ไม่ศูนย์ = flag ทันที
- [x] WMI fallback: `root\wmi` → `MSStorageDriver_ATAPISmartData` (raw 512-byte block) + `MSStorageDriver_FailurePredictStatus`
- [x] Detect `StorageAdapterProperty` ก่อนเลือก method (SATA vs NVMe)
- [x] Model/Serial: WMI `Win32_DiskDrive` single session

**Temperature Monitor**
- [x] `core/thermal_manager.cpp` — WMI `MSAcpi_ThermalZoneTemperature`
- [x] `gui/thermal_view.cpp` — zone cards + progress bar สี
- [x] Threshold: Peak > 95°C = red
- [x] Handle OEM ไม่มี sensor — skip gracefully ไม่หักคะแนน
- [ ] รอ test จริงบน laptop (desktop AMD ไม่มี CPU die temp ผ่าน ACPI)

**Ports**
- [x] `gui/ports_view.cpp` + `ports_window.cpp` — user ระบุจำนวน USB port
- [x] Phase 1 USB: `RegisterDeviceNotification` + `WM_DEVICECHANGE` จับ plug event, countdown 10 วิต่อ port, manual mark ไม่ตอบสนองพร้อมระบุชื่อ port
- [x] Phase 2 3.5mm: เสียบหูฟัง → Play 1 kHz tone → confirm ได้ยิน/ไม่ได้ยิน
- [x] Result รวม USB + 3.5mm เป็น ModuleResult เดียว

**Audio / Mic**
- [x] `core/audio_manager.cpp` — enumerate output devices (`QMediaDevices::audioOutputs`)
- [x] `gui/audio_view.cpp` — DeviceTestCard per device: sine 1 kHz → Play → ผู้ใช้ confirm ได้ยิน/ไม่ได้ยิน
- [x] `gui/audio_window.cpp` — wrapper window
- [x] Handle ไม่มี speaker — skip gracefully
- [x] `gui/mic_view.cpp` + `mic_window.cpp` — อัดเสียง 10 วิ → เล่นกลับ → confirm ได้ยิน/ไม่ได้ยิน (`QAudioSource` + `QAudioSink`)

**Connectivity**
- [x] `core/network_manager.cpp` — `GetAdaptersAddresses` + `IcmpSendEcho` (timeout 500ms)
- [x] No adapter → status `NoInterface` → badge "ข้าม" สีเทา ไม่ใช่ error
- [x] Wi-Fi: `WlanGetAvailableNetworkList` — scan SSIDs รอบข้าง + signal quality bar + connected SSID highlight
- [x] `gui/network_view.cpp` + `network_window.cpp` — adapter cards, Wi-Fi section, ping rows
- [ ] รอ test จริง (Wi-Fi + LAN)

### Deliverable

ทุก 10 โมดูลทำงานได้ — พร้อม end-to-end test ครั้งแรกตั้งแต่เปิดจนถึง summary

**หมายเหตุ — รอ test จริงบนเครื่อง target:**
- Battery: รอ laptop (desktop ไม่มีแบต)
- Wi-Fi / LAN: รอ test บนเครื่องจริง
- Temperature: รอ laptop (desktop AMD ไม่ expose CPU die temp ผ่าน ACPI)
- Screen Touchscreen: รอ laptop ที่มี touchscreen

---

## Phase 4 — Report + Release

**เป้าหมาย:** Export ได้, UX สมบูรณ์, พร้อม ship เป็น single EXE

### Tasks

**Scoring System**
- [x] กำหนด weight แต่ละโมดูล (Battery=20, SMART=20, Screen=15, Keyboard=15, Thermal/Ports/Audio/Touchpad=5 each, Network/Physical=0)
- [x] Cap rule: Battery Wear < 60% หรือ SMART warning → cap คะแนนรวมไม่เกิน 59 — `core/scorer.cpp`
- [x] `gui/summary_page.cpp` — verdict สี + รายการปัญหาเรียงความเสี่ยง
- [x] `gui/summary_window.cpp` — wrapper dialog + trigger Export

**Physical Checklist — Hinge Stress Test**
- [x] เพิ่ม item "Hinge: พับ-เปิดจอ 3-5 ครั้ง ขณะหน้าจอขาว — สังเกตอาการวูบ/สีเพี้ยน/จอดับ" ใน `physical_checklist_view.cpp`

**Report Export**
- [x] `gui/report_generator.cpp`
- [x] PNG: `QScreen::grabWindow` + overlay dead pixel circles + `QPixmap::save`
- [x] PNG Watermark: `QPainter` render Serial Number + Timestamp ทับบน QPixmap ก่อน save — ป้องกันสลับรูป
- [x] PDF: `QPrinter(PdfFormat)` + `QPainter` layout
- [x] PDF Hash: SHA-256 (`QCryptographicHash::Sha256`) จาก `SerialNumber + TestResult + SecretKey` พิมพ์ท้าย report — ป้องกันแก้ไขผลด้วย PDF editor
- [x] ข้อมูลใน report: timestamp, Serial, per-module result, dead pixel coordinates

**UX Pass**
- [x] ทุกหน้ามีคำอธิบาย "กำลังทำอะไร / ผลหมายความว่าอะไร"
- [x] Error + skip state ทุกโมดูล — graceful ไม่ crash
- [x] Progress indicator ระหว่าง Auto Scan (async QFutureWatcher + indeterminate bar)
- [ ] ทดสอบ offline scenario
- [x] ทดสอบ non-admin scenario (แจ้ง UAC อย่างถูกต้อง + runas fallback ใน main.cpp)

**Deployment**
- [x] Dynamic build + windeployqt — EXE 0.5 MB, รวม DLL 86 MB (zip แนบทั้งโฟลเดอร์)
- [x] Build Release config: `cmake --build build --config Release --parallel`
- [x] รันได้บน Windows ที่ไม่มี Qt runtime (DLL อยู่ใน build/bin ครบ)
- [ ] `gh release create` — release notes + zip แนบ

### Deliverable

**v1.0** — Single EXE พร้อมใช้งานหน้างาน

---

## Pending Decisions

- [ ] Scoring weight + verdict formula — กำหนดแยกต่างหาก
- [ ] Keyboard layout เพิ่มเติม (TKL, 60%, gaming)
- [ ] ภาษา UI: TH only หรือ TH/EN toggle