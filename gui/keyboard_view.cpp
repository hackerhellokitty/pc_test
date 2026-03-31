// ---------------------------------------------------------------------------
// gui/keyboard_view.cpp
// ---------------------------------------------------------------------------

#include "gui/keyboard_view.hpp"

#include <QApplication>
#include <QFile>
#include <QFont>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <QSizePolicy>

#include <windows.h>

namespace nbi {

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------
KeyboardView::KeyboardView(const QString& layout_json_path, QWidget* parent)
    : QWidget(parent)
{
    setFocusPolicy(Qt::StrongFocus);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setAttribute(Qt::WA_OpaquePaintEvent, false);
    s_active_instance = this;
    m_ll_hook = SetWindowsHookExW(WH_KEYBOARD_LL, llKeyboardProc, nullptr, 0);

    // -- Load JSON layout ---------------------------------------------------
    QFile file(layout_json_path);
    if (file.open(QIODevice::ReadOnly)) {
        const QByteArray raw = file.readAll();
        file.close();

        QJsonParseError err;
        const QJsonDocument doc = QJsonDocument::fromJson(raw, &err);

        if (err.error == QJsonParseError::NoError && doc.isArray()) {
            const QJsonArray arr = doc.array();
            m_keys.reserve(static_cast<std::size_t>(arr.size()));

            for (const QJsonValue& val : arr) {
                if (!val.isObject()) continue;
                const QJsonObject obj = val.toObject();

                KeyDef k;
                k.scan_code = obj.value(QStringLiteral("scan")).toInt(0);
                k.label     = obj.value(QStringLiteral("label")).toString();
                k.row       = static_cast<float>(
                    obj.value(QStringLiteral("row")).toDouble(0.0));
                k.col       = static_cast<float>(
                    obj.value(QStringLiteral("col")).toDouble(0.0));
                k.w         = static_cast<float>(
                    obj.value(QStringLiteral("w")).toDouble(1.0));
                k.state     = KeyState::Untested;

                if (k.scan_code != 0) {
                    m_keys.push_back(k);
                }
            }
        }
    }

    // -- Compute layout extents --------------------------------------------
    m_layout_cols = 0.0f;
    m_layout_rows = 0.0f;
    for (const KeyDef& k : m_keys) {
        const float right  = k.col + k.w;
        const float bottom = k.row + 1.0f;
        if (right  > m_layout_cols) m_layout_cols = right;
        if (bottom > m_layout_rows) m_layout_rows = bottom;
    }
}

// ---------------------------------------------------------------------------
// Destructor — uninstall native event filter
// ---------------------------------------------------------------------------
KeyboardView::~KeyboardView()
{
    if (m_ll_hook) { UnhookWindowsHookEx(m_ll_hook); m_ll_hook = nullptr; }
    if (s_active_instance == this) s_active_instance = nullptr;
}

// ---------------------------------------------------------------------------
// Static members
// ---------------------------------------------------------------------------
KeyboardView* KeyboardView::s_active_instance = nullptr;

LRESULT CALLBACK KeyboardView::llKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode == HC_ACTION && s_active_instance
        && (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN))
    {
        const KBDLLHOOKSTRUCT* kb = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
        int sc = static_cast<int>(kb->scanCode);
        if (kb->flags & LLKHF_EXTENDED) sc += 256;

        KeyboardView* v = s_active_instance;
        for (KeyDef& k : v->m_keys) {
            if (k.scan_code == sc) {
                if (k.state == KeyState::Untested) {
                    k.state = KeyState::Pressed;
                    v->update();
                    emit v->stateChanged();
                }
                break;
            }
        }
    }
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

// ---------------------------------------------------------------------------
// keyRect — compute the pixel rect for a key (scaled to widget size)
// ---------------------------------------------------------------------------
QRectF KeyboardView::keyRect(const KeyDef& k) const
{
    // Scale so the full layout fits the widget width.
    const double unit = (m_layout_cols > 0.0f)
        ? static_cast<double>(width()) / static_cast<double>(m_layout_cols)
        : kBaseUnit;

    const double rowStep = unit * 1.15;
    const double x = k.col * unit;
    const double y = k.row * rowStep;
    const double w = k.w  * unit - 3.0;
    const double h = unit         - 3.0;
    return QRectF(x, y, w, h);
}

// ---------------------------------------------------------------------------
// paintEvent
// ---------------------------------------------------------------------------
void KeyboardView::paintEvent(QPaintEvent* /*event*/)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    QFont font = p.font();
    font.setPointSize(9);
    p.setFont(font);

    for (const KeyDef& k : m_keys) {
        const QRectF r = keyRect(k);

        // Background colour per state
        QColor bg;
        switch (k.state) {
            case KeyState::Untested:     bg = QColor(QStringLiteral("#2d2d2d")); break;
            case KeyState::Pressed:      bg = QColor(QStringLiteral("#2e7d32")); break;
            case KeyState::MarkedBroken: bg = QColor(QStringLiteral("#c62828")); break;
        }

        // Border slightly lighter
        const QColor border = bg.lighter(140);

        p.setPen(QPen(border, 1.0));
        p.setBrush(bg);
        p.drawRoundedRect(r, 4.0, 4.0);

        // Label
        p.setPen(QColor(Qt::white));
        p.drawText(r, Qt::AlignCenter, k.label);
    }
}

// ---------------------------------------------------------------------------
// event — intercept before Qt consumes Tab, CapsLock, NumLock, etc.
// ---------------------------------------------------------------------------
bool KeyboardView::event(QEvent* event)
{
    if (event->type() == QEvent::KeyPress) {
        keyPressEvent(static_cast<QKeyEvent*>(event));
        return true;  // Mark as handled so Qt doesn't use Tab for focus change
    }
    return QWidget::event(event);
}

// ---------------------------------------------------------------------------
// keyPressEvent
// ---------------------------------------------------------------------------
void KeyboardView::keyPressEvent(QKeyEvent* event)
{
    // Qt on Windows: nativeScanCode() returns the value from KBDLLHOOKSTRUCT
    // which already encodes E0-extended keys as scan+256 (e.g. RCtrl=0xE11D=57629).
    // We strip the high E0 byte and re-encode as scan+256 to match our layout.
    const quint32 raw = event->nativeScanCode();
    int sc;
    if (raw > 0xFF) {
        // Extended key: low byte is the base scan code
        sc = static_cast<int>(raw & 0xFF) + 256;
    } else {
        sc = static_cast<int>(raw);
    }
    const bool extended = (sc > 256);

    for (KeyDef& k : m_keys) {
        if (k.scan_code == sc) {
            if (k.state == KeyState::Untested) {
                k.state = KeyState::Pressed;
                update();
                emit stateChanged();
            }
            return;
        }
    }
    QWidget::keyPressEvent(event);
}

// ---------------------------------------------------------------------------
// mousePressEvent — left-click grabs focus; right-click toggles MarkedBroken
// ---------------------------------------------------------------------------
void KeyboardView::mousePressEvent(QMouseEvent* event)
{
    // Always grab keyboard focus on any click so key events are delivered here.
    setFocus(Qt::MouseFocusReason);

    if (event->button() != Qt::RightButton) {
        QWidget::mousePressEvent(event);
        return;
    }

    const QPointF pos = event->position();
    for (KeyDef& k : m_keys) {
        if (keyRect(k).contains(pos)) {
            if (k.state == KeyState::MarkedBroken) {
                k.state = KeyState::Untested;
            } else {
                k.state = KeyState::MarkedBroken;
            }
            update();
            emit stateChanged();
            return;
        }
    }
    QWidget::mousePressEvent(event);
}

// ---------------------------------------------------------------------------
// untestedCount
// ---------------------------------------------------------------------------
int KeyboardView::untestedCount() const
{
    int count = 0;
    for (const KeyDef& k : m_keys) {
        if (k.state == KeyState::Untested) ++count;
    }
    return count;
}

// ---------------------------------------------------------------------------
// brokenKeys
// ---------------------------------------------------------------------------
QStringList KeyboardView::brokenKeys() const
{
    QStringList list;
    for (const KeyDef& k : m_keys) {
        if (k.state == KeyState::MarkedBroken) {
            list.append(k.label);
        }
    }
    return list;
}

// ---------------------------------------------------------------------------
// result
// ---------------------------------------------------------------------------
ModuleResult KeyboardView::result() const
{
    ModuleResult r;
    r.label = QStringLiteral("Keyboard");

    const QStringList broken = brokenKeys();
    if (!broken.isEmpty()) {
        r.status  = TestStatus::Fail;
        r.summary = QStringLiteral("Broken: ") + broken.join(QStringLiteral(", "));
        r.issues  = broken;
    } else if (untestedCount() == 0) {
        r.status  = TestStatus::Pass;
        r.summary = QStringLiteral("All keys OK");
    } else {
        r.status  = TestStatus::Running;
        r.summary = QStringLiteral("In progress…");
    }
    return r;
}

// ---------------------------------------------------------------------------
// reset
// ---------------------------------------------------------------------------
void KeyboardView::reset()
{
    for (KeyDef& k : m_keys) {
        k.state = KeyState::Untested;
    }
    update();
    emit stateChanged();
}

// ---------------------------------------------------------------------------
// minimumSizeHint / sizeHint
// ---------------------------------------------------------------------------
QSize KeyboardView::minimumSizeHint() const
{
    const int w = static_cast<int>(m_layout_cols * kBaseUnit);
    const int h = static_cast<int>(m_layout_rows * kBaseUnit * 1.15f);
    return QSize(w, h);
}

QSize KeyboardView::sizeHint() const
{
    return minimumSizeHint();
}

} // namespace nbi
