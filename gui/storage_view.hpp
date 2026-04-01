#pragma once

// ---------------------------------------------------------------------------
// gui/storage_view.hpp
// Widget that displays S.M.A.R.T. scan results for all physical drives.
// ---------------------------------------------------------------------------

#include <vector>

#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QWidget>

#include "common/types.hpp"
#include "core/storage_manager.hpp"

namespace nbi {

class StorageView : public QWidget {
    Q_OBJECT

public:
    explicit StorageView(QWidget* parent = nullptr);

    ModuleResult result() const;

signals:
    void finished(nbi::ModuleResult result);

private slots:
    void onScanClicked();
    void onDoneClicked();

private:
    void buildUi();
    void populateResults(const std::vector<DriveInfo>& drives);
    QWidget* makeDriveCard(const DriveInfo& d);

    QLabel*      m_lbl_status{nullptr};
    QWidget*     m_results_container{nullptr};
    QVBoxLayout* m_results_layout{nullptr};
    QPushButton* m_btn_scan{nullptr};
    QPushButton* m_btn_done{nullptr};

    std::vector<DriveInfo> m_drives;
};

} // namespace nbi
