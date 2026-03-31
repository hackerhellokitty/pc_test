// ---------------------------------------------------------------------------
// gui/screen_test_view.cpp
// ---------------------------------------------------------------------------

#include "gui/screen_test_view.hpp"

#include <windows.h>
#include <physicalmonitorenumerationapi.h>
#include <highlevelmonitorconfigurationapi.h>

#include <QApplication>
#include <QColor>
#include <QEvent>
#include <QFont>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QPushButton>
#include <QScreen>
#include <QSlider>
#include <QVBoxLayout>

#pragma comment(lib, "Dxva2.lib")

namespace nbi {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static QColor colorForStep(ColorStep step)
{
    switch (step) {
        case ColorStep::Red:   return QColor(255,   0,   0);
        case ColorStep::Green: return QColor(  0, 255,   0);
        case ColorStep::Blue:  return QColor(  0,   0, 255);
        case ColorStep::White: return QColor(255, 255, 255);
        case ColorStep::Black: return QColor(  0,   0,   0);
        default:               return QColor(  0,   0,   0);
    }
}

static QString nameForStep(ColorStep step)
{
    switch (step) {
        case ColorStep::Red:   return QStringLiteral("Red");
        case ColorStep::Green: return QStringLiteral("Green");
        case ColorStep::Blue:  return QStringLiteral("Blue");
        case ColorStep::White: return QStringLiteral("White");
        case ColorStep::Black: return QStringLiteral("Black");
        default:               return QStringLiteral("?");
    }
}

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------
ScreenTestView::ScreenTestView(QWidget* parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    buildUi();
    initBrightness();
    applyColorStep();
}

ScreenTestView::~ScreenTestView()
{
    destroyPhysicalMonitor();
}

// ---------------------------------------------------------------------------
// buildUi
// ---------------------------------------------------------------------------
void ScreenTestView::buildUi()
{
    // Overlay panel — semi-transparent control bar at top
    m_overlay = new QWidget(this);
    m_overlay->setObjectName(QStringLiteral("overlay"));
    m_overlay->setStyleSheet(QStringLiteral(
        "#overlay { background-color: rgba(0,0,0,180);"
        " border-radius: 8px; }"
    ));

    auto* ov_layout = new QVBoxLayout(m_overlay);
    ov_layout->setContentsMargins(12, 10, 12, 10);
    ov_layout->setSpacing(8);

    // Row 1: step name + hints
    m_lbl_step = new QLabel(m_overlay);
    {
        QFont f = m_lbl_step->font();
        f.setBold(true);
        f.setPointSize(14);
        m_lbl_step->setFont(f);
        m_lbl_step->setStyleSheet(QStringLiteral("color: white;"));
    }

    m_lbl_hint = new QLabel(
        QStringLiteral("←→ เปลี่ยนสี  •  คลิก = ปักจุดสงสัย"),
        m_overlay
    );
    m_lbl_hint->setStyleSheet(QStringLiteral("color: #cccccc; font-size: 11px;"));

    ov_layout->addWidget(m_lbl_step);
    ov_layout->addWidget(m_lbl_hint);

    // Row 2: navigation + dead pixel toggle
    auto* btn_row = new QHBoxLayout();
    btn_row->setSpacing(8);

    auto make_btn = [&](const QString& label, QWidget* par) -> QPushButton* {
        auto* b = new QPushButton(label, par);
        b->setStyleSheet(QStringLiteral(
            "QPushButton { font-size: 12px; padding: 4px 12px;"
            " border: 1px solid #666; border-radius: 4px;"
            " background-color: rgba(60,60,60,200); color: white; }"
            "QPushButton:hover { background-color: rgba(90,90,90,220); }"
            "QPushButton:checked { background-color: rgba(42,130,218,200); }"
        ));
        return b;
    };

    m_btn_prev       = make_btn(QStringLiteral("◀ Prev"),       m_overlay);
    m_btn_next       = make_btn(QStringLiteral("Next ▶"),       m_overlay);
    m_btn_dead_pixel = make_btn(
        QStringLiteral("มาร์คจุด Dead Pixel"),
        m_overlay
    );
    m_btn_dead_pixel->setCheckable(true);

    m_btn_done = make_btn(QStringLiteral("Done / เสร็จ"), m_overlay);

    btn_row->addWidget(m_btn_prev);
    btn_row->addWidget(m_btn_next);
    btn_row->addWidget(m_btn_dead_pixel);
    btn_row->addStretch(1);
    btn_row->addWidget(m_btn_done);
    ov_layout->addLayout(btn_row);

    // Row 3: brightness slider (hidden if DDC/CI not available)
    auto* bright_row = new QHBoxLayout();
    bright_row->setSpacing(8);

    auto* lbl_bright_title = new QLabel(QStringLiteral("ความสว่าง:"), m_overlay);
    lbl_bright_title->setStyleSheet(QStringLiteral("color: #cccccc; font-size: 12px;"));

    m_slider_brightness = new QSlider(Qt::Horizontal, m_overlay);
    m_slider_brightness->setRange(0, 100);
    m_slider_brightness->setValue(50);
    m_slider_brightness->setFixedWidth(160);
    m_slider_brightness->setEnabled(false);   // enabled after initBrightness

    m_lbl_brightness = new QLabel(QStringLiteral("50%"), m_overlay);
    m_lbl_brightness->setStyleSheet(QStringLiteral("color: #cccccc; font-size: 12px;"));
    m_lbl_brightness->setFixedWidth(36);

    bright_row->addWidget(lbl_bright_title);
    bright_row->addWidget(m_slider_brightness);
    bright_row->addWidget(m_lbl_brightness);
    bright_row->addStretch(1);
    ov_layout->addLayout(bright_row);

    m_overlay->adjustSize();

    // Install event filter so the overlay doesn't steal key events
    m_overlay->installEventFilter(this);

    // Connections
    connect(m_btn_prev,       &QPushButton::clicked,
            this,             &ScreenTestView::onPrevColor);
    connect(m_btn_next,       &QPushButton::clicked,
            this,             &ScreenTestView::onNextColor);
    connect(m_btn_dead_pixel, &QPushButton::clicked,
            this,             &ScreenTestView::onToggleDeadPixelMode);
    connect(m_btn_done,       &QPushButton::clicked,
            this,             &ScreenTestView::onDone);
    connect(m_slider_brightness, &QSlider::valueChanged,
            this,                &ScreenTestView::onBrightnessChanged);
}

// ---------------------------------------------------------------------------
// initBrightness — find first physical monitor and check DDC/CI support
// ---------------------------------------------------------------------------
void ScreenTestView::initBrightness()
{
    // Get the HMONITOR for the primary screen
    HMONITOR hmon = MonitorFromWindow(
        reinterpret_cast<HWND>(winId()),
        MONITOR_DEFAULTTOPRIMARY
    );

    DWORD count = 0;
    if (!GetNumberOfPhysicalMonitorsFromHMONITOR(hmon, &count) || count == 0)
        return;

    auto* monitors = new PHYSICAL_MONITOR[count];
    if (!GetPhysicalMonitorsFromHMONITOR(hmon, count, monitors)) {
        delete[] monitors;
        return;
    }

    // Use first monitor; store the HANDLE
    m_phys_monitor = monitors[0].hPhysicalMonitor;
    delete[] monitors;   // array itself no longer needed; handle is stored

    // Check whether brightness (VCP 0x10) is supported
    DWORD caps = 0, color_temps = 0;
    if (GetMonitorCapabilities(
            static_cast<HANDLE>(m_phys_monitor), &caps, &color_temps))
    {
        if (caps & MC_CAPS_BRIGHTNESS) {
            m_brightness_supported = true;

            // Read current brightness to seed the slider
            DWORD min_val = 0, cur_val = 50, max_val = 100;
            if (GetMonitorBrightness(
                    static_cast<HANDLE>(m_phys_monitor),
                    &min_val, &cur_val, &max_val))
            {
                // Normalise to 0–100
                if (max_val > min_val) {
                    const int pct = static_cast<int>(
                        (cur_val - min_val) * 100 / (max_val - min_val)
                    );
                    m_slider_brightness->setValue(pct);
                }
            }
            m_slider_brightness->setEnabled(true);
        }
    }
}

// ---------------------------------------------------------------------------
// setBrightness
// ---------------------------------------------------------------------------
bool ScreenTestView::setBrightness(int pct)
{
    if (!m_brightness_supported || !m_phys_monitor) return false;

    DWORD min_val = 0, cur_val = 0, max_val = 100;
    if (!GetMonitorBrightness(
            static_cast<HANDLE>(m_phys_monitor),
            &min_val, &cur_val, &max_val))
        return false;

    const DWORD target = min_val + static_cast<DWORD>(
        pct * static_cast<int>(max_val - min_val) / 100
    );
    return SetMonitorBrightness(static_cast<HANDLE>(m_phys_monitor), target);
}

// ---------------------------------------------------------------------------
// destroyPhysicalMonitor
// ---------------------------------------------------------------------------
void ScreenTestView::destroyPhysicalMonitor()
{
    if (m_phys_monitor) {
        DestroyPhysicalMonitor(static_cast<HANDLE>(m_phys_monitor));
        m_phys_monitor = nullptr;
    }
}

// ---------------------------------------------------------------------------
// applyColorStep — update background colour and overlay label
// ---------------------------------------------------------------------------
void ScreenTestView::applyColorStep()
{
    const QColor bg = colorForStep(m_step);

    // Update widget background
    QPalette pal = palette();
    pal.setColor(QPalette::Window, bg);
    setPalette(pal);
    setAutoFillBackground(true);

    // Overlay label
    if (m_lbl_step) {
        const QString name = nameForStep(m_step);
        const int idx = static_cast<int>(m_step) + 1;
        const int total = static_cast<int>(ColorStep::Count);
        m_lbl_step->setText(
            QStringLiteral("Color Test — %1  (%2/%3)")
                .arg(name).arg(idx).arg(total)
        );
    }

    // Adjust overlay text colour for visibility on White
    const bool on_white = (m_step == ColorStep::White);
    if (m_lbl_hint)
        m_lbl_hint->setStyleSheet(
            on_white
                ? QStringLiteral("color: #333333; font-size: 11px;")
                : QStringLiteral("color: #cccccc; font-size: 11px;")
        );

    update();
}

// ---------------------------------------------------------------------------
// paintEvent — draw dead-pixel markers on top of background
// ---------------------------------------------------------------------------
void ScreenTestView::paintEvent(QPaintEvent* event)
{
    QWidget::paintEvent(event);

    if (m_pins.empty()) return;

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Ring colour contrasts with the background each pin was recorded on
    auto ring_color = [](ColorStep bg) -> QColor {
        switch (bg) {
            case ColorStep::Red:   return QColor(  0, 255, 255);  // cyan on red
            case ColorStep::Green: return QColor(255,   0, 255);  // magenta on green
            case ColorStep::Blue:  return QColor(255, 255,   0);  // yellow on blue
            case ColorStep::White: return QColor(  0,   0,   0);  // black on white
            case ColorStep::Black: return QColor(255, 255,   0);  // yellow on black
            default:               return QColor(255, 255,   0);
        }
    };

    for (const PixelPin& pin : m_pins) {
        const QColor ring = ring_color(pin.bg);

        // Outer circle
        painter.setPen(QPen(ring, 2));
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(pin.pos, 10.0, 10.0);

        // Inner dot (same ring colour, filled)
        painter.setPen(Qt::NoPen);
        painter.setBrush(ring);
        painter.drawEllipse(pin.pos, 3.0, 3.0);
    }
}

// ---------------------------------------------------------------------------
// mousePressEvent
// ---------------------------------------------------------------------------
void ScreenTestView::mousePressEvent(QMouseEvent* event)
{
    if (!m_dead_pixel_mode) {
        QWidget::mousePressEvent(event);
        return;
    }

    if (event->button() == Qt::LeftButton) {
        m_pins.push_back({event->position(), m_step});
        update();
    } else if (event->button() == Qt::RightButton) {
        // Right-click removes the nearest marker within 20px
        const QPointF pos = event->position();
        auto it = m_pins.begin();
        auto nearest = m_pins.end();
        double min_dist = 20.0 * 20.0;
        for (; it != m_pins.end(); ++it) {
            const double dx = it->pos.x() - pos.x();
            const double dy = it->pos.y() - pos.y();
            const double dist2 = dx * dx + dy * dy;
            if (dist2 < min_dist) {
                min_dist = dist2;
                nearest = it;
            }
        }
        if (nearest != m_pins.end()) {
            m_pins.erase(nearest);
            update();
        }
    }
}

// ---------------------------------------------------------------------------
// keyPressEvent — arrow keys navigate color steps
// ---------------------------------------------------------------------------
void ScreenTestView::keyPressEvent(QKeyEvent* event)
{
    switch (event->key()) {
        case Qt::Key_Right:
        case Qt::Key_Space:
            onNextColor();
            break;
        case Qt::Key_Left:
            onPrevColor();
            break;
        case Qt::Key_Escape:
            onDone();
            break;
        default:
            QWidget::keyPressEvent(event);
    }
}

// ---------------------------------------------------------------------------
// eventFilter — ensure overlay child widgets forward key events to us
// ---------------------------------------------------------------------------
bool ScreenTestView::eventFilter(QObject* obj, QEvent* event)
{
    if (obj == m_overlay && event->type() == QEvent::KeyPress) {
        keyPressEvent(static_cast<QKeyEvent*>(event));
        return true;
    }
    return QWidget::eventFilter(obj, event);
}

// ---------------------------------------------------------------------------
// resizeEvent (implicit via QWidget) — reposition overlay
// ---------------------------------------------------------------------------
void ScreenTestView::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    updateOverlayPosition();
}

void ScreenTestView::updateOverlayPosition()
{
    if (!m_overlay) return;
    m_overlay->adjustSize();
    const int margin = 16;
    m_overlay->move(margin, margin);
    // Constrain width to widget width minus 2*margin
    const int max_w = width() - 2 * margin;
    if (m_overlay->width() > max_w)
        m_overlay->resize(max_w, m_overlay->height());
}

// ---------------------------------------------------------------------------
// Slots
// ---------------------------------------------------------------------------
void ScreenTestView::onNextColor()
{
    int next = static_cast<int>(m_step) + 1;
    if (next >= static_cast<int>(ColorStep::Count))
        next = 0;
    m_step = static_cast<ColorStep>(next);
    applyColorStep();
}

void ScreenTestView::onPrevColor()
{
    int prev = static_cast<int>(m_step) - 1;
    if (prev < 0)
        prev = static_cast<int>(ColorStep::Count) - 1;
    m_step = static_cast<ColorStep>(prev);
    applyColorStep();
}

void ScreenTestView::onToggleDeadPixelMode()
{
    m_dead_pixel_mode = m_btn_dead_pixel->isChecked();

    // Switch to Black background for dead-pixel scanning
    if (m_dead_pixel_mode) {
        m_step = ColorStep::Black;
        applyColorStep();
    }

    const QString hint_dp     = QStringLiteral("คลิกเพื่อปัก  •  คลิกขวา = ลบ");
    const QString hint_normal = QStringLiteral("←→ เปลี่ยนสี  •  คลิก = ปักจุดสงสัย");
    m_lbl_hint->setText(m_dead_pixel_mode ? hint_dp : hint_normal);
}

void ScreenTestView::onBrightnessChanged(int value)
{
    m_lbl_brightness->setText(QStringLiteral("%1%").arg(value));
    setBrightness(value);
}

void ScreenTestView::onDone()
{
    emit finished(result());
}

// ---------------------------------------------------------------------------
// result
// ---------------------------------------------------------------------------
ModuleResult ScreenTestView::result() const
{
    ModuleResult r;
    r.label  = QStringLiteral("Screen");

    if (m_pins.empty()) {
        r.status  = TestStatus::Pass;
        r.summary = QStringLiteral("No defects marked");
    } else {
        r.status  = TestStatus::Fail;
        r.summary = QStringLiteral("%1 defect(s) marked")
                        .arg(static_cast<int>(m_pins.size()));

        for (std::size_t i = 0; i < m_pins.size(); ++i) {
            const PixelPin& pin = m_pins[i];
            r.issues.append(
                QStringLiteral("#%1 (%2, %3) on %4 background")
                    .arg(i + 1)
                    .arg(static_cast<int>(pin.pos.x()))
                    .arg(static_cast<int>(pin.pos.y()))
                    .arg(nameForStep(pin.bg))
            );
        }
    }
    return r;
}

} // namespace nbi
