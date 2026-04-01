// ---------------------------------------------------------------------------
// gui/ports_window.cpp
// ---------------------------------------------------------------------------

#include "gui/ports_window.hpp"
#include <QVBoxLayout>

namespace nbi {

PortsWindow::PortsWindow(QWidget* parent)
    : QWidget(parent, Qt::Window)
{
    setWindowTitle(QStringLiteral("USB Port Test"));
    setMinimumSize(420, 420);
    setAttribute(Qt::WA_DeleteOnClose);

    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);

    m_view = new PortsView(this);
    lay->addWidget(m_view);

    connect(m_view, &PortsView::finished, this, &PortsWindow::finished);
    connect(m_view, &PortsView::finished, this, &PortsWindow::close);
}

} // namespace nbi
