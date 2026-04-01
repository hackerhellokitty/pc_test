// ---------------------------------------------------------------------------
// gui/physical_checklist_view.cpp
// ---------------------------------------------------------------------------

#include "gui/physical_checklist_view.hpp"

#include <QFont>
#include <QFrame>
#include <QHBoxLayout>
#include <QScrollArea>

namespace nbi {

// ---------------------------------------------------------------------------
// Checklist definition
// ---------------------------------------------------------------------------
static const std::vector<ChecklistItem> kChecklistItems = {
    {
        QStringLiteral("lid"),
        QStringLiteral("ฝาครอบ / หน้าจอด้านนอก"),
        QStringLiteral("ตรวจรอยแตก รอยร้าว รอยขีดข่วนลึก บนฝาครอบด้านนอก"),
        QStringLiteral("🖥️"),
        false,
    },
    {
        QStringLiteral("hinge"),
        QStringLiteral("บานพับ (Hinge)"),
        QStringLiteral("เปิด-ปิดฝาหลายครั้ง ตรวจว่าบานพับแน่น ไม่หลวม ไม่มีเสียงแตก ไม่มีรอยแยก — เลือก \"ไม่มี\" ถ้าเป็นแท็บเล็ตหรือไม่มีบานพับ"),
        QStringLiteral("🔩"),
        true,   // allow_na
    },
    {
        QStringLiteral("bezel"),
        QStringLiteral("กรอบจอ (Bezel)"),
        QStringLiteral("ตรวจกรอบรอบจอว่าไม่แตก ไม่ยกออก ไม่มีรอยกดบุ๋ม"),
        QStringLiteral("📺"),
        false,
    },
    {
        QStringLiteral("chassis_top"),
        QStringLiteral("ตัวเครื่องด้านบน (Palm Rest / Keyboard Deck)"),
        QStringLiteral("กดบริเวณพื้นที่วางมือ ตรวจว่าไม่บุ๋ม ไม่แตก ไม่มีรอยขีดข่วนลึก"),
        QStringLiteral("⌨️"),
        false,
    },
    {
        QStringLiteral("chassis_bottom"),
        QStringLiteral("ตัวเครื่องด้านล่าง"),
        QStringLiteral("พลิกเครื่อง ตรวจฝาล่างว่าไม่แตก ไม่มีรอยกระแทก ยางขาตั้งครบ"),
        QStringLiteral("🔋"),
        false,
    },
    {
        QStringLiteral("ports_cover"),
        QStringLiteral("ช่องพอร์ตและฝาปิด"),
        QStringLiteral("ตรวจช่อง USB / HDMI / SD / 3.5mm ว่าไม่หัก ไม่มีสิ่งแปลกปลอมอุด ฝาปิดครบ"),
        QStringLiteral("🔌"),
        false,
    },
    {
        QStringLiteral("keyboard_physical"),
        QStringLiteral("แป้นพิมพ์ (ภายนอก)"),
        QStringLiteral("ตรวจว่าไม่มีปุ่มหลุด ไม่มีปุ่มค้าง กดแล้วสปริงกลับปกติ ไม่มีรอยน้ำหก"),
        QStringLiteral("⌨️"),
        false,
    },
    {
        QStringLiteral("screen_physical"),
        QStringLiteral("หน้าจอ (ภายนอก)"),
        QStringLiteral("ตรวจรอยแตก รอยบุ๋ม pressure mark บนหน้าจอ (ดูขณะจอดำ)"),
        QStringLiteral("🔍"),
        false,
    },
};

// ===========================================================================
// ChecklistCard
// ===========================================================================
ChecklistCard::ChecklistCard(const ChecklistItem& item, QWidget* parent)
    : QWidget(parent), m_item(item)
{
    buildUi();
}

void ChecklistCard::buildUi()
{
    auto* card = new QFrame(this);
    card->setFrameShape(QFrame::StyledPanel);
    card->setObjectName(QStringLiteral("card"));
    card->setStyleSheet(QStringLiteral(
        "QFrame#card { border: 1px solid #444; border-radius: 6px;"
        " background-color: #2b2b2b; padding: 10px; }"
    ));

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->addWidget(card);

    auto* vl = new QVBoxLayout(card);
    vl->setSpacing(6);

    // Header: icon + title
    auto* header = new QHBoxLayout();
    auto* lbl_icon = new QLabel(m_item.icon, card);
    lbl_icon->setStyleSheet(QStringLiteral("font-size: 22px; border: none;"));
    lbl_icon->setFixedWidth(32);

    auto* lbl_title = new QLabel(m_item.title, card);
    {
        QFont f = lbl_title->font();
        f.setBold(true);
        f.setPointSize(12);
        lbl_title->setFont(f);
        lbl_title->setStyleSheet(QStringLiteral("color: #e0e0e0; border: none;"));
    }
    header->addWidget(lbl_icon);
    header->addWidget(lbl_title, 1);
    vl->addLayout(header);

    // Description
    auto* lbl_desc = new QLabel(m_item.description, card);
    lbl_desc->setStyleSheet(QStringLiteral("color: #aaaaaa; font-size: 11px; border: none;"));
    lbl_desc->setWordWrap(true);
    vl->addWidget(lbl_desc);

    // Bottom row: state label + buttons
    auto* bottom_row = new QHBoxLayout();
    bottom_row->setSpacing(8);

    m_lbl_state = new QLabel(QStringLiteral("ยังไม่ตรวจ"), card);
    m_lbl_state->setStyleSheet(QStringLiteral("color: #666; font-size: 11px; border: none;"));

    const QString base_btn = QStringLiteral(
        "QPushButton { font-size: 12px; padding: 4px 14px;"
        " border-radius: 4px; border: 1px solid #555; }"
        "QPushButton:disabled { color: #444; border-color: #333; background: #222; }"
    );

    m_btn_pass = new QPushButton(QStringLiteral("✓ ปกติ"), card);
    m_btn_pass->setStyleSheet(base_btn + QStringLiteral(
        "QPushButton { background-color: #1b5e20; color: #a5d6a7; border-color: #4caf50; }"
        "QPushButton:hover { background-color: #2e7d32; }"
    ));

    m_btn_fail = new QPushButton(QStringLiteral("✗ พบปัญหา"), card);
    m_btn_fail->setStyleSheet(base_btn + QStringLiteral(
        "QPushButton { background-color: #b71c1c; color: #ef9a9a; border-color: #f44336; }"
        "QPushButton:hover { background-color: #c62828; }"
    ));

    bottom_row->addWidget(m_lbl_state);
    bottom_row->addStretch(1);
    bottom_row->addWidget(m_btn_pass);
    bottom_row->addWidget(m_btn_fail);

    if (m_item.allow_na) {
        m_btn_na = new QPushButton(QStringLiteral("— ไม่มี"), card);
        m_btn_na->setStyleSheet(base_btn + QStringLiteral(
            "QPushButton { background-color: #37474f; color: #b0bec5; border-color: #607d8b; }"
            "QPushButton:hover { background-color: #455a64; }"
        ));
        bottom_row->addWidget(m_btn_na);
        connect(m_btn_na, &QPushButton::clicked, this, &ChecklistCard::onNaClicked);
    }

    vl->addLayout(bottom_row);

    connect(m_btn_pass, &QPushButton::clicked, this, &ChecklistCard::onPassClicked);
    connect(m_btn_fail, &QPushButton::clicked, this, &ChecklistCard::onFailClicked);
}

void ChecklistCard::onPassClicked() { setAnswer(ItemAnswer::Pass); }
void ChecklistCard::onFailClicked() { setAnswer(ItemAnswer::Fail); }
void ChecklistCard::onNaClicked()   { setAnswer(ItemAnswer::NotApplicable); }

void ChecklistCard::setAnswer(ItemAnswer a)
{
    m_answer = a;

    // Reset all buttons to enabled first
    m_btn_pass->setEnabled(true);
    m_btn_fail->setEnabled(true);
    if (m_btn_na) m_btn_na->setEnabled(true);

    QString border_color = QStringLiteral("#444");

    switch (a) {
        case ItemAnswer::Pass:
            m_lbl_state->setText(QStringLiteral("✓ ปกติ"));
            m_lbl_state->setStyleSheet(QStringLiteral("color: #4caf50; font-size: 11px; border: none;"));
            m_btn_pass->setEnabled(false);
            border_color = QStringLiteral("#4caf50");
            break;
        case ItemAnswer::Fail:
            m_lbl_state->setText(QStringLiteral("✗ พบปัญหา"));
            m_lbl_state->setStyleSheet(QStringLiteral("color: #f44336; font-size: 11px; border: none;"));
            m_btn_fail->setEnabled(false);
            border_color = QStringLiteral("#f44336");
            break;
        case ItemAnswer::NotApplicable:
            m_lbl_state->setText(QStringLiteral("— ไม่มี / N/A"));
            m_lbl_state->setStyleSheet(QStringLiteral("color: #607d8b; font-size: 11px; border: none;"));
            if (m_btn_na) m_btn_na->setEnabled(false);
            border_color = QStringLiteral("#607d8b");
            break;
        default:
            break;
    }

    auto* card = findChild<QFrame*>(QStringLiteral("card"));
    if (card)
        card->setStyleSheet(
            QStringLiteral("QFrame#card { border: 1px solid %1; border-radius: 6px;"
                           " background-color: #2b2b2b; padding: 10px; }").arg(border_color));

    emit answerChanged();
}

// ===========================================================================
// PhysicalChecklistView
// ===========================================================================
PhysicalChecklistView::PhysicalChecklistView(QWidget* parent)
    : QWidget(parent)
{
    m_items = kChecklistItems;
    buildUi();
    buildChecklist();
    updateDoneButton();
}

void PhysicalChecklistView::buildUi()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(16, 16, 16, 16);
    root->setSpacing(12);

    auto* lbl_title = new QLabel(QStringLiteral("Physical Inspection"), this);
    {
        QFont f = lbl_title->font();
        f.setBold(true);
        f.setPointSize(15);
        lbl_title->setFont(f);
    }
    root->addWidget(lbl_title);

    auto* lbl_desc = new QLabel(
        QStringLiteral("ตรวจสภาพทางกายภาพของเครื่อง — ต้องตอบครบทุกข้อก่อนกด Done"), this);
    lbl_desc->setStyleSheet(QStringLiteral("color: #aaaaaa; font-size: 12px;"));
    lbl_desc->setWordWrap(true);
    root->addWidget(lbl_desc);

    m_lbl_progress = new QLabel(this);
    m_lbl_progress->setStyleSheet(QStringLiteral("color: #888; font-size: 12px;"));
    root->addWidget(m_lbl_progress);

    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet(QStringLiteral("background: transparent;"));

    auto* container = new QWidget(scroll);
    container->setObjectName(QStringLiteral("cardsContainer"));
    container->setStyleSheet(QStringLiteral("background: transparent;"));
    auto* cards_layout = new QVBoxLayout(container);
    cards_layout->setContentsMargins(0, 0, 0, 0);
    cards_layout->setSpacing(10);
    cards_layout->setAlignment(Qt::AlignTop);

    scroll->setWidget(container);
    root->addWidget(scroll, 1);

    auto* bottom = new QHBoxLayout();
    m_btn_done = new QPushButton(QStringLiteral("Done"), this);
    m_btn_done->setFixedWidth(100);
    m_btn_done->setStyleSheet(QStringLiteral(
        "QPushButton { font-size: 12px; padding: 5px 12px;"
        " border: 1px solid #555; border-radius: 4px;"
        " background-color: #3a3a3a; color: #e0e0e0; }"
        "QPushButton:hover:enabled { background-color: #4a4a4a; }"
        "QPushButton:disabled { color: #555; border-color: #3a3a3a; background: #222; }"
    ));
    bottom->addStretch(1);
    bottom->addWidget(m_btn_done);
    root->addLayout(bottom);

    connect(m_btn_done, &QPushButton::clicked, this, &PhysicalChecklistView::onDoneClicked);
}

void PhysicalChecklistView::buildChecklist()
{
    auto* container = findChild<QWidget*>(QStringLiteral("cardsContainer"));
    if (!container) return;
    auto* layout = qobject_cast<QVBoxLayout*>(container->layout());
    if (!layout) return;

    for (const auto& item : m_items) {
        auto* card = new ChecklistCard(item, container);
        connect(card, &ChecklistCard::answerChanged,
                this, &PhysicalChecklistView::onCardAnswered);
        layout->addWidget(card);
        m_cards.push_back(card);
    }
}

void PhysicalChecklistView::onCardAnswered()
{
    updateDoneButton();

    int answered = 0;
    for (auto* c : m_cards)
        if (c->answered()) ++answered;

    emit progressChanged(answered, static_cast<int>(m_cards.size()));
}

void PhysicalChecklistView::updateDoneButton()
{
    const bool all = allAnswered();
    m_btn_done->setEnabled(all);

    int answered = 0;
    for (auto* c : m_cards)
        if (c->answered()) ++answered;

    const int total = static_cast<int>(m_cards.size());
    if (all)
        m_lbl_progress->setText(QStringLiteral("✓ ตอบครบแล้ว — กด Done เพื่อบันทึก"));
    else
        m_lbl_progress->setText(
            QStringLiteral("ตอบแล้ว %1 / %2 ข้อ").arg(answered).arg(total));
}

bool PhysicalChecklistView::allAnswered() const
{
    for (auto* c : m_cards)
        if (!c->answered()) return false;
    return !m_cards.empty();
}

void PhysicalChecklistView::onDoneClicked()
{
    emit finished(result());
}

ModuleResult PhysicalChecklistView::result() const
{
    ModuleResult r;
    r.label = QStringLiteral("Physical");

    if (m_cards.empty()) {
        r.status  = TestStatus::NotRun;
        r.summary = QStringLiteral("ยังไม่ได้ตรวจ");
        return r;
    }

    int fail_count = 0;
    for (std::size_t i = 0; i < m_cards.size(); ++i) {
        if (m_cards[i]->answer() == ItemAnswer::Fail) {
            ++fail_count;
            r.issues.append(
                QStringLiteral("พบปัญหา: %1").arg(m_items[i].title));
        }
    }

    if (!allAnswered()) {
        r.status  = TestStatus::NotRun;
        r.summary = QStringLiteral("ตอบไม่ครบ");
    } else if (fail_count == 0) {
        r.status  = TestStatus::Pass;
        r.summary = QStringLiteral("สภาพดี");
    } else {
        r.status  = TestStatus::Fail;
        r.summary = QStringLiteral("พบปัญหา %1 จุด").arg(fail_count);
    }

    return r;
}

} // namespace nbi
