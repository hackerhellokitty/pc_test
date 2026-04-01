#pragma once
// ---------------------------------------------------------------------------
// gui/summary_page.hpp
// Summary page: score, verdict, ordered problem list, Export button.
// ---------------------------------------------------------------------------
#include <array>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>
#include "common/types.hpp"
#include "core/scorer.hpp"

namespace nbi {

class SummaryPage : public QWidget {
    Q_OBJECT
public:
    explicit SummaryPage(QWidget* parent = nullptr);

    void setData(const DeviceInfo& device,
                 const std::array<ModuleResult, 10>& results);

signals:
    void exportRequested();   ///< user clicked Export Report

private:
    void buildUi();
    void populate(const DeviceInfo& device,
                  const std::array<ModuleResult, 10>& results,
                  const ScoreResult& score);

    QLabel*      m_lbl_verdict{nullptr};
    QLabel*      m_lbl_score{nullptr};
    QLabel*      m_lbl_cap{nullptr};
    QVBoxLayout* m_issues_layout{nullptr};
    QPushButton* m_btn_export{nullptr};
};

} // namespace nbi
