// ---------------------------------------------------------------------------
// gui/mic_view.cpp
// ---------------------------------------------------------------------------

#include "gui/mic_view.hpp"

#include <QAudioDevice>
#include <QAudioFormat>
#include <QFont>
#include <QFrame>
#include <QHBoxLayout>
#include <QMediaDevices>

namespace nbi {

MicView::MicView(QWidget* parent)
    : QWidget(parent)
{
    m_result.label  = QStringLiteral("Microphone");
    m_result.status = TestStatus::NotRun;
    buildUi();
}

MicView::~MicView()
{
    m_rec_timer->stop();

    if (m_source) {
        m_source->stop();
        m_source.reset();
    }
    if (m_rec_buf) {
        m_rec_buf->close();
        delete m_rec_buf;
        m_rec_buf = nullptr;
    }
    if (m_sink) {
        disconnect(m_sink.get(), nullptr, this, nullptr);
        m_sink->stop();
        m_sink.reset();
    }
    if (m_play_buf.isOpen())
        m_play_buf.close();
}

void MicView::buildUi()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(16, 16, 16, 16);
    root->setSpacing(12);

    // Title
    auto* lbl_title = new QLabel(QStringLiteral("Microphone Test"), this);
    {
        QFont f = lbl_title->font();
        f.setBold(true);
        f.setPointSize(15);
        lbl_title->setFont(f);
    }
    root->addWidget(lbl_title);

    // Hint
    m_lbl_hint = new QLabel(
        QStringLiteral("กด Record แล้วพูดหรือเป่าลมใส่ไมค์ ระบบจะอัด 10 วินาที"), this);
    m_lbl_hint->setStyleSheet(QStringLiteral("color: #aaaaaa; font-size: 12px;"));
    m_lbl_hint->setWordWrap(true);
    root->addWidget(m_lbl_hint);

    // Status
    m_lbl_status = new QLabel(QStringLiteral("พร้อมอัดเสียง"), this);
    m_lbl_status->setStyleSheet(QStringLiteral(
        "color: #e0e0e0; font-size: 14px; font-weight: bold;"));
    m_lbl_status->setAlignment(Qt::AlignCenter);
    root->addWidget(m_lbl_status);

    // Level bar (recording indicator)
    m_bar_level = new QProgressBar(this);
    m_bar_level->setRange(0, kRecordSecs);
    m_bar_level->setValue(0);
    m_bar_level->setTextVisible(false);
    m_bar_level->setFixedHeight(10);
    m_bar_level->setStyleSheet(QStringLiteral(
        "QProgressBar { background: #333; border-radius: 5px; border: none; }"
        "QProgressBar::chunk { background: #f44336; border-radius: 5px; }"
    ));
    m_bar_level->setVisible(false);
    root->addWidget(m_bar_level);

    root->addStretch(1);

    // Record button
    m_btn_record = new QPushButton(QStringLiteral("⏺  Record  (10 วิ)"), this);
    m_btn_record->setStyleSheet(QStringLiteral(
        "QPushButton { font-size: 13px; padding: 10px 30px;"
        " border: 2px solid #f44336; border-radius: 6px;"
        " background: #3a1a1a; color: #f44336; }"
        "QPushButton:hover { background: #4d2020; }"
        "QPushButton:disabled { color: #555; border-color: #444; background: #222; }"
    ));
    root->addWidget(m_btn_record, 0, Qt::AlignCenter);

    // Play button
    m_btn_play = new QPushButton(QStringLiteral("▶  Play back"), this);
    m_btn_play->setEnabled(false);
    m_btn_play->setStyleSheet(QStringLiteral(
        "QPushButton { font-size: 13px; padding: 10px 30px;"
        " border: 2px solid #2196f3; border-radius: 6px;"
        " background: #1a2a3a; color: #2196f3; }"
        "QPushButton:hover { background: #203545; }"
        "QPushButton:disabled { color: #555; border-color: #444; background: #222; }"
    ));
    root->addWidget(m_btn_play, 0, Qt::AlignCenter);

    root->addStretch(1);

    // Confirm buttons (hidden until played)
    auto* confirm_row = new QHBoxLayout();

    m_btn_heard = new QPushButton(QStringLiteral("✓  ได้ยิน"), this);
    m_btn_heard->setVisible(false);
    m_btn_heard->setStyleSheet(QStringLiteral(
        "QPushButton { font-size: 12px; padding: 8px 24px;"
        " border: 1px solid #4caf50; border-radius: 5px;"
        " background: #1b3a1f; color: #4caf50; }"
        "QPushButton:hover { background: #254d28; }"
    ));

    m_btn_not_heard = new QPushButton(QStringLiteral("✗  ไม่ได้ยิน"), this);
    m_btn_not_heard->setVisible(false);
    m_btn_not_heard->setStyleSheet(QStringLiteral(
        "QPushButton { font-size: 12px; padding: 8px 24px;"
        " border: 1px solid #f44336; border-radius: 5px;"
        " background: #3a1a1a; color: #f44336; }"
        "QPushButton:hover { background: #4d2020; }"
    ));

    confirm_row->addStretch(1);
    confirm_row->addWidget(m_btn_heard);
    confirm_row->addSpacing(12);
    confirm_row->addWidget(m_btn_not_heard);
    confirm_row->addStretch(1);
    root->addLayout(confirm_row);

    // Timer for recording countdown
    m_rec_timer = new QTimer(this);
    m_rec_timer->setInterval(1000);

    connect(m_btn_record,   &QPushButton::clicked, this, &MicView::onRecordClicked);
    connect(m_btn_play,     &QPushButton::clicked, this, &MicView::onPlayClicked);
    connect(m_btn_heard,    &QPushButton::clicked, this, &MicView::onHeardClicked);
    connect(m_btn_not_heard,&QPushButton::clicked, this, &MicView::onNotHeardClicked);
    connect(m_rec_timer,    &QTimer::timeout,      this, &MicView::onRecordTick);
}

// ---------------------------------------------------------------------------
void MicView::onRecordClicked()
{
    // Stop and release any previous session
    if (m_source) {
        m_source->stop();
        m_source.reset();
    }
    if (m_rec_buf) {
        m_rec_buf->close();
        delete m_rec_buf;
        m_rec_buf = nullptr;
    }

    // Find default input device
    const QAudioDevice dev = QMediaDevices::defaultAudioInput();
    if (dev.isNull()) {
        m_lbl_status->setText(QStringLiteral("ไม่พบไมโครโฟน"));
        m_lbl_status->setStyleSheet(QStringLiteral("color: #f44336; font-size: 14px; font-weight: bold;"));
        return;
    }

    QAudioFormat fmt;
    fmt.setSampleRate(44100);
    fmt.setChannelCount(1);
    fmt.setSampleFormat(QAudioFormat::Int16);

    m_recorded.clear();
    m_source = std::make_unique<QAudioSource>(dev, fmt);

    m_rec_buf = new QBuffer(&m_recorded, this);
    m_rec_buf->open(QIODevice::WriteOnly);
    m_source->start(m_rec_buf);

    m_rec_elapsed = 0;
    m_bar_level->setValue(0);
    m_bar_level->setVisible(true);
    m_btn_record->setEnabled(false);
    m_rec_timer->start();

    setPhase(1);
}

void MicView::onRecordTick()
{
    ++m_rec_elapsed;
    m_bar_level->setValue(m_rec_elapsed);
    m_lbl_status->setText(
        QStringLiteral("กำลังอัด... %1 / %2 วิ")
            .arg(m_rec_elapsed).arg(kRecordSecs));

    if (m_rec_elapsed >= kRecordSecs) {
        m_rec_timer->stop();
        if (m_source) {
            m_source->stop();
            m_source.reset();
        }
        if (m_rec_buf) {
            m_rec_buf->close();
            delete m_rec_buf;
            m_rec_buf = nullptr;
        }
        m_bar_level->setVisible(false);
        setPhase(2);
    }
}

void MicView::onPlayClicked()
{
    if (m_recorded.isEmpty()) return;

    // Stop and release previous sink safely
    if (m_sink) {
        disconnect(m_sink.get(), nullptr, this, nullptr);
        m_sink->stop();
        m_sink.reset();
    }
    if (m_play_buf.isOpen())
        m_play_buf.close();

    const QAudioDevice out_dev = QMediaDevices::defaultAudioOutput();
    if (out_dev.isNull()) {
        m_lbl_status->setText(QStringLiteral("ไม่พบลำโพง"));
        return;
    }

    QAudioFormat fmt;
    fmt.setSampleRate(44100);
    fmt.setChannelCount(1);
    fmt.setSampleFormat(QAudioFormat::Int16);

    m_sink = std::make_unique<QAudioSink>(out_dev, fmt);
    connect(m_sink.get(), &QAudioSink::stateChanged,
            this, &MicView::onPlaybackStateChanged);

    m_play_buf.setData(m_recorded);
    m_play_buf.open(QIODevice::ReadOnly);
    m_sink->start(&m_play_buf);

    m_btn_play->setEnabled(false);
    setPhase(3);
}

void MicView::onPlaybackStateChanged(QAudio::State state)
{
    if (state == QAudio::IdleState || state == QAudio::StoppedState) {
        // Disconnect before stopping to prevent re-entrant signal
        if (m_sink) disconnect(m_sink.get(), nullptr, this, nullptr);
        if (m_play_buf.isOpen()) m_play_buf.close();
        setPhase(4);
    }
}

void MicView::onHeardClicked()
{
    m_result.status  = TestStatus::Pass;
    m_result.summary = QStringLiteral("Microphone ใช้งานได้");
    emit finished(m_result);
}

void MicView::onNotHeardClicked()
{
    m_result.status  = TestStatus::Fail;
    m_result.summary = QStringLiteral("ไม่ได้ยินเสียงจาก Microphone");
    m_result.issues.append(m_result.summary);
    emit finished(m_result);
}

void MicView::setPhase(int phase)
{
    switch (phase) {
    case 1: // recording
        m_lbl_status->setText(QStringLiteral("กำลังอัด... 0 / 10 วิ"));
        m_lbl_status->setStyleSheet(
            QStringLiteral("color: #f44336; font-size: 14px; font-weight: bold;"));
        m_btn_play->setEnabled(false);
        m_btn_heard->setVisible(false);
        m_btn_not_heard->setVisible(false);
        break;

    case 2: // recorded — ready to play
        m_lbl_status->setText(QStringLiteral("อัดเสร็จแล้ว — กด Play เพื่อฟัง"));
        m_lbl_status->setStyleSheet(
            QStringLiteral("color: #2196f3; font-size: 14px; font-weight: bold;"));
        m_btn_record->setEnabled(true);
        m_btn_play->setEnabled(true);
        m_btn_heard->setVisible(false);
        m_btn_not_heard->setVisible(false);
        break;

    case 3: // playing
        m_lbl_status->setText(QStringLiteral("กำลังเล่น..."));
        m_lbl_status->setStyleSheet(
            QStringLiteral("color: #2196f3; font-size: 14px; font-weight: bold;"));
        m_btn_heard->setVisible(false);
        m_btn_not_heard->setVisible(false);
        break;

    case 4: // confirm
        m_lbl_status->setText(QStringLiteral("ได้ยินเสียงที่อัดไว้มั้ย?"));
        m_lbl_status->setStyleSheet(
            QStringLiteral("color: #e0e0e0; font-size: 14px; font-weight: bold;"));
        m_btn_play->setEnabled(true);  // allow replay
        m_btn_heard->setVisible(true);
        m_btn_not_heard->setVisible(true);
        break;

    default: // idle
        m_lbl_status->setText(QStringLiteral("พร้อมอัดเสียง"));
        m_lbl_status->setStyleSheet(
            QStringLiteral("color: #e0e0e0; font-size: 14px; font-weight: bold;"));
        break;
    }
}

ModuleResult MicView::result() const { return m_result; }

} // namespace nbi
