// ---------------------------------------------------------------------------
// gui/touchpad_window.cpp
// ---------------------------------------------------------------------------

#include "gui/touchpad_window.hpp"
#include <QVBoxLayout>

namespace nbi {

TouchpadWindow::TouchpadWindow(QWidget* parent)
    : QWidget(parent, Qt::Window)
{
    setWindowTitle(QStringLiteral("Touchpad Test"));
    setMinimumSize(560, 460);
    setAttribute(Qt::WA_DeleteOnClose);

    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);

    m_view = new TouchpadView(this);
    lay->addWidget(m_view);

    connect(m_view, &TouchpadView::finished, this, &TouchpadWindow::finished);
    connect(m_view, &TouchpadView::finished, this, &TouchpadWindow::close);
}

} // namespace nbi
