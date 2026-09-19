#include "ColumnFilterPopup.hpp"

#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

namespace ui::qt
{

namespace {
constexpr int kValueRole = Qt::UserRole;
}

ColumnFilterPopup::ColumnFilterPopup(const QString& columnTitle,
    const ColumnDistinctValues& distinct,
    const std::optional<QSet<QString>>& allowed,
    QWidget* parent)
    : QDialog(parent, Qt::Popup)
{
    setWindowTitle(tr("Filter: %1").arg(columnTitle));
    setMinimumWidth(280);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);

    m_search = new QLineEdit(this);
    m_search->setObjectName("columnFilterSearch");
    m_search->setPlaceholderText(tr("Search values…"));
    m_search->setClearButtonEnabled(true);
    layout->addWidget(m_search);

    m_selectAll = new QCheckBox(tr("(Select all)"), this);
    m_selectAll->setObjectName("columnFilterSelectAll");
    m_selectAll->setTristate(true);
    layout->addWidget(m_selectAll);

    m_list = new QListWidget(this);
    m_list->setObjectName("columnFilterList");
    m_list->setMinimumHeight(220);
    layout->addWidget(m_list, 1);

    m_updating = true;
    for (const auto& v : distinct.values)
    {
        const QString shown = v.value.isEmpty() ? tr("(empty)") : v.value;
        auto* item = new QListWidgetItem(QStringLiteral("%1   (%2)").arg(shown).arg(v.count), m_list);
        item->setData(kValueRole, v.value);
        item->setData(Qt::UserRole + 1, shown); // text the search box matches against
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(!allowed || allowed->contains(v.value) ? Qt::Checked : Qt::Unchecked);
    }
    m_updating = false;

    if (distinct.truncated)
    {
        auto* note = new QLabel(
            tr("Too many distinct values — showing the first %1.").arg(distinct.values.size()), this);
        note->setWordWrap(true);
        note->setStyleSheet("color: gray; font-style: italic;");
        layout->addWidget(note);
    }

    auto* buttons = new QHBoxLayout();
    auto* clear = new QPushButton(tr("Clear filter"), this);
    clear->setObjectName("columnFilterClear");
    auto* ok = new QPushButton(tr("OK"), this);
    ok->setObjectName("columnFilterOk");
    ok->setDefault(true);
    auto* cancel = new QPushButton(tr("Cancel"), this);
    cancel->setObjectName("columnFilterCancel");
    buttons->addWidget(clear);
    buttons->addStretch();
    buttons->addWidget(ok);
    buttons->addWidget(cancel);
    layout->addLayout(buttons);

    connect(m_search, &QLineEdit::textChanged, this, &ColumnFilterPopup::OnSearchChanged);
    connect(m_selectAll, &QCheckBox::clicked, this, &ColumnFilterPopup::OnSelectAllToggled);
    connect(m_list, &QListWidget::itemChanged, this, &ColumnFilterPopup::OnItemChanged);
    connect(ok, &QPushButton::clicked, this, &ColumnFilterPopup::Accept);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(clear, &QPushButton::clicked, this, [this] {
        emit Cleared();
        accept();
    });

    UpdateSelectAllState();
    m_search->setFocus();
}

QSet<QString> ColumnFilterPopup::CheckedValues() const
{
    QSet<QString> out;
    for (int i = 0; i < m_list->count(); ++i)
    {
        const auto* item = m_list->item(i);
        if (item->checkState() == Qt::Checked)
            out.insert(item->data(kValueRole).toString());
    }
    return out;
}

void ColumnFilterPopup::OnSearchChanged(const QString& text)
{
    for (int i = 0; i < m_list->count(); ++i)
    {
        auto* item = m_list->item(i);
        item->setHidden(!text.isEmpty() &&
            !item->data(Qt::UserRole + 1).toString().contains(text, Qt::CaseInsensitive));
    }
    UpdateSelectAllState();
}

// "Select all" acts on the rows currently visible (i.e. the search results).
void ColumnFilterPopup::OnSelectAllToggled()
{
    bool anyUnchecked = false;
    for (int i = 0; i < m_list->count(); ++i)
    {
        const auto* item = m_list->item(i);
        if (!item->isHidden() && item->checkState() != Qt::Checked)
            anyUnchecked = true;
    }
    const auto target = anyUnchecked ? Qt::Checked : Qt::Unchecked;

    m_updating = true;
    for (int i = 0; i < m_list->count(); ++i)
        if (!m_list->item(i)->isHidden())
            m_list->item(i)->setCheckState(target);
    m_updating = false;
    UpdateSelectAllState();
}

void ColumnFilterPopup::OnItemChanged(QListWidgetItem*)
{
    if (!m_updating)
        UpdateSelectAllState();
}

void ColumnFilterPopup::UpdateSelectAllState()
{
    int visible = 0, checked = 0;
    for (int i = 0; i < m_list->count(); ++i)
    {
        const auto* item = m_list->item(i);
        if (item->isHidden())
            continue;
        ++visible;
        if (item->checkState() == Qt::Checked)
            ++checked;
    }

    m_updating = true;
    m_selectAll->setCheckState(checked == 0 ? Qt::Unchecked
                               : checked == visible ? Qt::Checked
                                                    : Qt::PartiallyChecked);
    m_updating = false;
}

void ColumnFilterPopup::Accept()
{
    const QSet<QString> checked = CheckedValues();
    if (checked.size() == m_list->count())
        emit Cleared(); // everything allowed = no filter
    else
        emit Applied(checked);
    accept();
}

} // namespace ui::qt
