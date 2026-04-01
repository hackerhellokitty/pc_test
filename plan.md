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
- [ ] `core/device_watcher.cpp` — `RegisterDeviceNotification` + `WM_DEVICECHANGE`
- [ ] Filter `GUID_DEVINTERFACE_USB_DEVICE` + `GUID_DEVINTERFACE_DISK`
- [ ] `gui/port_audio_view.cpp` — port grid + "Detected!" feedback
- [ ] Manual mark พอร์ตที่ไม่ตอบสนอง

**Audio / Mic**
- [x] `core/audio_manager.cpp` — enumerate output devices (`QMediaDevices::audioOutputs`)
- [x] `gui/audio_view.cpp` — DeviceTestCard per device: sine 1 kHz → Play → ผู้ใช้ confirm ได้ยิน/ไม่ได้ยิน
- [x] `gui/audio_window.cpp` — wrapper window
- [x] Handle ไม่มี speaker — skip gracefully
- [ ] Mic test — `QAudioSource` + waveform RMS (ยังไม่ได้ทำ)

**Connectivity**
- [x] `core/network_manager.cpp` — `GetAdaptersAddresses` + `IcmpSendEcho` (timeout 500ms)
- [x] No adapter → status `NoInterface` → badge "ข้าม" สีเทา ไม่ใช่ error
- [x] Wi-Fi: `WlanGetAvailableNetworkList` — scan SSIDs รอบข้าง + signal quality bar + connected SSID highlight
- [x] `gui/network_view.cpp` + `network_window.cpp` — adapter cards, Wi-Fi section, ping rows
- [ ] รอ test จริง (Wi-Fi + LAN)

### Deliverable

ทุก 10 โมดูลทำงานได้ — พร้อม end-to-end test ครั้งแรกตั้งแต่เปิดจนถึง summary

---

## Phase 4 — Report + Release

**เป้าหมาย:** Export ได้, UX สมบูรณ์, พร้อม ship เป็น single EXE

### Tasks

**Physical Checklist**
- [x] `gui/physical_checklist_view.cpp` — guided checklist 8 รายการ (Lid, Hinge, Bezel, Palm Rest, Bottom, Ports, Keyboard, Screen)
- [x] 3 ตัวเลือกต่อข้อ: ✓ ปกติ / ✗ พบปัญหา / — ไม่มี (N/A, เฉพาะ Hinge)
- [x] Done disabled จนกว่าจะตอบครบ — progress counter realtime
- [x] `gui/physical_checklist_window.cpp` — wrapper window

**Scoring System**
- [ ] กำหนด weight แต่ละโมดูล (แยก spec)
- [ ] Cap rule: Battery Wear < 60% หรือ SMART warning → cap คะแนนรวม
- [ ] `gui/summary_page.cpp` — verdict สี + รายการปัญหาเรียงความเสี่ยง

**Report Export**
- [ ] `core/report_generator.cpp`
- [ ] PNG: `QScreen::grabWindow` + overlay dead pixel circles + `QPixmap::save`
- [ ] PDF: `QPrinter(PdfFormat)` + `QPainter` layout
- [ ] ข้อมูลใน report: timestamp, Serial, per-module result, dead pixel coordinates, checklist answers

**UX Pass**
- [ ] ทุกหน้ามีคำอธิบาย "กำลังทำอะไร / ผลหมายความว่าอะไร"
- [ ] Error + skip state ทุกโมดูล — graceful ไม่ crash
- [ ] Progress indicator ระหว่าง Auto Scan + Temperature stress
- [ ] ทดสอบ offline scenario
- [ ] ทดสอบ non-admin scenario (แจ้ง UAC อย่างถูกต้อง)

**Deployment**
- [ ] Static Qt build — `windeployqt --static`
- [ ] ตรวจ exe size < 30 MB
- [ ] ทดสอบบน Windows 10 clean install (ไม่มี Qt runtime)
- [ ] `gh release create` — release notes + exe แนบ

### Deliverable

**v1.0** — Single EXE พร้อมใช้งานหน้างาน

---

## Pending Decisions

- [ ] Scoring weight + verdict formula — กำหนดแยกต่างหาก
- [ ] Keyboard layout เพิ่มเติม (TKL, 60%, gaming)
- [ ] ภาษา UI: TH only หรือ TH/EN toggle