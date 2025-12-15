#pragma once

#include "config/ConfigObserver.hpp"

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
    void OnLoadConfigClicked();
    void OnSaveConfigAsClicked();
    void OnLogLevelChanged(int index);
    void OnBrowseDictionaryFileClicked();

    // Columns tab
    void OnColumnSelectionChanged();
    void OnAddColumn();
    void OnRemoveColumn();
    void OnApplyColumnChanges();
    void OnMoveColumnUp();
    void OnMoveColumnDown();

    // Dictionary tab
    void OnDictionarySelectionChanged();
    void OnAddDictionary();
    void OnRemoveDictionary();
    void OnApplyDictionaryChanges();
    void OnLoadDictionaryFile();
    void OnSaveDictionaryFile();

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

    // Plugins tab
    void OnPluginSelectionChanged();
    void OnBrowsePluginClicked();
    void OnLoadPluginClicked();
    void OnUnloadPluginClicked();
    void OnEnablePluginClicked();
    void OnAutoLoadPluginToggled(bool checked);
    void OnSetLicenseClicked();
    void OnRefreshPluginsClicked();

private:
    void BuildUi();

    void InitGeneralTab();
    void InitColumnsTab();
    void InitDictionaryTab();
    void InitColorsTab();
    void InitPluginsTab();

    void LoadConfigToUi();

    void RefreshColumnsList();
    void LoadSelectedColumnToEditors();
    void SwapColumns(int sourceRow, int targetRow);

    void RefreshDictionaryList();
    void LoadSelectedDictionaryToEditors();
    void RefreshColorMappings();
    void UpdateColorEditorsFromSelection(int row);
    void UpdateColorSwatches();
    void UpdateColorPreview();

    void RefreshPluginsList();
    void UpdatePluginDetails();

    QString ColorToHex(const QColor& color) const;
    QColor HexToColor(const QString& hex) const;

    void NotifyObservers();

private:
    QTabWidget* m_tabs {nullptr};

    // General tab widgets
    QWidget* m_generalTab {nullptr};
    QLineEdit* m_xmlRootEdit {nullptr};
    QLineEdit* m_xmlEventEdit {nullptr};
    QLineEdit* m_typeFilterFieldEdit {nullptr};
    QComboBox* m_logLevelCombo {nullptr};
    QLabel* m_configPathLabel {nullptr};
    QLineEdit* m_dictionaryFileEdit {nullptr};
    QPushButton* m_browseDictionaryButton {nullptr};

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

    // Dictionary tab widgets
    QWidget* m_dictionaryTab {nullptr};
    QTableWidget* m_dictionaryTable {nullptr};
    QLineEdit* m_dictKeyEdit {nullptr};
    QComboBox* m_dictConversionCombo {nullptr};
    QLineEdit* m_dictTooltipEdit {nullptr};
    QTableWidget* m_dictValueMapTable {nullptr};
    QPushButton* m_addDictValueButton {nullptr};
    QPushButton* m_removeDictValueButton {nullptr};
    QPushButton* m_addDictionaryButton {nullptr};
    QPushButton* m_removeDictionaryButton {nullptr};
    QPushButton* m_applyDictionaryChangesButton {nullptr};
    QPushButton* m_loadDictionaryFileButton {nullptr};
    QPushButton* m_saveDictionaryFileButton {nullptr};
    int m_selectedDictionaryRow {-1};

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

    // Plugins tab widgets
    QWidget* m_pluginsTab {nullptr};
    QTableWidget* m_pluginsTable {nullptr};
    QLabel* m_pluginIdLabel {nullptr};
    QLabel* m_pluginNameLabel {nullptr};
    QLabel* m_pluginVersionLabel {nullptr};
    QLabel* m_pluginAuthorLabel {nullptr};
    QLabel* m_pluginTypeLabel {nullptr};
    QLabel* m_pluginStatusLabel {nullptr};
    QLabel* m_pluginLicenseLabel {nullptr};
    QLabel* m_pluginPathLabel {nullptr};
    QLineEdit* m_pluginPathEdit {nullptr};
    QLineEdit* m_pluginLicenseEdit {nullptr};
    QPushButton* m_browsePluginButton {nullptr};
    QPushButton* m_loadPluginButton {nullptr};
    QPushButton* m_unloadPluginButton {nullptr};
    QPushButton* m_enablePluginButton {nullptr};
    QPushButton* m_setLicenseButton {nullptr};
    QPushButton* m_refreshPluginsButton {nullptr};
    QCheckBox* m_autoLoadPluginCheck {nullptr};
    int m_selectedPluginRow {-1};

    std::vector<config::ConfigObserver*> m_observers;
};

} // namespace ui::qt
