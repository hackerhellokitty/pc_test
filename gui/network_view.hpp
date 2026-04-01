#pragma once

// ---------------------------------------------------------------------------
// gui/network_view.hpp
// ---------------------------------------------------------------------------

#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

#include "common/types.hpp"
#include "core/network_manager.hpp"

namespace nbi {

class NetworkView : public QWidget {
    Q_OBJECT

public:
    explicit NetworkView(QWidget* parent = nullptr);

    ModuleResult result() const;

signals:
    void finished(nbi::ModuleResult result);

private slots:
    void onScanClicked();
    void onDoneClicked();

private:
    void buildUi();
    void populateResults(const NetworkResult& r);
    QWidget* makeAdapterCard(const AdapterInfo& ai);
    QWidget* makePingRow(const PingResult& pr);
    QWidget* makeWifiSection(const WifiInfo& wifi);
    QWidget* makeWifiNetworkRow(const WifiNetwork& wn);

    QLabel*      m_lbl_status{nullptr};
    QVBoxLayout* m_results_layout{nullptr};
    QWidget*     m_results_container{nullptr};
    QPushButton* m_btn_scan{nullptr};
    QPushButton* m_btn_done{nullptr};

    NetworkResult m_last_result;
    bool          m_scanned{false};
};

} // namespace nbi
