// ---------------------------------------------------------------------------
// gui/audio_window.cpp
// ---------------------------------------------------------------------------

#include "gui/audio_window.hpp"

#include <QVBoxLayout>

namespace nbi {

AudioWindow::AudioWindow(QWidget* parent)
    : QWidget(parent)
{
    setWindowTitle(QStringLiteral("Speaker Test"));
    setWindowFlags(Qt::Window);
    resize(540, 440);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_view = new AudioView(this);
    layout->addWidget(m_view);

    connect(m_view, &AudioView::finished, this, &AudioWindow::finished);
    connect(m_view, &AudioView::finished, this, &AudioWindow::close);
}

} // namespace nbi
