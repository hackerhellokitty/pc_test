#pragma once

#include <QWidget>
#include "common/types.hpp"

namespace nbi {

class StorageView;

class StorageWindow : public QWidget {
    Q_OBJECT

public:
    explicit StorageWindow(QWidget* parent = nullptr);

signals:
    void finished(nbi::ModuleResult result);

private slots:
    void onViewFinished(nbi::ModuleResult result);

private:
    StorageView* m_view{nullptr};
};

} // namespace nbi
