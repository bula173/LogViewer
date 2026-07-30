#include "FilterStatusBar.hpp"

#include <QLabel>
#include <QPushButton>
#include <QHBoxLayout>
#include <QStyleOption>
#include <QPainter>

namespace ui::qt
{

FilterStatusBar::FilterStatusBar(QWidget* parent)
    : QWidget(parent)
{
    CreateLayout();
}

void FilterStatusBar::CreateLayout()
{
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 4, 8, 4);
    layout->setSpacing(8);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setWordWrap(false);
    m_statusLabel->setStyleSheet("color: #666; font-size: 11px;");
    layout->addWidget(m_statusLabel, 1);

    m_clearButton = new QPushButton(tr("Clear All"), this);
    m_clearButton->setMaximumWidth(80);
    m_clearButton->setVisible(false);
    layout->addWidget(m_clearButton);

    connect(m_clearButton, &QPushButton::clicked, this, &FilterStatusBar::ClearAllFiltersRequested);

    setStyleSheet("background-color: #f5f5f5; border-top: 1px solid #ddd;");
}

void FilterStatusBar::UpdateFilterStatus(int totalEvents, int filteredCount,
                                         int activeFilterCount,
                                         const QString& filterDetails)
{
    if (activeFilterCount <= 0)
    {
        ClearStatus();
        return;
    }

    // Format the status message
    QString status = QString("📊 %1 filter%2 active | %3 / %4 event%5")
        .arg(activeFilterCount)
        .arg(activeFilterCount != 1 ? "s" : "")
        .arg(filteredCount)
        .arg(totalEvents)
        .arg(totalEvents != 1 ? "s" : "");

    if (!filterDetails.isEmpty())
    {
        status += QString(" | %1").arg(filterDetails);
    }

    m_statusLabel->setText(status);
    m_statusLabel->setToolTip(QString("Active filters: %1").arg(filterDetails));
    m_clearButton->setVisible(true);
    setVisible(true);
}

void FilterStatusBar::ClearStatus()
{
    m_statusLabel->setText(QString());
    m_statusLabel->setToolTip(QString());
    m_clearButton->setVisible(false);
}

} // namespace ui::qt
