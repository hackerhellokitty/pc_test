#pragma once

// ---------------------------------------------------------------------------
// gui/main_dashboard.hpp
// Main application window — device summary + module status grid.
// ---------------------------------------------------------------------------

#include <array>

#include <QCheckBox>
#include <QFrame>
#include <QLabel>
#include <QMainWindow>
#include <QVBoxLayout>

#include "common/types.hpp"
#include "gui/audio_window.hpp"
#include "gui/keyboard_window.hpp"
#include "gui/network_window.hpp"
#include "gui/physical_checklist_window.hpp"
#include "gui/screen_window.hpp"
#include "gui/storage_window.hpp"

namespace nbi {

// ---------------------------------------------------------------------------
// ModuleCard — a single diagnostic module tile in the grid
// ---------------------------------------------------------------------------
struct ModuleCard {
    QFrame*  frame{nullptr};
    QLabel*  name_label{nullptr};
    QLabel*  status_label{nullptr};
    QLabel*  dot_label{nullptr};     ///< Coloured "traffic light" indicator
};

// ---------------------------------------------------------------------------
// MainDashboard
// ---------------------------------------------------------------------------
class MainDashboard : public QMainWindow {
    Q_OBJECT

public:
    explicit MainDashboard(QWidget* parent = nullptr);
    ~MainDashboard() override = default;

    /// Update a specific module card with fresh results.
    void updateModuleCard(int index, const ModuleResult& result);

private slots:
    void onScreenCardClicked();
    void onScreenFinished(nbi::ModuleResult result);
    void onKeyboardCardClicked();
    void onKeyboardFinished(nbi::ModuleResult result);
    void onSmartCardClicked();
    void onSmartFinished(nbi::ModuleResult result);
    void onNetworkCardClicked();
    void onNetworkFinished(nbi::ModuleResult result);
    void onAudioCardClicked();
    void onAudioFinished(nbi::ModuleResult result);
    void onPhysicalCardClicked();
    void onPhysicalFinished(nbi::ModuleResult result);

private:
    void loadDeviceInfo();
    void buildUi();
    void populateDeviceLabels();

    /// Return the CSS colour string for a given TestStatus.
    static QString statusColor(TestStatus status);

    /// Return the text symbol for a given TestStatus.
    static QString statusDot(TestStatus status);

    // -----------------------------------------------------------------------
    // Data
    // -----------------------------------------------------------------------
    DeviceInfo m_device_info;

    // -----------------------------------------------------------------------
    // UI elements
    // -----------------------------------------------------------------------
    static constexpr int kModuleCount = 10;

    QLabel*    m_lbl_cpu{nullptr};
    QLabel*    m_lbl_gpu{nullptr};
    QLabel*    m_lbl_ram{nullptr};
    QLabel*    m_lbl_serial{nullptr};
    QLabel*    m_lbl_os{nullptr};

    QCheckBox* m_chk_touchscreen{nullptr};   ///< Screen card — user declares touchscreen present
    QCheckBox* m_chk_no_keyboard{nullptr};  ///< Keyboard card — tablet/no keyboard

    std::array<ModuleCard, kModuleCount> m_cards{};
};

} // namespace nbi
