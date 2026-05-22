#pragma once

#include <QApplication>
#include <QFile>
#include <QPalette>
#include <QColor>
#include <QStyle>
#include <QStyleFactory>
#include <QFont>
#include "Logger.hpp"

enum class ThemeType {
    Dark,
    Light,
    System
};

inline void ApplyTheme(QApplication& app, int themeInt)
{
    ThemeType theme = static_cast<ThemeType>(themeInt);
    
    // Use Fusion style for modern appearance across platforms
    app.setStyle(QStyleFactory::create("Fusion"));
    
    // Detect actual theme to use (for System theme, detect from system)
    ThemeType actualTheme = theme;
    if (theme == ThemeType::System) {
        // Detect if system uses dark or light theme
        QPalette systemPalette = app.palette();
        int lightness = systemPalette.color(QPalette::Window).lightness();
        actualTheme = (lightness < 128) ? ThemeType::Dark : ThemeType::Light;
        util::Logger::Info("System theme detected: {}", 
            (actualTheme == ThemeType::Dark) ? "Dark" : "Light");
    }
    
    QString themeFilePath;
    std::string themeName;
    
    if (actualTheme == ThemeType::Dark) {
        themeFilePath = ":/styles/dark_theme.qss";
        themeName = "Dark";
    } else {
        themeFilePath = ":/styles/light_theme.qss";
        themeName = "Light";
    }
    
    util::Logger::Info("Applying theme: {}", themeName);
    
    // Apply color palette based on actual theme FIRST
    QPalette palette;
    if (actualTheme == ThemeType::Dark) {
        palette.setColor(QPalette::Window, QColor(53, 53, 53));
        palette.setColor(QPalette::WindowText, QColor(255, 255, 255));
        palette.setColor(QPalette::Base, QColor(25, 25, 25));
        palette.setColor(QPalette::AlternateBase, QColor(53, 53, 53));
        palette.setColor(QPalette::ToolTipBase, QColor(255, 255, 255));
        palette.setColor(QPalette::ToolTipText, QColor(255, 255, 255));
        palette.setColor(QPalette::Text, QColor(255, 255, 255));
        palette.setColor(QPalette::Button, QColor(53, 53, 53));
        palette.setColor(QPalette::ButtonText, QColor(255, 255, 255));
        palette.setColor(QPalette::Highlight, QColor(42, 130, 218));
        palette.setColor(QPalette::HighlightedText, QColor(255, 255, 255));
        palette.setColor(QPalette::Link, QColor(42, 130, 218));
        palette.setColor(QPalette::LinkVisited, QColor(82, 163, 247));
        palette.setColor(QPalette::Disabled, QPalette::WindowText, QColor(127, 127, 127));
        palette.setColor(QPalette::Disabled, QPalette::Text, QColor(127, 127, 127));
        palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(127, 127, 127));
        palette.setColor(QPalette::Mid, QColor(37, 37, 37));
        palette.setColor(QPalette::Dark, QColor(25, 25, 25));
        palette.setColor(QPalette::Light, QColor(80, 80, 80));
    } else {
        // Light theme
        palette.setColor(QPalette::Window, QColor(245, 245, 245));
        palette.setColor(QPalette::WindowText, QColor(26, 26, 26));
        palette.setColor(QPalette::Base, QColor(255, 255, 255));
        palette.setColor(QPalette::AlternateBase, QColor(249, 249, 249));
        palette.setColor(QPalette::ToolTipBase, QColor(255, 255, 255));
        palette.setColor(QPalette::ToolTipText, QColor(26, 26, 26));
        palette.setColor(QPalette::Text, QColor(26, 26, 26));
        palette.setColor(QPalette::Button, QColor(245, 245, 245));
        palette.setColor(QPalette::ButtonText, QColor(26, 26, 26));
        palette.setColor(QPalette::Highlight, QColor(0, 120, 212));
        palette.setColor(QPalette::HighlightedText, QColor(255, 255, 255));
        palette.setColor(QPalette::Link, QColor(0, 120, 212));
        palette.setColor(QPalette::LinkVisited, QColor(0, 120, 212));
        palette.setColor(QPalette::Disabled, QPalette::WindowText, QColor(150, 150, 150));
        palette.setColor(QPalette::Disabled, QPalette::Text, QColor(150, 150, 150));
        palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(150, 150, 150));
        palette.setColor(QPalette::Mid, QColor(208, 208, 208));
        palette.setColor(QPalette::Dark, QColor(190, 190, 190));
        palette.setColor(QPalette::Light, QColor(224, 224, 224));
    }
    
    app.setPalette(palette);
    
    // Load and apply stylesheet AFTER palette (stylesheet overrides palette)
    QFile styleFile(themeFilePath);
    if (styleFile.open(QFile::ReadOnly)) {
        QString styleSheet = QLatin1String(styleFile.readAll());
        app.setStyleSheet(styleSheet);
        styleFile.close();
        util::Logger::Info("Theme stylesheet loaded successfully");
    } else {
        util::Logger::Warn("Failed to load theme stylesheet from: {}", 
            themeFilePath.toStdString());
    }
    
    // Note: Font styling is now handled via QSS stylesheets (dark_theme.qss and light_theme.qss)
    // This approach is more maintainable and provides better font rendering on all platforms
}

