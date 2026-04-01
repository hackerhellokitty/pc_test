#pragma once

// ---------------------------------------------------------------------------
// gui/thermal_view.hpp
// ---------------------------------------------------------------------------

#include <QLabel>
#include <QPushButton>
#include <QProgressBar>
#include <QVBoxLayout>
#include <QWidget>

#include "common/types.hpp"
#include "core/thermal_manager.hpp"

namespace nbi {

class ThermalView : public QWidget {
    Q_OBJECT
public:
    explicit ThermalView(QWidget* parent = nullptr);
    ModuleResult result() const;

signals:
    void finished(nbi::ModuleResult result);

private slots:
    void onScanClicked();
    void onDoneClicked();

private:
    void buildUi();
    void populateResults(const ThermalResult& r);

    QLabel*      m_lbl_status{nullptr};
    QVBoxLayout* m_results_layout{nullptr};
    QWidget*     m_results_container{nullptr};
    QPushButton* m_btn_scan{nullptr};
    QPushButton* m_btn_done{nullptr};

    ThermalResult m_last_result;
    bool          m_scanned{false};
};

} // namespace nbi
