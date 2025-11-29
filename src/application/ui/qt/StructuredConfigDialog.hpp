#pragma once

#include <QDialog>
#include <vector>
#include <string>

class QTabWidget;
class QLineEdit;
class QComboBox;
class QTableWidget;
class QCheckBox;
class QSpinBox;
class QPushButton;
class QFrame;
class QLabel;

namespace config {
    class ConfigObserver {
    public:
        virtual ~ConfigObserver() = default;
        virtual void OnConfigChanged() = 0;
    };
}

namespace ui::qt
{

class StructuredConfigDialog : public QDialog
{
    Q_OBJECT

public:
    explicit StructuredConfigDialog(QWidget* parent = nullptr);

    void AddObserver(config::ConfigObserver* observer);
    void RemoveObserver(config::ConfigObserver* observer);

private slots:
    void OnSaveClicked();
    void OnCancelClicked();

    // General tab
    void OnOpenConfigFileClicked();
    void OnLogLevelChanged(int index);

    // Columns tab
    void OnColumnSelectionChanged();
    void OnAddColumn();
    void OnRemoveColumn();
    void OnApplyColumnChanges();
    void OnMoveColumnUp();
    void OnMoveColumnDown();

    // Colors tab
    void OnColorColumnChanged(int index);
    void OnColorMappingSelectionChanged();
    void OnBgColorButton();
    void OnFgColorButton();
    void OnDefaultBgColor();
    void OnDefaultFgColor();
    void OnAddColorMapping();
    void OnUpdateColorMapping();
    void OnDeleteColorMapping();

private:
    void BuildUi();

    void InitGeneralTab();
    void InitColumnsTab();
    void InitColorsTab();

    void LoadConfigToUi();

    void RefreshColumnsList();
    void LoadSelectedColumnToEditors();
    void SwapColumns(int sourceRow, int targetRow);

    void RefreshColorMappings();
    void UpdateColorEditorsFromSelection(int row);
    void UpdateColorSwatches();
    void UpdateColorPreview();

    QString ColorToHex(const QColor& color) const;
    QColor HexToColor(const QString& hex) const;

    void NotifyObservers();

private:
    QTabWidget* m_tabs {nullptr};

    // General tab widgets
    QWidget* m_generalTab {nullptr};
    QLineEdit* m_xmlRootEdit {nullptr};
    QLineEdit* m_xmlEventEdit {nullptr};
    QComboBox* m_logLevelCombo {nullptr};
    QLabel* m_configPathLabel {nullptr};

    // Columns tab widgets
    QWidget* m_columnsTab {nullptr};
    QTableWidget* m_columnsTable {nullptr};
    QLineEdit* m_columnNameEdit {nullptr};
    QCheckBox* m_columnVisibleCheck {nullptr};
    QSpinBox* m_columnWidthSpin {nullptr};
    QPushButton* m_moveUpButton {nullptr};
    QPushButton* m_moveDownButton {nullptr};
    QPushButton* m_addColumnButton {nullptr};
    QPushButton* m_removeColumnButton {nullptr};
    QPushButton* m_applyColumnChangesButton {nullptr};
    int m_selectedColumnRow {-1};

    // Colors tab widgets
    QWidget* m_colorsTab {nullptr};
    QComboBox* m_colorColumnCombo {nullptr};
    QTableWidget* m_colorMappingsTable {nullptr};
    QLineEdit* m_colorValueEdit {nullptr};
    QPushButton* m_bgColorButton {nullptr};
    QPushButton* m_fgColorButton {nullptr};
    QPushButton* m_defaultBgButton {nullptr};
    QPushButton* m_defaultFgButton {nullptr};
    QPushButton* m_addColorButton {nullptr};
    QPushButton* m_updateColorButton {nullptr};
    QPushButton* m_deleteColorButton {nullptr};
    QFrame* m_bgColorSwatch {nullptr};
    QFrame* m_fgColorSwatch {nullptr};
    QFrame* m_previewPanel {nullptr};
    QLabel* m_previewLabel {nullptr};

    QColor m_bgColor;
    QColor m_fgColor;

    std::vector<config::ConfigObserver*> m_observers;
};

} // namespace ui::qt
