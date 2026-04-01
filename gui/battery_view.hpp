#pragma once

// ---------------------------------------------------------------------------
// gui/battery_view.hpp
// ---------------------------------------------------------------------------

#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

#include "common/types.hpp"
#include "core/battery_manager.hpp"

namespace nbi {

class BatteryView : public QWidget {
    Q_OBJECT
public:
    explicit BatteryView(QWidget* parent = nullptr);
    ModuleResult result() const;

signals:
    void finished(nbi::ModuleResult result);

private slots:
    void onScanClicked();
    void onDoneClicked();

private:
    void buildUi();
    void populateResults(const BatteryResult& r);
    QWidget* makeBatteryCard(const BatteryInfo& b);

    QLabel*      m_lbl_status{nullptr};
    QVBoxLayout* m_results_layout{nullptr};
    QWidget*     m_results_container{nullptr};
    QPushButton* m_btn_scan{nullptr};
    QPushButton* m_btn_done{nullptr};

    BatteryResult m_last_result;
    bool          m_scanned{false};
};

} // namespace nbi
