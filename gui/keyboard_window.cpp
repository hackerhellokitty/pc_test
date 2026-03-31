// ---------------------------------------------------------------------------
// gui/keyboard_window.cpp
// ---------------------------------------------------------------------------

#include "gui/keyboard_window.hpp"

#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSizePolicy>
#include <QSpacerItem>
#include <QVBoxLayout>

namespace nbi {

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------
KeyboardWindow::KeyboardWindow(QWidget* parent)
    : QWidget(parent, Qt::Window)
{
    setWindowTitle(QStringLiteral("Keyboard Test"));
    setMinimumSize(900, 380);
    setAttribute(Qt::WA_DeleteOnClose, false); // caller sets WA_DeleteOnClose

    // -- Root layout --------------------------------------------------------
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(16, 16, 16, 16);
    root->setSpacing(10);

    // -- Title label --------------------------------------------------------
    auto* lbl_title = new QLabel(QStringLiteral("Keyboard Test"), this);
    {
        QFont f = lbl_title->font();
        f.setBold(true);
        f.setPointSize(16);
        lbl_title->setFont(f);
    }
    root->addWidget(lbl_title);

    // -- Instructions -------------------------------------------------------
    auto* lbl_instr = new QLabel(
        QStringLiteral("กดปุ่มทุกปุ่มบนคีย์บอร์ด  •  สีเขียว = ผ่าน  •  คลิกขวา = มาร์คว่าเสีย"),
        this
    );
    lbl_instr->setStyleSheet(QStringLiteral("color: #aaaaaa; font-size: 11px;"));
    root->addWidget(lbl_instr);

    // -- Keyboard view ------------------------------------------------------
    m_view = new KeyboardView(
        QStringLiteral(":/keyboard_layouts/en_standard.json"), this);
    root->addWidget(m_view, 1);

    // -- Bottom bar ---------------------------------------------------------
    auto* bottom = new QHBoxLayout();
    bottom->setSpacing(8);

    m_lbl_remaining = new QLabel(this);
    m_lbl_remaining->setStyleSheet(
        QStringLiteral("font-size: 12px; color: #cccccc;"));
    bottom->addWidget(m_lbl_remaining);

    bottom->addStretch(1);

    m_btn_reset = new QPushButton(QStringLiteral("Reset"), this);
    m_btn_reset->setFixedWidth(90);
    bottom->addWidget(m_btn_reset);

    m_btn_done = new QPushButton(QStringLiteral("Done"), this);
    m_btn_done->setFixedWidth(90);
    bottom->addWidget(m_btn_done);

    root->addLayout(bottom);

    // -- Connections --------------------------------------------------------
    connect(m_view,      &KeyboardView::stateChanged,
            this,        &KeyboardWindow::onStateChanged);
    connect(m_btn_reset, &QPushButton::clicked,
            this,        &KeyboardWindow::onResetClicked);
    connect(m_btn_done,  &QPushButton::clicked,
            this,        &KeyboardWindow::onDoneClicked);

    // Initialise remaining label
    updateRemainingLabel();

    // Give keyboard focus to the view immediately
    m_view->setFocus();
}

// ---------------------------------------------------------------------------
// updateRemainingLabel
// ---------------------------------------------------------------------------
void KeyboardWindow::updateRemainingLabel()
{
    const int n = m_view->untestedCount();
    m_lbl_remaining->setText(QStringLiteral("เหลือ: %1 ปุ่ม").arg(n));
}

// ---------------------------------------------------------------------------
// Slots
// ---------------------------------------------------------------------------
void KeyboardWindow::onStateChanged()
{
    updateRemainingLabel();
}

void KeyboardWindow::onResetClicked()
{
    m_view->reset();
    updateRemainingLabel();
}

void KeyboardWindow::onDoneClicked()
{
    emit finished(m_view->result());
    close();
}

} // namespace nbi
