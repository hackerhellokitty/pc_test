#pragma once

// ---------------------------------------------------------------------------
// gui/audio_window.hpp
// ---------------------------------------------------------------------------

#include <QWidget>
#include "common/types.hpp"
#include "gui/audio_view.hpp"

namespace nbi {

class AudioWindow : public QWidget {
    Q_OBJECT
public:
    explicit AudioWindow(QWidget* parent = nullptr);

signals:
    void finished(nbi::ModuleResult result);

private:
    AudioView* m_view{nullptr};
};

} // namespace nbi
