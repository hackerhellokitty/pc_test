// ---------------------------------------------------------------------------
// gui/ports_view.cpp
// ---------------------------------------------------------------------------

#include "gui/ports_view.hpp"

#include <cmath>

#include <QAudioDevice>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMediaDevices>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QTimer>

#include <dbt.h>
#include <initguid.h>
#include <usbiodef.h>   // GUID_DEVINTERFACE_USB_DEVICE

#ifndef M_PI
#  define M_PI 3.14159265358979323846
#endif

namespace nbi {

static constexpr int kCountdownSec = 10;

// ---------------------------------------------------------------------------
// Helper: generate stereo 16-bit 44100 Hz sine PCM
// ---------------------------------------------------------------------------
static QByteArray makeTone(double freq_hz, int duration_ms)
{
    const int sr          = 44100;
    const int num_samples = sr * duration_ms / 1000;
    QByteArray buf(num_samples * 4, '\0');
    auto* data = reinterpret_cast<int16_t*>(buf.data());
    const double step        = 2.0 * M_PI * freq_hz / sr;
    const int    fade_frames = sr * 50 / 1000;

    for (int i = 0; i < num_samples; ++i) {
        float env = 1.0f;
        if (i < fade_frames)
            env = static_cast<float>(i) / fade_frames;
        else if (i > num_samples - fade_frames)
            env = static_cast<float>(num_samples - i) / fade_frames;

        const int16_t s = static_cast<int16_t>(
            0.5f * env * 32767.0f * static_cast<float>(std::sin(step * i)));
        data[i * 2]     = s;
        data[i * 2 + 1] = s;
    }
    return buf;
}

// ===========================================================================
// Construction / destruction
// ===========================================================================
PortsView::PortsView(QWidget* parent)
    : QWidget(parent)
{
    m_root_layout = new QVBoxLayout(this);
    m_root_layout->setContentsMargins(16, 16, 16, 16);
    m_root_layout->setSpacing(12);

    m_countdown_timer = new QTimer(this);
    m_countdown_timer->setInterval(1000);
    connect(m_countdown_timer, &QTimer::timeout, this, &PortsView::onCountdownTick);

    // Pre-generate 1 kHz tone 2 s for headphone test
    m_jack_pcm = makeTone(1000.0, 2000);

    buildSetupUi();
}

PortsView::~PortsView()
{
    unregisterDeviceNotification();
}

// ===========================================================================
// Phase 0 — Setup
// ===========================================================================
void PortsView::buildSetupUi()
{
    m_setup_widget = new QWidget(this);
    auto* lay = new QVBoxLayout(m_setup_widget);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(12);

    auto* title = new QLabel(QStringLiteral("Port Test"), m_setup_widget);
    title->setStyleSheet(QStringLiteral("font-size: 16px; font-weight: bold; color: #e0e0e0;"));
    title->setAlignment(Qt::AlignCenter);
    lay->addWidget(title);

    auto* desc = new QLabel(
        QStringLiteral("ระบุจำนวน USB port ของเครื่องนี้ก่อนเริ่มทดสอบ\n"
                       "(หลังจากเทส USB จะทดสอบ 3.5mm jack อัตโนมัติ)"),
        m_setup_widget);
    desc->setStyleSheet(QStringLiteral("font-size: 12px; color: #aaaaaa;"));
    desc->setAlignment(Qt::AlignCenter);
    desc->setWordWrap(true);
    lay->addWidget(desc);

    auto* row = new QWidget(m_setup_widget);
    auto* row_lay = new QHBoxLayout(row);
    row_lay->setContentsMargins(0, 0, 0, 0);
    row_lay->setSpacing(8);
    row_lay->setAlignment(Qt::AlignCenter);

    auto* lbl = new QLabel(QStringLiteral("จำนวน USB port:"), row);
    lbl->setStyleSheet(QStringLiteral("font-size: 13px; color: #cccccc;"));
    row_lay->addWidget(lbl);

    m_spn_count = new QSpinBox(row);
    m_spn_count->setRange(1, 8);
    m_spn_count->setValue(2);
    m_spn_count->setStyleSheet(QStringLiteral(
        "QSpinBox { font-size: 13px; padding: 4px 8px;"
        " border: 1px solid #555; border-radius: 4px;"
        " background-color: #3a3a3a; color: #e0e0e0; }"
    ));
    row_lay->addWidget(m_spn_count);
    lay->addWidget(row, 0, Qt::AlignCenter);

    auto* btn_start = new QPushButton(QStringLiteral("เริ่มทดสอบ"), m_setup_widget);
    btn_start->setStyleSheet(QStringLiteral(
        "QPushButton { font-size: 13px; padding: 6px 24px;"
        " border: 1px solid #555; border-radius: 4px;"
        " background-color: #3a3a3a; color: #e0e0e0; }"
        "QPushButton:hover { background-color: #4a4a4a; }"
        "QPushButton:pressed { background-color: #2a2a2a; }"
    ));
    connect(btn_start, &QPushButton::clicked, this, &PortsView::onSetPortCount);
    lay->addWidget(btn_start, 0, Qt::AlignCenter);

    lay->addStretch();
    m_root_layout->addWidget(m_setup_widget);
}

// ===========================================================================
// Phase 1 — USB test
// ===========================================================================
void PortsView::buildTestUi(int count)
{
    m_usb_total = count;

    m_usb_widget = new QWidget(this);
    auto* lay = new QVBoxLayout(m_usb_widget);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(10);

    auto* title = new QLabel(QStringLiteral("USB Port Test  (1/2)"), m_usb_widget);
    title->setStyleSheet(QStringLiteral("font-size: 16px; font-weight: bold; color: #e0e0e0;"));
    title->setAlignment(Qt::AlignCenter);
    lay->addWidget(title);

    m_lbl_instruction = new QLabel(QString{}, m_usb_widget);
    m_lbl_instruction->setStyleSheet(QStringLiteral(
        "font-size: 12px; color: #f0c040;"
        " background-color: #2b2b2b; padding: 6px; border-radius: 4px;"));
    m_lbl_instruction->setAlignment(Qt::AlignCenter);
    m_lbl_instruction->setWordWrap(true);
    lay->addWidget(m_lbl_instruction);

    // Scroll area
    auto* scroll = new QScrollArea(m_usb_widget);
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet(QStringLiteral("QScrollArea { border: none; background: transparent; }"));

    auto* container = new QWidget();
    container->setStyleSheet(QStringLiteral("background: transparent;"));
    m_slots_layout = new QVBoxLayout(container);
    m_slots_layout->setContentsMargins(0, 0, 0, 0);
    m_slots_layout->setSpacing(6);

    const QString btn_fail_style = QStringLiteral(
        "QPushButton { font-size: 11px; padding: 3px 10px;"
        " border: 1px solid #555; border-radius: 4px;"
        " background-color: #4a2a2a; color: #ff8080; }"
        "QPushButton:hover { background-color: #5a3a3a; }"
        "QPushButton:disabled { color: #555; border-color: #333; background-color: #2b2b2b; }"
    );

    m_slots.resize(count);
    for (int i = 0; i < count; ++i) {
        PortSlot& slot = m_slots[i];
        slot.index = i + 1;
        slot.label = QStringLiteral("Port %1").arg(i + 1);

        slot.frame = new QFrame(container);
        slot.frame->setFrameShape(QFrame::StyledPanel);
        slot.frame->setStyleSheet(QStringLiteral(
            "QFrame { border: 1px solid #3a3a3a; border-radius: 6px; background-color: #2b2b2b; }"));

        auto* rl = new QHBoxLayout(slot.frame);
        rl->setContentsMargins(10, 6, 10, 6);
        rl->setSpacing(8);

        slot.name_label = new QLabel(slot.label, slot.frame);
        slot.name_label->setStyleSheet(QStringLiteral(
            "font-size: 13px; font-weight: bold; color: #cccccc; border: none;"));
        slot.name_label->setMinimumWidth(80);
        rl->addWidget(slot.name_label);

        slot.status_label = new QLabel(QStringLiteral("รอ…"), slot.frame);
        slot.status_label->setStyleSheet(QStringLiteral(
            "font-size: 12px; color: #888888; border: none;"));
        rl->addWidget(slot.status_label, 1);

        slot.btn_fail = new QPushButton(QStringLiteral("ไม่ตอบสนอง"), slot.frame);
        slot.btn_fail->setStyleSheet(btn_fail_style);
        slot.btn_fail->setEnabled(false);
        const int idx = i;
        connect(slot.btn_fail, &QPushButton::clicked, this, [this, idx]() {
            onMarkFailed(idx);
        });
        rl->addWidget(slot.btn_fail);

        m_slots_layout->addWidget(slot.frame);
    }
    m_slots_layout->addStretch();
    scroll->setWidget(container);
    lay->addWidget(scroll, 1);

    // Activate first slot
    if (!m_slots.isEmpty()) {
        m_slots[0].btn_fail->setEnabled(true);
        activateSlotFrame(0);
        m_lbl_instruction->setText(
            QStringLiteral("กรุณาเสียบ USB drive เข้า %1").arg(m_slots[0].label));
    }

    m_btn_usb_finish = new QPushButton(QStringLiteral("เสร็จสิ้น USB → ไปต่อ 3.5mm"), m_usb_widget);
    m_btn_usb_finish->setEnabled(false);
    m_btn_usb_finish->setStyleSheet(QStringLiteral(
        "QPushButton { font-size: 13px; padding: 6px 24px;"
        " border: 1px solid #555; border-radius: 4px;"
        " background-color: #3a3a3a; color: #e0e0e0; }"
        "QPushButton:hover { background-color: #4a4a4a; }"
        "QPushButton:pressed { background-color: #2a2a2a; }"
        "QPushButton:disabled { color: #555; border-color: #333; }"
    ));
    connect(m_btn_usb_finish, &QPushButton::clicked, this, &PortsView::onUsbFinishClicked);
    lay->addWidget(m_btn_usb_finish, 0, Qt::AlignCenter);

    m_root_layout->addWidget(m_usb_widget);
    registerDeviceNotification();
}

void PortsView::activateSlotFrame(int idx)
{
    if (idx < 0 || idx >= m_slots.size()) return;
    m_slots[idx].frame->setStyleSheet(QStringLiteral(
        "QFrame { border: 1px solid #f0c040; border-radius: 6px; background-color: #2b2b2b; }"));
}

void PortsView::onSetPortCount()
{
    const int count = m_spn_count->value();
    m_setup_widget->hide();
    buildTestUi(count);
}

void PortsView::onMarkFailed(int slot_index)
{
    if (slot_index < 0 || slot_index >= m_slots.size()) return;
    if (m_waiting_removal) return;

    bool ok = false;
    const QString name = QInputDialog::getText(
        this,
        QStringLiteral("ระบุตำแหน่ง port"),
        QStringLiteral("ระบุตำแหน่งของ port ที่ไม่ตอบสนอง\nเช่น: ซ้าย, ซ้ายบน, ขวาล่าง"),
        QLineEdit::Normal,
        m_slots[slot_index].label,
        &ok
    );
    if (!ok) return;

    PortSlot& slot = m_slots[slot_index];
    slot.failed = true;
    if (!name.trimmed().isEmpty())
        slot.label = name.trimmed();

    updateSlotUi(slot_index);
    advanceToNextSlot(slot_index);
}

void PortsView::handleUsbArrival()
{
    if (m_next_slot >= m_slots.size()) return;
    if (m_waiting_removal) return;

    m_waiting_removal = true;
    m_countdown_sec   = kCountdownSec;

    m_slots[m_next_slot].btn_fail->setEnabled(false);
    m_slots[m_next_slot].status_label->setText(
        QStringLiteral("ตรวจพบ USB — กรุณาถอดออกใน %1 วิ…").arg(m_countdown_sec));
    m_slots[m_next_slot].status_label->setStyleSheet(QStringLiteral(
        "font-size: 12px; color: #f0c040; border: none;"));
    m_lbl_instruction->setText(
        QStringLiteral("ตรวจพบ USB ใน %1 — กรุณาถอด USB แล้วเสียบ port ถัดไป (%2 วิ)")
            .arg(m_slots[m_next_slot].label).arg(m_countdown_sec));

    m_countdown_timer->start();
}

void PortsView::onCountdownTick()
{
    --m_countdown_sec;

    if (m_next_slot < m_slots.size()) {
        m_slots[m_next_slot].status_label->setText(
            QStringLiteral("ตรวจพบ USB — กรุณาถอดออกใน %1 วิ…").arg(m_countdown_sec));
        m_lbl_instruction->setText(
            QStringLiteral("ตรวจพบ USB ใน %1 — กรุณาถอด USB แล้วเสียบ port ถัดไป (%2 วิ)")
                .arg(m_slots[m_next_slot].label).arg(m_countdown_sec));
    }

    if (m_countdown_sec > 0) return;

    m_countdown_timer->stop();
    m_waiting_removal = false;

    const int handled = m_next_slot;
    m_slots[handled].detected = true;
    updateSlotUi(handled);
    advanceToNextSlot(handled);
}

void PortsView::advanceToNextSlot(int just_handled)
{
    if (just_handled >= 0 && just_handled < m_slots.size())
        m_slots[just_handled].btn_fail->setEnabled(false);

    m_next_slot = just_handled + 1;
    while (m_next_slot < m_slots.size() &&
           (m_slots[m_next_slot].detected || m_slots[m_next_slot].failed))
        ++m_next_slot;

    if (m_next_slot < m_slots.size()) {
        PortSlot& next = m_slots[m_next_slot];
        next.btn_fail->setEnabled(true);
        activateSlotFrame(m_next_slot);
        m_lbl_instruction->setText(
            QStringLiteral("กรุณาเสียบ USB drive เข้า %1").arg(next.label));
    } else {
        m_lbl_instruction->setText(QStringLiteral("ทดสอบ USB ครบทุก port แล้ว — กดปุ่มด้านล่างเพื่อทดสอบ 3.5mm"));
        m_btn_usb_finish->setEnabled(true);
    }
}

void PortsView::updateSlotUi(int idx)
{
    if (idx < 0 || idx >= m_slots.size()) return;
    PortSlot& slot = m_slots[idx];

    if (slot.detected) {
        slot.name_label->setText(slot.label);
        slot.status_label->setText(QStringLiteral("✓ OK"));
        slot.status_label->setStyleSheet(QStringLiteral("font-size: 12px; color: #4caf50; border: none;"));
        slot.frame->setStyleSheet(QStringLiteral(
            "QFrame { border: 1px solid #4caf50; border-radius: 6px; background-color: #2b2b2b; }"));
    } else if (slot.failed) {
        slot.name_label->setText(slot.label);
        slot.status_label->setText(QStringLiteral("✗ ไม่ตอบสนอง"));
        slot.status_label->setStyleSheet(QStringLiteral("font-size: 12px; color: #f44336; border: none;"));
        slot.frame->setStyleSheet(QStringLiteral(
            "QFrame { border: 1px solid #f44336; border-radius: 6px; background-color: #2b2b2b; }"));
    }
}

void PortsView::onUsbFinishClicked()
{
    // Collect USB result
    for (const PortSlot& s : m_slots) {
        if (s.failed) {
            m_usb_has_fail = true;
            m_usb_failed_names << s.label;
        }
    }

    unregisterDeviceNotification();
    m_countdown_timer->stop();

    m_usb_widget->hide();
    buildJackUi();
}

// ===========================================================================
// Phase 2 — 3.5mm jack
// ===========================================================================
void PortsView::buildJackUi()
{
    m_jack_widget = new QWidget(this);
    auto* lay = new QVBoxLayout(m_jack_widget);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(16);

    auto* title = new QLabel(QStringLiteral("3.5mm Jack Test  (2/2)"), m_jack_widget);
    title->setStyleSheet(QStringLiteral("font-size: 16px; font-weight: bold; color: #e0e0e0;"));
    title->setAlignment(Qt::AlignCenter);
    lay->addWidget(title);

    // Instruction banner
    auto* lbl_instr = new QLabel(
        QStringLiteral("กรุณาเสียบหูฟัง (3.5mm) เข้าช่องเสียงของเครื่อง\nแล้วกด Play เพื่อทดสอบ"),
        m_jack_widget);
    lbl_instr->setStyleSheet(QStringLiteral(
        "font-size: 12px; color: #f0c040;"
        " background-color: #2b2b2b; padding: 8px; border-radius: 4px;"));
    lbl_instr->setAlignment(Qt::AlignCenter);
    lbl_instr->setWordWrap(true);
    lay->addWidget(lbl_instr);

    // Card frame
    auto* card = new QFrame(m_jack_widget);
    card->setFrameShape(QFrame::StyledPanel);
    card->setStyleSheet(QStringLiteral(
        "QFrame { border: 1px solid #444; border-radius: 6px; background-color: #2b2b2b; }"));
    auto* card_lay = new QVBoxLayout(card);
    card_lay->setContentsMargins(16, 12, 16, 12);
    card_lay->setSpacing(10);

    m_lbl_jack_status = new QLabel(QStringLiteral("กด Play เพื่อเล่นเสียงทดสอบ"), card);
    m_lbl_jack_status->setStyleSheet(QStringLiteral("font-size: 12px; color: #aaaaaa; border: none;"));
    m_lbl_jack_status->setAlignment(Qt::AlignCenter);
    card_lay->addWidget(m_lbl_jack_status);

    const QString base_style = QStringLiteral(
        "QPushButton { font-size: 12px; padding: 5px 18px;"
        " border: 1px solid #555; border-radius: 4px; }"
        "QPushButton:disabled { color: #555; border-color: #3a3a3a; background-color: #2b2b2b; }"
    );

    auto* btn_row = new QHBoxLayout();
    btn_row->setSpacing(8);
    btn_row->setAlignment(Qt::AlignCenter);

    m_btn_jack_play = new QPushButton(QStringLiteral("▶ Play"), card);
    m_btn_jack_play->setStyleSheet(base_style + QStringLiteral(
        "QPushButton { background-color: #3a3a3a; color: #e0e0e0; }"
        "QPushButton:hover { background-color: #4a4a4a; }"
    ));

    m_btn_jack_heard = new QPushButton(QStringLiteral("✓ ได้ยิน"), card);
    m_btn_jack_heard->setStyleSheet(base_style + QStringLiteral(
        "QPushButton { background-color: #1b5e20; color: #a5d6a7; border-color: #4caf50; }"
        "QPushButton:hover { background-color: #2e7d32; }"
    ));
    m_btn_jack_heard->setEnabled(false);

    m_btn_jack_not_heard = new QPushButton(QStringLiteral("✗ ไม่ได้ยิน"), card);
    m_btn_jack_not_heard->setStyleSheet(base_style + QStringLiteral(
        "QPushButton { background-color: #b71c1c; color: #ef9a9a; border-color: #f44336; }"
        "QPushButton:hover { background-color: #c62828; }"
    ));
    m_btn_jack_not_heard->setEnabled(false);

    btn_row->addWidget(m_btn_jack_play);
    btn_row->addWidget(m_btn_jack_heard);
    btn_row->addWidget(m_btn_jack_not_heard);
    card_lay->addLayout(btn_row);

    lay->addWidget(card);
    lay->addStretch();

    connect(m_btn_jack_play,      &QPushButton::clicked, this, &PortsView::onJackPlayClicked);
    connect(m_btn_jack_heard,     &QPushButton::clicked, this, &PortsView::onJackHeard);
    connect(m_btn_jack_not_heard, &QPushButton::clicked, this, &PortsView::onJackNotHeard);

    m_root_layout->addWidget(m_jack_widget);
}

void PortsView::onJackPlayClicked()
{
    if (m_jack_sink && m_jack_sink->state() != QAudio::StoppedState)
        m_jack_sink->stop();

    playJackTone();
}

void PortsView::playJackTone()
{
    const QAudioDevice dev = QMediaDevices::defaultAudioOutput();
    if (dev.isNull()) {
        m_lbl_jack_status->setText(QStringLiteral("ไม่พบอุปกรณ์เสียง"));
        return;
    }

    QAudioFormat fmt;
    fmt.setSampleRate(44100);
    fmt.setChannelCount(2);
    fmt.setSampleFormat(QAudioFormat::Int16);

    if (!dev.isFormatSupported(fmt)) {
        m_lbl_jack_status->setText(QStringLiteral("Format ไม่รองรับ"));
        return;
    }

    m_jack_sink = std::make_unique<QAudioSink>(dev, fmt);
    connect(m_jack_sink.get(), &QAudioSink::stateChanged,
            this, &PortsView::onJackPlaybackStateChanged);

    m_jack_buffer.close();
    m_jack_buffer.setData(m_jack_pcm);
    m_jack_buffer.open(QIODevice::ReadOnly);

    m_btn_jack_play->setEnabled(false);
    m_btn_jack_heard->setEnabled(false);
    m_btn_jack_not_heard->setEnabled(false);
    m_lbl_jack_status->setText(QStringLiteral("กำลังเล่น… ได้ยินเสียงในหูฟังไหม?"));
    m_lbl_jack_status->setStyleSheet(QStringLiteral("font-size: 12px; color: #f0c040; border: none;"));

    m_jack_sink->start(&m_jack_buffer);
}

void PortsView::onJackPlaybackStateChanged(QAudio::State state)
{
    if (state != QAudio::IdleState) return;
    m_btn_jack_play->setEnabled(true);
    if (!m_jack_outcome_set) {
        m_btn_jack_heard->setEnabled(true);
        m_btn_jack_not_heard->setEnabled(true);
        m_lbl_jack_status->setText(QStringLiteral("ได้ยินเสียงในหูฟังไหม?"));
        m_lbl_jack_status->setStyleSheet(QStringLiteral("font-size: 12px; color: #aaaaaa; border: none;"));
    }
}

void PortsView::onJackHeard()
{
    m_jack_heard       = true;
    m_jack_outcome_set = true;
    m_btn_jack_heard->setEnabled(false);
    m_btn_jack_not_heard->setEnabled(false);
    m_lbl_jack_status->setText(QStringLiteral("✓ ได้ยิน — 3.5mm jack ปกติ"));
    m_lbl_jack_status->setStyleSheet(QStringLiteral("font-size: 12px; color: #4caf50; border: none;"));
    emitResult();
}

void PortsView::onJackNotHeard()
{
    m_jack_heard       = false;
    m_jack_outcome_set = true;
    m_btn_jack_heard->setEnabled(false);
    m_btn_jack_not_heard->setEnabled(false);
    m_lbl_jack_status->setText(QStringLiteral("✗ ไม่ได้ยิน — 3.5mm jack ผิดปกติ"));
    m_lbl_jack_status->setStyleSheet(QStringLiteral("font-size: 12px; color: #f44336; border: none;"));
    emitResult();
}

// ===========================================================================
// Result
// ===========================================================================
void PortsView::emitResult()
{
    if (m_jack_sink)
        m_jack_sink->stop();

    ModuleResult r;
    r.label = QStringLiteral("Ports");

    const bool jack_ok = m_jack_heard;
    const bool usb_ok  = !m_usb_has_fail;

    if (usb_ok && jack_ok) {
        r.status  = TestStatus::Pass;
        r.summary = QStringLiteral("USB + 3.5mm ปกติ (%1 port)").arg(m_usb_total);
    } else {
        r.status = TestStatus::Fail;
        QStringList parts;
        if (!usb_ok)
            parts << QStringLiteral("USB %1 port ผิดปกติ").arg(m_usb_failed_names.size());
        if (!jack_ok)
            parts << QStringLiteral("3.5mm jack ไม่ตอบสนอง");
        r.summary = parts.join(QStringLiteral(", "));

        for (const QString& name : m_usb_failed_names)
            r.issues << QStringLiteral("USB port ไม่ตอบสนอง: %1").arg(name);
        if (!jack_ok)
            r.issues << QStringLiteral("3.5mm jack: ไม่ได้ยินเสียง");
    }

    emit finished(r);
}

// ===========================================================================
// Device notification
// ===========================================================================
void PortsView::registerDeviceNotification()
{
    HWND hwnd = reinterpret_cast<HWND>(winId());
    if (!hwnd) return;

    DEV_BROADCAST_DEVICEINTERFACE filter{};
    filter.dbcc_size       = sizeof(filter);
    filter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
    filter.dbcc_classguid  = GUID_DEVINTERFACE_USB_DEVICE;

    m_dev_notify = RegisterDeviceNotificationW(
        hwnd, &filter, DEVICE_NOTIFY_WINDOW_HANDLE);
}

void PortsView::unregisterDeviceNotification()
{
    if (m_dev_notify) {
        UnregisterDeviceNotification(m_dev_notify);
        m_dev_notify = nullptr;
    }
}

bool PortsView::nativeEvent(const QByteArray& /*event_type*/,
                             void* message, qintptr* /*result*/)
{
    MSG* msg = static_cast<MSG*>(message);
    if (msg->message == WM_DEVICECHANGE &&
        msg->wParam  == DBT_DEVICEARRIVAL)
    {
        auto* hdr = reinterpret_cast<DEV_BROADCAST_HDR*>(msg->lParam);
        if (hdr && hdr->dbch_devicetype == DBT_DEVTYP_DEVICEINTERFACE)
            handleUsbArrival();
    }
    return false;
}

} // namespace nbi
