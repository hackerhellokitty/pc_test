#pragma once

// ---------------------------------------------------------------------------
// gui/touchpad_view.hpp
// Touchpad test — draw on canvas via touchpad (mouse events).
// Coverage % from grid, user confirms "ใช้งานได้" / "มีปัญหา".
// ---------------------------------------------------------------------------

#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

#include "common/types.hpp"
#include "gui/touch_canvas_view.hpp"

namespace nbi {

class TouchpadView : public QWidget {
    Q_OBJECT
public:
    explicit TouchpadView(QWidget* parent = nullptr);
    ModuleResult result() const;

signals:
    void finished(nbi::ModuleResult result);

private slots:
    void onCoverageChanged(float pct);
    void onPassClicked();
    void onFailClicked();

private:
    void buildUi();

    TouchCanvasView* m_canvas{nullptr};
    QLabel*          m_lbl_hint{nullptr};
    QLabel*          m_lbl_coverage{nullptr};
    QPushButton*     m_btn_pass{nullptr};
    QPushButton*     m_btn_fail{nullptr};

    float        m_coverage{0.0f};
    ModuleResult m_result;
};

} // namespace nbi
