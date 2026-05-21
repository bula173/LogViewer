#include "SearchBar.hpp"

#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QShortcut>
#include <QToolButton>

namespace ui::qt {

SearchBar::SearchBar(QWidget* parent)
    : QWidget(parent)
{
    setFixedHeight(34);
    // Subtle separator at the bottom
    setStyleSheet("SearchBar { border-bottom: 1px solid palette(mid); }");

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(6, 2, 6, 2);
    layout->setSpacing(4);

    auto* label = new QLabel(tr("Find:"), this);
    layout->addWidget(label);

    m_edit = new QLineEdit(this);
    m_edit->setPlaceholderText(tr("Search in events…"));
    m_edit->setMinimumWidth(200);
    m_edit->setClearButtonEnabled(true);
    layout->addWidget(m_edit);

    // Case-sensitive toggle
    m_caseBtn = new QToolButton(this);
    m_caseBtn->setText("Aa");
    m_caseBtn->setCheckable(true);
    m_caseBtn->setToolTip(tr("Match case"));
    m_caseBtn->setFixedWidth(32);
    layout->addWidget(m_caseBtn);

    // Prev / Next
    m_prevBtn = new QPushButton("▲", this);
    m_prevBtn->setToolTip(tr("Previous match (Shift+Enter)"));
    m_prevBtn->setFixedWidth(28);
    m_prevBtn->setEnabled(false);
    layout->addWidget(m_prevBtn);

    m_nextBtn = new QPushButton("▼", this);
    m_nextBtn->setToolTip(tr("Next match (Enter)"));
    m_nextBtn->setFixedWidth(28);
    m_nextBtn->setEnabled(false);
    layout->addWidget(m_nextBtn);

    // Match counter
    m_matchLabel = new QLabel(this);
    m_matchLabel->setMinimumWidth(70);
    m_matchLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    layout->addWidget(m_matchLabel);

    layout->addStretch(1);

    // Close
    m_closeBtn = new QPushButton("✕", this);
    m_closeBtn->setToolTip(tr("Close (Esc)"));
    m_closeBtn->setFixedWidth(28);
    layout->addWidget(m_closeBtn);

    // ── Connections ───────────────────────────────────────────────────────
    connect(m_edit,     &QLineEdit::textChanged, this, &SearchBar::HandleTextChanged);
    connect(m_caseBtn,  &QToolButton::toggled,   this, &SearchBar::HandleCaseToggled);
    connect(m_prevBtn,  &QPushButton::clicked,   this, &SearchBar::NavigatePrev);
    connect(m_nextBtn,  &QPushButton::clicked,   this, &SearchBar::NavigateNext);
    connect(m_closeBtn, &QPushButton::clicked,   this, &SearchBar::CloseBar);

    // Enter → next match; Shift+Enter → previous match
    connect(m_edit, &QLineEdit::returnPressed, this, [this]() {
        emit NavigateNext();
    });

    // Escape closes the bar
    auto* esc = new QShortcut(Qt::Key_Escape, this);
    connect(esc, &QShortcut::activated, this, &SearchBar::CloseBar);
}

// ---------------------------------------------------------------------------
void SearchBar::HandleTextChanged(const QString& text)
{
    emit SearchChanged(text, m_caseBtn->isChecked());
}

void SearchBar::HandleCaseToggled()
{
    emit SearchChanged(m_edit->text(), m_caseBtn->isChecked());
}

void SearchBar::SetMatchInfo(int current, int total)
{
    const bool empty = m_edit->text().isEmpty();
    if (total == 0)
    {
        m_matchLabel->setText(empty ? QString() : tr("No matches"));
        m_matchLabel->setStyleSheet(empty ? QString() : "color: #c0392b;");
    }
    else
    {
        m_matchLabel->setText(tr("%1 / %2").arg(current).arg(total));
        m_matchLabel->setStyleSheet("color: #27ae60;");
    }
    m_prevBtn->setEnabled(total > 0);
    m_nextBtn->setEnabled(total > 0);
}

void SearchBar::FocusInput()
{
    show();
    m_edit->setFocus();
    m_edit->selectAll();
}

void SearchBar::CloseBar()
{
    m_edit->clear();
    hide();
    emit Closed();
}

} // namespace ui::qt
