#include "StartupSplash.hpp"

#include <QApplication>
#include <QColor>
#include <QEventLoop>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPainter>
#include <QPushButton>
#include <QScreen>
#include <QVBoxLayout>

namespace ui::qt {

StartupSplash::StartupSplash(const QString& version, QWidget* parent)
    : QWidget(parent,
              Qt::SplashScreen | Qt::FramelessWindowHint |
                  Qt::WindowStaysOnTopHint)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setFixedSize(500, 340);

    // ── Root container (gives us the rounded dark card) ───────────────────
    auto* card   = new QWidget(this);
    auto* outerL = new QVBoxLayout(this);
    outerL->setContentsMargins(0, 0, 0, 0);
    outerL->addWidget(card);

    card->setObjectName("splashCard");
    card->setStyleSheet(
        "QWidget#splashCard {"
        "  background-color: #1e1e2e;"
        "  border-radius: 10px;"
        "  border: 1px solid #44475a;"
        "}"
        "QListWidget {"
        "  background: transparent;"
        "  border: none;"
        "  color: #cdd6f4;"
        "  font-size: 12px;"
        "}"
        "QListWidget::item { padding: 2px 4px; }"
        "QLabel#title  { color: #cdd6f4; font-size: 18px; font-weight: bold; }"
        "QLabel#sub    { color: #6c7086; font-size: 11px; }"
        "QLabel#status { color: #6c7086; font-size: 11px; font-style: italic; }"
        "QPushButton {"
        "  background: #89b4fa; color: #1e1e2e;"
        "  border-radius: 4px; padding: 5px 18px; font-weight: bold;"
        "}"
        "QPushButton:hover { background: #b4d0ff; }");

    auto* cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(24, 20, 24, 16);
    cardLayout->setSpacing(10);

    // ── Header ────────────────────────────────────────────────────────────
    {
        auto* titleLabel = new QLabel("LogViewer", card);
        titleLabel->setObjectName("title");

        auto* subLabel = new QLabel(QString("Version %1  —  starting up").arg(version), card);
        subLabel->setObjectName("sub");

        cardLayout->addWidget(titleLabel);
        cardLayout->addWidget(subLabel);
    }

    // ── Separator line ────────────────────────────────────────────────────
    {
        auto* sep = new QWidget(card);
        sep->setFixedHeight(1);
        sep->setStyleSheet("background-color: #44475a;");
        cardLayout->addWidget(sep);
    }

    // ── Log list ──────────────────────────────────────────────────────────
    m_log = new QListWidget(card);
    m_log->setFocusPolicy(Qt::NoFocus);
    m_log->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_log->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    cardLayout->addWidget(m_log, 1);

    // ── Bottom row: status label + optional continue button ───────────────
    {
        auto* bottomRow = new QHBoxLayout();

        m_statusLabel = new QLabel(card);
        m_statusLabel->setObjectName("status");
        bottomRow->addWidget(m_statusLabel, 1);

        m_continueBtn = new QPushButton(tr("Continue"), card);
        m_continueBtn->setVisible(false);
        bottomRow->addWidget(m_continueBtn);

        cardLayout->addLayout(bottomRow);
    }

    // ── Centre on primary screen ──────────────────────────────────────────
    if (const QScreen* screen = QApplication::primaryScreen())
    {
        const QRect sg = screen->geometry();
        move(sg.center() - rect().center());
    }
}

// ── Public slots ──────────────────────────────────────────────────────────

void StartupSplash::Step(const QString& message)
{
    m_statusLabel->setText(message);
    AddRow(u8"✓", message, QColor("#a6e3a1")); // green ✓
    Flush();
}

void StartupSplash::Warn(const QString& message)
{
    AddRow(u8"⚠", message, QColor("#f9e2af")); // orange ⚠
    Flush();
}

void StartupSplash::Error(const QString& message)
{
    m_hasErrors = true;
    AddRow(u8"✗", message, QColor("#f38ba8")); // red ✗
    Flush();
}

void StartupSplash::Finish(QWidget* /*mainWindow*/)
{
    if (!m_hasErrors)
    {
        close();
        return;
    }

    m_statusLabel->setText(tr("Issues were detected during startup — see above."));
    m_statusLabel->setStyleSheet("color: #f38ba8; font-size: 11px;");
    m_continueBtn->setVisible(true);

    QEventLoop loop;
    connect(m_continueBtn, &QPushButton::clicked, &loop, &QEventLoop::quit);
    loop.exec();

    close();
}

// ── Private helpers ───────────────────────────────────────────────────────

void StartupSplash::AddRow(const QString& icon, const QString& text,
                            const QColor& color)
{
    auto* item = new QListWidgetItem(icon + "  " + text, m_log);
    item->setForeground(color);
    m_log->scrollToBottom();
}

void StartupSplash::Flush()
{
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
}

} // namespace ui::qt
