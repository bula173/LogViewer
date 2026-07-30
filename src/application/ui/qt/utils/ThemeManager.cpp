#include "ThemeManager.hpp"

#include "Logger.hpp"
#include "../../../config/Config.hpp"

#include <QApplication>
#include <QCoreApplication>
#include <QFont>
#include <QStandardPaths>
#include <nlohmann/json.hpp>

#include <fstream>
#include <filesystem>

namespace ui::qt::utils {

ThemeManager& ThemeManager::getInstance()
{
    static ThemeManager instance;
    return instance;
}

ThemeManager::ThemeManager()
{
    m_colorScheme = GetLightScheme();
    setTheme(Theme::Light);
}

void ThemeManager::setTheme(Theme theme)
{
    m_currentTheme = theme;

    switch (theme)
    {
        case Theme::Light:
            m_colorScheme = GetLightScheme();
            break;
        case Theme::Dark:
            m_colorScheme = GetDarkScheme();
            break;
        case Theme::HighContrast:
            m_colorScheme = GetHighContrastScheme();
            break;
        case Theme::Custom:
            // Keep existing custom color scheme
            break;
    }

    ApplyColorScheme();
    util::Logger::Info("[ThemeManager] Theme switched to {}", static_cast<int>(theme));
}

ThemeManager::ColorScheme ThemeManager::GetLightScheme() const
{
    return {
        "Light",
        QColor(255, 255, 255),      // background
        QColor(0, 0, 0),             // foreground
        QColor(0, 120, 215),         // accent (Windows blue)
        QColor(200, 0, 0),           // error (red)
        QColor(255, 152, 0),         // warning (orange)
        QColor(76, 175, 80),         // success (green)
        QColor(200, 200, 200),       // border
        QColor(0, 120, 215),         // selection
        QColor(240, 240, 240)        // alternate row
    };
}

ThemeManager::ColorScheme ThemeManager::GetDarkScheme() const
{
    return {
        "Dark",
        QColor(30, 30, 30),          // background
        QColor(230, 230, 230),       // foreground
        QColor(100, 200, 255),       // accent (light blue)
        QColor(255, 100, 100),       // error (light red)
        QColor(255, 200, 100),       // warning (light orange)
        QColor(150, 230, 150),       // success (light green)
        QColor(80, 80, 80),          // border
        QColor(50, 120, 200),        // selection
        QColor(45, 45, 45)           // alternate row
    };
}

ThemeManager::ColorScheme ThemeManager::GetHighContrastScheme() const
{
    return {
        "HighContrast",
        QColor(0, 0, 0),             // background (pure black)
        QColor(255, 255, 255),       // foreground (pure white)
        QColor(255, 255, 0),         // accent (yellow)
        QColor(255, 0, 0),           // error (pure red)
        QColor(255, 165, 0),         // warning (orange)
        QColor(0, 200, 0),           // success (pure green)
        QColor(255, 255, 255),       // border (white)
        QColor(255, 255, 0),         // selection (yellow)
        QColor(50, 50, 50)           // alternate row
    };
}

void ThemeManager::setColorScheme(const ColorScheme& scheme)
{
    m_colorScheme = scheme;
    m_currentTheme = Theme::Custom;
    ApplyColorScheme();
    util::Logger::Info("[ThemeManager] Custom color scheme applied");
}

QFont ThemeManager::getFont(const QString& componentName) const
{
    auto it = m_fonts.find(componentName);
    if (it != m_fonts.end())
        return it->second;

    // Return default font
    QFont defaultFont;
    defaultFont.setPointSize(10);
    return defaultFont;
}

void ThemeManager::setFont(const QString& componentName, const QFont& font)
{
    m_fonts[componentName] = font;
    util::Logger::Debug("[ThemeManager] Font set for component: {}", componentName.toStdString());
}

QString ThemeManager::getStylesheet() const
{
    QString bg = m_colorScheme.background.name();
    QString fg = m_colorScheme.foreground.name();
    QString accent = m_colorScheme.accent.name();
    QString border = m_colorScheme.borderColor.name();
    QString altRow = m_colorScheme.alternateRowColor.name();
    QString selection = m_colorScheme.selectionColor.name();

    QString stylesheet = QString(
        "QMainWindow { background-color: %1; color: %2; }"
        "QDialog { background-color: %1; color: %2; }"
        "QWidget { background-color: %1; color: %2; }"
        "QTableView, QTableWidget { background-color: %1; color: %2; }"
        "QTableView::item { padding: 2px; border: 1px solid %3; }"
        "QTableView::item:selected { background-color: %4; }"
        "QTableView::item:alternate { background-color: %5; }"
        "QHeaderView::section { background-color: %1; color: %2; border: 1px solid %3; padding: 3px; }"
        "QPushButton { background-color: %4; color: white; border: 1px solid %3; padding: 4px 8px; border-radius: 2px; }"
        "QPushButton:hover { background-color: %4; opacity: 0.8; }"
        "QPushButton:pressed { background-color: %3; }"
        "QLineEdit { background-color: %1; color: %2; border: 1px solid %3; padding: 2px; }"
        "QLineEdit:focus { border: 2px solid %4; }"
        "QComboBox { background-color: %1; color: %2; border: 1px solid %3; padding: 2px; }"
        "QComboBox:focus { border: 2px solid %4; }"
        "QCheckBox, QRadioButton { color: %2; }"
        "QLabel { color: %2; }"
        "QTreeView, QListView { background-color: %1; color: %2; }"
        "QMenuBar { background-color: %1; color: %2; }"
        "QMenuBar::item:selected { background-color: %4; }"
        "QMenu { background-color: %1; color: %2; }"
        "QMenu::item:selected { background-color: %4; }"
        "QDockWidget { color: %2; }"
        "QTabBar::tab { background-color: %5; color: %2; padding: 4px 12px; }"
        "QTabBar::tab:selected { background-color: %1; border-bottom: 2px solid %4; }"
    );

    return stylesheet.arg(bg, fg, border, accent, altRow);
}

void ThemeManager::ApplyColorScheme()
{
    QString stylesheet = getStylesheet();
    auto* app = qobject_cast<QApplication*>(QCoreApplication::instance());
    if (app)
        app->setStyleSheet(stylesheet);

    util::Logger::Debug("[ThemeManager] Stylesheet applied");
}

void ThemeManager::saveTheme(const QString& name)
{
    try
    {
        nlohmann::json j;
        j["version"] = "1.0";
        j["name"] = name.toStdString();
        j["theme"] = static_cast<int>(m_currentTheme);

        // Save color scheme
        j["colorScheme"] = {
            {"name", m_colorScheme.name.toStdString()},
            {"background", m_colorScheme.background.name().toStdString()},
            {"foreground", m_colorScheme.foreground.name().toStdString()},
            {"accent", m_colorScheme.accent.name().toStdString()},
            {"errorColor", m_colorScheme.errorColor.name().toStdString()},
            {"warningColor", m_colorScheme.warningColor.name().toStdString()},
            {"successColor", m_colorScheme.successColor.name().toStdString()},
            {"borderColor", m_colorScheme.borderColor.name().toStdString()},
            {"selectionColor", m_colorScheme.selectionColor.name().toStdString()},
            {"alternateRowColor", m_colorScheme.alternateRowColor.name().toStdString()}
        };

        // Save fonts
        j["fonts"] = nlohmann::json::object();
        for (const auto& [component, font] : m_fonts)
        {
            j["fonts"][component.toStdString()] = {
                {"family", font.family().toStdString()},
                {"pointSize", font.pointSize()},
                {"bold", font.bold()},
                {"italic", font.italic()}
            };
        }

        // Save to ~/.logviewer/themes/
        auto themesDir = std::filesystem::path(
            QStandardPaths::writableLocation(QStandardPaths::AppDataLocation).toStdString()
        ) / "themes";
        std::filesystem::create_directories(themesDir);

        std::ofstream file(themesDir / (name.toStdString() + ".json"));
        file << j.dump(2);
        file.close();

        util::Logger::Info("[ThemeManager] Theme '{}' saved", name.toStdString());
    }
    catch (const std::exception& e)
    {
        util::Logger::Error("[ThemeManager] Failed to save theme: {}", e.what());
    }
}

bool ThemeManager::loadTheme(const QString& name)
{
    try
    {
        auto themesDir = std::filesystem::path(
            QStandardPaths::writableLocation(QStandardPaths::AppDataLocation).toStdString()
        ) / "themes";

        std::ifstream file(themesDir / (name.toStdString() + ".json"));
        if (!file.is_open())
            return false;

        nlohmann::json j = nlohmann::json::parse(file);

        // Load color scheme
        const auto& cs = j["colorScheme"];
        m_colorScheme.name = QString::fromStdString(cs.value("name", ""));
        m_colorScheme.background = QColor(QString::fromStdString(cs.value("background", "#FFFFFF")));
        m_colorScheme.foreground = QColor(QString::fromStdString(cs.value("foreground", "#000000")));
        m_colorScheme.accent = QColor(QString::fromStdString(cs.value("accent", "#0078D4")));
        m_colorScheme.errorColor = QColor(QString::fromStdString(cs.value("errorColor", "#C80000")));
        m_colorScheme.warningColor = QColor(QString::fromStdString(cs.value("warningColor", "#FF9800")));
        m_colorScheme.successColor = QColor(QString::fromStdString(cs.value("successColor", "#4CAF50")));
        m_colorScheme.borderColor = QColor(QString::fromStdString(cs.value("borderColor", "#C8C8C8")));
        m_colorScheme.selectionColor = QColor(QString::fromStdString(cs.value("selectionColor", "#0078D4")));
        m_colorScheme.alternateRowColor = QColor(QString::fromStdString(cs.value("alternateRowColor", "#F0F0F0")));

        // Load fonts
        if (j.contains("fonts"))
        {
            for (const auto& [component, fontData] : j["fonts"].items())
            {
                QFont font;
                font.setFamily(QString::fromStdString(fontData.value("family", "Segoe UI")));
                font.setPointSize(fontData.value("pointSize", 10));
                font.setBold(fontData.value("bold", false));
                font.setItalic(fontData.value("italic", false));
                m_fonts[QString::fromStdString(component)] = font;
            }
        }

        m_currentTheme = static_cast<Theme>(j.value("theme", 0));
        ApplyColorScheme();

        util::Logger::Info("[ThemeManager] Theme '{}' loaded", name.toStdString());
        return true;
    }
    catch (const std::exception& e)
    {
        util::Logger::Error("[ThemeManager] Failed to load theme: {}", e.what());
        return false;
    }
}

std::vector<QString> ThemeManager::getAvailableThemes() const
{
    std::vector<QString> themes;
    try
    {
        auto themesDir = std::filesystem::path(
            QStandardPaths::writableLocation(QStandardPaths::AppDataLocation).toStdString()
        ) / "themes";

        if (std::filesystem::exists(themesDir))
        {
            for (const auto& entry : std::filesystem::directory_iterator(themesDir))
            {
                if (entry.path().extension() == ".json")
                    themes.push_back(QString::fromStdString(entry.path().stem().string()));
            }
        }
    }
    catch (const std::exception& e)
    {
        util::Logger::Warn("[ThemeManager] Could not list themes: {}", e.what());
    }

    // Add built-in themes
    themes.push_back("Light");
    themes.push_back("Dark");
    themes.push_back("HighContrast");

    return themes;
}

std::vector<ThemeManager::ColorScheme> ThemeManager::getColorSchemes() const
{
    return {GetLightScheme(), GetDarkScheme(), GetHighContrastScheme()};
}

void ThemeManager::resetToDefault()
{
    setTheme(Theme::Light);
    m_fonts.clear();
    ApplyColorScheme();
    util::Logger::Info("[ThemeManager] Reset to default theme");
}

} // namespace ui::qt::utils
