#pragma once

// ---------------------------------------------------------------------------
// gui/keyboard_view.hpp
// Interactive keyboard diagram widget for the NBI Keyboard Test module.
// ---------------------------------------------------------------------------

#include <vector>

#include <QRectF>
#include <QString>
#include <QWidget>

#include <windows.h>

#include "common/types.hpp"

namespace nbi {

// ---------------------------------------------------------------------------
// KeyState — per-key test state
// ---------------------------------------------------------------------------
enum class KeyState {
    Untested,
    Pressed,
    MarkedBroken,
};

// ---------------------------------------------------------------------------
// KeyDef — one key in the layout
// ---------------------------------------------------------------------------
struct KeyDef {
    int     scan_code{0};
    QString label;
    float   row{0.0f};
    float   col{0.0f};
    float   w{1.0f};
    KeyState state{KeyState::Untested};
};

// ---------------------------------------------------------------------------
// KeyboardView
// ---------------------------------------------------------------------------
class KeyboardView : public QWidget {
    Q_OBJECT

public:
    explicit KeyboardView(const QString& layout_json_path,
                          QWidget* parent = nullptr);
    ~KeyboardView() override;

    // -- Query ---------------------------------------------------------------
    int         untestedCount() const;
    QStringList brokenKeys()    const;
    ModuleResult result()       const;

    // -- Control -------------------------------------------------------------
    void reset();

    // -- Size hints ----------------------------------------------------------
    QSize minimumSizeHint() const override;
    QSize sizeHint()        const override;

signals:
    void stateChanged();

protected:
    bool event           (QEvent*       event) override;
    void paintEvent      (QPaintEvent*  event) override;
    void keyPressEvent   (QKeyEvent*    event) override;
    void mousePressEvent (QMouseEvent*  event) override;

private:
    // Pixel rect for a key, scaled to current widget size.
    QRectF keyRect(const KeyDef& k) const;

    // Low-level keyboard hook handle (WH_KEYBOARD_LL)
    static KeyboardView* s_active_instance;
    static LRESULT CALLBACK llKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);
    HHOOK m_ll_hook{nullptr};

    // Layout data loaded from JSON.
    std::vector<KeyDef> m_keys;

    // Bounding extent of the layout in key-unit coordinates.
    float m_layout_cols{0.0f};  ///< max col + w
    float m_layout_rows{0.0f};  ///< max row + 1

    static constexpr int kBaseUnit = 48;  ///< pixels per key unit at 1:1 scale
};

} // namespace nbi
