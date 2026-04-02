# Notebook Inspector (NBI)

เครื่องมือตรวจสอบฮาร์ดแวร์โน้ตบุ๊ก สำหรับใช้งานหน้าร้านซ่อม  
Single EXE (+ DLL), ต้องการสิทธิ์ Administrator, รองรับ Windows 10/11 x64

---

## Features

| โมดูล | สิ่งที่ทดสอบ |
|---|---|
| **Screen** | Color cycle (R/G/B/W/Black), Brightness DDC/CI, Dead pixel pin, Touchscreen canvas |
| **Keyboard** | Layout map (104/TKL), WH_KEYBOARD_LL hook, มาร์คปุ่มเสีย, ข้ามได้ถ้าไม่มีคีย์บอร์ด |
| **Touchpad** | Canvas + coverage %, gesture detection (click/double/right/scroll) |
| **Battery** | WMI BatteryFullChargeCapacity — Wear Level, Cycle Count, flag < 60% |
| **S.M.A.R.T.** | SATA/NVMe raw attributes, WMI fallback, flag Reallocated/Pending/Uncorrectable ≠ 0 |
| **Temperature** | WMI MSAcpi_ThermalZoneTemperature — zone cards, flag > 95°C, graceful skip ถ้าไม่มี sensor |
| **Ports** | USB plug detection (RegisterDeviceNotification), 3.5mm jack tone test |
| **Audio** | enumerate output devices, sine tone per device, ผู้ใช้ confirm ได้ยิน |
| **Mic** | อัด 10 วิ → เล่นกลับ → ผู้ใช้ confirm |
| **Network** | NIC enumeration, Wi-Fi scan (SSID/signal/connected), ping gateway + 8.8.8.8 |
| **Physical** | 8 รายการ checklist (Hinge, Display, Chassis, Ports visual ฯลฯ) |

### Scoring & Report
- คะแนนรวม 0–100 จาก weight แต่ละโมดูล (Battery=20, S.M.A.R.T.=20, Screen=15, Keyboard=15, ฯลฯ)
- Cap rule: Battery Wear < 60% หรือ S.M.A.R.T. warning → คะแนนรวมไม่เกิน 59
- Export **PDF** (QPrinter + SHA-256 hash ป้องกันแก้ไข) และ **PNG** (watermark Serial + timestamp)

---

## Stack

| | |
|---|---|
| Language | C++20 |
| GUI | Qt 6.11 (Widgets, Multimedia, PrintSupport, Concurrent) |
| Build | MSVC 2022 + CMake + MSBuild |
| Hardware | Win32 API, WMI, DDC/CI (Dxva2), Native Wi-Fi (Wlanapi), ICMP |
| Platform | Windows 10/11 x64 |

---

## Build

**Prerequisites**
- Qt 6.11 msvc2022_64
- Visual Studio 2022 (MSVC)
- CMake ≥ 3.25

```bash
cmake -B build -G "Visual Studio 18 2026" -A x64 \
  -DCMAKE_PREFIX_PATH="C:/Qt/6.11.0/msvc2022_64"

cmake --build build --config Release --parallel
```

Output: `build/bin/NotebookInspector.exe` พร้อม Qt DLL ครบ (windeployqt อัตโนมัติ)

> **Note:** ต้องระบุ `--config Release` เสมอ ไม่งั้น CMake จะ default เป็น Debug

---

## Usage

1. รันในฐานะ Administrator (UAC prompt ขึ้นอัตโนมัติ — ถ้าไม่ใช่ admin จะมีตัวเลือก re-launch)
2. หน้า Dashboard แสดง spec เครื่อง (CPU / GPU / RAM / Serial / OS) — สแกนแบบ async ไม่บล็อค UI
3. กด **Start** ที่การ์ดแต่ละโมดูลเพื่อเริ่มทดสอบ
4. ผลแต่ละโมดูลแสดงเป็น traffic light: 🟢 Pass / 🔴 Fail / ⚫ ยังไม่ทดสอบ / 🔵 ข้าม
5. กด **สรุปผล + Export Report** เพื่อดูคะแนนรวมและ export PDF/PNG

---

## Project Structure

```
common/     — shared types: TestStatus, DeviceInfo, ModuleResult
core/       — hardware logic (auto_scan, battery_manager, storage_manager,
              thermal_manager, network_manager, audio_manager, scorer)
gui/        — Qt widgets per module (view + window pairs)
resources/  — .qrc, keyboard layout JSON (en_standard, en_tkl)
```

---

## Roadmap

- [x] Phase 1 — Foundation (WMI scan, dashboard, UAC, single instance)
- [x] Phase 2 — MVP (Screen, Keyboard, Touchpad)
- [x] Phase 3 — Hardware Depth (Battery, S.M.A.R.T., Thermal, Ports, Audio, Mic, Network, Physical)
- [x] Phase 4 — Report + Release (Scoring, Summary page, PDF/PNG export, UX pass)
