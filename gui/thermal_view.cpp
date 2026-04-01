// ---------------------------------------------------------------------------
// gui/thermal_view.cpp
// ---------------------------------------------------------------------------

#include "gui/thermal_view.hpp"

#include <QFont>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QScrollArea>

#include "core/thermal_manager.hpp"

namespace nbi {

ThermalView::ThermalView(QWidget* parent)
    : QWidget(parent)
{
    buildUi();
}

void ThermalView::buildUi()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(16, 16, 16, 16);
    root->setSpacing(12);

    auto* lbl_title = new QLabel(QStringLiteral("CPU Temperature"), this);
    {
        QFont f = lbl_title->font();
        f.setBold(true);
        f.setPointSize(15);
        lbl_title->setFont(f);
    }
    root->addWidget(lbl_title);

    m_lbl_status = new QLabel(
        QStringLiteral("กด Scan เพื่ออ่านอุณหภูมิ"), this);
    m_lbl_status->setStyleSheet(QStringLiteral("color: #aaaaaa; font-size: 12px;"));
    root->addWidget(m_lbl_status);

    // Scroll area for results
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    m_results_container = new QWidget(scroll);
    m_results_layout    = new QVBoxLayout(m_results_container);
    m_results_layout->setContentsMargins(0, 0, 0, 0);
    m_results_layout->setSpacing(8);
    m_results_layout->addStretch(1);
    scroll->setWidget(m_results_container);
    root->addWidget(scroll, 1);

    // Buttons
    auto* btn_row = new QHBoxLayout();
    m_btn_scan = new QPushButton(QStringLiteral("Scan"), this);
    m_btn_done = new QPushButton(QStringLiteral("Done"), this);
    m_btn_done->setEnabled(false);
    btn_row->addWidget(m_btn_scan);
    btn_row->addStretch(1);
    btn_row->addWidget(m_btn_done);
    root->addLayout(btn_row);

    connect(m_btn_scan, &QPushButton::clicked, this, &ThermalView::onScanClicked);
    connect(m_btn_done, &QPushButton::clicked, this, &ThermalView::onDoneClicked);
}

void ThermalView::onScanClicked()
{
    m_btn_scan->setEnabled(false);
    m_lbl_status->setText(QStringLiteral("กำลังอ่านอุณหภูมิ..."));

    m_last_result = ThermalManager::scan();
    m_scanned     = true;

    populateResults(m_last_result);

    if (m_last_result.sensor_missing) {
        m_lbl_status->setText(QStringLiteral("ไม่พบ thermal sensor — บางเครื่องไม่รองรับ"));
    } else {
        m_lbl_status->setText(
            QStringLiteral("CPU %1°C  |  พบ %2 zone")
                .arg(m_last_result.cpu_temp_c)
                .arg(m_last_result.zones.size()));
    }

    m_btn_done->setEnabled(true);
}

void ThermalView::onDoneClicked()
{
    emit finished(ThermalManager::evaluate(m_last_result));
}

ModuleResult ThermalView::result() const
{
    return ThermalManager::evaluate(m_last_result);
}

void ThermalView::populateResults(const ThermalResult& r)
{
    // Clear old widgets
    while (m_results_layout->count() > 1) {
        auto* item = m_results_layout->takeAt(0);
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }

    if (r.sensor_missing) {
        auto* lbl = new QLabel(
            QStringLiteral("ไม่รองรับ thermal sensor\n"
                           "เครื่องนี้ไม่ expose CPU temperature ผ่าน ACPI\n"
                           "ใช้ HWMonitor หรือ HWiNFO แทน"),
            m_results_container);
        lbl->setStyleSheet(QStringLiteral("color: #888888; font-size: 12px;"));
        lbl->setAlignment(Qt::AlignCenter);
        lbl->setWordWrap(true);
        m_results_layout->insertWidget(0, lbl);
        return;
    }

    // Card per zone
    for (const auto& z : r.zones) {
        auto* card = new QFrame(m_results_container);
        card->setFrameShape(QFrame::StyledPanel);

        const bool hot      = z.temp_c >= 95;
        const bool warm     = z.temp_c >= 75;
        const QString color = hot  ? QStringLiteral("#f44336")
                            : warm ? QStringLiteral("#ff9800")
                                   : QStringLiteral("#4caf50");

        card->setStyleSheet(
            QStringLiteral("QFrame { border: 1px solid %1; border-radius: 6px; "
                           "background: #1e1e1e; padding: 8px; }").arg(color));

        auto* lay = new QVBoxLayout(card);
        lay->setSpacing(6);

        // Zone name + temp
        auto* row = new QHBoxLayout();
        auto* lbl_name = new QLabel(z.name, card);
        {
            QFont f = lbl_name->font();
            f.setBold(true);
            lbl_name->setFont(f);
            lbl_name->setStyleSheet(QStringLiteral("color: #e0e0e0; border: none;"));
        }
        auto* lbl_temp = new QLabel(
            QStringLiteral("%1°C").arg(z.temp_c), card);
        lbl_temp->setStyleSheet(
            QStringLiteral("font-size: 20px; font-weight: bold; color: %1; border: none;")
                .arg(color));

        row->addWidget(lbl_name);
        row->addStretch(1);
        row->addWidget(lbl_temp);
        lay->addLayout(row);

        // Progress bar 0–120°C
        auto* bar = new QProgressBar(card);
        bar->setRange(0, 120);
        bar->setValue(qMin(z.temp_c, 120));
        bar->setTextVisible(false);
        bar->setFixedHeight(8);
        bar->setStyleSheet(
            QStringLiteral("QProgressBar { background: #333; border-radius: 4px; border: none; }"
                           "QProgressBar::chunk { background: %1; border-radius: 4px; }").arg(color));
        lay->addWidget(bar);

        // Status text + source tag
        const QString status = hot  ? QStringLiteral("ร้อนมาก — ตรวจสอบ thermal paste / พัดลม")
                             : warm ? QStringLiteral("อุณหภูมิสูง")
                                    : QStringLiteral("ปกติ");
        auto* lbl_status = new QLabel(status, card);
        lbl_status->setStyleSheet(
            QStringLiteral("color: %1; font-size: 11px; border: none;").arg(color));
        lay->addWidget(lbl_status);

        m_results_layout->insertWidget(m_results_layout->count() - 1, card);
    }
}

} // namespace nbi
