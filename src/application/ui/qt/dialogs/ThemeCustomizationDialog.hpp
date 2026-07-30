#pragma once

#include <QDialog>
#include <QString>

class QComboBox;
class QPushButton;
class QSpinBox;
class QColorDialog;
class QLabel;

namespace ui::qt {

/**
 * @brief Theme customization dialog for v1.10.0 Phase 8.
 *
 * Allows users to:
 * - Switch between predefined themes (Light, Dark, HighContrast)
 * - Customize colors (background, foreground, accent, etc.)
 * - Customize fonts for different components
 * - Save/load custom themes
 * - Preview changes in real-time
 */
class ThemeCustomizationDialog : public QDialog
{
    Q_OBJECT

  public:
    explicit ThemeCustomizationDialog(QWidget* parent = nullptr);

  private slots:
    void OnThemeChanged(int index);
    void OnEditBackground();
    void OnEditForeground();
    void OnEditAccent();
    void OnEditError();
    void OnEditWarning();
    void OnEditSuccess();
    void OnSaveTheme();
    void OnLoadTheme();
    void OnReset();
    void OnApply();

  private:
    void BuildLayout();
    void UpdateColorPreview();
    void RefreshThemeList();

    QComboBox* m_themeCombo {nullptr};
    QPushButton* m_backgroundColorBtn {nullptr};
    QPushButton* m_foregroundColorBtn {nullptr};
    QPushButton* m_accentColorBtn {nullptr};
    QPushButton* m_errorColorBtn {nullptr};
    QPushButton* m_warningColorBtn {nullptr};
    QPushButton* m_successColorBtn {nullptr};
    QComboBox* m_fontFamilyCombo {nullptr};
    QSpinBox* m_fontSizeSpinBox {nullptr};
    QLabel* m_previewLabel {nullptr};
    QPushButton* m_saveThemeBtn {nullptr};
    QPushButton* m_loadThemeBtn {nullptr};
    QPushButton* m_resetBtn {nullptr};
    QPushButton* m_applyBtn {nullptr};
};

} // namespace ui::qt
