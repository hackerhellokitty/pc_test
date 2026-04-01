#pragma once

// ---------------------------------------------------------------------------
// gui/keyboard_window.hpp
// Standalone window that hosts the keyboard test.
// ---------------------------------------------------------------------------

#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

#include "common/types.hpp"
#include "gui/keyboard_view.hpp"

namespace nbi {

// ---------------------------------------------------------------------------
// KeyboardWindow
// ---------------------------------------------------------------------------
class KeyboardWindow : public QWidget {
    Q_OBJECT

public:
    explicit KeyboardWindow(QWidget* parent = nullptr);
    ~KeyboardWindow() override = default;

signals:
    void finished(nbi::ModuleResult result);

private slots:
    void onStateChanged();
    void onDoneClicked();
    void onResetClicked();
    void onLayoutChanged(int index);

private:
    void updateRemainingLabel();
    void swapLayout(const QString& json_path);

    KeyboardView* m_view{nullptr};
    QVBoxLayout*  m_view_container{nullptr};
    QLabel*       m_lbl_remaining{nullptr};
    QPushButton*  m_btn_reset{nullptr};
    QPushButton*  m_btn_done{nullptr};
    QComboBox*    m_combo_layout{nullptr};
};

} // namespace nbi
