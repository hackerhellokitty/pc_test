# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

**Notebook Inspector (NBI)** — A Windows laptop hardware diagnostics tool. Single-EXE, requires Administrator, targets Windows 10/11 x64.

**Status:** Pre-implementation. `plan.md` contains the 5-phase roadmap; `skill.md` contains API/implementation knowledge.

## Stack

| Layer | Technology |
|---|---|
| Language | C++20 |
| GUI | Qt 6.7 (Widgets, Multimedia) |
| Build | MSVC + CMake + Ninja |
| Hardware | Win32 API, WMI, DeviceIoControl |
| Report | QPrinter (PDF), QPixmap (PNG) |
| Platform | Windows 10/11 x64 |

## Build Commands

```bash
# Configure (set Qt path first)
cmake -B build -G Ninja \
  -DCMAKE_PREFIX_PATH="C:/Qt/6.7.x/msvc2022_64" \
  -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build --parallel

# Static deployment (after build)
windeployqt --static build/NBI.exe
```

CMake key settings:
- `find_package(Qt6 REQUIRED COMPONENTS Widgets Multimedia)`
- `set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")` for static `/MT`
- `qt_add_resources(...)` for `.qrc` (icons, keyboard layouts, checklist images)

Target: single `.exe` < 30 MB, no Qt runtime required.

## Planned Folder Structure

```
common/       — shared structs: TestStatus, DeviceInfo, ModuleResult
core/         — hardware logic (auto_scan, battery, storage, thermal, audio, network, device_watcher)
gui/          — Qt widgets per module (main_dashboard, screen_test_view, keyboard_view, etc.)
resources/    — .qrc, keyboard JSON layouts, checklist images
```

## Architecture

The app is organized as independent test modules, each producing a `ModuleResult`. The main dashboard aggregates results into a scored summary with traffic-light indicators.

**Admin elevation** is required at startup (manifest-based: `requestedExecutionLevel level="requireAdministrator"`). Runtime fallback: `ShellExecuteW(L"runas", ...)`.

**Single instance** enforced via `CreateMutex`.

## Key Implementation Notes (from skill.md)

### WMI
- Always call `CoInitializeEx` + `CoInitializeSecurity` before any WMI query
- Battery: `SELECT DesignCapacity, FullChargeCapacity, CycleCount FROM BatteryFullCapacityTermulated`
- Thermal: `SELECT CurrentTemperature FROM MSAcpi_ThermalZoneTemperature` — unit is tenths of Kelvin → `°C = (val - 2732) / 10`
- Some OEM laptops expose no thermal WMI sensor — detect and skip gracefully, no score penalty

### S.M.A.R.T. Storage
- Primary: `DeviceIoControl(IOCTL_STORAGE_QUERY_PROPERTY)` — open via `\\.\PhysicalDrive0`, requires Admin
- Fallback for NVMe: WMI `MSFT_Disk` → `HealthStatus`
- Must detect SATA vs NVMe via `StorageAdapterProperty` before choosing command set
- Flag immediately (before scoring) if Reallocated Sectors (05), Pending (C5), or Uncorrectable (C6) ≠ 0

### Monitor / Brightness
- `EnumDisplayMonitors` + `GetPhysicalMonitorsFromHMONITOR` for multi-monitor
- Call `GetMonitorCapabilities` before `SetMonitorBrightness` — DDC/CI may not be supported
- Always call `DestroyPhysicalMonitors` to avoid handle leak

### USB / Device Watcher
- `RegisterDeviceNotification` with `DEV_BROADCAST_DEVICEINTERFACE`
- Filter only `GUID_DEVINTERFACE_USB_DEVICE` and `GUID_DEVINTERFACE_DISK` to avoid duplicate events from USB hubs

### Keyboard
- Use `QKeyEvent::nativeScanCode()` — distinguishes left/right modifiers; `QKeyEvent::key()` does not
- Store layout maps as JSON (separate file per layout: TH standard, EN standard, etc.)
- Unknown scan codes → display as hex for debugging

### Audio
- `QMediaDevices::audioInputs/audioOutputs()` to enumerate
- `QAudioSource` + `QIODevice` → raw PCM → RMS for waveform
- Recommended: 44100 Hz, 16-bit, stereo
- No mic/speaker → module skip, not crash

### Touch Canvas
- `QWidget::mouseMoveEvent` for touchpad, `QTouchEvent` for touchscreen (`Qt::WA_AcceptTouchEvents`)
- Store strokes as `QVector<QVector<QPointF>>`, redraw in `paintEvent`
- Grid-based coverage %: divide canvas into cells, count cells with stroke

### Network
- `GetAdaptersAddresses` to enumerate; no active adapter → status `NoInterface`, gray badge "ข้าม"
- Ping via `IcmpCreateFile` + `IcmpSendEcho`, timeout 500ms

### Report Export
- PNG: `QScreen::grabWindow(winId())` + `QPainter::drawEllipse` overlay for dead pixels
- PDF: `QPrinter(QPrinter::PdfFormat)` + `QPainter` for custom layout
- Include: timestamp (`QDateTime::currentDateTime().toString(Qt::ISODate)`), serial number (WMI `Win32_BIOS.SerialNumber`), per-module results

## Scoring Rules
- Battery wear < 60% OR any SMART warning → **cap total score** regardless of other modules
- Temperature thresholds: Peak > 95°C = red; Delta (stress−idle) > 40°C = fan/thermal paste flag
- Scoring weights are TBD (tracked in `plan.md` Pending Decisions)

## Pending Decisions
- Scoring weight formula and verdict thresholds
- Additional keyboard layouts (gaming, TKL, 60%)
- UI language: Thai-only or TH/EN toggle
