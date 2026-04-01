// ---------------------------------------------------------------------------
// gui/thermal_window.cpp
// ---------------------------------------------------------------------------

#include "gui/thermal_window.hpp"
#include <QVBoxLayout>

namespace nbi {

ThermalWindow::ThermalWindow(QWidget* parent)
    : QWidget(parent, Qt::Window)
{
    setWindowTitle(QStringLiteral("Temperature Monitor"));
    setMinimumSize(420, 380);
    setAttribute(Qt::WA_DeleteOnClose);

    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);

    m_view = new ThermalView(this);
    lay->addWidget(m_view);

    connect(m_view, &ThermalView::finished, this, &ThermalWindow::finished);
    connect(m_view, &ThermalView::finished, this, &ThermalWindow::close);
}

} // namespace nbi
