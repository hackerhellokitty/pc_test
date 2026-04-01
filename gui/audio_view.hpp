#pragma once

// ---------------------------------------------------------------------------
// gui/audio_view.hpp
// Speaker test: enumerate output devices, play sine tone, user confirms.
// ---------------------------------------------------------------------------

#include <memory>
#include <vector>

#include <QAudioFormat>
#include <QAudioSink>
#include <QBuffer>
#include <QByteArray>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#include "common/types.hpp"
#include "core/audio_manager.hpp"

namespace nbi {

// ---------------------------------------------------------------------------
// ToneGenerator — generates a sine-wave PCM buffer
// ---------------------------------------------------------------------------
class ToneGenerator {
public:
    /// Build a stereo 16-bit 44100 Hz sine buffer of given duration.
    static QByteArray generate(double freq_hz, int duration_ms,
                               int sample_rate = 44100, float amplitude = 0.5f);
};

// ---------------------------------------------------------------------------
// DeviceTestCard — one card per output device
// ---------------------------------------------------------------------------
class DeviceTestCard : public QWidget {
    Q_OBJECT
public:
    explicit DeviceTestCard(const AudioDevice& dev, QWidget* parent = nullptr);

    bool heard() const { return m_heard; }
    QString deviceId() const { return m_dev.id; }

signals:
    void outcomeChanged();

private slots:
    void onPlayClicked();
    void onHeardClicked();
    void onNotHeardClicked();
    void onPlaybackFinished();

private:
    void buildUi();
    void playTone();
    void setOutcome(bool heard);

    AudioDevice m_dev;
    bool        m_heard{false};
    bool        m_outcome_set{false};

    QPushButton* m_btn_play{nullptr};
    QPushButton* m_btn_heard{nullptr};
    QPushButton* m_btn_not_heard{nullptr};
    QLabel*      m_lbl_status{nullptr};

    std::unique_ptr<QAudioSink> m_sink;
    QBuffer                     m_buffer;
    QByteArray                  m_pcm_data;
};

// ---------------------------------------------------------------------------
// AudioView
// ---------------------------------------------------------------------------
class AudioView : public QWidget {
    Q_OBJECT
public:
    explicit AudioView(QWidget* parent = nullptr);

    ModuleResult result() const;

signals:
    void finished(nbi::ModuleResult result);

private slots:
    void onDoneClicked();

private:
    void buildUi();
    void populateDevices();

    AudioResult              m_result;
    std::vector<DeviceTestCard*> m_cards;

    QVBoxLayout* m_cards_layout{nullptr};
    QLabel*      m_lbl_status{nullptr};
    QPushButton* m_btn_done{nullptr};
};

} // namespace nbi
