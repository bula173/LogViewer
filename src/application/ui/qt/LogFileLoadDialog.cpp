#include "LogFileLoadDialog.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QGroupBox>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QFileInfo>

namespace ui::qt
{

LogFileLoadDialog::LogFileLoadDialog(const QString& filePath, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Load Log File"));
    setMinimumWidth(450);

    auto* mainLayout = new QVBoxLayout(this);

    // Info label
    auto* infoLabel = new QLabel(
        tr("A log file is already loaded. How would you like to proceed?"),
        this);
    infoLabel->setWordWrap(true);
    mainLayout->addWidget(infoLabel);

    mainLayout->addSpacing(10);

    // File info
    QFileInfo fileInfo(filePath);
    auto* fileLabel = new QLabel(
        tr("<b>New file:</b> %1").arg(fileInfo.fileName()),
        this);
    mainLayout->addWidget(fileLabel);

    mainLayout->addSpacing(15);

    // Load mode options
    auto* modeGroup = new QGroupBox(tr("Load Mode"), this);
    auto* modeLayout = new QVBoxLayout(modeGroup);

    m_replaceRadio = new QRadioButton(
        tr("Replace - Clear existing data and load new file"),
        modeGroup);
    m_replaceRadio->setChecked(true);
    modeLayout->addWidget(m_replaceRadio);

    modeLayout->addSpacing(5);

    m_mergeRadio = new QRadioButton(
        tr("Merge - Combine logs by timestamp (adds 'Source' column)"),
        modeGroup);
    modeLayout->addWidget(m_mergeRadio);

    mainLayout->addWidget(modeGroup);

    mainLayout->addSpacing(10);

    // Source aliases for merge mode
    auto* aliasGroup = new QGroupBox(tr("Source Aliases (for merge mode)"), this);
    auto* aliasLayout = new QVBoxLayout(aliasGroup);

    // Existing file alias
    auto* existingLabel = new QLabel(
        tr("Display name for existing loaded data:"),
        aliasGroup);
    aliasLayout->addWidget(existingLabel);

    m_existingFileAliasEdit = new QLineEdit(aliasGroup);
    m_existingFileAliasEdit->setText(tr("Original"));
    m_existingFileAliasEdit->setPlaceholderText(tr("e.g., Server1, Frontend, Database"));
    m_existingFileAliasEdit->setEnabled(false);
    aliasLayout->addWidget(m_existingFileAliasEdit);

    aliasLayout->addSpacing(10);

    // New file alias
    auto* newLabel = new QLabel(
        tr("Display name for new log file:"),
        aliasGroup);
    aliasLayout->addWidget(newLabel);

    m_newFileAliasEdit = new QLineEdit(aliasGroup);
    m_newFileAliasEdit->setText(fileInfo.completeBaseName());
    m_newFileAliasEdit->setPlaceholderText(tr("e.g., Server2, Backend, Cache"));
    m_newFileAliasEdit->setEnabled(false);
    aliasLayout->addWidget(m_newFileAliasEdit);

    mainLayout->addWidget(aliasGroup);

    // Enable/disable aliases based on mode
    connect(m_replaceRadio, &QRadioButton::toggled, this, [this](bool checked) {
        m_existingFileAliasEdit->setEnabled(!checked);
        m_newFileAliasEdit->setEnabled(!checked);
    });

    mainLayout->addStretch();

    // Buttons
    auto* buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
        this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttonBox);
}

LogFileLoadDialog::LoadMode LogFileLoadDialog::GetLoadMode() const
{
    return m_mergeRadio->isChecked() ? LoadMode::Merge : LoadMode::Replace;
}

QString LogFileLoadDialog::GetNewFileAlias() const
{
    return m_newFileAliasEdit->text().trimmed();
}

QString LogFileLoadDialog::GetExistingFileAlias() const
{
    return m_existingFileAliasEdit->text().trimmed();
}

} // namespace ui::qt
