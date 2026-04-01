// ---------------------------------------------------------------------------
// gui/summary_page.cpp
// ---------------------------------------------------------------------------
#include "gui/summary_page.hpp"
#include <QFrame>
#include <QScrollArea>
#include <QGroupBox>
#include <QHBoxLayout>
#include "core/scorer.hpp"

namespace nbi {

SummaryPage::SummaryPage(QWidget* parent)
    : QWidget(parent)
{
    buildUi();
}

void SummaryPage::buildUi()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(16, 16, 16, 16);
    root->setSpacing(12);

    // Title
    auto* lbl_title = new QLabel(QStringLiteral("ผลการตรวจสภาพ"), this);
    lbl_title->setAlignment(Qt::AlignCenter);
    lbl_title->setStyleSheet(QStringLiteral("font-size: 18px; font-weight: bold; color: #e0e0e0;"));
    root->addWidget(lbl_title);

    // Score + verdict row
    auto* score_row = new QHBoxLayout();
    m_lbl_score = new QLabel(QStringLiteral("—"), this);
    m_lbl_score->setAlignment(Qt::AlignCenter);
    m_lbl_score->setStyleSheet(QStringLiteral("font-size: 48px; font-weight: bold; color: #e0e0e0;"));

    m_lbl_verdict = new QLabel(QStringLiteral("—"), this);
    m_lbl_verdict->setAlignment(Qt::AlignCenter);
    m_lbl_verdict->setStyleSheet(QStringLiteral("font-size: 28px; font-weight: bold;"));

    score_row->addStretch();
    score_row->addWidget(m_lbl_score);
    score_row->addSpacing(24);
    score_row->addWidget(m_lbl_verdict);
    score_row->addStretch();
    root->addLayout(score_row);

    // Cap warning
    m_lbl_cap = new QLabel(this);
    m_lbl_cap->setAlignment(Qt::AlignCenter);
    m_lbl_cap->setStyleSheet(QStringLiteral("font-size: 12px; color: #f44336;"));
    m_lbl_cap->setVisible(false);
    root->addWidget(m_lbl_cap);

    // Issues section
    auto* issues_group = new QGroupBox(QStringLiteral("รายการปัญหา (เรียงตามความเสี่ยง)"), this);
    issues_group->setStyleSheet(QStringLiteral("QGroupBox { font-size: 13px; font-weight: bold; }"));

    auto* scroll = new QScrollArea(this);
    auto* issues_container = new QWidget();
    m_issues_layout = new QVBoxLayout(issues_container);
    m_issues_layout->setSpacing(6);
    m_issues_layout->addStretch();
    scroll->setWidget(issues_container);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    auto* group_layout = new QVBoxLayout(issues_group);
    group_layout->addWidget(scroll);
    root->addWidget(issues_group, 1);

    // Export button
    m_btn_export = new QPushButton(QStringLiteral("Export Report (PDF + PNG)"), this);
    m_btn_export->setStyleSheet(QStringLiteral(
        "QPushButton { font-size: 13px; padding: 8px 24px;"
        " background-color: #1565c0; color: white; border-radius: 6px; }"
        "QPushButton:hover { background-color: #1976d2; }"
        "QPushButton:pressed { background-color: #0d47a1; }"
    ));
    m_btn_export->setEnabled(false);
    root->addWidget(m_btn_export, 0, Qt::AlignCenter);

    connect(m_btn_export, &QPushButton::clicked, this, &SummaryPage::exportRequested);
}

void SummaryPage::setData(const DeviceInfo& device,
                          const std::array<ModuleResult, 10>& results)
{
    ScoreResult score = Scorer::compute(results);
    populate(device, results, score);
    m_btn_export->setEnabled(true);
}

void SummaryPage::populate(const DeviceInfo& /*device*/,
                            const std::array<ModuleResult, 10>& results,
                            const ScoreResult& score)
{
    // Score label
    m_lbl_score->setText(QStringLiteral("%1").arg(score.final_score));

    // Verdict label
    m_lbl_verdict->setText(score.verdict);
    m_lbl_verdict->setStyleSheet(
        QStringLiteral("font-size: 28px; font-weight: bold; color: %1;")
            .arg(score.verdict_color)
    );

    // Cap label
    if (score.capped) {
        m_lbl_cap->setText(
            QStringLiteral("⚠ คะแนนถูก cap เนื่องจาก: %1").arg(score.cap_reason)
        );
        m_lbl_cap->setVisible(true);
    } else {
        m_lbl_cap->setVisible(false);
    }

    // Clear old issue rows
    while (QLayoutItem* item = m_issues_layout->takeAt(0)) {
        if (QWidget* w = item->widget()) w->deleteLater();
        delete item;
    }

    // Risk-ordered module indices: Battery(3), SMART(4), Screen(0), Keyboard(1),
    //   Thermal(5), Ports(6), Audio(7), Touchpad(2), Network(8), Physical(9)
    static constexpr std::array<int, 10> kRiskOrder = {3,4,0,1,5,6,7,2,8,9};

    static constexpr std::array<const char*, 10> kNames = {
        "Screen","Keyboard","Touchpad","Battery","S.M.A.R.T.",
        "Temperature","Ports","Audio","Network","Physical"
    };

    bool any_issue = false;
    for (int idx : kRiskOrder) {
        const auto& r = results[static_cast<std::size_t>(idx)];
        if (r.status != TestStatus::Fail) continue;
        any_issue = true;

        auto* row = new QFrame(this);
        row->setStyleSheet(QStringLiteral(
            "QFrame { background-color: #3a1a1a; border: 1px solid #f44336; border-radius: 4px; padding: 4px; }"
        ));
        auto* row_layout = new QVBoxLayout(row);
        row_layout->setContentsMargins(8, 4, 8, 4);

        auto* lbl_title = new QLabel(
            QStringLiteral("🔴 %1 — %2").arg(
                QString::fromUtf8(kNames[static_cast<std::size_t>(idx)]),
                r.summary
            ), row
        );
        lbl_title->setStyleSheet(QStringLiteral("font-size: 13px; font-weight: bold; color: #f44336; border: none;"));
        row_layout->addWidget(lbl_title);

        for (const QString& issue : r.issues) {
            auto* lbl_issue = new QLabel(QStringLiteral("  • ") + issue, row);
            lbl_issue->setStyleSheet(QStringLiteral("font-size: 11px; color: #cccccc; border: none;"));
            lbl_issue->setWordWrap(true);
            row_layout->addWidget(lbl_issue);
        }

        m_issues_layout->insertWidget(m_issues_layout->count() - 1, row);
    }

    if (!any_issue) {
        auto* lbl_ok = new QLabel(QStringLiteral("ไม่พบปัญหา"), this);
        lbl_ok->setAlignment(Qt::AlignCenter);
        lbl_ok->setStyleSheet(QStringLiteral("font-size: 13px; color: #4caf50;"));
        m_issues_layout->insertWidget(0, lbl_ok);
    }
}

} // namespace nbi
