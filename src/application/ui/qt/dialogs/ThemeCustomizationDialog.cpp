#include "ThemeCustomizationDialog.hpp"

#include "../utils/ThemeManager.hpp"
#include "Logger.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QComboBox>
#include <QPushButton>
#include <QSpinBox>
#include <QLabel>
#include <QFontComboBox>
#include <QColorDialog>
#include <QInputDialog>
#include <QMessageBox>

namespace ui::qt {

ThemeCustomizationDialog::ThemeCustomizationDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Theme Customization - v1.10.0"));
    setMinimumWidth(500);
    setMinimumHeight(600);
    BuildLayout();
}

void ThemeCustomizationDialog::BuildLayout()
{
    auto* mainLayout = new QVBoxLayout(this);

    // ── Theme selection ───────────────────────────────────────────────────
    auto* themeGroup = new QGroupBox(tr("Theme"), this);
    auto* themeLayout = new QHBoxLayout(themeGroup);
    themeLayout->addWidget(new QLabel(tr("Current Theme:"), this));
    m_themeCombo = new QComboBox(this);
    m_themeCombo->addItem(tr("Light"));
    m_themeCombo->addItem(tr("Dark"));
    m_themeCombo->addItem(tr("HighContrast"));
    connect(m_themeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ThemeCustomizationDialog::OnThemeChanged);
    themeLayout->addWidget(m_themeCombo, 1);
    mainLayout->addWidget(themeGroup);

    // ── Color customization ───────────────────────────────────────────────
    auto* colorGroup = new QGroupBox(tr("Color Scheme"), this);
    auto* colorLayout = new QVBoxLayout(colorGroup);

    // Background
    auto* bgRow = new QHBoxLayout();
    bgRow->addWidget(new QLabel(tr("Background:"), this));
    m_backgroundColorBtn = new QPushButton(tr("Choose..."), this);
    m_backgroundColorBtn->setMaximumWidth(100);
    connect(m_backgroundColorBtn, &QPushButton::clicked, this, &ThemeCustomizationDialog::OnEditBackground);
    bgRow->addWidget(m_backgroundColorBtn);
    bgRow->addStretch();
    colorLayout->addLayout(bgRow);

    // Foreground
    auto* fgRow = new QHBoxLayout();
    fgRow->addWidget(new QLabel(tr("Foreground:"), this));
    m_foregroundColorBtn = new QPushButton(tr("Choose..."), this);
    m_foregroundColorBtn->setMaximumWidth(100);
    connect(m_foregroundColorBtn, &QPushButton::clicked, this, &ThemeCustomizationDialog::OnEditForeground);
    fgRow->addWidget(m_foregroundColorBtn);
    fgRow->addStretch();
    colorLayout->addLayout(fgRow);

    // Accent
    auto* accentRow = new QHBoxLayout();
    accentRow->addWidget(new QLabel(tr("Accent:"), this));
    m_accentColorBtn = new QPushButton(tr("Choose..."), this);
    m_accentColorBtn->setMaximumWidth(100);
    connect(m_accentColorBtn, &QPushButton::clicked, this, &ThemeCustomizationDialog::OnEditAccent);
    accentRow->addWidget(m_accentColorBtn);
    accentRow->addStretch();
    colorLayout->addLayout(accentRow);

    // Error, Warning, Success
    auto* msgRow = new QHBoxLayout();
    msgRow->addWidget(new QLabel(tr("Status Colors:"), this));
    m_errorColorBtn = new QPushButton(tr("Error"), this);
    m_errorColorBtn->setMaximumWidth(60);
    connect(m_errorColorBtn, &QPushButton::clicked, this, &ThemeCustomizationDialog::OnEditError);
    msgRow->addWidget(m_errorColorBtn);

    m_warningColorBtn = new QPushButton(tr("Warning"), this);
    m_warningColorBtn->setMaximumWidth(70);
    connect(m_warningColorBtn, &QPushButton::clicked, this, &ThemeCustomizationDialog::OnEditWarning);
    msgRow->addWidget(m_warningColorBtn);

    m_successColorBtn = new QPushButton(tr("Success"), this);
    m_successColorBtn->setMaximumWidth(70);
    connect(m_successColorBtn, &QPushButton::clicked, this, &ThemeCustomizationDialog::OnEditSuccess);
    msgRow->addWidget(m_successColorBtn);
    msgRow->addStretch();
    colorLayout->addLayout(msgRow);

    mainLayout->addWidget(colorGroup);

    // ── Font customization ────────────────────────────────────────────────
    auto* fontGroup = new QGroupBox(tr("Font Settings"), this);
    auto* fontLayout = new QVBoxLayout(fontGroup);

    auto* familyRow = new QHBoxLayout();
    familyRow->addWidget(new QLabel(tr("Font Family:"), this));
    m_fontFamilyCombo = new QFontComboBox(this);
    familyRow->addWidget(m_fontFamilyCombo, 1);
    fontLayout->addLayout(familyRow);

    auto* sizeRow = new QHBoxLayout();
    sizeRow->addWidget(new QLabel(tr("Font Size:"), this));
    m_fontSizeSpinBox = new QSpinBox(this);
    m_fontSizeSpinBox->setMinimum(8);
    m_fontSizeSpinBox->setMaximum(24);
    m_fontSizeSpinBox->setValue(10);
    sizeRow->addWidget(m_fontSizeSpinBox);
    sizeRow->addStretch();
    fontLayout->addLayout(sizeRow);

    mainLayout->addWidget(fontGroup);

    // ── Preview ───────────────────────────────────────────────────────────
    auto* previewGroup = new QGroupBox(tr("Preview"), this);
    auto* previewLayout = new QVBoxLayout(previewGroup);
    m_previewLabel = new QLabel(tr("Sample Text Preview\nButton | TextField | Status"), this);
    m_previewLabel->setAlignment(Qt::AlignCenter);
    m_previewLabel->setMinimumHeight(80);
    previewLayout->addWidget(m_previewLabel);
    mainLayout->addWidget(previewGroup);

    // ── Theme management ─────────────────────────────────────────────────
    auto* mgmtGroup = new QGroupBox(tr("Theme Management"), this);
    auto* mgmtLayout = new QHBoxLayout(mgmtGroup);
    m_saveThemeBtn = new QPushButton(tr("Save Theme..."), this);
    connect(m_saveThemeBtn, &QPushButton::clicked, this, &ThemeCustomizationDialog::OnSaveTheme);
    mgmtLayout->addWidget(m_saveThemeBtn);

    m_loadThemeBtn = new QPushButton(tr("Load Theme..."), this);
    connect(m_loadThemeBtn, &QPushButton::clicked, this, &ThemeCustomizationDialog::OnLoadTheme);
    mgmtLayout->addWidget(m_loadThemeBtn);

    m_resetBtn = new QPushButton(tr("Reset to Default"), this);
    connect(m_resetBtn, &QPushButton::clicked, this, &ThemeCustomizationDialog::OnReset);
    mgmtLayout->addWidget(m_resetBtn);

    mainLayout->addWidget(mgmtGroup);

    // ── Buttons ───────────────────────────────────────────────────────────
    auto* btnLayout = new QHBoxLayout();
    m_applyBtn = new QPushButton(tr("Apply"), this);
    m_applyBtn->setDefault(true);
    connect(m_applyBtn, &QPushButton::clicked, this, &ThemeCustomizationDialog::OnApply);
    btnLayout->addStretch();
    btnLayout->addWidget(m_applyBtn);

    auto* closeBtn = new QPushButton(tr("Close"), this);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    btnLayout->addWidget(closeBtn);

    mainLayout->addLayout(btnLayout);

    setLayout(mainLayout);
    UpdateColorPreview();
}

void ThemeCustomizationDialog::OnThemeChanged(int index)
{
    auto& themeManager = utils::ThemeManager::getInstance();
    auto theme = static_cast<utils::ThemeManager::Theme>(index);
    themeManager.setTheme(theme);
    UpdateColorPreview();
}

void ThemeCustomizationDialog::OnEditBackground()
{
    auto& themeManager = utils::ThemeManager::getInstance();
    auto scheme = themeManager.colorScheme();
    QColor color = QColorDialog::getColor(scheme.background, this, tr("Background Color"));
    if (color.isValid())
    {
        scheme.background = color;
        themeManager.setColorScheme(scheme);
        UpdateColorPreview();
    }
}

void ThemeCustomizationDialog::OnEditForeground()
{
    auto& themeManager = utils::ThemeManager::getInstance();
    auto scheme = themeManager.colorScheme();
    QColor color = QColorDialog::getColor(scheme.foreground, this, tr("Foreground Color"));
    if (color.isValid())
    {
        scheme.foreground = color;
        themeManager.setColorScheme(scheme);
        UpdateColorPreview();
    }
}

void ThemeCustomizationDialog::OnEditAccent()
{
    auto& themeManager = utils::ThemeManager::getInstance();
    auto scheme = themeManager.colorScheme();
    QColor color = QColorDialog::getColor(scheme.accent, this, tr("Accent Color"));
    if (color.isValid())
    {
        scheme.accent = color;
        themeManager.setColorScheme(scheme);
        UpdateColorPreview();
    }
}

void ThemeCustomizationDialog::OnEditError()
{
    auto& themeManager = utils::ThemeManager::getInstance();
    auto scheme = themeManager.colorScheme();
    QColor color = QColorDialog::getColor(scheme.errorColor, this, tr("Error Color"));
    if (color.isValid())
    {
        scheme.errorColor = color;
        themeManager.setColorScheme(scheme);
    }
}

void ThemeCustomizationDialog::OnEditWarning()
{
    auto& themeManager = utils::ThemeManager::getInstance();
    auto scheme = themeManager.colorScheme();
    QColor color = QColorDialog::getColor(scheme.warningColor, this, tr("Warning Color"));
    if (color.isValid())
    {
        scheme.warningColor = color;
        themeManager.setColorScheme(scheme);
    }
}

void ThemeCustomizationDialog::OnEditSuccess()
{
    auto& themeManager = utils::ThemeManager::getInstance();
    auto scheme = themeManager.colorScheme();
    QColor color = QColorDialog::getColor(scheme.successColor, this, tr("Success Color"));
    if (color.isValid())
    {
        scheme.successColor = color;
        themeManager.setColorScheme(scheme);
    }
}

void ThemeCustomizationDialog::OnSaveTheme()
{
    bool ok;
    QString themeName = QInputDialog::getText(this, tr("Save Theme"),
        tr("Theme name:"), QLineEdit::Normal, QString(), &ok);
    if (ok && !themeName.isEmpty())
    {
        auto& themeManager = utils::ThemeManager::getInstance();
        themeManager.saveTheme(themeName);
        RefreshThemeList();
        QMessageBox::information(this, tr("Success"), tr("Theme '%1' saved").arg(themeName));
    }
}

void ThemeCustomizationDialog::OnLoadTheme()
{
    auto& themeManager = utils::ThemeManager::getInstance();
    auto themes = themeManager.getAvailableThemes();

    QStringList themeList(themes.begin(), themes.end());
    bool ok;
    QString themeName = QInputDialog::getItem(this, tr("Load Theme"),
        tr("Select theme:"), themeList, 0, false, &ok);

    if (ok && !themeName.isEmpty())
    {
        if (themeManager.loadTheme(themeName))
        {
            UpdateColorPreview();
            QMessageBox::information(this, tr("Success"), tr("Theme '%1' loaded").arg(themeName));
        }
        else
        {
            QMessageBox::warning(this, tr("Failed"), tr("Could not load theme '%1'").arg(themeName));
        }
    }
}

void ThemeCustomizationDialog::OnReset()
{
    auto& themeManager = utils::ThemeManager::getInstance();
    themeManager.resetToDefault();
    m_themeCombo->setCurrentIndex(0);
    UpdateColorPreview();
    QMessageBox::information(this, tr("Reset"), tr("Theme reset to default"));
}

void ThemeCustomizationDialog::OnApply()
{
    util::Logger::Info("[ThemeCustomizationDialog] Theme applied and saved");
    QMessageBox::information(this, tr("Applied"), tr("Theme changes applied"));
    accept();
}

void ThemeCustomizationDialog::UpdateColorPreview()
{
    auto& themeManager = utils::ThemeManager::getInstance();
    auto scheme = themeManager.colorScheme();

    m_previewLabel->setStyleSheet(QString(
        "background-color: %1; color: %2; padding: 10px; border: 1px solid %3;"
    ).arg(scheme.background.name(), scheme.foreground.name(), scheme.borderColor.name()));
}

void ThemeCustomizationDialog::RefreshThemeList()
{
    // Update theme combo if needed
}

} // namespace ui::qt
