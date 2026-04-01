// ---------------------------------------------------------------------------
// gui/battery_view.cpp
// ---------------------------------------------------------------------------

#include "gui/battery_view.hpp"

#include <QApplication>
#include <QFont>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QProgressBar>
#include <QScrollArea>

namespace nbi {

BatteryView::BatteryView(QWidget* parent)
    : QWidget(parent)
{
    buildUi();
}

void BatteryView::buildUi()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(16, 16, 16, 16);
    root->setSpacing(12);

    auto* lbl_title = new QLabel(QStringLiteral("Battery Health"), this);
    {
        QFont f = lbl_title->font();
        f.setBold(true);
        f.setPointSize(15);
        lbl_title->setFont(f);
    }
    root->addWidget(lbl_title);

    auto* lbl_desc = new QLabel(
        QStringLiteral("อ่านความจุแบตเตอรี่จาก WMI — Wear Level = FullCharge / Design × 100"), this);
    lbl_desc->setStyleSheet(QStringLiteral("color: #aaaaaa; font-size: 12px;"));
    lbl_desc->setWordWrap(true);
    root->addWidget(lbl_desc);

    m_lbl_status = new QLabel(QStringLiteral("กด Scan เพื่ออ่านข้อมูลแบตเตอรี่"), this);
    m_lbl_status->setStyleSheet(QStringLiteral("color: #aaaaaa; font-size: 12px;"));
    root->addWidget(m_lbl_status);

    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet(QStringLiteral("background: transparent;"));

    m_results_container = new QWidget(scroll);
    m_results_container->setStyleSheet(QStringLiteral("background: transparent;"));
    m_results_layout = new QVBoxLayout(m_results_container);
    m_results_layout->setContentsMargins(0, 0, 0, 0);
    m_results_layout->setSpacing(10);
    m_results_layout->setAlignment(Qt::AlignTop);

    scroll->setWidget(m_results_container);
    root->addWidget(scroll, 1);

    const QString btn_style = QStringLiteral(
        "QPushButton { font-size: 12px; padding: 5px 12px;"
        " border: 1px solid #555; border-radius: 4px;"
        " background-color: #3a3a3a; color: #e0e0e0; }"
        "QPushButton:hover { background-color: #4a4a4a; }"
        "QPushButton:disabled { color: #666; }"
    );

    auto* bottom = new QHBoxLayout();
    m_btn_scan = new QPushButton(QStringLiteral("Scan"), this);
    m_btn_scan->setFixedWidth(100);
    m_btn_scan->setStyleSheet(btn_style);

    m_btn_done = new QPushButton(QStringLiteral("Done"), this);
    m_btn_done->setFixedWidth(100);
    m_btn_done->setStyleSheet(btn_style);

    bottom->addStretch(1);
    bottom->addWidget(m_btn_scan);
    bottom->addWidget(m_btn_done);
    root->addLayout(bottom);

    connect(m_btn_scan, &QPushButton::clicked, this, &BatteryView::onScanClicked);
    connect(m_btn_done, &QPushButton::clicked, this, &BatteryView::onDoneClicked);
}

QWidget* BatteryView::makeBatteryCard(const BatteryInfo& b)
{
    auto* card = new QFrame(m_results_container);
    card->setFrameShape(QFrame::StyledPanel);

    const QString border_color = b.flagged
        ? QStringLiteral("#f44336")
        : (b.wear_level >= 0 ? QStringLiteral("#4caf50") : QStringLiteral("#888888"));

    card->setStyleSheet(
        QStringLiteral("QFrame { border: 1px solid %1; border-radius: 6px;"
                        " background-color: #2b2b2b; padding: 8px; }").arg(border_color));

    auto* vl = new QVBoxLayout(card);
    vl->setSpacing(8);

    // Header: name + badge
    auto* header = new QHBoxLayout();

    auto* lbl_name = new QLabel(b.name, card);
    {
        QFont f = lbl_name->font();
        f.setBold(true);
        f.setPointSize(12);
        lbl_name->setFont(f);
        lbl_name->setStyleSheet(QStringLiteral("color: #e0e0e0; border: none;"));
    }

    const QString badge_text = b.flagged
        ? QStringLiteral("⚠ เสื่อม")
        : (b.wear_level >= 0 ? QStringLiteral("✓ ปกติ") : QStringLiteral("? ไม่มีข้อมูล"));
    auto* lbl_badge = new QLabel(badge_text, card);
    lbl_badge->setStyleSheet(
        QStringLiteral("font-size: 11px; font-weight: bold; color: %1; border: none;")
            .arg(border_color));

    header->addWidget(lbl_name);
    header->addStretch(1);
    header->addWidget(lbl_badge);
    vl->addLayout(header);

    // Wear level progress bar
    if (b.wear_level >= 0) {
        auto* bar_row = new QHBoxLayout();

        auto* lbl_bar_label = new QLabel(QStringLiteral("Wear Level"), card);
        lbl_bar_label->setStyleSheet(QStringLiteral("color: #888; font-size: 11px; border: none;"));
        lbl_bar_label->setFixedWidth(85);

        auto* bar = new QProgressBar(card);
        bar->setRange(0, 100);
        bar->setValue(b.wear_level);
        bar->setFixedHeight(12);
        bar->setTextVisible(false);

        const QString bar_color = b.wear_level < 60
            ? QStringLiteral("#f44336")
            : (b.wear_level < 80 ? QStringLiteral("#f0c040") : QStringLiteral("#4caf50"));

        bar->setStyleSheet(QStringLiteral(
            "QProgressBar { background-color: #3a3a3a; border: none; border-radius: 4px; }"
            "QProgressBar::chunk { background-color: %1; border-radius: 4px; }"
        ).arg(bar_color));

        auto* lbl_pct = new QLabel(QStringLiteral("%1%").arg(b.wear_level), card);
        lbl_pct->setFixedWidth(40);
        lbl_pct->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        lbl_pct->setStyleSheet(
            QStringLiteral("font-size: 12px; font-weight: bold; color: %1; border: none;")
                .arg(bar_color));

        bar_row->addWidget(lbl_bar_label);
        bar_row->addWidget(bar, 1);
        bar_row->addWidget(lbl_pct);
        vl->addLayout(bar_row);
    }

    // Stats grid
    auto* grid = new QGridLayout();
    grid->setHorizontalSpacing(20);
    grid->setVerticalSpacing(3);

    auto add_row = [&](int row, const QString& key, const QString& val) {
        auto* k = new QLabel(key, card);
        k->setStyleSheet(QStringLiteral("color: #888; font-size: 11px; border: none;"));
        auto* v = new QLabel(val, card);
        v->setStyleSheet(QStringLiteral("font-size: 11px; color: #e0e0e0; border: none;"));
        grid->addWidget(k, row, 0);
        grid->addWidget(v, row, 1);
    };

    auto mwh = [](uint32_t v) -> QString {
        return v > 0 ? QStringLiteral("%1 mWh").arg(v) : QStringLiteral("—");
    };

    add_row(0, QStringLiteral("Design Capacity"),     mwh(b.design_capacity));
    add_row(1, QStringLiteral("Full Charge Capacity"), mwh(b.full_charge_capacity));
    add_row(2, QStringLiteral("Cycle Count"),
        b.cycle_count > 0 ? QString::number(b.cycle_count) : QStringLiteral("—"));

    vl->addLayout(grid);

    if (!b.error_detail.isEmpty()) {
        auto* lbl_err = new QLabel(QStringLiteral("⚠ %1").arg(b.error_detail), card);
        lbl_err->setStyleSheet(QStringLiteral("color: #f0c040; font-size: 11px; border: none;"));
        lbl_err->setWordWrap(true);
        vl->addWidget(lbl_err);
    }

    return card;
}

void BatteryView::populateResults(const BatteryResult& r)
{
    while (m_results_layout->count() > 0) {
        auto* item = m_results_layout->takeAt(0);
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }

    if (r.no_battery) {
        auto* lbl = new QLabel(
            QStringLiteral("ไม่พบแบตเตอรี่ในระบบ"), m_results_container);
        lbl->setStyleSheet(QStringLiteral("color: #5c6bc0; font-size: 12px;"));
        m_results_layout->addWidget(lbl);
        return;
    }

    for (const auto& b : r.batteries)
        m_results_layout->addWidget(makeBatteryCard(b));
}

void BatteryView::onScanClicked()
{
    m_btn_scan->setEnabled(false);
    m_lbl_status->setText(QStringLiteral("กำลังอ่านข้อมูลแบตเตอรี่…"));
    QApplication::processEvents();

    m_last_result = BatteryManager::scan();
    m_scanned = true;

    populateResults(m_last_result);

    const ModuleResult mr = BatteryManager::evaluate(m_last_result);
    m_lbl_status->setText(
        QStringLiteral("สแกนเสร็จ — %1").arg(mr.summary));
    m_btn_scan->setEnabled(true);
}

void BatteryView::onDoneClicked()
{
    emit finished(result());
}

ModuleResult BatteryView::result() const
{
    if (!m_scanned) {
        ModuleResult r;
        r.label  = QStringLiteral("Battery");
        r.status = TestStatus::NotRun;
        return r;
    }
    return BatteryManager::evaluate(m_last_result);
}

} // namespace nbi
