// ---------------------------------------------------------------------------
// gui/keyboard_window.cpp
// ---------------------------------------------------------------------------

#include "gui/keyboard_window.hpp"

#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSizePolicy>
#include <QVBoxLayout>

namespace nbi {

// Layout options: display name → resource path
struct LayoutOption {
    const char* label;
    const char* path;
};

static constexpr LayoutOption kLayouts[] = {
    { "Full (104) — มี Numpad",       ":/keyboard_layouts/en_standard.json" },
    { "TKL (87) — ไม่มี Numpad",      ":/keyboard_layouts/en_tkl.json"      },
};
static constexpr int kLayoutCount = static_cast<int>(std::size(kLayouts));

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------
KeyboardWindow::KeyboardWindow(QWidget* parent)
    : QWidget(parent, Qt::Window)
{
    setWindowTitle(QStringLiteral("Keyboard Test"));
    setMinimumSize(900, 380);
    setAttribute(Qt::WA_DeleteOnClose, false);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(16, 16, 16, 16);
    root->setSpacing(10);

    // -- Title + layout selector row -----------------------------------------
    auto* title_row = new QHBoxLayout();

    auto* lbl_title = new QLabel(QStringLiteral("Keyboard Test"), this);
    {
        QFont f = lbl_title->font();
        f.setBold(true);
        f.setPointSize(16);
        lbl_title->setFont(f);
    }
    title_row->addWidget(lbl_title);
    title_row->addStretch(1);

    auto* lbl_layout = new QLabel(QStringLiteral("Layout:"), this);
    lbl_layout->setStyleSheet(QStringLiteral("color: #aaaaaa; font-size: 12px;"));
    title_row->addWidget(lbl_layout);

    m_combo_layout = new QComboBox(this);
    m_combo_layout->setStyleSheet(QStringLiteral(
        "QComboBox { font-size: 12px; padding: 3px 8px;"
        " border: 1px solid #555; border-radius: 4px;"
        " background-color: #3a3a3a; color: #e0e0e0; }"
        "QComboBox::drop-down { border: none; }"
        "QComboBox QAbstractItemView { background-color: #3a3a3a; color: #e0e0e0; }"
    ));
    for (int i = 0; i < kLayoutCount; ++i)
        m_combo_layout->addItem(QString::fromUtf8(kLayouts[i].label));

    title_row->addWidget(m_combo_layout);
    root->addLayout(title_row);

    // -- Instructions --------------------------------------------------------
    auto* lbl_instr = new QLabel(
        QStringLiteral("กดปุ่มทุกปุ่มบนคีย์บอร์ด  •  สีเขียว = ผ่าน  •  คลิกขวา = มาร์คว่าเสีย"),
        this);
    lbl_instr->setStyleSheet(QStringLiteral("color: #aaaaaa; font-size: 11px;"));
    root->addWidget(lbl_instr);

    // -- View container (swap layout here) -----------------------------------
    m_view_container = new QVBoxLayout();
    m_view_container->setContentsMargins(0, 0, 0, 0);

    m_view = new KeyboardView(
        QStringLiteral(":/keyboard_layouts/en_standard.json"), this);
    m_view_container->addWidget(m_view);
    root->addLayout(m_view_container, 1);

    // -- Bottom bar ----------------------------------------------------------
    auto* bottom = new QHBoxLayout();
    bottom->setSpacing(8);

    m_lbl_remaining = new QLabel(this);
    m_lbl_remaining->setStyleSheet(
        QStringLiteral("font-size: 12px; color: #cccccc;"));
    bottom->addWidget(m_lbl_remaining);

    bottom->addStretch(1);

    const QString btn_style = QStringLiteral(
        "QPushButton { font-size: 12px; padding: 4px 12px;"
        " border: 1px solid #555; border-radius: 4px;"
        " background-color: #3a3a3a; color: #e0e0e0; }"
        "QPushButton:hover { background-color: #4a4a4a; }"
    );

    m_btn_reset = new QPushButton(QStringLiteral("Reset"), this);
    m_btn_reset->setFixedWidth(90);
    m_btn_reset->setStyleSheet(btn_style);
    bottom->addWidget(m_btn_reset);

    m_btn_done = new QPushButton(QStringLiteral("Done"), this);
    m_btn_done->setFixedWidth(90);
    m_btn_done->setStyleSheet(btn_style);
    bottom->addWidget(m_btn_done);

    root->addLayout(bottom);

    // -- Connections ---------------------------------------------------------
    connect(m_view,         &KeyboardView::stateChanged,
            this,           &KeyboardWindow::onStateChanged);
    connect(m_btn_reset,    &QPushButton::clicked,
            this,           &KeyboardWindow::onResetClicked);
    connect(m_btn_done,     &QPushButton::clicked,
            this,           &KeyboardWindow::onDoneClicked);
    connect(m_combo_layout, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,           &KeyboardWindow::onLayoutChanged);

    updateRemainingLabel();
    m_view->setFocus();
}

// ---------------------------------------------------------------------------
// swapLayout — replace KeyboardView with a new layout
// ---------------------------------------------------------------------------
void KeyboardWindow::swapLayout(const QString& json_path)
{
    // Remove old view from layout and delete it
    m_view_container->removeWidget(m_view);
    m_view->deleteLater();

    m_view = new KeyboardView(json_path, this);
    m_view_container->addWidget(m_view);

    connect(m_view, &KeyboardView::stateChanged,
            this,   &KeyboardWindow::onStateChanged);

    updateRemainingLabel();
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

void KeyboardWindow::onLayoutChanged(int index)
{
    if (index < 0 || index >= kLayoutCount) return;
    swapLayout(QString::fromUtf8(kLayouts[index].path));
}

} // namespace nbi
