#pragma once

// ---------------------------------------------------------------------------
// gui/physical_checklist_window.hpp
// ---------------------------------------------------------------------------

#include <QWidget>
#include "common/types.hpp"
#include "gui/physical_checklist_view.hpp"

namespace nbi {

class PhysicalChecklistWindow : public QWidget {
    Q_OBJECT
public:
    explicit PhysicalChecklistWindow(QWidget* parent = nullptr);

signals:
    void finished(nbi::ModuleResult result);

private:
    PhysicalChecklistView* m_view{nullptr};
};

} // namespace nbi
