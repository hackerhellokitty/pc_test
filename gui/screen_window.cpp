// ---------------------------------------------------------------------------
// gui/screen_window.cpp
// ---------------------------------------------------------------------------

#include "gui/screen_window.hpp"
#include "gui/screen_test_view.hpp"
#include "gui/touch_canvas_view.hpp"

#include <QApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScreen>
#include <QVBoxLayout>

namespace nbi {

ScreenWindow::ScreenWindow(bool has_touch, QWidget* parent)
    : QWidget(parent, Qt::Window)
    , m_has_touch(has_touch)
{
    setWindowTitle(QStringLiteral("Screen Test"));
    setAttribute(Qt::WA_DeleteOnClose, false);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    m_stack = new QStackedWidget(this);
    root->addWidget(m_stack);

    // -- Page 0: color cycle + dead pixel test --------------------------------
    m_color_view = new ScreenTestView(this);
    m_stack->addWidget(m_color_view);

    connect(m_color_view, &ScreenTestView::finished,
            this,         &ScreenWindow::onColorTestDone);

    // -- Page 1: touch canvas (only added when has_touch) --------------------
    if (m_has_touch) {
        // Wrapper so we can add a title bar + Done button on top of the canvas
        auto* touch_page = new QWidget(this);
        touch_page->setStyleSheet(QStringLiteral("background: #1a1a1a;"));

        auto* tp_layout = new QVBoxLayout(touch_page);
        tp_layout->setContentsMargins(0, 0, 0, 0);
        tp_layout->setSpacing(0);

        // Title bar
        auto* title_bar = new QWidget(touch_page);
        title_bar->setFixedHeight(44);
        title_bar->setStyleSheet(QStringLiteral("background: rgba(0,0,0,180);"));

        auto* tb_layout = new QHBoxLayout(title_bar);
        tb_layout->setContentsMargins(16, 0, 16, 0);

        auto* lbl_title = new QLabel(
            QStringLiteral("Touchscreen Test — ลากนิ้วให้ทั่วจอ"),
            title_bar
        );
        lbl_title->setStyleSheet(QStringLiteral(
            "color: white; font-size: 13px; font-weight: bold;"));

        auto* btn_done = new QPushButton(QStringLiteral("Done / เสร็จ"), title_bar);
        btn_done->setFixedWidth(110);
        btn_done->setStyleSheet(QStringLiteral(
            "QPushButton { font-size: 12px; padding: 4px 12px;"
            " border: 1px solid #666; border-radius: 4px;"
            " background: rgba(60,60,60,200); color: white; }"
            "QPushButton:hover { background: rgba(90,90,90,220); }"
        ));

        tb_layout->addWidget(lbl_title);
        tb_layout->addStretch(1);
        tb_layout->addWidget(btn_done);

        m_touch_view = new TouchCanvasView(touch_page);

        tp_layout->addWidget(title_bar);
        tp_layout->addWidget(m_touch_view, 1);

        m_stack->addWidget(touch_page);

        connect(btn_done, &QPushButton::clicked,
                this,     &ScreenWindow::onTouchDone);
    }

    // Full-screen on primary display
    QScreen* screen = QApplication::primaryScreen();
    if (screen)
        setGeometry(screen->geometry());

    showFullScreen();
    m_color_view->setFocus();
}

// ---------------------------------------------------------------------------
// onColorTestDone — color/dead-pixel page finished
// ---------------------------------------------------------------------------
void ScreenWindow::onColorTestDone(nbi::ModuleResult result)
{
    m_color_result = result;

    if (m_has_touch) {
        // Advance to touch canvas page
        m_stack->setCurrentIndex(1);
        m_touch_view->setFocus();
    } else {
        emit finished(m_color_result);
        close();
    }
}

// ---------------------------------------------------------------------------
// onTouchDone — touch canvas page finished; merge results
// ---------------------------------------------------------------------------
void ScreenWindow::onTouchDone()
{
    const float pct = m_touch_view->coveragePct();

    ModuleResult r = m_color_result;  // start from color test result

    // Append touch coverage to summary/issues
    const QString touch_line =
        QStringLiteral("Touch coverage: %1%").arg(static_cast<int>(pct));

    if (pct < 80.0f) {
        // Low coverage counts as a problem
        r.status = TestStatus::Fail;
        r.issues.append(touch_line + QStringLiteral(" (ต่ำกว่า 80%)"));
    } else {
        r.issues.append(touch_line);
    }

    // Rebuild summary
    if (r.issues.isEmpty()) {
        r.summary = QStringLiteral("Pass");
    } else {
        const int defects = static_cast<int>(
            std::count_if(r.issues.begin(), r.issues.end(),
                [](const QString& s){ return !s.startsWith(QStringLiteral("Touch")); })
        );
        r.summary = defects > 0
            ? QStringLiteral("%1 defect(s), touch %2%")
                  .arg(defects).arg(static_cast<int>(pct))
            : QStringLiteral("No defects, touch %1%").arg(static_cast<int>(pct));
    }

    emit finished(r);
    close();
}

} // namespace nbi
