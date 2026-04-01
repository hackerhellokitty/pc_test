// ---------------------------------------------------------------------------
// gui/touchpad_view.cpp
// ---------------------------------------------------------------------------

#include "gui/touchpad_view.hpp"

#include <QFont>
#include <QFrame>
#include <QHBoxLayout>
#include <QSplitter>

namespace nbi {

TouchpadView::TouchpadView(QWidget* parent)
    : QWidget(parent)
{
    buildUi();
    m_result.label  = QStringLiteral("Touchpad");
    m_result.status = TestStatus::NotRun;
}

void TouchpadView::buildUi()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(16, 16, 16, 16);
    root->setSpacing(10);

    // Title
    auto* lbl_title = new QLabel(QStringLiteral("Touchpad Test"), this);
    {
        QFont f = lbl_title->font();
        f.setBold(true);
        f.setPointSize(15);
        lbl_title->setFont(f);
    }
    root->addWidget(lbl_title);

    // Hint
    m_lbl_hint = new QLabel(
        QStringLiteral("① ลากนิ้วบน touchpad ให้ทั่วพื้นที่วาด"
                       "   ② ทดสอบ gesture ในกล่องด้านล่าง"
                       "   ③ กด Done"), this);
    m_lbl_hint->setStyleSheet(QStringLiteral("color: #aaaaaa; font-size: 11px;"));
    m_lbl_hint->setWordWrap(true);
    root->addWidget(m_lbl_hint);

    // Draw canvas
    m_canvas = new TouchCanvasView(this);
    m_canvas->setMinimumHeight(200);
    root->addWidget(m_canvas, 1);

    // Coverage label
    m_lbl_coverage = new QLabel(QStringLiteral("Coverage: 0%"), this);
    m_lbl_coverage->setStyleSheet(QStringLiteral("color: #e0e0e0; font-size: 12px;"));
    m_lbl_coverage->setAlignment(Qt::AlignCenter);
    root->addWidget(m_lbl_coverage);

    // Gesture test box
    m_gestures = new GestureTestWidget(this);
    root->addWidget(m_gestures);

    // Gesture status
    m_lbl_gesture_status = new QLabel(QStringLiteral("Gesture: 0 / 4"), this);
    m_lbl_gesture_status->setStyleSheet(QStringLiteral("color: #aaaaaa; font-size: 11px;"));
    m_lbl_gesture_status->setAlignment(Qt::AlignCenter);
    root->addWidget(m_lbl_gesture_status);

    // Buttons
    auto* btn_row = new QHBoxLayout();

    m_btn_pass = new QPushButton(QStringLiteral("✓  ใช้งานได้"), this);
    m_btn_pass->setStyleSheet(QStringLiteral(
        "QPushButton { font-size: 12px; padding: 6px 20px;"
        " border: 1px solid #4caf50; border-radius: 5px;"
        " background: #1b3a1f; color: #4caf50; }"
        "QPushButton:hover { background: #254d28; }"
        "QPushButton:disabled { color: #555; border-color: #444; background: #222; }"
    ));

    m_btn_fail = new QPushButton(QStringLiteral("✗  มีปัญหา"), this);
    m_btn_fail->setStyleSheet(QStringLiteral(
        "QPushButton { font-size: 12px; padding: 6px 20px;"
        " border: 1px solid #f44336; border-radius: 5px;"
        " background: #3a1a1a; color: #f44336; }"
        "QPushButton:hover { background: #4d2020; }"
        "QPushButton:disabled { color: #555; border-color: #444; background: #222; }"
    ));

    btn_row->addStretch(1);
    btn_row->addWidget(m_btn_pass);
    btn_row->addSpacing(12);
    btn_row->addWidget(m_btn_fail);
    btn_row->addStretch(1);
    root->addLayout(btn_row);

    connect(m_canvas,   &TouchCanvasView::coverageChanged,
            this, &TouchpadView::onCoverageChanged);
    connect(m_gestures, &GestureTestWidget::gestureDetected,
            this, &TouchpadView::onGestureDetected);
    connect(m_btn_pass, &QPushButton::clicked, this, &TouchpadView::onPassClicked);
    connect(m_btn_fail, &QPushButton::clicked, this, &TouchpadView::onFailClicked);
}

void TouchpadView::onCoverageChanged(float pct)
{
    m_coverage = pct;
    m_lbl_coverage->setText(
        QStringLiteral("Coverage: %1%").arg(static_cast<int>(pct)));
}

void TouchpadView::onGestureDetected()
{
    ++m_gesture_count;
    if (m_gestures->allDone()) {
        m_lbl_gesture_status->setText(QStringLiteral("Gesture: ✓ ครบทุก gesture"));
        m_lbl_gesture_status->setStyleSheet(
            QStringLiteral("color: #4caf50; font-size: 11px;"));
    } else {
        m_lbl_gesture_status->setText(
            QStringLiteral("Gesture: %1 / 4").arg(m_gesture_count));
    }
}

void TouchpadView::onPassClicked()
{
    const bool all_gesture = m_gestures->allDone();
    m_result.status  = TestStatus::Pass;
    m_result.summary = QStringLiteral("Touchpad ปกติ — coverage %1%%%2")
        .arg(static_cast<int>(m_coverage))
        .arg(all_gesture ? QStringLiteral(", gesture ครบ")
                         : QStringLiteral(", gesture ไม่ครบ"));
    emit finished(m_result);
}

void TouchpadView::onFailClicked()
{
    m_result.status  = TestStatus::Fail;
    m_result.summary = QStringLiteral("Touchpad มีปัญหา — coverage %1%")
                           .arg(static_cast<int>(m_coverage));
    m_result.issues.append(m_result.summary);
    emit finished(m_result);
}

ModuleResult TouchpadView::result() const
{
    return m_result;
}

} // namespace nbi
