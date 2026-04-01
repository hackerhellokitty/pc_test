#pragma once

// ---------------------------------------------------------------------------
// gui/gesture_test_widget.hpp
// A clickable box that detects: single click, double click, right click, scroll.
// Each detected gesture lights up green automatically.
// ---------------------------------------------------------------------------

#include <QFrame>
#include <QLabel>
#include <QGridLayout>
#include <QTimer>
#include <QWidget>

namespace nbi {

// One gesture row — label + status indicator
class GestureRow : public QWidget {
    Q_OBJECT
public:
    explicit GestureRow(const QString& label, QWidget* parent = nullptr);
    void setDone(bool done);
    bool isDone() const { return m_done; }

private:
    QLabel* m_lbl_name{nullptr};
    QLabel* m_lbl_status{nullptr};
    bool    m_done{false};
};

// The detection area — intercepts mouse/wheel events
class GestureTestWidget : public QFrame {
    Q_OBJECT
public:
    explicit GestureTestWidget(QWidget* parent = nullptr);

    bool allDone() const;

signals:
    void gestureDetected();   // emitted whenever any gesture completes

protected:
    void mousePressEvent  (QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    void mouseDoubleClickEvent(QMouseEvent* e) override;
    void wheelEvent       (QWheelEvent* e) override;

private:
    void markDone(int index);

    GestureRow* m_rows[4]{};
    // 0 = single click, 1 = double click, 2 = right click, 3 = scroll

    bool m_pending_single{false};  // suppress single after double-click
};

} // namespace nbi
