#pragma once

#include <QWidget>
#include "common/types.hpp"
#include "gui/mic_view.hpp"

namespace nbi {

class MicWindow : public QWidget {
    Q_OBJECT
public:
    explicit MicWindow(QWidget* parent = nullptr);

signals:
    void finished(nbi::ModuleResult result);

private:
    MicView* m_view{nullptr};
};

} // namespace nbi
