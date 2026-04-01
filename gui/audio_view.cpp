// ---------------------------------------------------------------------------
// gui/audio_view.cpp
// ---------------------------------------------------------------------------

#include "gui/audio_view.hpp"

#include <cmath>

#include <QApplication>
#include <QAudioDevice>
#include <QAudioFormat>
#include <QFont>
#include <QFrame>
#include <QHBoxLayout>
#include <QMediaDevices>
#include <QScrollArea>

#ifndef M_PI
#  define M_PI 3.14159265358979323846
#endif

namespace nbi {

// ===========================================================================
// ToneGenerator
// ===========================================================================
QByteArray ToneGenerator::generate(double freq_hz, int duration_ms,
                                   int sample_rate, float amplitude)
{
    const int num_samples = sample_rate * duration_ms / 1000;
    // Stereo, 16-bit signed PCM = 4 bytes per frame
    QByteArray buf;
    buf.resize(num_samples * 4);

    auto* data = reinterpret_cast<int16_t*>(buf.data());
    const double step = 2.0 * M_PI * freq_hz / sample_rate;

    for (int i = 0; i < num_samples; ++i) {
        // Apply simple fade-in/out (50 ms) to avoid clicks
        float env = 1.0f;
        const int fade_samples = sample_rate * 50 / 1000;
        if (i < fade_samples)
            env = static_cast<float>(i) / fade_samples;
        else if (i > num_samples - fade_samples)
            env = static_cast<float>(num_samples - i) / fade_samples;

        const int16_t sample = static_cast<int16_t>(
            amplitude * env * 32767.0f * static_cast<float>(std::sin(step * i)));
        data[i * 2]     = sample;  // L
        data[i * 2 + 1] = sample;  // R
    }

    return buf;
}

// ===========================================================================
// DeviceTestCard
// ===========================================================================
DeviceTestCard::DeviceTestCard(const AudioDevice& dev, QWidget* parent)
    : QWidget(parent), m_dev(dev)
{
    buildUi();
    // Pre-generate 1 kHz tone, 1.5 s
    m_pcm_data = ToneGenerator::generate(1000.0, 1500);
}

void DeviceTestCard::buildUi()
{
    auto* card = new QFrame(this);
    card->setFrameShape(QFrame::StyledPanel);
    card->setStyleSheet(QStringLiteral(
        "QFrame { border: 1px solid #444; border-radius: 6px;"
        " background-color: #2b2b2b; padding: 10px; }"
    ));

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->addWidget(card);

    auto* vl = new QVBoxLayout(card);
    vl->setSpacing(8);

    // Device name
    const QString badge = m_dev.is_default ? QStringLiteral(" [Default]") : QString{};
    auto* lbl_name = new QLabel(m_dev.name + badge, card);
    {
        QFont f = lbl_name->font();
        f.setBold(true);
        f.setPointSize(12);
        lbl_name->setFont(f);
        lbl_name->setStyleSheet(QStringLiteral("color: #e0e0e0; border: none;"));
    }
    vl->addWidget(lbl_name);

    // Status label
    m_lbl_status = new QLabel(QStringLiteral("กด Play เพื่อเล่นเสียงทดสอบ"), card);
    m_lbl_status->setStyleSheet(QStringLiteral("color: #aaaaaa; font-size: 11px; border: none;"));
    vl->addWidget(m_lbl_status);

    // Button row
    auto* btn_row = new QHBoxLayout();
    btn_row->setSpacing(8);

    const QString base_style = QStringLiteral(
        "QPushButton { font-size: 12px; padding: 5px 14px;"
        " border: 1px solid #555; border-radius: 4px; }"
        "QPushButton:disabled { color: #555; border-color: #3a3a3a; }"
    );

    m_btn_play = new QPushButton(QStringLiteral("▶ Play"), card);
    m_btn_play->setStyleSheet(base_style + QStringLiteral(
        "QPushButton { background-color: #3a3a3a; color: #e0e0e0; }"
        "QPushButton:hover { background-color: #4a4a4a; }"
    ));

    m_btn_heard = new QPushButton(QStringLiteral("✓ ได้ยิน"), card);
    m_btn_heard->setStyleSheet(base_style + QStringLiteral(
        "QPushButton { background-color: #1b5e20; color: #a5d6a7; border-color: #4caf50; }"
        "QPushButton:hover { background-color: #2e7d32; }"
    ));
    m_btn_heard->setEnabled(false);

    m_btn_not_heard = new QPushButton(QStringLiteral("✗ ไม่ได้ยิน"), card);
    m_btn_not_heard->setStyleSheet(base_style + QStringLiteral(
        "QPushButton { background-color: #b71c1c; color: #ef9a9a; border-color: #f44336; }"
        "QPushButton:hover { background-color: #c62828; }"
    ));
    m_btn_not_heard->setEnabled(false);

    btn_row->addWidget(m_btn_play);
    btn_row->addWidget(m_btn_heard);
    btn_row->addWidget(m_btn_not_heard);
    btn_row->addStretch(1);
    vl->addLayout(btn_row);

    connect(m_btn_play,      &QPushButton::clicked, this, &DeviceTestCard::onPlayClicked);
    connect(m_btn_heard,     &QPushButton::clicked, this, &DeviceTestCard::onHeardClicked);
    connect(m_btn_not_heard, &QPushButton::clicked, this, &DeviceTestCard::onNotHeardClicked);
}

void DeviceTestCard::onPlayClicked()
{
    // Stop previous playback
    if (m_sink && m_sink->state() != QAudio::StoppedState)
        m_sink->stop();

    playTone();
}

void DeviceTestCard::playTone()
{
    // Find the QAudioDevice matching our id
    QAudioDevice target_dev;
    for (const QAudioDevice& d : QMediaDevices::audioOutputs()) {
        if (d.id() == m_dev.id) {
            target_dev = d;
            break;
        }
    }
    if (target_dev.isNull()) {
        m_lbl_status->setText(QStringLiteral("ไม่พบอุปกรณ์"));
        return;
    }

    QAudioFormat fmt;
    fmt.setSampleRate(44100);
    fmt.setChannelCount(2);
    fmt.setSampleFormat(QAudioFormat::Int16);

    if (!target_dev.isFormatSupported(fmt)) {
        m_lbl_status->setText(QStringLiteral("Format ไม่รองรับ"));
        return;
    }

    m_sink = std::make_unique<QAudioSink>(target_dev, fmt);
    connect(m_sink.get(), &QAudioSink::stateChanged,
            this, [this](QAudio::State s) {
                if (s == QAudio::IdleState)
                    onPlaybackFinished();
            });

    m_buffer.close();
    m_buffer.setData(m_pcm_data);
    m_buffer.open(QIODevice::ReadOnly);

    m_btn_play->setEnabled(false);
    m_lbl_status->setText(QStringLiteral("กำลังเล่น… ได้ยินเสียงไหม?"));
    m_btn_heard->setEnabled(false);
    m_btn_not_heard->setEnabled(false);

    m_sink->start(&m_buffer);
}

void DeviceTestCard::onPlaybackFinished()
{
    m_btn_play->setEnabled(true);
    if (!m_outcome_set) {
        m_btn_heard->setEnabled(true);
        m_btn_not_heard->setEnabled(true);
        m_lbl_status->setText(QStringLiteral("ได้ยินเสียงไหม?"));
    }
}

void DeviceTestCard::onHeardClicked()
{
    setOutcome(true);
}

void DeviceTestCard::onNotHeardClicked()
{
    setOutcome(false);
}

void DeviceTestCard::setOutcome(bool heard)
{
    m_heard        = heard;
    m_outcome_set  = true;

    m_btn_heard->setEnabled(false);
    m_btn_not_heard->setEnabled(false);

    if (heard) {
        m_lbl_status->setText(QStringLiteral("✓ ได้ยิน — ผ่าน"));
        m_lbl_status->setStyleSheet(QStringLiteral("color: #4caf50; font-size: 11px; border: none;"));
    } else {
        m_lbl_status->setText(QStringLiteral("✗ ไม่ได้ยิน — ไม่ผ่าน"));
        m_lbl_status->setStyleSheet(QStringLiteral("color: #f44336; font-size: 11px; border: none;"));
    }

    emit outcomeChanged();
}

// ===========================================================================
// AudioView
// ===========================================================================
AudioView::AudioView(QWidget* parent)
    : QWidget(parent)
{
    buildUi();
    populateDevices();
}

void AudioView::buildUi()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(16, 16, 16, 16);
    root->setSpacing(12);

    // Title
    auto* lbl_title = new QLabel(QStringLiteral("Speaker Test"), this);
    {
        QFont f = lbl_title->font();
        f.setBold(true);
        f.setPointSize(15);
        lbl_title->setFont(f);
    }
    root->addWidget(lbl_title);

    // Description
    auto* lbl_desc = new QLabel(
        QStringLiteral("กด Play บนแต่ละอุปกรณ์ แล้วยืนยันว่าได้ยินเสียงหรือไม่"), this);
    lbl_desc->setStyleSheet(QStringLiteral("color: #aaaaaa; font-size: 12px;"));
    root->addWidget(lbl_desc);

    // Status
    m_lbl_status = new QLabel(QString{}, this);
    m_lbl_status->setStyleSheet(QStringLiteral("color: #aaaaaa; font-size: 12px;"));
    root->addWidget(m_lbl_status);

    // Scroll area for device cards
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet(QStringLiteral("background: transparent;"));

    auto* container = new QWidget(scroll);
    container->setStyleSheet(QStringLiteral("background: transparent;"));
    m_cards_layout = new QVBoxLayout(container);
    m_cards_layout->setContentsMargins(0, 0, 0, 0);
    m_cards_layout->setSpacing(10);
    m_cards_layout->setAlignment(Qt::AlignTop);

    scroll->setWidget(container);
    root->addWidget(scroll, 1);

    // Bottom bar
    auto* bottom = new QHBoxLayout();
    m_btn_done = new QPushButton(QStringLiteral("Done"), this);
    m_btn_done->setFixedWidth(100);
    m_btn_done->setStyleSheet(QStringLiteral(
        "QPushButton { font-size: 12px; padding: 5px 12px;"
        " border: 1px solid #555; border-radius: 4px;"
        " background-color: #3a3a3a; color: #e0e0e0; }"
        "QPushButton:hover { background-color: #4a4a4a; }"
    ));
    bottom->addStretch(1);
    bottom->addWidget(m_btn_done);
    root->addLayout(bottom);

    connect(m_btn_done, &QPushButton::clicked, this, &AudioView::onDoneClicked);
}

void AudioView::populateDevices()
{
    m_result.outputs = AudioManager::enumerateOutputs();

    if (m_result.outputs.empty()) {
        m_result.no_device = true;
        auto* lbl = new QLabel(
            QStringLiteral("ไม่พบอุปกรณ์เสียง — ข้ามโมดูลนี้"), this);
        lbl->setStyleSheet(QStringLiteral("color: #5c6bc0; font-size: 12px;"));
        m_cards_layout->addWidget(lbl);
        return;
    }

    m_lbl_status->setText(
        QStringLiteral("พบ %1 อุปกรณ์เสียง").arg(m_result.outputs.size()));

    for (const auto& dev : m_result.outputs) {
        auto* card = new DeviceTestCard(dev, this);
        connect(card, &DeviceTestCard::outcomeChanged, this, [this] {
            // Update status summary
            int done = 0;
            for (auto* c : m_cards) {
                // A card is "done" if it has been played and answered
                // We count by checking btn state via a flag approach:
                // DeviceTestCard::heard() only meaningful after outcome set
            }
            (void)done;
        });
        m_cards_layout->addWidget(card);
        m_cards.push_back(card);
    }
}

void AudioView::onDoneClicked()
{
    emit finished(result());
}

ModuleResult AudioView::result() const
{
    AudioResult r = m_result;
    r.outcomes.clear();

    for (auto* card : m_cards) {
        AudioResult::DeviceOutcome o;
        o.device_id = card->deviceId();
        o.heard     = card->heard();
        r.outcomes.push_back(o);
    }

    return AudioManager::evaluate(r);
}

} // namespace nbi
