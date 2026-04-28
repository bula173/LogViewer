#pragma once

#include "ActorDefinition.hpp"

#include <QWidget>
#include <vector>

class QTableWidget;
class QTableWidgetItem;
class QPushButton;
class QLabel;

namespace ui::qt {

/**
 * @brief Left-dock panel for defining named actors by regular expression.
 *
 * Each actor definition specifies:
 *  - A display name
 *  - A QRegularExpression pattern
 *  - An optional log field to match against (empty = search all fields)
 *
 * Definitions can be saved to / loaded from a JSON file.
 * When the definition list changes the DefinitionsChanged signal is emitted
 * so the Actors panel can refresh its results.
 */
class ActorDefinitionsPanel : public QWidget
{
    Q_OBJECT

  public:
    explicit ActorDefinitionsPanel(QWidget* parent = nullptr);

    [[nodiscard]] const std::vector<ActorDefinition>& Definitions() const
    {
        return m_definitions;
    }

  signals:
    void DefinitionsChanged(const std::vector<ActorDefinition>& defs);
    /// Emitted when the user clicks "Apply Filter" — caller should filter events
    /// to those matching the current enabled actor definitions.
    void RequestApplyFilter();
    /// Emitted when the user clicks "Clear Filter".
    void RequestClearFilter();

  private slots:
    void HandleAdd();
    void HandleEdit();
    void HandleRemove();
    void HandleSave();
    void HandleSaveAs();
    void HandleLoad();
    void HandleSelectionChanged();
    void HandleItemChanged(QTableWidgetItem* item);

  private:
    void BuildLayout();
    void RebuildTable();
    void EmitAndSave();
    void SetStatus(const QString& msg, bool isError);

    /// Open an add/edit dialog; returns false if user cancelled.
    bool EditDefinition(ActorDefinition& def, bool isNew);

    /// Default file path in the application config directory.
    static std::string DefaultFilePath();

    QTableWidget* m_table    {nullptr};
    QPushButton*  m_editBtn  {nullptr};
    QPushButton*  m_removeBtn{nullptr};
    QPushButton*  m_applyFilterBtn {nullptr};
    QPushButton*  m_clearFilterBtn {nullptr};
    QLabel*       m_statusLabel {nullptr};

    std::vector<ActorDefinition> m_definitions;
    std::string                  m_currentFilePath; ///< last saved/loaded path
    bool                         m_rebuilding {false}; ///< true while RebuildTable runs
};

} // namespace ui::qt
