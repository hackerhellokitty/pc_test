// ---------------------------------------------------------------------------
// gui/touch_canvas_view.cpp
// ---------------------------------------------------------------------------

#include "gui/touch_canvas_view.hpp"

#include <QEvent>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QTouchEvent>
#include <QVBoxLayout>

namespace nbi {

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------
TouchCanvasView::TouchCanvasView(QWidget* parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_AcceptTouchEvents);
    setMouseTracking(false);

    // Layout: canvas fills the widget; thin bottom bar shows coverage
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // Spacer so the canvas area gets all the vertical space
    root->addStretch(1);

    // Bottom bar
    auto* bar = new QWidget(this);
    bar->setFixedHeight(36);
    bar->setStyleSheet(QStringLiteral("background: rgba(0,0,0,160); border-radius: 0px;"));

    auto* bar_layout = new QHBoxLayout(bar);
    bar_layout->setContentsMargins(12, 0, 12, 0);
    bar_layout->setSpacing(12);

    m_lbl_coverage = new QLabel(QStringLiteral("Coverage: 0%"), bar);
    m_lbl_coverage->setStyleSheet(QStringLiteral("color: white; font-size: 12px;"));

    m_btn_reset = new QPushButton(QStringLiteral("ล้างใหม่"), bar);
    m_btn_reset->setFixedWidth(80);
    m_btn_reset->setStyleSheet(QStringLiteral(
        "QPushButton { font-size: 11px; padding: 3px 8px;"
        " border: 1px solid #666; border-radius: 4px;"
        " background: rgba(60,60,60,200); color: white; }"
        "QPushButton:hover { background: rgba(90,90,90,220); }"
    ));

    bar_layout->addWidget(m_lbl_coverage);
    bar_layout->addStretch(1);
    bar_layout->addWidget(m_btn_reset);

    root->addWidget(bar);

    connect(m_btn_reset, &QPushButton::clicked, this, &TouchCanvasView::reset);
}

// ---------------------------------------------------------------------------
// coveragePct
// ---------------------------------------------------------------------------
float TouchCanvasView::coveragePct() const
{
    return static_cast<float>(m_covered_cells) * 100.0f / kTotalCells;
}

// ---------------------------------------------------------------------------
// reset
// ---------------------------------------------------------------------------
void TouchCanvasView::reset()
{
    m_strokes.clear();
    m_current_stroke = nullptr;
    m_covered_cells  = 0;
    std::fill(&m_cells[0][0], &m_cells[0][0] + kTotalCells, false);
    updateLabel();
    update();
}

// ---------------------------------------------------------------------------
// addPoint — append to current stroke and mark coverage cell
// ---------------------------------------------------------------------------
void TouchCanvasView::addPoint(const QPointF& pt)
{
    if (!m_current_stroke) return;
    m_current_stroke->push_back(pt);
    markCell(pt);
    update();
}

void TouchCanvasView::markCell(const QPointF& pt)
{
    // Canvas area height = widget height minus bottom bar (36px)
    const int canvas_h = height() - 36;
    if (canvas_h <= 0 || width() <= 0) return;

    const int col = static_cast<int>(pt.x() / width()    * kGridCols);
    const int row = static_cast<int>(pt.y() / canvas_h   * kGridRows);

    if (col < 0 || col >= kGridCols || row < 0 || row >= kGridRows) return;
    if (!m_cells[row][col]) {
        m_cells[row][col] = true;
        ++m_covered_cells;
        updateLabel();
        emit coverageChanged(coveragePct());
    }
}

void TouchCanvasView::updateLabel()
{
    m_lbl_coverage->setText(
        QStringLiteral("Coverage: %1%  (%2/%3 cells)")
            .arg(static_cast<int>(coveragePct()))
            .arg(m_covered_cells)
            .arg(kTotalCells)
    );
}

// ---------------------------------------------------------------------------
// paintEvent
// ---------------------------------------------------------------------------
void TouchCanvasView::paintEvent(QPaintEvent* event)
{
    QWidget::paintEvent(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const int canvas_h = height() - 36;

    // Draw faint grid
    painter.setPen(QPen(QColor(255, 255, 255, 25), 0.5));
    const float cell_w = static_cast<float>(width())    / kGridCols;
    const float cell_h = static_cast<float>(canvas_h)   / kGridRows;
    for (int c = 1; c < kGridCols; ++c)
        painter.drawLine(QPointF(c * cell_w, 0), QPointF(c * cell_w, canvas_h));
    for (int r = 1; r < kGridRows; ++r)
        painter.drawLine(QPointF(0, r * cell_h), QPointF(width(), r * cell_h));

    // Highlight covered cells
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(42, 130, 218, 60));
    for (int r = 0; r < kGridRows; ++r) {
        for (int c = 0; c < kGridCols; ++c) {
            if (m_cells[r][c]) {
                painter.drawRect(QRectF(
                    c * cell_w, r * cell_h, cell_w, cell_h
                ));
            }
        }
    }

    // Draw strokes
    painter.setPen(QPen(QColor(255, 220, 80), 2.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(Qt::NoBrush);
    for (const auto& stroke : m_strokes) {
        if (stroke.size() < 2) continue;
        QPainterPath path;
        path.moveTo(stroke.front());
        for (std::size_t i = 1; i < stroke.size(); ++i)
            path.lineTo(stroke[i]);
        painter.drawPath(path);
    }
}

// ---------------------------------------------------------------------------
// event — handle QTouchEvent (touchscreen)
// ---------------------------------------------------------------------------
bool TouchCanvasView::event(QEvent* event)
{
    switch (event->type()) {
        case QEvent::TouchBegin:
        case QEvent::TouchUpdate:
        case QEvent::TouchEnd: {
            auto* te = static_cast<QTouchEvent*>(event);
            for (const auto& point : te->points()) {
                if (event->type() == QEvent::TouchBegin
                    || point.state() == QEventPoint::Pressed)
                {
                    m_strokes.emplace_back();
                    m_current_stroke = &m_strokes.back();
                }
                if (m_current_stroke)
                    addPoint(point.position());
            }
            if (event->type() == QEvent::TouchEnd)
                m_current_stroke = nullptr;
            event->accept();
            return true;
        }
        default:
            return QWidget::event(event);
    }
}

// ---------------------------------------------------------------------------
// mousePressEvent / mouseMoveEvent — touchpad fallback
// ---------------------------------------------------------------------------
void TouchCanvasView::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        m_strokes.emplace_back();
        m_current_stroke = &m_strokes.back();
        addPoint(event->position());
    }
}

void TouchCanvasView::mouseMoveEvent(QMouseEvent* event)
{
    if (event->buttons() & Qt::LeftButton)
        addPoint(event->position());
}

} // namespace nbi
