#pragma once

// ---------------------------------------------------------------------------
// gui/screen_test_view.hpp
// Full-screen Screen Test widget.
//
// Modes:
//   ColorCycle  — cycles through solid fills: R → G → B → White → Black
//   DeadPixel   — black canvas; user clicks to pin suspected dead pixels
//
// Brightness slider uses DDC/CI via GetMonitorCapabilities /
// SetMonitorBrightness when supported; silently disabled otherwise.
// ---------------------------------------------------------------------------

#include <vector>

#include <QLabel>
#include <QPointF>
#include <QPushButton>
#include <QSlider>
#include <QWidget>

#include "common/types.hpp"

// Forward-declare Win32 HMONITOR to avoid pulling in <windows.h> in the header
struct HMONITOR__;
using HMONITOR = HMONITOR__*;

namespace nbi {

// ---------------------------------------------------------------------------
// ColorStep — one entry in the color-cycle sequence
// ---------------------------------------------------------------------------
enum class ColorStep : int {
    Red   = 0,
    Green = 1,
    Blue  = 2,
    White = 3,
    Black = 4,
    Count = 5,   // sentinel
};

// ---------------------------------------------------------------------------
// ScreenTestView
// ---------------------------------------------------------------------------
class ScreenTestView : public QWidget {
    Q_OBJECT

public:
    explicit ScreenTestView(QWidget* parent = nullptr);
    ~ScreenTestView() override;

    ModuleResult result() const;

signals:
    void finished(nbi::ModuleResult result);

protected:
    void paintEvent      (QPaintEvent*  event) override;
    void resizeEvent     (QResizeEvent* event) override;
    void mousePressEvent (QMouseEvent*  event) override;
    void keyPressEvent   (QKeyEvent*    event) override;
    bool eventFilter     (QObject* obj, QEvent* event) override;

private slots:
    void onNextColor();
    void onPrevColor();
    void onToggleDeadPixelMode();
    void onBrightnessChanged(int value);
    void onDone();

private:
    void buildUi();
    void applyColorStep();
    void updateOverlayPosition();

    // DDC/CI brightness helpers
    void     initBrightness();
    bool     setBrightness(int pct);   // 0–100; returns false if DDC/CI unavailable
    void     destroyPhysicalMonitor();

    // -------------------------------------------------------------------------
    // State
    // -------------------------------------------------------------------------

    // One marked suspect pixel — position + background color when pinned
    struct PixelPin {
        QPointF   pos;
        ColorStep bg;
    };

    ColorStep m_step{ColorStep::Red};
    bool      m_dead_pixel_mode{false};
    std::vector<PixelPin> m_pins;

    // DDC/CI handle (nullptr if unsupported)
    void* m_phys_monitor{nullptr};        // HANDLE alias to avoid <windows.h>
    bool  m_brightness_supported{false};

    // -------------------------------------------------------------------------
    // UI elements (overlay panel visible on top of color fill)
    // -------------------------------------------------------------------------
    QWidget*     m_overlay{nullptr};
    QLabel*      m_lbl_step{nullptr};
    QLabel*      m_lbl_hint{nullptr};
    QPushButton* m_btn_prev{nullptr};
    QPushButton* m_btn_next{nullptr};
    QPushButton* m_btn_dead_pixel{nullptr};
    QPushButton* m_btn_done{nullptr};
    QSlider*     m_slider_brightness{nullptr};
    QLabel*      m_lbl_brightness{nullptr};
};

} // namespace nbi
