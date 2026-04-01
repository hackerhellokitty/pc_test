// ---------------------------------------------------------------------------
// gui/network_window.cpp
// ---------------------------------------------------------------------------

#include "gui/network_window.hpp"

#include <QVBoxLayout>

namespace nbi {

NetworkWindow::NetworkWindow(QWidget* parent)
    : QWidget(parent)
{
    setWindowTitle(QStringLiteral("Network Connectivity"));
    setWindowFlags(Qt::Window);
    resize(520, 420);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_view = new NetworkView(this);
    layout->addWidget(m_view);

    connect(m_view, &NetworkView::finished,
            this,   &NetworkWindow::finished);
    connect(m_view, &NetworkView::finished,
            this,   &NetworkWindow::close);
}

} // namespace nbi
