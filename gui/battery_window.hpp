#pragma once

#include <QWidget>
#include "common/types.hpp"
#include "gui/battery_view.hpp"

namespace nbi {

class BatteryWindow : public QWidget {
    Q_OBJECT
public:
    explicit BatteryWindow(QWidget* parent = nullptr);

signals:
    void finished(nbi::ModuleResult result);

private:
    BatteryView* m_view{nullptr};
};

} // namespace nbi
