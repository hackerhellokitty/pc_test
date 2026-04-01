#pragma once

// ---------------------------------------------------------------------------
// gui/mic_view.hpp
// Microphone test:
//   1. กด Record → อัด 10 วิ via QAudioSource
//   2. กด Play → เล่นกลับผ่าน QAudioSink (default output)
//   3. ระบบถาม "ได้ยินมั้ย?" → Pass / Fail
// ---------------------------------------------------------------------------

#include <memory>

#include <QAudioSink>
#include <QAudioSource>
#include <QBuffer>
#include <QByteArray>
#include <QLabel>
#include <QPushButton>
#include <QProgressBar>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#include "common/types.hpp"

namespace nbi {

class MicView : public QWidget {
    Q_OBJECT
public:
    explicit MicView(QWidget* parent = nullptr);
    ModuleResult result() const;

signals:
    void finished(nbi::ModuleResult result);

private slots:
    void onRecordClicked();
    void onPlayClicked();
    void onHeardClicked();
    void onNotHeardClicked();
    void onRecordTick();
    void onPlaybackStateChanged(QAudio::State state);

private:
    void buildUi();
    void setPhase(int phase);  // 0=idle 1=recording 2=recorded 3=playing 4=confirm

    // UI
    QLabel*      m_lbl_status{nullptr};
    QLabel*      m_lbl_hint{nullptr};
    QProgressBar* m_bar_level{nullptr};
    QPushButton* m_btn_record{nullptr};
    QPushButton* m_btn_play{nullptr};
    QPushButton* m_btn_heard{nullptr};
    QPushButton* m_btn_not_heard{nullptr};

    // Audio
    std::unique_ptr<QAudioSource> m_source;
    std::unique_ptr<QAudioSink>   m_sink;
    QByteArray  m_recorded;
    QBuffer     m_play_buf;

    QTimer* m_rec_timer{nullptr};
    int     m_rec_elapsed{0};

    static constexpr int kRecordSecs = 10;

    ModuleResult m_result;
};

} // namespace nbi
