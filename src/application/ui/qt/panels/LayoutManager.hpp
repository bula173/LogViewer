#pragma once

#include <QByteArray>
#include <QMap>
#include <QString>
#include <vector>

namespace ui::qt {

/// Describes a complete window layout: dock visibility + dock positions
/// (for user layouts) + content tab visibility and active tab.
struct LayoutDescriptor {
    QString name;
    bool    isBuiltIn {false};

    /// Content tab label → visible.  Tabs absent from the map are left as-is.
    QMap<QString, bool> tabVisibility;
    /// Content tab to activate (by label).  Empty = leave current.
    QString activeTab;

    bool filtersDockVisible {true};
    bool detailsDockVisible {true};
    bool bottomDockVisible  {false};

    /// Full QMainWindow::saveState() byte array.
    /// Non-empty only for user-created layouts; empty for built-in layouts
    /// (which control panels declaratively without touching dock positions).
    QByteArray windowState;
};

/// Manages named window layouts.
///
/// Built-in layouts are static and computed on demand.
/// User layouts are persisted in QSettings under the "userLayouts" group
/// and loaded at construction time.
class LayoutManager {
public:
    LayoutManager();

    /// Returns the two predefined layouts (XML Log Analysis, CAN Bus Analysis).
    static std::vector<LayoutDescriptor> BuiltInLayouts();

    /// Returns user-saved layouts in save order.
    const std::vector<LayoutDescriptor>& UserLayouts() const;

    /// Save or overwrite a user layout (matched by name).
    void Save(LayoutDescriptor layout);

    /// Delete a user layout by name. No-op if not found.
    void Remove(const QString& name);

private:
    void SaveToSettings() const;
    void LoadFromSettings();

    std::vector<LayoutDescriptor> m_userLayouts;
};

} // namespace ui::qt
