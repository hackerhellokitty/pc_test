#pragma once
// ---------------------------------------------------------------------------
// gui/summary_window.hpp
// Wrapper window for SummaryPage.
// ---------------------------------------------------------------------------
#include <array>
#include <QDialog>
#include "common/types.hpp"

namespace nbi {

class SummaryPage;

class SummaryWindow : public QDialog {
    Q_OBJECT
public:
    /// screen_widget: widget to grab for PNG (pass the dashboard/main window).
    explicit SummaryWindow(const DeviceInfo& device,
                           const std::array<ModuleResult, 10>& results,
                           QWidget* screen_widget,
                           QWidget* parent = nullptr);

private:
    SummaryPage* m_page{nullptr};
};

} // namespace nbi
