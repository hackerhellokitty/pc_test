#pragma once

#include <QWidget>
#include "common/types.hpp"
#include "gui/ports_view.hpp"

namespace nbi {

class PortsWindow : public QWidget {
    Q_OBJECT
public:
    explicit PortsWindow(QWidget* parent = nullptr);

signals:
    void finished(nbi::ModuleResult result);

private:
    PortsView* m_view{nullptr};
};

} // namespace nbi
