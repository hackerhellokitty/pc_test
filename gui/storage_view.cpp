// ---------------------------------------------------------------------------
// gui/storage_view.cpp
// ---------------------------------------------------------------------------

#include "gui/storage_view.hpp"

#include <QApplication>
#include <QFont>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QSizePolicy>

#include "core/storage_manager.hpp"

namespace nbi {

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------
StorageView::StorageView(QWidget* parent)
    : QWidget(parent)
{
    buildUi();
}

// ---------------------------------------------------------------------------
// buildUi
// ---------------------------------------------------------------------------
void StorageView::buildUi()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(16, 16, 16, 16);
    root->setSpacing(12);

    // Title
    auto* lbl_title = new QLabel(QStringLiteral("S.M.A.R.T. Storage Health"), this);
    {
        QFont f = lbl_title->font();
        f.setBold(true);
        f.setPointSize(15);
        lbl_title->setFont(f);
    }
    root->addWidget(lbl_title);

    // Status line
    m_lbl_status = new QLabel(
        QStringLiteral("กด Scan เพื่อเริ่มอ่าน SMART data"), this);
    m_lbl_status->setStyleSheet(QStringLiteral("color: #aaaaaa; font-size: 12px;"));
    root->addWidget(m_lbl_status);

    // Scroll area for drive cards
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

    // Bottom bar
    auto* bottom = new QHBoxLayout();
    bottom->setSpacing(8);

    m_btn_scan = new QPushButton(QStringLiteral("Scan"), this);
    m_btn_scan->setFixedWidth(100);
    m_btn_scan->setStyleSheet(QStringLiteral(
        "QPushButton { font-size: 12px; padding: 5px 12px;"
        " border: 1px solid #555; border-radius: 4px;"
        " background-color: #3a3a3a; color: #e0e0e0; }"
        "QPushButton:hover { background-color: #4a4a4a; }"
        "QPushButton:disabled { color: #666; }"
    ));

    m_btn_done = new QPushButton(QStringLiteral("Done"), this);
    m_btn_done->setFixedWidth(100);
    m_btn_done->setStyleSheet(m_btn_scan->styleSheet());

    bottom->addStretch(1);
    bottom->addWidget(m_btn_scan);
    bottom->addWidget(m_btn_done);
    root->addLayout(bottom);

    connect(m_btn_scan, &QPushButton::clicked, this, &StorageView::onScanClicked);
    connect(m_btn_done, &QPushButton::clicked, this, &StorageView::onDoneClicked);
}

// ---------------------------------------------------------------------------
// makeDriveCard
// ---------------------------------------------------------------------------
QWidget* StorageView::makeDriveCard(const DriveInfo& d)
{
    auto* card = new QFrame(m_results_container);
    card->setFrameShape(QFrame::StyledPanel);

    const QString border_color = d.flagged
        ? QStringLiteral("#f44336")
        : (d.smart_available ? QStringLiteral("#4caf50") : QStringLiteral("#888888"));

    card->setStyleSheet(
        QStringLiteral("QFrame { border: 1px solid %1; border-radius: 6px;"
                        " background-color: #2b2b2b; padding: 8px; }")
            .arg(border_color)
    );

    auto* layout = new QVBoxLayout(card);
    layout->setSpacing(6);

    // Header row: drive name + status badge
    auto* header_row = new QHBoxLayout();

    const QString drive_label = d.model.isEmpty()
        ? QStringLiteral("PhysicalDrive%1").arg(d.index)
        : QStringLiteral("Drive %1 — %2").arg(d.index).arg(d.model);

    auto* lbl_name = new QLabel(drive_label, card);
    {
        QFont f = lbl_name->font();
        f.setBold(true);
        f.setPointSize(12);
        lbl_name->setFont(f);
        lbl_name->setStyleSheet(QStringLiteral("color: #e0e0e0; border: none;"));
    }

    const QString badge_text = d.flagged
        ? QStringLiteral("⚠ WARNING")
        : (d.smart_available ? QStringLiteral("✓ Healthy") : QStringLiteral("? No Data"));
    auto* lbl_badge = new QLabel(badge_text, card);
    lbl_badge->setStyleSheet(
        QStringLiteral("font-size: 11px; font-weight: bold; color: %1; border: none;")
            .arg(border_color)
    );

    header_row->addWidget(lbl_name);
    header_row->addStretch(1);
    header_row->addWidget(lbl_badge);
    layout->addLayout(header_row);

    // Info row: bus + size + serial
    auto* lbl_info = new QLabel(card);
    {
        const QString bus_str = (d.bus == BusType::NVMe) ? QStringLiteral("NVMe")
                              : (d.bus == BusType::Sata) ? QStringLiteral("SATA")
                              : QStringLiteral("Unknown");
        const double gb = static_cast<double>(d.size_bytes) / (1024.0 * 1024.0 * 1024.0);
        const QString size_str = d.size_bytes > 0
            ? QStringLiteral("%1 GB").arg(gb, 0, 'f', 0)
            : QStringLiteral("? GB");
        const QString serial_str = d.serial.isEmpty()
            ? QStringLiteral("S/N: —")
            : QStringLiteral("S/N: %1").arg(d.serial);

        lbl_info->setText(
            QStringLiteral("%1  •  %2  •  %3").arg(bus_str, size_str, serial_str)
        );
        lbl_info->setStyleSheet(QStringLiteral("color: #888888; font-size: 11px; border: none;"));
    }
    layout->addWidget(lbl_info);

    // Critical SMART attributes
    auto* grid = new QGridLayout();
    grid->setHorizontalSpacing(20);
    grid->setVerticalSpacing(4);

    auto add_attr = [&](int row, const QString& name, uint32_t val, bool critical) {
        auto* k = new QLabel(name, card);
        k->setStyleSheet(QStringLiteral("color: #888888; font-size: 11px; border: none;"));

        const bool bad = critical && val != 0;
        auto* v = new QLabel(QString::number(val), card);
        v->setStyleSheet(
            QStringLiteral("font-size: 11px; font-weight: %1; color: %2; border: none;")
                .arg(bad ? QStringLiteral("bold") : QStringLiteral("normal"))
                .arg(bad ? QStringLiteral("#f44336") : QStringLiteral("#e0e0e0"))
        );
        grid->addWidget(k, row, 0);
        grid->addWidget(v, row, 1);
    };

    if (d.bus == BusType::NVMe || d.bus == BusType::Other || d.bus == BusType::Unknown) {
        add_attr(0, QStringLiteral("Media Errors"),   d.media_errors,  true);
        add_attr(1, QStringLiteral("% Lifetime Used"), d.percent_used, d.percent_used >= 90);
        if (!d.health_status.isEmpty()) {
            add_attr(2, QStringLiteral("Health Status (WMI)"), 0, false);
            // Override value label with text
            auto* v_text = new QLabel(d.health_status, card);
            v_text->setStyleSheet(QStringLiteral(
                "font-size: 11px; color: %1; border: none;")
                .arg(d.flagged ? QStringLiteral("#f44336") : QStringLiteral("#4caf50")));
            grid->addWidget(v_text, 2, 1);
        }
    } else {
        add_attr(0, QStringLiteral("Reallocated Sectors (05)"),    d.reallocated,   true);
        add_attr(1, QStringLiteral("Pending Sectors (C5)"),        d.pending,       true);
        add_attr(2, QStringLiteral("Uncorrectable Sectors (C6)"),  d.uncorrectable, true);
    }

    layout->addLayout(grid);

    // Error detail if flagged
    if (d.flagged && !d.error_detail.isEmpty()) {
        auto* lbl_err = new QLabel(
            QStringLiteral("⚠ %1").arg(d.error_detail), card);
        lbl_err->setStyleSheet(
            QStringLiteral("color: #f44336; font-size: 11px; border: none;"));
        lbl_err->setWordWrap(true);
        layout->addWidget(lbl_err);
    }

    return card;
}

// ---------------------------------------------------------------------------
// populateResults
// ---------------------------------------------------------------------------
void StorageView::populateResults(const std::vector<DriveInfo>& drives)
{
    // Remove old cards
    while (m_results_layout->count() > 0) {
        auto* item = m_results_layout->takeAt(0);
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }

    if (drives.empty()) {
        auto* lbl = new QLabel(QStringLiteral("ไม่พบ drive"), m_results_container);
        lbl->setStyleSheet(QStringLiteral("color: #888; font-size: 12px;"));
        m_results_layout->addWidget(lbl);
        return;
    }

    for (std::size_t i = 0; i < drives.size(); ++i)
        m_results_layout->addWidget(makeDriveCard(drives[i]));
}

// ---------------------------------------------------------------------------
// Slots
// ---------------------------------------------------------------------------
void StorageView::onScanClicked()
{
    m_btn_scan->setEnabled(false);
    m_lbl_status->setText(QStringLiteral("กำลังอ่าน SMART…"));
    QApplication::processEvents();

    m_drives = StorageManager::scan();

    populateResults(m_drives);

    const ModuleResult r = StorageManager::evaluate(m_drives);
    m_lbl_status->setText(
        QStringLiteral("สแกนเสร็จ — %1").arg(r.summary)
    );
    m_btn_scan->setEnabled(true);
}

void StorageView::onDoneClicked()
{
    emit finished(result());
}

// ---------------------------------------------------------------------------
// result
// ---------------------------------------------------------------------------
ModuleResult StorageView::result() const
{
    return StorageManager::evaluate(m_drives);
}

} // namespace nbi
