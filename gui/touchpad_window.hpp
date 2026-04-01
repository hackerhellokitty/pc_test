#pragma once

#include <QWidget>
#include "common/types.hpp"
#include "gui/touchpad_view.hpp"

namespace nbi {

class TouchpadWindow : public QWidget {
    Q_OBJECT
public:
    explicit TouchpadWindow(QWidget* parent = nullptr);

signals:
    void finished(nbi::ModuleResult result);

private:
    TouchpadView* m_view{nullptr};
};

} // namespace nbi
