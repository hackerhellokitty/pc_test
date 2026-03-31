#pragma once

// ---------------------------------------------------------------------------
// gui/touch_canvas_view.hpp
// Touch canvas for touchscreen verification.
//
// User draws on the canvas with their finger.  The widget tracks strokes and
// computes a coverage percentage using a grid of cells (kGridCols × kGridRows).
// A cell is "covered" when at least one stroke point falls inside it.
// ---------------------------------------------------------------------------

#include <vector>

#include <QLabel>
#include <QPointF>
#include <QPushButton>
#include <QWidget>

namespace nbi {

class TouchCanvasView : public QWidget {
    Q_OBJECT

public:
    explicit TouchCanvasView(QWidget* parent = nullptr);

    /// Coverage 0.0–100.0 %
    float coveragePct() const;

    void reset();

    QSize minimumSizeHint() const override { return {400, 300}; }
    QSize sizeHint()        const override { return {700, 450}; }

signals:
    void coverageChanged(float pct);

protected:
    void paintEvent      (QPaintEvent*  event) override;
    bool event           (QEvent*       event) override;        // QTouchEvent
    void mouseMoveEvent  (QMouseEvent*  event) override;        // touchpad fallback
    void mousePressEvent (QMouseEvent*  event) override;

private:
    void addPoint(const QPointF& pt);
    void markCell(const QPointF& pt);
    void updateLabel();

    // Grid dimensions
    static constexpr int kGridCols = 20;
    static constexpr int kGridRows = 15;
    static constexpr int kTotalCells = kGridCols * kGridRows;

    // Stroke storage: each stroke is a list of points
    std::vector<std::vector<QPointF>> m_strokes;
    std::vector<QPointF>*             m_current_stroke{nullptr};

    // Coverage grid — true = at least one point in this cell
    bool m_cells[kGridRows][kGridCols]{};
    int  m_covered_cells{0};

    // UI
    QLabel*      m_lbl_coverage{nullptr};
    QPushButton* m_btn_reset{nullptr};
};

} // namespace nbi
