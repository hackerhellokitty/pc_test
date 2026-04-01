// ---------------------------------------------------------------------------
// gui/gesture_test_widget.cpp
// ---------------------------------------------------------------------------

#include "gui/gesture_test_widget.hpp"

#include <QFont>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QVBoxLayout>
#include <QWheelEvent>

namespace nbi {

// ---------------------------------------------------------------------------
// GestureRow
// ---------------------------------------------------------------------------
GestureRow::GestureRow(const QString& label, QWidget* parent)
    : QWidget(parent)
{
    auto* lay = new QHBoxLayout(this);
    lay->setContentsMargins(8, 4, 8, 4);

    m_lbl_name = new QLabel(label, this);
    m_lbl_name->setStyleSheet(QStringLiteral("color: #cccccc; font-size: 12px;"));

    m_lbl_status = new QLabel(QStringLiteral("○  รอ"), this);
    m_lbl_status->setStyleSheet(QStringLiteral("color: #666666; font-size: 12px;"));
    m_lbl_status->setFixedWidth(80);
    m_lbl_status->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    lay->addWidget(m_lbl_name);
    lay->addStretch(1);
    lay->addWidget(m_lbl_status);
}

void GestureRow::setDone(bool done)
{
    m_done = done;
    if (done) {
        m_lbl_status->setText(QStringLiteral("●  ✓"));
        m_lbl_status->setStyleSheet(
            QStringLiteral("color: #4caf50; font-size: 12px; font-weight: bold;"));
    }
}

// ---------------------------------------------------------------------------
// GestureTestWidget
// ---------------------------------------------------------------------------
GestureTestWidget::GestureTestWidget(QWidget* parent)
    : QFrame(parent)
{
    setFrameShape(QFrame::StyledPanel);
    setStyleSheet(QStringLiteral(
        "GestureTestWidget { border: 1px solid #444; border-radius: 6px;"
        " background: #1e1e1e; }"
    ));
    setMinimumHeight(160);
    setCursor(Qt::PointingHandCursor);

    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 6, 0, 6);
    lay->setSpacing(0);

    auto* lbl_title = new QLabel(
        QStringLiteral("  คลิกในกล่องนี้เพื่อทดสอบ gesture"), this);
    {
        QFont f = lbl_title->font();
        f.setItalic(true);
        lbl_title->setFont(f);
        lbl_title->setStyleSheet(
            QStringLiteral("color: #666; font-size: 11px; border: none;"));
    }
    lay->addWidget(lbl_title);

    // Divider
    auto* line = new QFrame(this);
    line->setFrameShape(QFrame::HLine);
    line->setStyleSheet(QStringLiteral("color: #333;"));
    lay->addWidget(line);

    const char* labels[] = {
        "Single Click   (คลิก 1 ครั้ง)",
        "Double Click   (ดับเบิลคลิก)",
        "Right Click    (คลิกขวา)",
        "Scroll         (เลื่อนนิ้ว 2 นิ้ว / scroll wheel)",
    };
    for (int i = 0; i < 4; ++i) {
        m_rows[i] = new GestureRow(QString::fromUtf8(labels[i]), this);
        lay->addWidget(m_rows[i]);
        if (i < 3) {
            auto* sep = new QFrame(this);
            sep->setFrameShape(QFrame::HLine);
            sep->setStyleSheet(QStringLiteral("color: #2a2a2a; border: none;"
                                              " background: #2a2a2a; max-height: 1px;"));
            lay->addWidget(sep);
        }
    }
}

bool GestureTestWidget::allDone() const
{
    for (int i = 0; i < 4; ++i)
        if (!m_rows[i]->isDone()) return false;
    return true;
}

void GestureTestWidget::markDone(int index)
{
    if (index < 0 || index >= 4) return;
    if (!m_rows[index]->isDone()) {
        m_rows[index]->setDone(true);
        emit gestureDetected();
    }
}

void GestureTestWidget::mousePressEvent(QMouseEvent* e)
{
    if (e->button() == Qt::RightButton) {
        markDone(2);  // right click
    }
    QFrame::mousePressEvent(e);
}

void GestureTestWidget::mouseReleaseEvent(QMouseEvent* e)
{
    if (e->button() == Qt::LeftButton && !m_pending_single) {
        m_pending_single = true;
        // Use a short timer to distinguish single vs double click
        QTimer* t = new QTimer(this);
        t->setSingleShot(true);
        connect(t, &QTimer::timeout, this, [this, t]() {
            if (m_pending_single) {
                markDone(0);  // single click
                m_pending_single = false;
            }
            t->deleteLater();
        });
        t->start(250);
    }
    QFrame::mouseReleaseEvent(e);
}

void GestureTestWidget::mouseDoubleClickEvent(QMouseEvent* e)
{
    if (e->button() == Qt::LeftButton) {
        m_pending_single = false;  // cancel pending single
        markDone(1);  // double click
    }
    QFrame::mouseDoubleClickEvent(e);
}

void GestureTestWidget::wheelEvent(QWheelEvent* e)
{
    markDone(3);  // scroll
    QFrame::wheelEvent(e);
}

} // namespace nbi
