#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QRadioButton>
#include <QString>

namespace ui::qt
{

/**
 * @brief Dialog shown when loading a new log file while data already exists
 * 
 * Allows user to choose between:
 * - Replace: Clear existing data and load new file
 * - Merge: Combine logs by timestamp with source tracking
 */
class LogFileLoadDialog : public QDialog
{
    Q_OBJECT

public:
    enum class LoadMode
    {
        Replace,
        Merge
    };

    explicit LogFileLoadDialog(const QString& filePath, QWidget* parent = nullptr);

    LoadMode GetLoadMode() const;
    QString GetNewFileAlias() const;
    QString GetExistingFileAlias() const;

private:
    QRadioButton* m_replaceRadio {nullptr};
    QRadioButton* m_mergeRadio {nullptr};
    QLineEdit* m_newFileAliasEdit {nullptr};
    QLineEdit* m_existingFileAliasEdit {nullptr};
};

} // namespace ui::qt
