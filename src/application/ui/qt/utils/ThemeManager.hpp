#pragma once

#include <QString>
#include <QColor>
#include <QFont>
#include <map>
#include <vector>
#include <memory>

namespace ui::qt::utils {

/**
 * @brief Manages application themes and customization (v1.10.0 Phase 8).
 *
 * Features:
 * - Predefined light/dark themes
 * - Custom color schemes
 * - Font customization per component
 * - Theme persistence across sessions
 * - Runtime theme switching
 * - Per-component styling overrides
 */
class ThemeManager
{
  public:
    struct ColorScheme
    {
        QString name;
        QColor background;
        QColor foreground;
        QColor accent;
        QColor errorColor;
        QColor warningColor;
        QColor successColor;
        QColor borderColor;
        QColor selectionColor;
        QColor alternateRowColor;
    };

    struct FontConfig
    {
        QString family;
        int pointSize {10};
        bool bold {false};
        bool italic {false};
    };

    enum class Theme
    {
        Light,
        Dark,
        HighContrast,
        Custom
    };

    static ThemeManager& getInstance();

    /// Set the active theme
    void setTheme(Theme theme);
    Theme theme() const { return m_currentTheme; }

    /// Get current color scheme
    const ColorScheme& colorScheme() const { return m_colorScheme; }

    /// Set custom color scheme
    void setColorScheme(const ColorScheme& scheme);

    /// Get font for a component
    QFont getFont(const QString& componentName) const;
    void setFont(const QString& componentName, const QFont& font);

    /// Get stylesheet for Qt application
    QString stylesheet() const;

    /// Save theme to file
    void saveTheme(const QString& name);

    /// Load theme from file
    bool loadTheme(const QString& name);

    /// Get available theme names
    std::vector<QString> getAvailableThemes() const;

    /// Get available color schemes
    std::vector<ColorScheme> getColorSchemes() const;

    /// Reset to default theme
    void resetToDefault();

  private:
    ThemeManager();

    ColorScheme GetLightScheme() const;
    ColorScheme GetDarkScheme() const;
    ColorScheme GetHighContrastScheme() const;

    void ApplyColorScheme();

    Theme m_currentTheme {Theme::Light};
    ColorScheme m_colorScheme;
    std::map<QString, QFont> m_fonts;
    std::vector<ColorScheme> m_customSchemes;
};

} // namespace ui::qt::utils
