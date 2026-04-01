// ---------------------------------------------------------------------------
// gui/mic_window.cpp
// ---------------------------------------------------------------------------

#include "gui/mic_window.hpp"
#include <QVBoxLayout>

namespace nbi {

MicWindow::MicWindow(QWidget* parent)
    : QWidget(parent, Qt::Window)
{
    setWindowTitle(QStringLiteral("Microphone Test"));
    setMinimumSize(400, 380);
    setAttribute(Qt::WA_DeleteOnClose);

    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);

    m_view = new MicView(this);
    lay->addWidget(m_view);

    connect(m_view, &MicView::finished, this, &MicWindow::finished);
    connect(m_view, &MicView::finished, this, &MicWindow::close);
}

} // namespace nbi
