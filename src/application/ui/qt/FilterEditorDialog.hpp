#pragma once

#include "filters/Filter.hpp"

#include <QDialog>
#include <memory>

class QCheckBox;
class QComboBox;
class QDialogButtonBox;
class QLineEdit;
class QSpinBox;
class QStackedWidget;

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

  private:
    void BuildUi();
    QWidget* BuildColumnPage();
    QWidget* BuildParameterPage();
    void PopulateColumns();
    bool ValidateAndPersist();

    filters::FilterPtr m_filter;

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
};

} // namespace ui::qt
