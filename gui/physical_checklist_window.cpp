// ---------------------------------------------------------------------------
// gui/physical_checklist_window.cpp
// ---------------------------------------------------------------------------

#include "gui/physical_checklist_window.hpp"

#include <QVBoxLayout>

namespace nbi {

PhysicalChecklistWindow::PhysicalChecklistWindow(QWidget* parent)
    : QWidget(parent)
{
    setWindowTitle(QStringLiteral("Physical Inspection"));
    setWindowFlags(Qt::Window);
    resize(560, 620);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_view = new PhysicalChecklistView(this);
    layout->addWidget(m_view);

    connect(m_view, &PhysicalChecklistView::finished, this, &PhysicalChecklistWindow::finished);
    connect(m_view, &PhysicalChecklistView::finished, this, &PhysicalChecklistWindow::close);
}

} // namespace nbi
