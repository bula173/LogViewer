# Changelog

All notable changes to LogViewer are documented here.

## [1.5.2] — 2026-05-21

### Bug fixes

- **Plugin extraction on Windows** ([#9](https://github.com/bula173/LogViewer/issues/9)) — the Zip Slip guard used a hardcoded `"/"` suffix appended to a `weakly_canonical()` path, which returns native backslash separators on Windows. The resulting mismatch caused every entry in the ZIP to be silently rejected as a path-traversal attempt, leaving an empty extraction directory with no `config.json`. Replaced the fragile string-prefix check with `lexically_relative()` which is portable and separator-agnostic. Also moved the canonical-root computation outside the per-entry loop.

## [1.5.1] — 2026-05-21

### Bug fixes

- **File dialog directories** — all file dialogs (Open Log, Save/Open Session, Export CSV/JSON/XML) now open in the last-used directory (persisted via `QSettings`) rather than the application install path. Falls back to Documents on first run.
- **Startup splash** — init progress and errors are now shown in a custom splash window at startup; users can acknowledge any issues before the main window appears.
- **Plugin ZIP packaging** — fixed a build-system issue where `config.json` was not reliably included in the plugin ZIP archive on configurations where the DLL output directory differs from the CMake binary directory (e.g. multi-config generators).

## [1.5.0] — 2026-05-20

### New features

- **Analysis panels** — seven right-dock panels for deep log inspection:
  - *Timeline Chart* — interactive bar histogram of event volume over time with zoom and brush selection
  - *Trace Viewer* — sequence-diagram-style view of events grouped by actor
  - *Stats Summary* — key metrics, event-type chart, Top-N value frequency table, per-field fill-rate statistics
  - *Pattern Analysis* — template clustering that groups structurally similar log lines
  - *Bookmarks* — annotate and navigate to important events
  - *Scenarios* — define multi-step event sequences and export matches as JSON Lines
  - *Actors* — event attribution tree built from actor definitions
- **Actor auto-discovery** — "Discover…" button in the Actor Definitions panel scans loaded log data, scores fields by cardinality and actor-hint keywords, and lets the user review and import suggestions in one step
- **Session save/load** — persist and restore the open file path and filter state across application restarts
- **View → Tabs menu** — hide or show individual content tabs to keep the layout focused
- **Startup update check** — when a new version is found at startup the Update dialog opens automatically (non-modal), not just the status-bar badge

### Performance improvements

- Lazy dirty-flag panel refresh: panels only recompute when their tab is visible; a single 150 ms debounce timer replaces five simultaneous `Refresh()` calls on every filter change
- In-panel search debounced (150 ms) to avoid O(n×m) rebuilds on every keystroke
- Pattern Analysis clustering capped at 5 000 events per type to prevent UI freezes on large logs
- Stats Summary field-statistics section sampled at 50 000 events to bound O(n×m) work without blocking the UI thread

### Code quality

- `PanelUtils.hpp` — shared `ParseTimestamp`, `VisibleIndices`, `kTsFields`, `kMsgFields` utilities eliminate duplication across Timeline, Trace, Bookmarks, Scenarios, Actors, and Stats panels
- Removed redundant *Events Over Time* chart from Stats Summary (covered by the interactive Timeline panel)
- Session file I/O switched from `std::ofstream`/`std::ifstream` to `QFile` for correct Unicode path handling on Windows
- Fixed `ScenariosPanel` double `OnScenarioChanged` call and JSON Lines string-escaping
- Fixed two implicit `size_t → double` conversion warnings in `StatsSummaryPanel`
- Tab tooltips on macOS now use an event filter + `QToolTip::showText()` to bypass platform-style interception

## [1.4.0] — 2026-05-19

### New features

- Time Range Filter panel
- Filter Profiles panel (save and restore named filter states)
- Pattern Analysis panel (initial version)
- Stats Summary panel (rewritten with richer metrics)
- Actors panel and Actor Definitions panel (QTreeWidget-based)
- Installation manifest system — tracks installed libraries, prevents overwriting during upgrades

### Bug fixes

- `FieldTranslator` cache fix — `SetTranslation`/`RemoveTranslation` now invalidate the cache correctly

### Testing

- Specification-based tests added for `Result`, plugins, `FieldTranslator`, `ParserFactory`, `KeyEncryption`, and the filter subsystem

## [1.3.0] and earlier

See git history.
