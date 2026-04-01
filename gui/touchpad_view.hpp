#pragma once

// ---------------------------------------------------------------------------
// gui/touchpad_view.hpp
// Touchpad test:
//   - Draw zone: ลากนิ้วเพื่อทดสอบ tracking + coverage %
//   - Gesture zone: single/double/right click + scroll detection
// User confirms ใช้งานได้ / มีปัญหา at the end.
// ---------------------------------------------------------------------------

#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

#include "common/types.hpp"
#include "gui/touch_canvas_view.hpp"
#include "gui/gesture_test_widget.hpp"

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
    void onGestureDetected();
    void onPassClicked();
    void onFailClicked();

private:
    void buildUi();

    TouchCanvasView*  m_canvas{nullptr};
    GestureTestWidget* m_gestures{nullptr};
    QLabel*           m_lbl_hint{nullptr};
    QLabel*           m_lbl_coverage{nullptr};
    QLabel*           m_lbl_gesture_status{nullptr};
    QPushButton*      m_btn_pass{nullptr};
    QPushButton*      m_btn_fail{nullptr};

    float        m_coverage{0.0f};
    int          m_gesture_count{0};
    ModuleResult m_result;
};

} // namespace nbi
