// ---------------------------------------------------------------------------
// gui/storage_window.cpp
// ---------------------------------------------------------------------------

#include "gui/storage_window.hpp"
#include "gui/storage_view.hpp"

#include <QVBoxLayout>

namespace nbi {

StorageWindow::StorageWindow(QWidget* parent)
    : QWidget(parent, Qt::Window)
{
    setWindowTitle(QStringLiteral("S.M.A.R.T. Storage Health"));
    setMinimumSize(620, 480);
    setAttribute(Qt::WA_DeleteOnClose, false);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_view = new StorageView(this);
    layout->addWidget(m_view);

    connect(m_view, &StorageView::finished,
            this,   &StorageWindow::onViewFinished);
}

void StorageWindow::onViewFinished(nbi::ModuleResult result)
{
    emit finished(result);
    close();
}

} // namespace nbi
