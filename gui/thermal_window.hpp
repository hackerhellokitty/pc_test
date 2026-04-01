#pragma once

#include <QWidget>
#include "common/types.hpp"
#include "gui/thermal_view.hpp"

namespace nbi {

class ThermalWindow : public QWidget {
    Q_OBJECT
public:
    explicit ThermalWindow(QWidget* parent = nullptr);

signals:
    void finished(nbi::ModuleResult result);

private:
    ThermalView* m_view{nullptr};
};

} // namespace nbi
