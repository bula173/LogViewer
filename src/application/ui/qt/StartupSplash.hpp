#pragma once

#include <QWidget>

class QListWidget;
class QLabel;
class QPushButton;

namespace ui::qt {

/**
 * @brief Frameless startup window shown while the application initialises.
 *
 * Displays each initialisation step as it happens and highlights warnings
 * (orange) and errors (red). If any errors were recorded, Finish() shows a
 * "Continue" button and waits for the user to acknowledge before closing.
 *
 * Typical usage in main():
 * @code
 *   StartupSplash splash(appVersion);
 *   splash.show();
 *
 *   splash.Step("Loading configuration…");
 *   SetupConfig();          // may call splash.Warn / splash.Error
 *
 *   splash.Step("Building UI…");
 *   MainWindow window(controller, events, &splash);
 *
 *   window.show();
 *   splash.Finish(&window); // blocks if errors, then closes
 * @endcode
 */
class StartupSplash : public QWidget
{
    Q_OBJECT

  public:
    explicit StartupSplash(const QString& version, QWidget* parent = nullptr);

    /// Add a normal progress step (green check-mark).
    void Step(const QString& message);

    /// Add a warning row (orange, non-fatal).
    void Warn(const QString& message);

    /// Add an error row (red) and set the error flag.
    void Error(const QString& message);

    /// Close immediately if no errors were recorded; otherwise show a
    /// "Continue" button and block in a local event loop until clicked.
    void Finish(QWidget* mainWindow);

    [[nodiscard]] bool HasErrors() const { return m_hasErrors; }

  private:
    void AddRow(const QString& icon, const QString& text, const QColor& color);
    void Flush(); ///< processEvents so each step is visible immediately

    QListWidget* m_log         {nullptr};
    QLabel*      m_statusLabel {nullptr};
    QPushButton* m_continueBtn {nullptr};
    bool         m_hasErrors   {false};
};

} // namespace ui::qt
