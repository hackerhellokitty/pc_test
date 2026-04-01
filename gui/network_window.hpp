#pragma once

// ---------------------------------------------------------------------------
// gui/network_window.hpp
// ---------------------------------------------------------------------------

#include <QWidget>
#include "common/types.hpp"
#include "gui/network_view.hpp"

namespace nbi {

class NetworkWindow : public QWidget {
    Q_OBJECT

public:
    explicit NetworkWindow(QWidget* parent = nullptr);

signals:
    void finished(nbi::ModuleResult result);

private:
    NetworkView* m_view{nullptr};
};

} // namespace nbi
