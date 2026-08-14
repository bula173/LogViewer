---
name: qt-widgets-ui-design
description: >-
  Design or audit desktop UI built with Qt Widgets (QMainWindow, QDockWidget,
  layouts, model/view, QSS). Use when creating or reviewing panels, dialogs,
  toolbars, dock layouts, delegates, or stylesheets in a Qt6 C++ Widgets
  application. Complements qt-ui-design (which targets QML/embedded) and
  qt-cpp-review (which reviews implementation correctness, not UX).
license: LicenseRef-Qt-Commercial OR BSD-3-Clause
compatibility: >-
  Designed for Claude Code, GitHub Copilot, and similar agents.
disable-model-invocation: false
metadata:
  author: local-project
  version: "1.0"
  qt-version: "6.x"
  category: conceptual
  changelog: "Initial release"
---
# Qt Widgets UI Design

Design and audit guidance for desktop UI built on **QWidget / QMainWindow / QDockWidget**, as distinct from QML-based UI (see `qt-ui-design` for that). Use this skill when the change touches a panel's layout, a dock's placement, a dialog's flow, a stylesheet, or a model/view delegate.

Small edits — *"move this button"*, *"widen the column"*, *"change this label"* — do not need the full checklist. Apply section 1 silently.

## 0. Context check (before designing a new panel/dialog)

Skip items already answered by the conversation or by reading the existing dock/panel code.

1. **Dock area** — main, left, right, or bottom? Check whether it must coexist with plugin-contributed panels in the same area (see `docs/PLUGIN_SYSTEM.md`).
2. **Floatable/closable** — should this dock be user-detachable, or fixed?
3. **Data volume** — is this panel backed by a model that can hold large row counts? If so it must use a view (QTableView/QTreeView) over a QAbstractItemModel, never a widget-per-row loop — see section 3.
4. **Theme** — does the app support light/dark switching here? Check `Config` for an active theme/palette source before hardcoding colors.
5. **Existing pattern** — is there a sibling panel (e.g. another `*Panel.cpp` under `src/application/ui/qt/`) that already solves this layout problem? Reuse its structure rather than inventing a new one.

If the request is an **audit of existing widget UI code**, skip to section 4.

---

## 1. Layout principles (apply silently)

- **Layouts, not fixed geometry.** Never call `setGeometry`/`move` for normal widgets; use `QVBoxLayout`/`QHBoxLayout`/`QGridLayout`/`QFormLayout` so the panel survives resizing, font-size changes, and DPI scaling.
- **QSplitter for user-resizable regions**, `QGridLayout`/`QFormLayout` for label+field rows. Set stretch factors deliberately — don't leave every widget at equal stretch and call it done.
- **QFormLayout for settings/property dialogs** — label-left, field-right is the Qt-native idiom; deviating from it reads as non-native.
- **Minimum sizes over fixed sizes.** Prefer `setMinimumWidth`/`setMinimumSize` to `setFixedSize` unless the widget genuinely cannot resize (e.g. a fixed icon).
- **Group related controls** in a `QGroupBox` or a bordered frame; don't let a dialog become an undifferentiated stack of fields.
- **Toolbar economy.** Every `QAction` on a toolbar should be reachable from a menu too — toolbars are a shortcut, not the only path. Use `QToolButton` with `setPopupMode` for grouped/related actions instead of proliferating top-level buttons.
- **Dock defaults.** New docks should declare a sensible default area and allowed areas explicitly (`setAllowedAreas`), and a reasonable default size via `resizeDocks` on first show — don't leave it to Qt's arbitrary initial split.
- **Status feedback.** Long operations (parsing large logs, AI calls) must show a `QProgressBar`/`QProgressDialog` or a status-bar message — never a UI that appears frozen for more than ~400 ms.

## 2. Style and theming

- **QSS scope narrowly.** Prefer setting `objectName` and scoping stylesheet rules to it (`QPushButton#applyFilterButton { ... }`) over broad selectors that leak into child widgets like `QDialog QPushButton { ... }`, which can unexpectedly restyle nested dialogs/message boxes.
- **No hardcoded colors in C++.** Route colors through the app's theme/palette mechanism (check `Config`/theme manager) so dark mode and custom themes stay correct. If no theme system exists for this widget yet, flag that rather than inlining a hex color.
- **Respect `QPalette` for native look.** Where the app doesn't need custom branding, prefer `QPalette` roles over QSS — it stays correct across platform styles (Fusion, native macOS/Windows) automatically.
- **Icons at multiple DPIs.** Use `QIcon` with an `.svg` source or multi-resolution `.png` set, not a single fixed-size bitmap, so panels stay sharp on HiDPI displays.

## 3. Model/view for data-heavy panels

This app is built for **millions of log entries** via a virtual list architecture (see `db/EventsContainer`) — this constraint is not optional for any panel showing log rows.

- **Never populate rows as individual widgets** (`QListWidget`/`QTableWidget` with per-row items, or a layout of custom row widgets) for anything backed by log data. Use `QAbstractItemModel`/`QAbstractTableModel` + `QTableView`/`QTreeView`/`QListView` so Qt only materializes visible rows.
- **Custom rendering → `QStyledItemDelegate`**, not a widget per cell. Widgets-per-cell defeats the virtualization the view provides and is a common source of scroll lag — flag it in review.
- **Sorting/filtering → `QSortFilterProxyModel`** (or the project's existing filter pipeline) between the source model and the view, not by mutating the source model's row order in place.
- **Selection and signals** should flow through the model's `QItemSelectionModel`, not ad hoc widget state, so multiple views over the same model (if any) stay in sync.

## 4. Accessibility and interaction

- **Tab order** must follow visual/reading order — verify with `setTabOrder` when the layout doesn't produce the right order automatically.
- **Every interactive widget needs a label or `setAccessibleName`** — icon-only toolbar buttons and bare `QLineEdit`s are common misses.
- **Keyboard operability.** Every action reachable by mouse (button, context menu item) must also be reachable by keyboard (shortcut, menu, or Tab+Space/Enter). Don't ship a mouse-only interaction.
- **Standard shortcuts via `QKeySequence::StandardKey`** (Copy, Find, Save, …) rather than inventing new bindings for actions that have a platform convention.
- **Destructive actions confirm.** Deleting a filter profile, clearing bookmarks, discarding unsaved session state — gate behind a confirmation (`QMessageBox`) or make it undoable.
- **Tooltips for icon-only controls**, and status-bar or inline hints for non-obvious interactions (drag-to-reorder, right-click menus).

## 5. Audit instructions

When asked to review existing Qt Widgets UI code, check the file(s) against sections 1–4 and report only genuine issues, categorized:

- **Critical** — data-heavy panel using per-row widgets instead of model/view; mouse-only interaction with no keyboard path; hardcoded absolute geometry that breaks resizing; destructive action with no confirmation.
- **Warning** — broad/leaking QSS selectors; hardcoded colors bypassing the theme system; missing accessible names; toolbar action with no keyboard equivalent; fixed-size widget that should have a minimum instead.
- **Opportunity** — inconsistent layout pattern vs. a sibling panel; missing tooltip; stretch factors left at Qt defaults where a deliberate ratio would read better.

Don't flag style choices the surrounding codebase already applies consistently (e.g. project's own spacing conventions) — match existing patterns per `CLAUDE.md`'s surgical-changes rule, and only add commentary on unrelated pre-existing issues rather than fixing them unasked.

## 6. References

- `qt-ui-design` — shared UX laws (Jakob's Law, Hick's Law, proximity/similarity, etc.), typography scale, and motion budgets apply here too; this skill only adds the Widgets-specific mechanics.
- `qt-cpp-review` — for ownership/threading/model-contract correctness of the same code, run that skill separately; this skill covers UX/design, not implementation safety.
- Qt docs: Model/View Programming, Qt Style Sheets Reference, Layout Management.
