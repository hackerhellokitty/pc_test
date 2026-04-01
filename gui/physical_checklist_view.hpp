#pragma once

// ---------------------------------------------------------------------------
// gui/physical_checklist_view.hpp
// Guided physical inspection checklist — user answers each item before Done.
// ---------------------------------------------------------------------------

#include <vector>

#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

#include "common/types.hpp"

namespace nbi {

// ---------------------------------------------------------------------------
// ChecklistItem — definition of one inspection point
// ---------------------------------------------------------------------------
struct ChecklistItem {
    QString id;
    QString title;
    QString description;
    QString icon;
    bool    allow_na{false};  ///< Show "ไม่มี / N/A" button
};

// ---------------------------------------------------------------------------
// ItemAnswer
// ---------------------------------------------------------------------------
enum class ItemAnswer { Unanswered, Pass, Fail, NotApplicable };

// ---------------------------------------------------------------------------
// ChecklistCard — one card per inspection item
// ---------------------------------------------------------------------------
class ChecklistCard : public QWidget {
    Q_OBJECT
public:
    explicit ChecklistCard(const ChecklistItem& item, QWidget* parent = nullptr);

    ItemAnswer answer() const { return m_answer; }
    QString    itemId() const { return m_item.id; }
    bool       answered() const { return m_answer != ItemAnswer::Unanswered; }

signals:
    void answerChanged();

private slots:
    void onPassClicked();
    void onFailClicked();
    void onNaClicked();

private:
    void buildUi();
    void setAnswer(ItemAnswer a);

    ChecklistItem m_item;
    ItemAnswer    m_answer{ItemAnswer::Unanswered};

    QLabel*      m_lbl_state{nullptr};
    QPushButton* m_btn_pass{nullptr};
    QPushButton* m_btn_fail{nullptr};
    QPushButton* m_btn_na{nullptr};   ///< nullptr if allow_na == false
};

// ---------------------------------------------------------------------------
// PhysicalChecklistView
// ---------------------------------------------------------------------------
class PhysicalChecklistView : public QWidget {
    Q_OBJECT
public:
    explicit PhysicalChecklistView(QWidget* parent = nullptr);

    ModuleResult result() const;
    bool         allAnswered() const;

signals:
    void finished(nbi::ModuleResult result);
    void progressChanged(int answered, int total);

private slots:
    void onDoneClicked();
    void onCardAnswered();

private:
    void buildUi();
    void buildChecklist();
    void updateDoneButton();

    std::vector<ChecklistItem>  m_items;
    std::vector<ChecklistCard*> m_cards;

    QLabel*      m_lbl_progress{nullptr};
    QPushButton* m_btn_done{nullptr};
};

} // namespace nbi
