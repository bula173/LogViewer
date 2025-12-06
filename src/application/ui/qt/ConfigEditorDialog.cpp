#include "ui/qt/ConfigEditorDialog.hpp"

#include "application/util/Logger.hpp"
#include "config/Config.hpp"

#include <QDialogButtonBox>
#include <QFile>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTextStream>
#include <QVBoxLayout>

#include <filesystem>

namespace ui::qt
{

ConfigEditorDialog::ConfigEditorDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Edit Config"));

    auto* mainLayout = new QVBoxLayout(this);
    m_editor = new QPlainTextEdit(this);
    m_editor->setLineWrapMode(QPlainTextEdit::NoWrap);
    mainLayout->addWidget(m_editor, 1);

    auto* buttonLayout = new QHBoxLayout();
    m_saveButton = new QPushButton(tr("Save"), this);
    m_reloadButton = new QPushButton(tr("Reload"), this);
    m_closeButton = new QPushButton(tr("Close"), this);

    buttonLayout->addStretch();
    buttonLayout->addWidget(m_reloadButton);
    buttonLayout->addWidget(m_saveButton);
    buttonLayout->addWidget(m_closeButton);
    mainLayout->addLayout(buttonLayout);

    connect(m_saveButton, &QPushButton::clicked, this,
        &ConfigEditorDialog::OnSaveClicked);
    connect(m_reloadButton, &QPushButton::clicked, this,
        &ConfigEditorDialog::OnReloadClicked);
    connect(m_closeButton, &QPushButton::clicked, this,
        &ConfigEditorDialog::OnCloseClicked);

    LoadConfig();
}

void ConfigEditorDialog::LoadConfig()
{
    const auto& path = config::GetConfig().GetConfigFilePath();
    if (path.empty() || !std::filesystem::exists(path))
    {
        util::Logger::Warn("[ConfigEditorDialog] Config file does not exist: '{}'", path);
        m_editor->setPlainText(tr("// Config file does not exist.\n"));
        return;
    }

    QFile file(QString::fromStdString(path));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        util::Logger::Error(
            "[ConfigEditorDialog] Failed to open config file: '{}'", path);
        QMessageBox::critical(this, tr("Config"),
            tr("Failed to open config file."));
        return;
    }

    QTextStream in(&file);
    // in.setCodec("UTF-8");
    m_editor->setPlainText(in.readAll());
}

bool ConfigEditorDialog::SaveConfig()
{
    const auto& path = config::GetConfig().GetConfigFilePath();
    if (path.empty())
    {
        QMessageBox::warning(this, tr("Config"),
            tr("Config file path is not set."));
        return false;
    }

    QFile file(QString::fromStdString(path));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
    {
        util::Logger::Error(
            "[ConfigEditorDialog] Failed to write config file: '{}'", path);
        QMessageBox::critical(this, tr("Config"),
            tr("Failed to write config file."));
        return false;
    }

    const auto text = m_editor->toPlainText();
    if (text.trimmed().isEmpty())
    {
        util::Logger::Warn(
            "[ConfigEditorDialog] Config content is empty; skipping reload.");
        QMessageBox::warning(this, tr("Config"),
            tr("Config file is empty. It was saved, but not reloaded "
               "because it is not valid JSON."));
        return true;
    }

    QTextStream out(&file);
    out << text;

    try
    {
        config::GetConfig().Reload();
        util::Logger::Info(
            "[ConfigEditorDialog] Config saved and reloaded from '{}'", path);
    }
    catch (const std::exception& ex)
    {
        util::Logger::Error(
            "[ConfigEditorDialog] Failed to reload config: {}", ex.what());
        QMessageBox::warning(this, tr("Config"),
            tr("Config saved but reload failed: %1").arg(ex.what()));
    }

    return true;
}

void ConfigEditorDialog::OnSaveClicked()
{
    if (SaveConfig())
        QMessageBox::information(this, tr("Config"),
            tr("Configuration saved successfully."));
}

void ConfigEditorDialog::OnReloadClicked()
{
    LoadConfig();
}

void ConfigEditorDialog::OnCloseClicked()
{
    close();
}

} // namespace ui::qt