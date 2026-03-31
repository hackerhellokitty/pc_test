#pragma once

// ---------------------------------------------------------------------------
// gui/screen_window.hpp
// Top-level window wrapping ScreenTestView + (optional) TouchCanvasView.
//
// Flow when has_touch == true:
//   Page 0: ScreenTestView  →  "Done" advances to page 1
//   Page 1: TouchCanvasView →  "Done" emits finished() and closes
//
// Flow when has_touch == false:
//   Page 0: ScreenTestView  →  "Done" emits finished() and closes immediately
// ---------------------------------------------------------------------------

#include <QStackedWidget>
#include <QWidget>
#include "common/types.hpp"

namespace nbi {

class ScreenTestView;
class TouchCanvasView;

class ScreenWindow : public QWidget {
    Q_OBJECT

public:
    explicit ScreenWindow(bool has_touch, QWidget* parent = nullptr);

signals:
    void finished(nbi::ModuleResult result);

private slots:
    void onColorTestDone(nbi::ModuleResult result);
    void onTouchDone();

private:
    QStackedWidget* m_stack{nullptr};
    ScreenTestView* m_color_view{nullptr};
    TouchCanvasView* m_touch_view{nullptr};

    bool         m_has_touch{false};
    ModuleResult m_color_result;   // saved while touch page is shown
};

} // namespace nbi
