#pragma once

// ---------------------------------------------------------------------------
// gui/ports_view.hpp
// Port test: Phase 1 = USB ports, Phase 2 = 3.5mm headphone jack.
// ---------------------------------------------------------------------------

#include <memory>

#include <QAudioSink>
#include <QBuffer>
#include <QByteArray>
#include <QFrame>
#include <QLabel>
#include <QSpinBox>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>
#include <QVector>
#include <QWidget>
#include <windows.h>

#include "common/types.hpp"

namespace nbi {

// ---------------------------------------------------------------------------
// PortSlot — one row representing a single USB port under test
// ---------------------------------------------------------------------------
struct PortSlot {
    int         index{0};
    QString     label;
    bool        detected{false};
    bool        failed{false};

    QFrame*      frame{nullptr};
    QLabel*      name_label{nullptr};
    QLabel*      status_label{nullptr};
    QPushButton* btn_fail{nullptr};
};

// ---------------------------------------------------------------------------
// PortsView
// Phase 1: USB plug detection with 10-second countdown per port.
// Phase 2: 3.5mm jack — instruct user to plug headphones, play tone, confirm.
// ---------------------------------------------------------------------------
class PortsView : public QWidget {
    Q_OBJECT

public:
    explicit PortsView(QWidget* parent = nullptr);
    ~PortsView() override;

signals:
    void finished(nbi::ModuleResult result);

private slots:
    void onSetPortCount();
    void onMarkFailed(int slot_index);
    void onUsbFinishClicked();
    void onCountdownTick();
    // Phase 2 slots
    void onJackPlayClicked();
    void onJackHeard();
    void onJackNotHeard();
    void onJackPlaybackStateChanged(QAudio::State state);

protected:
    bool nativeEvent(const QByteArray& event_type, void* message, qintptr* result) override;

private:
    // -----------------------------------------------------------------------
    // Phase 1 — USB
    // -----------------------------------------------------------------------
    void buildSetupUi();
    void buildTestUi(int count);
    void activateSlotFrame(int idx);
    void advanceToNextSlot(int just_handled);
    void updateSlotUi(int idx);
    void handleUsbArrival();
    void registerDeviceNotification();
    void unregisterDeviceNotification();

    // -----------------------------------------------------------------------
    // Phase 2 — 3.5mm jack
    // -----------------------------------------------------------------------
    void buildJackUi();
    void playJackTone();

    // -----------------------------------------------------------------------
    // Result
    // -----------------------------------------------------------------------
    void emitResult();

    // -----------------------------------------------------------------------
    // Widgets — setup
    // -----------------------------------------------------------------------
    QWidget*     m_setup_widget{nullptr};
    QSpinBox*    m_spn_count{nullptr};

    // -----------------------------------------------------------------------
    // Widgets — USB test
    // -----------------------------------------------------------------------
    QWidget*     m_usb_widget{nullptr};
    QLabel*      m_lbl_instruction{nullptr};
    QVBoxLayout* m_slots_layout{nullptr};
    QPushButton* m_btn_usb_finish{nullptr};

    // -----------------------------------------------------------------------
    // Widgets — 3.5mm test
    // -----------------------------------------------------------------------
    QWidget*     m_jack_widget{nullptr};
    QLabel*      m_lbl_jack_status{nullptr};
    QPushButton* m_btn_jack_play{nullptr};
    QPushButton* m_btn_jack_heard{nullptr};
    QPushButton* m_btn_jack_not_heard{nullptr};

    // -----------------------------------------------------------------------
    // Root layout
    // -----------------------------------------------------------------------
    QVBoxLayout* m_root_layout{nullptr};

    // -----------------------------------------------------------------------
    // State — USB
    // -----------------------------------------------------------------------
    QVector<PortSlot> m_slots;
    int               m_next_slot{0};
    QTimer*           m_countdown_timer{nullptr};
    int               m_countdown_sec{0};
    bool              m_waiting_removal{false};
    HDEVNOTIFY        m_dev_notify{nullptr};

    // -----------------------------------------------------------------------
    // State — 3.5mm
    // -----------------------------------------------------------------------
    std::unique_ptr<QAudioSink> m_jack_sink;
    QBuffer                     m_jack_buffer;
    QByteArray                  m_jack_pcm;
    bool                        m_jack_heard{false};
    bool                        m_jack_outcome_set{false};

    // -----------------------------------------------------------------------
    // Result accumulator
    // -----------------------------------------------------------------------
    bool              m_usb_has_fail{false};
    QStringList       m_usb_failed_names;
    int               m_usb_total{0};
};

} // namespace nbi
