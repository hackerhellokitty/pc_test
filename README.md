# Notebook Inspector (NBI)

เครื่องมือตรวจสอบฮาร์ดแวร์โน้ตบุ๊ก สำหรับใช้งานหน้าร้านซ่อม  
Single EXE, ต้องการสิทธิ์ Administrator, รองรับ Windows 10/11 x64

---

## Features (Phase 2 — MVP)

| โมดูล | สิ่งที่ทดสอบ |
|---|---|
| **Screen** | Color cycle (R/G/B/W/Black), Brightness DDC/CI, Dead pixel pin พร้อมระบุสีพื้นหลัง, Touchscreen canvas (optional) |
| **Keyboard** | กดปุ่มทุกปุ่ม, แสดง layout map, มาร์คปุ่มเสีย |
| **Touchpad** | *(coming soon)* |
| **Battery** | *(Phase 3)* |
| **S.M.A.R.T.** | *(Phase 3)* |
| **Temperature** | *(Phase 3)* |
| **Ports** | *(Phase 3)* |
| **Audio** | *(Phase 3)* |
| **Network** | *(Phase 3)* |
| **Physical** | *(Phase 4)* |

---

## Stack

| | |
|---|---|
| Language | C++20 |
| GUI | Qt 6.11 (Widgets) |
| Build | MSVC 2022 + CMake + Ninja |
| Hardware | Win32 API, WMI, DDC/CI (Dxva2) |
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
4. ผลแต่ละโมดูลแสดงเป็น traffic light: 🟢 Pass / 🔴 Fail / ⚫ ยังไม่ทดสอบ

---

## Project Structure

```
common/     — shared types: TestStatus, DeviceInfo, ModuleResult
core/       — hardware logic (auto_scan, ...)
gui/        — Qt widgets per module
resources/  — .qrc, keyboard layout JSON
```

---

## Roadmap

- [x] Phase 1 — Foundation (WMI scan, dashboard)
- [x] Phase 2 — MVP (Screen, Keyboard) ← กำลังทำ
- [ ] Phase 3 — Hardware Depth (Battery, SMART, Thermal, Ports, Audio, Network)
- [ ] Phase 4 — Report Export + Release
