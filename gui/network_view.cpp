// ---------------------------------------------------------------------------
// gui/network_view.cpp
// ---------------------------------------------------------------------------

#include "gui/network_view.hpp"

#include <QApplication>
#include <QFont>
#include <QFrame>
#include <QFutureWatcher>
#include <QHBoxLayout>
#include <QProgressBar>
#include <QScrollArea>
#include <QSizePolicy>
#include <QtConcurrent/QtConcurrent>

namespace nbi {

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------
NetworkView::NetworkView(QWidget* parent)
    : QWidget(parent)
{
    buildUi();
}

// ---------------------------------------------------------------------------
// buildUi
// ---------------------------------------------------------------------------
void NetworkView::buildUi()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(16, 16, 16, 16);
    root->setSpacing(12);

    auto* lbl_title = new QLabel(QStringLiteral("Network Connectivity"), this);
    {
        QFont f = lbl_title->font();
        f.setBold(true);
        f.setPointSize(15);
        lbl_title->setFont(f);
    }
    root->addWidget(lbl_title);

    auto* lbl_desc = new QLabel(
        QStringLiteral("ตรวจสอบ NIC ที่เชื่อมต่ออยู่, สัญญาณ Wi-Fi รอบข้าง และ ping อินเทอร์เน็ต"), this);
    lbl_desc->setStyleSheet(QStringLiteral("color: #aaaaaa; font-size: 12px;"));
    lbl_desc->setWordWrap(true);
    root->addWidget(lbl_desc);

    m_lbl_status = new QLabel(
        QStringLiteral("กด Scan เพื่อเริ่มตรวจสอบเครือข่าย"), this);
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
    bottom->setSpacing(8);

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

    connect(m_btn_scan, &QPushButton::clicked, this, &NetworkView::onScanClicked);
    connect(m_btn_done, &QPushButton::clicked, this, &NetworkView::onDoneClicked);
}

// ---------------------------------------------------------------------------
// makeAdapterCard
// ---------------------------------------------------------------------------
QWidget* NetworkView::makeAdapterCard(const AdapterInfo& ai)
{
    auto* card = new QFrame(m_results_container);
    card->setFrameShape(QFrame::StyledPanel);
    card->setStyleSheet(QStringLiteral(
        "QFrame { border: 1px solid #4caf50; border-radius: 6px;"
        " background-color: #2b2b2b; padding: 8px; }"
    ));

    auto* layout = new QVBoxLayout(card);
    layout->setSpacing(4);

    auto* lbl_name = new QLabel(ai.name, card);
    {
        QFont f = lbl_name->font();
        f.setBold(true);
        f.setPointSize(12);
        lbl_name->setFont(f);
        lbl_name->setStyleSheet(QStringLiteral("color: #e0e0e0; border: none;"));
    }
    layout->addWidget(lbl_name);

    auto* lbl_info = new QLabel(
        QStringLiteral("%1  •  IP: %2").arg(ai.type, ai.ip_address), card);
    lbl_info->setStyleSheet(QStringLiteral("color: #888888; font-size: 11px; border: none;"));
    layout->addWidget(lbl_info);

    return card;
}

// ---------------------------------------------------------------------------
// makePingRow
// ---------------------------------------------------------------------------
QWidget* NetworkView::makePingRow(const PingResult& pr)
{
    auto* row = new QFrame(m_results_container);
    row->setFrameShape(QFrame::NoFrame);
    row->setStyleSheet(QStringLiteral("background: transparent; border: none;"));

    auto* hl = new QHBoxLayout(row);
    hl->setContentsMargins(4, 0, 4, 0);
    hl->setSpacing(8);

    const QString icon       = pr.reachable ? QStringLiteral("✓") : QStringLiteral("✗");
    const QString icon_color = pr.reachable ? QStringLiteral("#4caf50") : QStringLiteral("#f44336");

    auto* lbl_icon = new QLabel(icon, row);
    lbl_icon->setStyleSheet(
        QStringLiteral("color: %1; font-size: 14px; font-weight: bold;").arg(icon_color));
    lbl_icon->setFixedWidth(20);

    auto* lbl_host = new QLabel(pr.host, row);
    lbl_host->setStyleSheet(QStringLiteral("color: #e0e0e0; font-size: 12px;"));

    const QString rtt_text = pr.reachable
        ? QStringLiteral("%1 ms").arg(pr.rtt_ms)
        : QStringLiteral("timeout");

    auto* lbl_rtt = new QLabel(rtt_text, row);
    lbl_rtt->setStyleSheet(QStringLiteral("color: #888888; font-size: 12px;"));
    lbl_rtt->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    hl->addWidget(lbl_icon);
    hl->addWidget(lbl_host, 1);
    hl->addWidget(lbl_rtt);

    return row;
}

// ---------------------------------------------------------------------------
// makeWifiNetworkRow  — one SSID row with signal bar
// ---------------------------------------------------------------------------
QWidget* NetworkView::makeWifiNetworkRow(const WifiNetwork& wn)
{
    auto* row = new QFrame(m_results_container);
    row->setFrameShape(QFrame::NoFrame);

    const bool is_connected = wn.connected;
    const QString row_bg = is_connected
        ? QStringLiteral("background-color: #1e3a1e; border: 1px solid #4caf50; border-radius: 4px;")
        : QStringLiteral("background: transparent; border: none;");
    row->setStyleSheet(row_bg);

    auto* hl = new QHBoxLayout(row);
    hl->setContentsMargins(6, 4, 6, 4);
    hl->setSpacing(8);

    // Lock icon
    auto* lbl_lock = new QLabel(wn.secured ? QStringLiteral("🔒") : QStringLiteral("🔓"), row);
    lbl_lock->setFixedWidth(18);
    lbl_lock->setStyleSheet(QStringLiteral("font-size: 11px; border: none; background: transparent;"));

    // SSID
    const QString ssid_text = is_connected
        ? QStringLiteral("▶ %1").arg(wn.ssid)
        : wn.ssid;
    auto* lbl_ssid = new QLabel(ssid_text, row);
    lbl_ssid->setStyleSheet(
        QStringLiteral("font-size: 12px; color: %1; border: none; background: transparent;")
            .arg(is_connected ? QStringLiteral("#4caf50") : QStringLiteral("#e0e0e0")));

    // Signal strength bar
    auto* bar = new QProgressBar(row);
    bar->setRange(0, 100);
    bar->setValue(wn.signal_quality);
    bar->setFixedWidth(80);
    bar->setFixedHeight(10);
    bar->setTextVisible(false);

    // Color bar by signal quality
    QString bar_color;
    if (wn.signal_quality >= 67)
        bar_color = QStringLiteral("#4caf50");  // good — green
    else if (wn.signal_quality >= 34)
        bar_color = QStringLiteral("#f0c040");  // fair — amber
    else
        bar_color = QStringLiteral("#f44336");  // poor — red

    bar->setStyleSheet(QStringLiteral(
        "QProgressBar { background-color: #3a3a3a; border: none; border-radius: 3px; }"
        "QProgressBar::chunk { background-color: %1; border-radius: 3px; }"
    ).arg(bar_color));

    // Signal % label
    auto* lbl_sig = new QLabel(QStringLiteral("%1%").arg(wn.signal_quality), row);
    lbl_sig->setFixedWidth(36);
    lbl_sig->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    lbl_sig->setStyleSheet(QStringLiteral(
        "font-size: 11px; color: #888888; border: none; background: transparent;"));

    hl->addWidget(lbl_lock);
    hl->addWidget(lbl_ssid, 1);
    hl->addWidget(bar);
    hl->addWidget(lbl_sig);

    return row;
}

// ---------------------------------------------------------------------------
// makeWifiSection
// ---------------------------------------------------------------------------
QWidget* NetworkView::makeWifiSection(const WifiInfo& wifi)
{
    auto* container = new QWidget(m_results_container);
    container->setStyleSheet(QStringLiteral("background: transparent;"));
    auto* vl = new QVBoxLayout(container);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(6);

    // Section header
    auto* lbl_header = new QLabel(QStringLiteral("Wi-Fi"), container);
    {
        QFont f = lbl_header->font();
        f.setBold(true);
        f.setPointSize(11);
        lbl_header->setFont(f);
        lbl_header->setStyleSheet(QStringLiteral("color: #cccccc;"));
    }
    vl->addWidget(lbl_header);

    if (!wifi.available) {
        auto* lbl = new QLabel(QStringLiteral("ไม่พบ Wi-Fi adapter"), container);
        lbl->setStyleSheet(QStringLiteral("color: #888; font-size: 12px;"));
        vl->addWidget(lbl);
        return container;
    }

    // Adapter name + radio state
    const QString radio_text = wifi.radio_on
        ? QStringLiteral("✓ Radio เปิด")
        : QStringLiteral("✗ Radio ปิด / ไม่ได้เชื่อมต่อ");
    const QString radio_color = wifi.radio_on
        ? QStringLiteral("#4caf50") : QStringLiteral("#f44336");

    auto* lbl_adapter = new QLabel(
        QStringLiteral("%1  •  %2").arg(wifi.adapter_name, radio_text), container);
    lbl_adapter->setStyleSheet(
        QStringLiteral("font-size: 12px; color: %1;").arg(radio_color));
    vl->addWidget(lbl_adapter);

    // Connected SSID
    if (!wifi.connected_ssid.isEmpty()) {
        auto* lbl_conn = new QLabel(
            QStringLiteral("เชื่อมต่ออยู่กับ: %1").arg(wifi.connected_ssid), container);
        lbl_conn->setStyleSheet(QStringLiteral("font-size: 12px; color: #4caf50;"));
        vl->addWidget(lbl_conn);
    } else if (wifi.radio_on) {
        auto* lbl_conn = new QLabel(QStringLiteral("ยังไม่ได้เชื่อมต่อ"), container);
        lbl_conn->setStyleSheet(QStringLiteral("font-size: 12px; color: #f0c040;"));
        vl->addWidget(lbl_conn);
    }

    // Network list
    if (!wifi.networks.empty()) {
        auto* lbl_list = new QLabel(
            QStringLiteral("สัญญาณที่พบ (%1 เครือข่าย)").arg(wifi.networks.size()),
            container);
        lbl_list->setStyleSheet(QStringLiteral("font-size: 11px; color: #888888;"));
        vl->addWidget(lbl_list);

        // Show up to 15 networks to avoid overflow
        const std::size_t limit = std::min(wifi.networks.size(), std::size_t{15});
        for (std::size_t i = 0; i < limit; ++i)
            vl->addWidget(makeWifiNetworkRow(wifi.networks[i]));

        if (wifi.networks.size() > limit) {
            auto* lbl_more = new QLabel(
                QStringLiteral("... และอีก %1 เครือข่าย")
                    .arg(wifi.networks.size() - limit), container);
            lbl_more->setStyleSheet(QStringLiteral("font-size: 11px; color: #666;"));
            vl->addWidget(lbl_more);
        }
    } else if (wifi.radio_on) {
        auto* lbl = new QLabel(QStringLiteral("ไม่พบสัญญาณ Wi-Fi"), container);
        lbl->setStyleSheet(QStringLiteral("color: #888; font-size: 12px;"));
        vl->addWidget(lbl);
    }

    return container;
}

// ---------------------------------------------------------------------------
// populateResults
// ---------------------------------------------------------------------------
void NetworkView::populateResults(const NetworkResult& r)
{
    while (m_results_layout->count() > 0) {
        auto* item = m_results_layout->takeAt(0);
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }

    if (r.no_interface) {
        auto* lbl = new QLabel(
            QStringLiteral("ไม่พบ Network Interface — ข้ามโมดูลนี้"),
            m_results_container);
        lbl->setStyleSheet(QStringLiteral("color: #5c6bc0; font-size: 12px;"));
        m_results_layout->addWidget(lbl);
        return;
    }

    // --- Adapters ---
    auto* lbl_adapters = new QLabel(QStringLiteral("Network Adapters"), m_results_container);
    {
        QFont f = lbl_adapters->font();
        f.setBold(true);
        f.setPointSize(11);
        lbl_adapters->setFont(f);
        lbl_adapters->setStyleSheet(QStringLiteral("color: #cccccc;"));
    }
    m_results_layout->addWidget(lbl_adapters);

    for (const auto& ai : r.adapters)
        m_results_layout->addWidget(makeAdapterCard(ai));

    // --- Wi-Fi section ---
    m_results_layout->addWidget(makeWifiSection(r.wifi));

    // --- Ping ---
    auto* lbl_ping = new QLabel(QStringLiteral("Ping Tests"), m_results_container);
    {
        QFont f = lbl_ping->font();
        f.setBold(true);
        f.setPointSize(11);
        lbl_ping->setFont(f);
        lbl_ping->setStyleSheet(QStringLiteral("color: #cccccc;"));
    }
    m_results_layout->addWidget(lbl_ping);

    if (r.pings.empty()) {
        auto* lbl = new QLabel(QStringLiteral("ไม่มีเป้าหมาย ping"), m_results_container);
        lbl->setStyleSheet(QStringLiteral("color: #888; font-size: 12px;"));
        m_results_layout->addWidget(lbl);
    } else {
        for (const auto& pr : r.pings)
            m_results_layout->addWidget(makePingRow(pr));
    }
}

// ---------------------------------------------------------------------------
// Slots
// ---------------------------------------------------------------------------
void NetworkView::onScanClicked()
{
    m_btn_scan->setEnabled(false);
    m_btn_done->setEnabled(false);
    m_lbl_status->setText(QStringLiteral("กำลังตรวจสอบเครือข่าย…"));

    if (!m_watcher) {
        m_watcher = new QFutureWatcher<NetworkResult>(this);
        connect(m_watcher, &QFutureWatcher<NetworkResult>::finished,
                this, &NetworkView::onScanFinished);
    }

    m_watcher->setFuture(QtConcurrent::run([]() {
        return NetworkManager::scan();
    }));
}

void NetworkView::onScanFinished()
{
    m_last_result = m_watcher->result();
    m_scanned = true;

    populateResults(m_last_result);

    const ModuleResult mr = NetworkManager::evaluate(m_last_result);
    m_lbl_status->setText(
        QStringLiteral("ตรวจสอบเสร็จ — %1").arg(mr.summary));
    m_btn_scan->setEnabled(true);
    m_btn_done->setEnabled(true);
}

void NetworkView::onDoneClicked()
{
    emit finished(result());
}

// ---------------------------------------------------------------------------
// result
// ---------------------------------------------------------------------------
ModuleResult NetworkView::result() const
{
    if (!m_scanned) {
        ModuleResult r;
        r.label  = QStringLiteral("Network");
        r.status = TestStatus::NotRun;
        return r;
    }
    return NetworkManager::evaluate(m_last_result);
}

} // namespace nbi
