# Notebook Inspector (NBI)

เครื่องมือตรวจสอบฮาร์ดแวร์โน้ตบุ๊ก สำหรับใช้งานหน้าร้านซ่อม  
Single EXE, ต้องการสิทธิ์ Administrator, รองรับ Windows 10/11 x64

---

## Features (Phase 3 — กำลังพัฒนา)

| โมดูล | สถานะ | สิ่งที่ทดสอบ |
|---|---|---|
| **Screen** | ✅ Done | Color cycle (R/G/B/W/Black), Brightness DDC/CI, Dead pixel pin, Touchscreen canvas (optional) |
| **Keyboard** | ✅ Done | Layout map (104/TKL), WH_KEYBOARD_LL hook, มาร์คปุ่มเสีย, ข้ามได้ถ้าไม่มีคีย์บอร์ด |
| **Touchpad** | ⏳ รอทำ | — |
| **Battery** | ⏳ รอทำ | — |
| **S.M.A.R.T.** | ✅ Done | SATA/NVMe raw attributes, WMI fallback, Reallocated/Pending/Uncorrectable |
| **Temperature** | ⏳ รอทำ | — |
| **Ports** | ⏳ รอทำ | — |
| **Audio** | ✅ Done (Speaker) | enumerate output devices, sine tone per device, ผู้ใช้ confirm ได้ยิน |
| **Network** | ✅ Done | NIC, Wi-Fi scan (SSID/signal/connected), ping gateway + 8.8.8.8 |
| **Physical** | ✅ Done | 8 รายการ checklist, N/A option สำหรับ Hinge, Done lock จนกว่าตอบครบ |

---

## Stack

| | |
|---|---|
| Language | C++20 |
| GUI | Qt 6.11 (Widgets, Multimedia) |
| Build | MSVC 2022 + CMake + Ninja |
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

Output: `build/bin/NotebookInspector.exe`

---

## Usage

1. รันในฐานะ Administrator (UAC prompt จะขึ้นอัตโนมัติ)
2. หน้า Dashboard แสดง spec เครื่อง (CPU / GPU / RAM / Serial / OS)
3. กด **Start** ที่การ์ดแต่ละโมดูลเพื่อเริ่มทดสอบ
4. ผลแต่ละโมดูลแสดงเป็น traffic light: 🟢 Pass / 🔴 Fail / ⚫ ยังไม่ทดสอบ / 🔵 ข้าม

---

## Project Structure

```
common/     — shared types: TestStatus, DeviceInfo, ModuleResult
core/       — hardware logic (auto_scan, storage_manager, network_manager, audio_manager, ...)
gui/        — Qt widgets per module
resources/  — .qrc, keyboard layout JSON (en_standard, en_tkl)
```

---

## Roadmap

- [x] Phase 1 — Foundation (WMI scan, dashboard)
- [x] Phase 2 — MVP (Screen, Keyboard)
- [ ] Phase 3 — Hardware Depth ← กำลังทำ
  - [x] S.M.A.R.T. Storage
  - [x] Network (Wi-Fi + LAN + ping)
  - [x] Audio — Speaker
  - [x] Physical Checklist
  - [ ] Battery, Temperature, Ports, Mic
- [ ] Phase 4 — Report Export + Release
