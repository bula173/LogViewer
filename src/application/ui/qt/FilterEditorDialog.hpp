#pragma once

#include "Filter.hpp"

#include <QDialog>
#include <memory>

class QCheckBox;
class QComboBox;
class QDialogButtonBox;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QPushButton;
class QSpinBox;
class QStackedWidget;
class QVBoxLayout;

namespace ui::qt
{

class FilterEditorDialog : public QDialog
{
    Q_OBJECT

  public:
    explicit FilterEditorDialog(QWidget* parent = nullptr,
        filters::FilterPtr filter = nullptr);

    filters::FilterPtr GetFilter() const { return m_filter; }

  private slots:
    void OnFilterTypeChanged(int index);
    void HandleAccept();
    void OnAddCondition();
    void OnRemoveCondition();
    void OnConditionSelectionChanged();
    void OnConditionItemDoubleClicked(QListWidgetItem* item);

  private:
    void BuildUi();
    QWidget* BuildColumnPage();
    QWidget* BuildParameterPage();
    void PopulateColumns();
    bool ValidateAndPersist();
    void UpdateConditionList();
    void LoadConditionIntoEditor(int conditionIndex);
    void SaveConditionFromEditor(int conditionIndex);
    filters::FilterCondition CreateConditionFromEditor();
    void ClearConditionEditor();

    filters::FilterPtr m_filter;
    std::vector<filters::FilterCondition> m_conditions;

    // Main UI elements
    QComboBox* m_filterTypeCombo {nullptr};
    QStackedWidget* m_filterPages {nullptr};
    QComboBox* m_columnCombo {nullptr};
    QLineEdit* m_parameterKeyEdit {nullptr};
    QSpinBox* m_depthSpin {nullptr};
    QLineEdit* m_nameEdit {nullptr};
    QLineEdit* m_patternEdit {nullptr};
    QCheckBox* m_caseSensitiveCheck {nullptr};
    QCheckBox* m_invertedCheck {nullptr};
    QDialogButtonBox* m_buttonBox {nullptr};

    // Complex filter UI elements
    QListWidget* m_conditionsList {nullptr};
    QPushButton* m_addConditionBtn {nullptr};
    QPushButton* m_removeConditionBtn {nullptr};
    QWidget* m_conditionEditorWidget {nullptr};
    QComboBox* m_conditionTypeCombo {nullptr};
    QComboBox* m_conditionColumnCombo {nullptr};
    QLineEdit* m_conditionParameterKeyEdit {nullptr};
    QSpinBox* m_conditionDepthSpin {nullptr};
    QLineEdit* m_conditionPatternEdit {nullptr};
    QCheckBox* m_conditionCaseSensitiveCheck {nullptr};

    int m_currentEditingCondition {-1};
};

} // namespace ui::qt
