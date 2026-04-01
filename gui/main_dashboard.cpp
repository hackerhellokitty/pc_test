// ---------------------------------------------------------------------------
// gui/main_dashboard.cpp
// ---------------------------------------------------------------------------

#include "gui/main_dashboard.hpp"

#include <QApplication>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QPushButton>
#include <QScrollArea>
#include <QSizePolicy>
#include <QStatusBar>
#include <QString>
#include <QVBoxLayout>

#include "core/auto_scan.hpp"
#include "gui/audio_window.hpp"
#include "gui/battery_window.hpp"
#include "gui/network_window.hpp"
#include "gui/physical_checklist_window.hpp"
#include "gui/thermal_window.hpp"
#include "gui/touchpad_window.hpp"

namespace nbi {

// Module display names — order matches m_cards index
static constexpr std::array<const char*, 10> kModuleNames = {
    "Screen",
    "Keyboard",
    "Touchpad",
    "Battery",
    "S.M.A.R.T.",
    "Temperature",
    "Ports",
    "Audio",
    "Network",
    "Physical",
};

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------
MainDashboard::MainDashboard(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("Notebook Inspector"));
    setMinimumSize(900, 600);

    buildUi();
    loadDeviceInfo();
    populateDeviceLabels();
}

// ---------------------------------------------------------------------------
// UI construction
// ---------------------------------------------------------------------------
void MainDashboard::buildUi()
{
    // Central widget + root layout
    auto* central = new QWidget(this);
    setCentralWidget(central);

    auto* root_layout = new QVBoxLayout(central);
    root_layout->setContentsMargins(12, 12, 12, 12);
    root_layout->setSpacing(12);

    // ------------------------------------------------------------------
    // TOP SECTION — Device information summary
    // ------------------------------------------------------------------
    auto* info_group = new QGroupBox(QStringLiteral("Device Information"), central);
    info_group->setStyleSheet(QStringLiteral(
        "QGroupBox { font-weight: bold; font-size: 13px; }"
    ));

    auto* info_grid = new QGridLayout(info_group);
    info_grid->setColumnStretch(1, 1);
    info_grid->setHorizontalSpacing(16);
    info_grid->setVerticalSpacing(6);

    auto make_key = [](const QString& text, QWidget* parent) -> QLabel* {
        auto* lbl = new QLabel(text, parent);
        lbl->setStyleSheet(QStringLiteral("color: #888; font-size: 12px;"));
        lbl->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
        return lbl;
    };
    auto make_val = [](QWidget* parent) -> QLabel* {
        auto* lbl = new QLabel(QStringLiteral("—"), parent);
        lbl->setStyleSheet(QStringLiteral("font-size: 12px;"));
        lbl->setTextInteractionFlags(Qt::TextSelectableByMouse);
        return lbl;
    };

    int row = 0;
    info_grid->addWidget(make_key(QStringLiteral("CPU:"),    info_group), row,   0);
    m_lbl_cpu = make_val(info_group);
    info_grid->addWidget(m_lbl_cpu, row++, 1);

    info_grid->addWidget(make_key(QStringLiteral("GPU:"),    info_group), row,   0);
    m_lbl_gpu = make_val(info_group);
    info_grid->addWidget(m_lbl_gpu, row++, 1);

    info_grid->addWidget(make_key(QStringLiteral("RAM:"),    info_group), row,   0);
    m_lbl_ram = make_val(info_group);
    info_grid->addWidget(m_lbl_ram, row++, 1);

    info_grid->addWidget(make_key(QStringLiteral("Serial:"), info_group), row,   0);
    m_lbl_serial = make_val(info_group);
    info_grid->addWidget(m_lbl_serial, row++, 1);

    info_grid->addWidget(make_key(QStringLiteral("OS:"),     info_group), row,   0);
    m_lbl_os = make_val(info_group);
    info_grid->addWidget(m_lbl_os, row++, 1);

    root_layout->addWidget(info_group);

    // ------------------------------------------------------------------
    // BOTTOM SECTION — Module cards grid (2 rows × 5 columns)
    // ------------------------------------------------------------------
    auto* modules_group = new QGroupBox(QStringLiteral("Diagnostic Modules"), central);
    modules_group->setStyleSheet(QStringLiteral(
        "QGroupBox { font-weight: bold; font-size: 13px; }"
    ));

    auto* modules_grid = new QGridLayout(modules_group);
    modules_grid->setSpacing(10);

    for (int i = 0; i < kModuleCount; ++i) {
        ModuleCard& card = m_cards[static_cast<std::size_t>(i)];

        // Outer frame
        card.frame = new QFrame(modules_group);
        card.frame->setFrameShape(QFrame::StyledPanel);
        card.frame->setFrameShadow(QFrame::Raised);
        card.frame->setStyleSheet(QStringLiteral(
            "QFrame { border: 1px solid #3a3a3a; border-radius: 6px;"
            " background-color: #2b2b2b; }"
        ));
        card.frame->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        card.frame->setMinimumSize(140, 90);

        auto* card_layout = new QVBoxLayout(card.frame);
        card_layout->setContentsMargins(10, 8, 10, 8);
        card_layout->setSpacing(6);
        card_layout->setAlignment(Qt::AlignCenter);

        // Module name
        card.name_label = new QLabel(
            QString::fromUtf8(kModuleNames[static_cast<std::size_t>(i)]),
            card.frame
        );
        card.name_label->setAlignment(Qt::AlignCenter);
        card.name_label->setStyleSheet(QStringLiteral(
            "font-size: 13px; font-weight: bold; color: #e0e0e0; border: none;"
        ));

        // Traffic-light dot row
        auto* dot_row = new QWidget(card.frame);
        dot_row->setStyleSheet(QStringLiteral("background: transparent; border: none;"));
        auto* dot_row_layout = new QHBoxLayout(dot_row);
        dot_row_layout->setContentsMargins(0, 0, 0, 0);
        dot_row_layout->setSpacing(6);
        dot_row_layout->setAlignment(Qt::AlignCenter);

        card.dot_label = new QLabel(QStringLiteral("●"), dot_row);
        card.dot_label->setStyleSheet(
            QStringLiteral("font-size: 16px; color: %1; border: none;")
                .arg(statusColor(TestStatus::NotRun))
        );
        card.dot_label->setAlignment(Qt::AlignCenter);

        // Status text
        card.status_label = new QLabel(QStringLiteral("—"), dot_row);
        card.status_label->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
        card.status_label->setStyleSheet(QStringLiteral(
            "font-size: 11px; color: #aaaaaa; border: none;"
        ));

        dot_row_layout->addWidget(card.dot_label);
        dot_row_layout->addWidget(card.status_label);

        card_layout->addWidget(card.name_label);
        card_layout->addWidget(dot_row);

        // Card index 0 = Screen — touchscreen toggle + Start button
        if (i == 0) {
            m_chk_touchscreen = new QCheckBox(QStringLiteral("มี Touchscreen"), card.frame);
            m_chk_touchscreen->setStyleSheet(QStringLiteral(
                "QCheckBox { font-size: 11px; color: #cccccc; }"
                "QCheckBox::indicator { width: 13px; height: 13px; }"
            ));
            card_layout->addWidget(m_chk_touchscreen, 0, Qt::AlignCenter);

            auto* btn_start = new QPushButton(QStringLiteral("Start"), card.frame);
            btn_start->setStyleSheet(QStringLiteral(
                "QPushButton { font-size: 11px; padding: 3px 10px;"
                " border: 1px solid #555; border-radius: 4px;"
                " background-color: #3a3a3a; color: #e0e0e0; }"
                "QPushButton:hover { background-color: #4a4a4a; }"
                "QPushButton:pressed { background-color: #2a2a2a; }"
            ));
            card_layout->addWidget(btn_start, 0, Qt::AlignCenter);
            connect(btn_start, &QPushButton::clicked,
                    this, &MainDashboard::onScreenCardClicked);
        }

        // Card index 1 = Keyboard — checkbox "ไม่มีคีย์บอร์ด" + Start button
        if (i == 1) {
            m_chk_no_keyboard = new QCheckBox(QStringLiteral("ไม่มีคีย์บอร์ด"), card.frame);
            m_chk_no_keyboard->setStyleSheet(QStringLiteral(
                "QCheckBox { font-size: 11px; color: #cccccc; }"
                "QCheckBox::indicator { width: 13px; height: 13px; }"
            ));
            card_layout->addWidget(m_chk_no_keyboard, 0, Qt::AlignCenter);

            auto* btn_start = new QPushButton(QStringLiteral("Start"), card.frame);
            btn_start->setStyleSheet(QStringLiteral(
                "QPushButton { font-size: 11px; padding: 3px 10px;"
                " border: 1px solid #555; border-radius: 4px;"
                " background-color: #3a3a3a; color: #e0e0e0; }"
                "QPushButton:hover { background-color: #4a4a4a; }"
                "QPushButton:pressed { background-color: #2a2a2a; }"
            ));
            card_layout->addWidget(btn_start, 0, Qt::AlignCenter);
            connect(btn_start, &QPushButton::clicked,
                    this, &MainDashboard::onKeyboardCardClicked);
        }

        // Card index 2 = Touchpad — checkbox "ไม่มี Touchpad" + Start button
        if (i == 2) {
            m_chk_no_touchpad = new QCheckBox(QStringLiteral("ไม่มี Touchpad"), card.frame);
            m_chk_no_touchpad->setStyleSheet(QStringLiteral(
                "QCheckBox { font-size: 11px; color: #cccccc; }"
                "QCheckBox::indicator { width: 13px; height: 13px; }"
            ));
            card_layout->addWidget(m_chk_no_touchpad, 0, Qt::AlignCenter);

            auto* btn_start = new QPushButton(QStringLiteral("Start"), card.frame);
            btn_start->setStyleSheet(QStringLiteral(
                "QPushButton { font-size: 11px; padding: 3px 10px;"
                " border: 1px solid #555; border-radius: 4px;"
                " background-color: #3a3a3a; color: #e0e0e0; }"
                "QPushButton:hover { background-color: #4a4a4a; }"
                "QPushButton:pressed { background-color: #2a2a2a; }"
            ));
            card_layout->addWidget(btn_start, 0, Qt::AlignCenter);
            connect(btn_start, &QPushButton::clicked,
                    this, &MainDashboard::onTouchpadCardClicked);
        }

        // Card index 3 = Battery — checkbox "เป็น PC" + Start button
        if (i == 3) {
            m_chk_no_battery = new QCheckBox(QStringLiteral("เป็น PC / ไม่มีแบต"), card.frame);
            m_chk_no_battery->setStyleSheet(QStringLiteral(
                "QCheckBox { font-size: 11px; color: #cccccc; }"
                "QCheckBox::indicator { width: 13px; height: 13px; }"
            ));
            card_layout->addWidget(m_chk_no_battery, 0, Qt::AlignCenter);

            auto* btn_start = new QPushButton(QStringLiteral("Start"), card.frame);
            btn_start->setStyleSheet(QStringLiteral(
                "QPushButton { font-size: 11px; padding: 3px 10px;"
                " border: 1px solid #555; border-radius: 4px;"
                " background-color: #3a3a3a; color: #e0e0e0; }"
                "QPushButton:hover { background-color: #4a4a4a; }"
                "QPushButton:pressed { background-color: #2a2a2a; }"
            ));
            card_layout->addWidget(btn_start, 0, Qt::AlignCenter);
            connect(btn_start, &QPushButton::clicked,
                    this, &MainDashboard::onBatteryCardClicked);
        }

        // Card index 7 = Audio — add a Start button
        if (i == 7) {
            auto* btn_start = new QPushButton(QStringLiteral("Start"), card.frame);
            btn_start->setStyleSheet(QStringLiteral(
                "QPushButton { font-size: 11px; padding: 3px 10px;"
                " border: 1px solid #555; border-radius: 4px;"
                " background-color: #3a3a3a; color: #e0e0e0; }"
                "QPushButton:hover { background-color: #4a4a4a; }"
                "QPushButton:pressed { background-color: #2a2a2a; }"
            ));
            card_layout->addWidget(btn_start, 0, Qt::AlignCenter);
            connect(btn_start, &QPushButton::clicked,
                    this, &MainDashboard::onAudioCardClicked);
        }

        // Card index 9 = Physical — add a Start button
        if (i == 9) {
            auto* btn_start = new QPushButton(QStringLiteral("Start"), card.frame);
            btn_start->setStyleSheet(QStringLiteral(
                "QPushButton { font-size: 11px; padding: 3px 10px;"
                " border: 1px solid #555; border-radius: 4px;"
                " background-color: #3a3a3a; color: #e0e0e0; }"
                "QPushButton:hover { background-color: #4a4a4a; }"
                "QPushButton:pressed { background-color: #2a2a2a; }"
            ));
            card_layout->addWidget(btn_start, 0, Qt::AlignCenter);
            connect(btn_start, &QPushButton::clicked,
                    this, &MainDashboard::onPhysicalCardClicked);
        }

        // Card index 8 = Network — add a Start button
        if (i == 8) {
            auto* btn_start = new QPushButton(QStringLiteral("Start"), card.frame);
            btn_start->setStyleSheet(QStringLiteral(
                "QPushButton { font-size: 11px; padding: 3px 10px;"
                " border: 1px solid #555; border-radius: 4px;"
                " background-color: #3a3a3a; color: #e0e0e0; }"
                "QPushButton:hover { background-color: #4a4a4a; }"
                "QPushButton:pressed { background-color: #2a2a2a; }"
            ));
            card_layout->addWidget(btn_start, 0, Qt::AlignCenter);
            connect(btn_start, &QPushButton::clicked,
                    this, &MainDashboard::onNetworkCardClicked);
        }

        // Card index 5 = Temperature — add a Start button
        if (i == 5) {
            auto* btn_start = new QPushButton(QStringLiteral("Start"), card.frame);
            btn_start->setStyleSheet(QStringLiteral(
                "QPushButton { font-size: 11px; padding: 3px 10px;"
                " border: 1px solid #555; border-radius: 4px;"
                " background-color: #3a3a3a; color: #e0e0e0; }"
                "QPushButton:hover { background-color: #4a4a4a; }"
                "QPushButton:pressed { background-color: #2a2a2a; }"
            ));
            card_layout->addWidget(btn_start, 0, Qt::AlignCenter);
            connect(btn_start, &QPushButton::clicked,
                    this, &MainDashboard::onThermalCardClicked);
        }

        // Card index 4 = S.M.A.R.T. — add a Start button
        if (i == 4) {
            auto* btn_start = new QPushButton(QStringLiteral("Start"), card.frame);
            btn_start->setStyleSheet(QStringLiteral(
                "QPushButton { font-size: 11px; padding: 3px 10px;"
                " border: 1px solid #555; border-radius: 4px;"
                " background-color: #3a3a3a; color: #e0e0e0; }"
                "QPushButton:hover { background-color: #4a4a4a; }"
                "QPushButton:pressed { background-color: #2a2a2a; }"
            ));
            card_layout->addWidget(btn_start, 0, Qt::AlignCenter);
            connect(btn_start, &QPushButton::clicked,
                    this, &MainDashboard::onSmartCardClicked);
        }

        // Place in grid: 2 rows × 5 columns
        int col = i % 5;
        int grow = i / 5;
        modules_grid->addWidget(card.frame, grow, col);
    }

    // Make both grid rows share space equally
    modules_grid->setRowStretch(0, 1);
    modules_grid->setRowStretch(1, 1);
    for (int c = 0; c < 5; ++c) modules_grid->setColumnStretch(c, 1);

    root_layout->addWidget(modules_group, 1);

    // Status bar
    statusBar()->showMessage(
        QStringLiteral("Notebook Inspector  |  Windows 10/11 x64 only  |  Requires Administrator"));
}

// ---------------------------------------------------------------------------
// loadDeviceInfo
// Calls AutoScan synchronously (acceptable for Phase 1 startup).
// ---------------------------------------------------------------------------
void MainDashboard::loadDeviceInfo()
{
    statusBar()->showMessage(QStringLiteral("Scanning hardware…"));
    QApplication::processEvents();   // Let the window paint before blocking

    m_device_info = AutoScan::scan();

    statusBar()->showMessage(QStringLiteral("Hardware scan complete."), 5000);
}

// ---------------------------------------------------------------------------
// populateDeviceLabels
// ---------------------------------------------------------------------------
void MainDashboard::populateDeviceLabels()
{
    m_lbl_cpu->setText(
        m_device_info.cpu_name.isEmpty()
            ? QStringLiteral("N/A") : m_device_info.cpu_name
    );
    m_lbl_gpu->setText(
        m_device_info.gpu_name.isEmpty()
            ? QStringLiteral("N/A") : m_device_info.gpu_name
    );
    m_lbl_ram->setText(
        m_device_info.ram_gb > 0
            ? QStringLiteral("%1 GB").arg(m_device_info.ram_gb)
            : QStringLiteral("N/A")
    );
    m_lbl_serial->setText(
        m_device_info.serial_number.isEmpty()
            ? QStringLiteral("N/A") : m_device_info.serial_number
    );
    m_lbl_os->setText(
        m_device_info.os_version.isEmpty()
            ? QStringLiteral("N/A") : m_device_info.os_version
    );
}

// ---------------------------------------------------------------------------
// updateModuleCard
// ---------------------------------------------------------------------------
void MainDashboard::updateModuleCard(int index, const ModuleResult& result)
{
    if (index < 0 || index >= kModuleCount) return;

    ModuleCard& card = m_cards[static_cast<std::size_t>(index)];

    // Dot colour
    card.dot_label->setStyleSheet(
        QStringLiteral("font-size: 16px; color: %1; border: none;")
            .arg(statusColor(result.status))
    );

    // Status text
    QString status_text;
    switch (result.status) {
        case TestStatus::NotRun:  status_text = QStringLiteral("—");        break;
        case TestStatus::Running: status_text = QStringLiteral("Running…"); break;
        case TestStatus::Pass:    status_text = QStringLiteral("Pass");     break;
        case TestStatus::Fail:    status_text = QStringLiteral("Fail");     break;
        case TestStatus::Skipped: status_text = QStringLiteral("Skipped");  break;
    }
    if (!result.summary.isEmpty()) {
        status_text = result.summary;
    }
    card.status_label->setText(status_text);

    // Card border colour reflects status
    const QString border_color = statusColor(result.status);
    card.frame->setStyleSheet(
        QStringLiteral(
            "QFrame { border: 1px solid %1; border-radius: 6px;"
            " background-color: #2b2b2b; }"
        ).arg(border_color)
    );

    // Tooltip with issues list
    if (!result.issues.isEmpty()) {
        card.frame->setToolTip(result.issues.join(QStringLiteral("\n")));
    } else {
        card.frame->setToolTip(QString{});
    }
}

// ---------------------------------------------------------------------------
// statusColor
// ---------------------------------------------------------------------------
QString MainDashboard::statusColor(TestStatus status)
{
    switch (status) {
        case TestStatus::NotRun:  return QStringLiteral("#888888");  // gray
        case TestStatus::Running: return QStringLiteral("#f0c040");  // amber
        case TestStatus::Pass:    return QStringLiteral("#4caf50");  // green
        case TestStatus::Fail:    return QStringLiteral("#f44336");  // red
        case TestStatus::Skipped: return QStringLiteral("#5c6bc0");  // blue-gray
    }
    return QStringLiteral("#888888");
}

// ---------------------------------------------------------------------------
// statusDot
// ---------------------------------------------------------------------------
QString MainDashboard::statusDot(TestStatus /*status*/)
{
    return QStringLiteral("●");
}

// ---------------------------------------------------------------------------
// onScreenCardClicked
// ---------------------------------------------------------------------------
void MainDashboard::onScreenCardClicked()
{
    const bool has_touch = m_chk_touchscreen->isChecked();
    auto* w = new ScreenWindow(has_touch, this);
    connect(w, &ScreenWindow::finished,
            this, &MainDashboard::onScreenFinished);
    w->setAttribute(Qt::WA_DeleteOnClose);
    w->show();
}

// ---------------------------------------------------------------------------
// onScreenFinished
// ---------------------------------------------------------------------------
void MainDashboard::onScreenFinished(nbi::ModuleResult result)
{
    updateModuleCard(0, result);
}

// ---------------------------------------------------------------------------
// onSmartCardClicked
// ---------------------------------------------------------------------------
void MainDashboard::onSmartCardClicked()
{
    auto* w = new StorageWindow(this);
    connect(w, &StorageWindow::finished,
            this, &MainDashboard::onSmartFinished);
    w->setAttribute(Qt::WA_DeleteOnClose);
    w->show();
}

void MainDashboard::onSmartFinished(nbi::ModuleResult result)
{
    updateModuleCard(4, result);
}

// ---------------------------------------------------------------------------
// onBatteryCardClicked
// ---------------------------------------------------------------------------
void MainDashboard::onBatteryCardClicked()
{
    if (m_chk_no_battery && m_chk_no_battery->isChecked()) {
        ModuleResult r;
        r.label   = QStringLiteral("Battery");
        r.status  = TestStatus::Skipped;
        r.summary = QStringLiteral("เป็น PC / ไม่มีแบต");
        onBatteryFinished(r);
        return;
    }

    auto* w = new BatteryWindow(this);
    connect(w, &BatteryWindow::finished, this, &MainDashboard::onBatteryFinished);
    w->setAttribute(Qt::WA_DeleteOnClose);
    w->show();
}

void MainDashboard::onBatteryFinished(nbi::ModuleResult result)
{
    updateModuleCard(3, result);
}

// ---------------------------------------------------------------------------
// onPhysicalCardClicked
// ---------------------------------------------------------------------------
void MainDashboard::onPhysicalCardClicked()
{
    auto* w = new PhysicalChecklistWindow(this);
    connect(w, &PhysicalChecklistWindow::finished, this, &MainDashboard::onPhysicalFinished);
    w->setAttribute(Qt::WA_DeleteOnClose);
    w->show();
}

void MainDashboard::onPhysicalFinished(nbi::ModuleResult result)
{
    updateModuleCard(9, result);
}

// ---------------------------------------------------------------------------
// onAudioCardClicked
// ---------------------------------------------------------------------------
void MainDashboard::onAudioCardClicked()
{
    auto* w = new AudioWindow(this);
    connect(w, &AudioWindow::finished, this, &MainDashboard::onAudioFinished);
    w->setAttribute(Qt::WA_DeleteOnClose);
    w->show();
}

void MainDashboard::onAudioFinished(nbi::ModuleResult result)
{
    updateModuleCard(7, result);
}

// ---------------------------------------------------------------------------
// onNetworkCardClicked
// ---------------------------------------------------------------------------
void MainDashboard::onNetworkCardClicked()
{
    auto* w = new NetworkWindow(this);
    connect(w, &NetworkWindow::finished,
            this, &MainDashboard::onNetworkFinished);
    w->setAttribute(Qt::WA_DeleteOnClose);
    w->show();
}

void MainDashboard::onNetworkFinished(nbi::ModuleResult result)
{
    updateModuleCard(8, result);
}

// ---------------------------------------------------------------------------
// onThermalCardClicked
// ---------------------------------------------------------------------------
void MainDashboard::onThermalCardClicked()
{
    auto* w = new ThermalWindow(this);
    connect(w, &ThermalWindow::finished, this, &MainDashboard::onThermalFinished);
    w->setAttribute(Qt::WA_DeleteOnClose);
    w->show();
}

void MainDashboard::onThermalFinished(nbi::ModuleResult result)
{
    updateModuleCard(5, result);
}

// ---------------------------------------------------------------------------
// onTouchpadCardClicked
// ---------------------------------------------------------------------------
void MainDashboard::onTouchpadCardClicked()
{
    if (m_chk_no_touchpad && m_chk_no_touchpad->isChecked()) {
        ModuleResult r;
        r.label   = QStringLiteral("Touchpad");
        r.status  = TestStatus::Skipped;
        r.summary = QStringLiteral("ไม่มี Touchpad");
        onTouchpadFinished(r);
        return;
    }

    auto* w = new TouchpadWindow(this);
    connect(w, &TouchpadWindow::finished, this, &MainDashboard::onTouchpadFinished);
    w->setAttribute(Qt::WA_DeleteOnClose);
    w->show();
}

void MainDashboard::onTouchpadFinished(nbi::ModuleResult result)
{
    updateModuleCard(2, result);
}

// ---------------------------------------------------------------------------
// onKeyboardCardClicked
// ---------------------------------------------------------------------------
void MainDashboard::onKeyboardCardClicked()
{
    if (m_chk_no_keyboard && m_chk_no_keyboard->isChecked()) {
        ModuleResult r;
        r.label   = QStringLiteral("Keyboard");
        r.status  = TestStatus::Skipped;
        r.summary = QStringLiteral("ไม่มีคีย์บอร์ด");
        onKeyboardFinished(r);
        return;
    }

    auto* w = new KeyboardWindow(this);
    connect(w, &KeyboardWindow::finished,
            this, &MainDashboard::onKeyboardFinished);
    w->setAttribute(Qt::WA_DeleteOnClose);
    w->show();
}

// ---------------------------------------------------------------------------
// onKeyboardFinished
// ---------------------------------------------------------------------------
void MainDashboard::onKeyboardFinished(nbi::ModuleResult result)
{
    updateModuleCard(1, result);
}

} // namespace nbi
