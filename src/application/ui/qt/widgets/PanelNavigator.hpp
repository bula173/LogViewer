#pragma once

#include <QDialog>
#include <QListWidget>
#include <QLineEdit>
#include <vector>
#include <functional>

namespace ui::qt::widgets
{

struct PanelInfo
{
    QString name;
    QString icon;  // Emoji or icon indicator
    int tabIndex;
    QString category;  // "Filters", "Analysis", "Details", etc.
    std::function<void()> onSelect;
};

class PanelNavigator : public QDialog
{
    Q_OBJECT
  public:
    explicit PanelNavigator(QWidget* parent = nullptr);

    void addPanel(const PanelInfo& panel);
    void clearPanels();
    void show();

  private slots:
    void onSearchTextChanged(const QString& text);
    void onItemDoubleClicked(QListWidgetItem* item);
    void onItemHighlighted(int row);
    void onEnterPressed();

  private:
    void updateFilteredList();
    void selectCurrentItem();

    QLineEdit* m_searchInput;
    QListWidget* m_panelList;
    std::vector<PanelInfo> m_allPanels;
};

}  // namespace ui::qt::widgets
