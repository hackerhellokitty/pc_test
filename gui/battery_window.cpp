// ---------------------------------------------------------------------------
// gui/battery_window.cpp
// ---------------------------------------------------------------------------

#include "gui/battery_window.hpp"
#include <QVBoxLayout>

namespace nbi {

BatteryWindow::BatteryWindow(QWidget* parent)
    : QWidget(parent)
{
    setWindowTitle(QStringLiteral("Battery Health"));
    setWindowFlags(Qt::Window);
    resize(520, 420);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_view = new BatteryView(this);
    layout->addWidget(m_view);

    connect(m_view, &BatteryView::finished, this, &BatteryWindow::finished);
    connect(m_view, &BatteryView::finished, this, &BatteryWindow::close);
}

} // namespace nbi
