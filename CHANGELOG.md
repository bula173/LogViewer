# Changelog

All notable changes to LogViewer are documented here.

## [Unreleased]

### New features

- **Export/Import Filter Profiles** — the Filter Profiles panel can now export a selected profile to a `.filters.json` file and import one or more profiles from a file, merging them into the current list (with a prompt to overwrite on name collisions). Enables sharing filter setups between teammates.
- **Customizable keyboard shortcuts** — the Keyboard Shortcuts dialog (Help → Keyboard Shortcuts) now reflects the application's actual live bindings instead of a hand-maintained (and previously stale/incomplete) list. Double-click any entry to rebind it; conflicting assignments are rejected. Includes **Reset to Default**, **Reset All**, and a **Print…** button for a print-friendly cheat sheet. Custom bindings persist to `<appdata>/keybindings.json`.

### Performance

- **Regex filter matching** no longer takes a shared global lock and allocates a cache-key string on every single value it tests — `RegexFilterStrategy` now caches its last-compiled pattern per filter instance instead. Matters most on large files with regex filters active, where this ran once per event per condition.
- **JSON array-format loading** (`[...]` files) now streams via a SAX callback instead of parsing the entire file into one in-memory DOM tree before emitting any events — bounds peak memory and restores progress feedback during load. Events are also now batched (matching CSV/DLT/ASC/Evlog) instead of notified one at a time.
- **`LogEvent` no longer builds a per-event `unordered_map` index.** Every loaded event was allocating its own hash table for its handful of fields; at large event counts this was plausibly the single largest memory cost in the whole load path (100M events → 100M separate hash tables plus that many extra heap allocations). `findByKey()` now does a linear scan over the event's field vector — faster in practice at this scale (no hashing, cache-friendly), and removes the per-event allocation entirely.

### Fixes

- **Fixed a crash on clicking an event row after loading a file** (`EXC_BAD_ACCESS` inside `FieldTranslator`'s internal map, reported as random-looking heap corruption). Root cause: `plugins/ai/Config.hpp` declared its own `struct config::Config` and `config::GetConfig()` — under the exact same namespace and name as the real application config in `src/application/config/Config.hpp`, but a completely different, much smaller struct. Since the AI plugin is a separate dylib sharing the same process, this was a One Definition Rule violation: calls to `config::GetConfig()` from inside the plugin could bind to the *main app's* real `Config` object (a strong symbol beats the plugin's weak/inline one) while the plugin's compiled code still used its own small struct's field offsets — writing plugin fields at byte offsets that actually landed inside `FieldTranslator`'s internal map, corrupting it once at startup. The plugin's config is now namespaced under `ai` instead, so it can never collide with the app's.
- The in-app Keyboard Shortcuts reference had drifted out of sync with the real menu bindings (missing entries for Export, Generate Report, Group Events, Tag & Annotate; a wrong shortcut listed for Jump to Timestamp). It's now generated from the same registry the menus use, so it can't drift again.
- `packaging/create_self_signed_cert.ps1`/`.bat` called `makecert.exe`/`pvk2pfx.exe`, which Microsoft has removed from current Windows SDK releases — the script would fail with "not recognized" on a fresh install. Rewritten to use PowerShell's built-in `New-SelfSignedCertificate`, which needs no separate SDK download.

### Build system

- macOS release builds now fall back to ad-hoc code signing (`codesign --sign -`) when no `MACOS_CERTIFICATE` secret is configured, instead of shipping the app bundle with no signature at all. This does not satisfy Gatekeeper (only a paid Apple Developer ID + notarization does), but keeps the bundle's signature internally consistent after `macdeployqt` copies in Qt's frameworks, and is required outright on Apple Silicon. See `packaging/CODE_SIGNING_README.md`.
- `packaging/CODE_SIGNING_README.md` now documents the actual GitHub Actions signing path (`WINDOWS_CERTIFICATE`/`WINDOWS_CERTIFICATE_PWD` secrets) for self-signed certificates, not just the local CMake-flag path.

## [1.11.0] — 2026-08-15

### New features

- **Dashboard tab** — log overview and statistics panel with a "Generate Report" action that recalculates statistics on demand.
- **Unified search bar** — Ctrl+F/Cmd+F with live match counting and a "Details…" panel for advanced query options, replacing the old `SearchBar`.
- **Auto-switch view based on file type**, plus a reorganized left panel (icons, grouping) for discoverability.

### Fixes

- Resolved a Ctrl+F/Cmd+F crash on macOS caused by duplicate shortcuts, and made Ctrl+F work reliably on Linux via a window-level shortcut.
- Removed an unsafe `const_cast` in `ReportGenerator` that could cause memory corruption; fixed an index mismatch in report generation and added thread-safe access.
- Fixed several security- and network-related issues, and improved file I/O error handling for sessions and exports.
- Fixed a logger initialization-order bug where the configured log level wasn't applied after config load.
- **SearchEngine**: fixed a stale-pattern cache bug (recompiling a pattern on an existing engine kept matching the previous pattern) and a broken advanced-query parser (`"A OR B"` was silently evaluated as `"A AND B"`).
- **ExportManager**: XML export error reporting called a nonexistent `QXmlStreamWriter::errorString()`; now reads the error from the underlying file device.
- **Installation manifest**: fixed Qt5 library names left over from the Qt6 migration, and a broken version lookup that always reported "unknown" for every bundled library.
- Fixed the Windows installer being flagged by Microsoft Defender (`Wacatac.B!ml`) — `OllamaClient::IsAvailable()` made a live network probe on every AI panel refresh, matching a background-thread-plus-outbound-connection pattern AV heuristics treat as a C2 beacon; now checks configuration state only, consistent with the other AI providers.

### Refactoring

- **Weak_ptr observer pattern** — `IModelObservable` observers are now tracked via `std::weak_ptr`, removing the dangling-pointer risk of the previous raw-pointer observer list (see `docs/ARCHITECTURE_IMPROVEMENTS.md`).
- Adopted C++20 `std::ranges`, `std::optional`, and three-way comparison (`<=>`) across several call sites (`BookmarksPanel`, filter lookups, `Version`).
- `MainWindowFileOpsHelper` extracted from `MainWindow` as the first step of a broader architecture cleanup (Phase 3).

### Build system

- Fixed the SDK's `find_package(LogViewer)` package config, which never actually expanded `@PACKAGE_INIT@`/`@PACKAGE_VERSION@` — version-compatibility checks were silently always empty.
- Added LTO for Release builds of the application's own code, auto-detected ccache instead of hardcoding it, removed an unused `gflags` dependency, and collapsed several duplicate CMake presets.
- CI: fixed a silent script-death bug in the Qt6-detection fallback (`bash -e` + unguarded `pkg-config`), a CMake target conflict from missing `SKIP_EXAMPLES`, and a VirusTotal scan crash caused by CPack leaving a duplicate installer copy that got uploaded twice.

### Testing

- Added functional test suites for `SearchEngine`, filters, and large-file handling, and expanded unit/functional coverage for v1.11.0 bug fixes.
- Fixed several pre-existing compile and logic errors across the test suite (`FilterManagerTest`, `FilterWorkflowTest`, `LargeFileHandlingTest`, `ReportGenerationTest`) left over from earlier refactors.

### Documentation

- Replaced a self-contradicting proprietary/MIT license file with the correct MIT license text.
- Reconciled version numbers across `CHANGELOG.md`, `ROADMAP.md`, `USER_MANUAL.md`, and `REQUIREMENTS.md`, which had drifted independently.
- Consolidated overlapping plugin documentation and removed dead wxWidgets content (the project has been Qt-only for some time).

## [1.10.0] — 2026-07-31

### New features

- **Unified Preferences dialog** consolidating all application settings into one place.
- **Local Gemma 2B inference via llama.cpp** — replaces the earlier heuristic-fallback `GemmaInferenceEngine` with actual on-device LLM inference for actor discovery; model download available from the Help menu.
- **Theme Customization dialog**, enhanced export options, improved Bookmarks UI, keyboard navigation, and event tagging (delivered as staged phases 1–11 of the v1.10.0 plan).
- **Smart notifications, event grouping, and report generation.**
- **Column width persistence** and session auto-save.
- **Filter AND logic** and advanced search query support, with filter/search performance optimizations.

### Fixes

- Fixed Windows Unicode file-path handling in `BookmarksPanel` and `ExportDialog`.
- Fixed several crash and correctness issues found in code review (8 correctness issues, 4 refactors).
- Preserved active filter state across sorting; auto-apply type filters on selection change; added stale-index validation in `RefreshView` and cleared stale sort cache after merge to prevent merge+sort crashes.

### Refactoring

- Unified Export dialog and reorganized Analysis menu for discoverability; added tooltips and status-bar hints.
- Prepared the Gemma inference architecture for real LLM integration ahead of the llama.cpp switch.

## [1.7.2] — 2026-06-02

### New features

- **Actor discovery + sequence diagram panel** — auto-discovers communicating actors from log data and renders a sequence diagram.
- **Startup update check** — notifies the user via a message box when a newer version is available.

## [1.7.1] — 2026-06-02

### Refactoring

- Consolidated plugin management into a single **Tools → Manage Plugins…** entry point.

### Fixes

- Corrected `PluginManagerDialog` enable/disable logic.
- Auto-reload the ASC log when a DBC is loaded, and wired signal selection through to the events filter.

## [1.7.0] — 2026-06-01

### New features

- **Named window layouts** with predefined presets for XML and CAN workflows.
- **Side-by-side log comparison** with three synchronization modes (binary-search based).
- **Real-time file tailing** (Follow File, Ctrl+T).
- **JSON/NDJSON log parser.**
- **Plugin Manager dialog** (Tools → Manage Plugins…).
- Copy as JSON/CSV, and jump-to-timestamp (Ctrl+G).
- Scenarios now auto-persist in `QSettings` and survive without a session file.
- Richer `TraceViewerPanel` — search bar, actor/type breakdown, span call tree.

### Refactoring

- Upgraded to C++23 and rewrote `Result<void, E>` using `std::expected`.
- Applied several patterns across the codebase: index types, async filtering, interface cleanup.

### Testing

- Added unit tests for the DLT parser, Evlog parser, and `EvlogTemplateRegistry`.

## [1.6.1] — 2026-05-22

### New features

- **DLT parser** — native support for AUTOSAR Diagnostic Log and Trace (`.dlt`) binary files. Parses storage headers (DLT\x01 magic), standard headers (HTYP flags, WEID/WSID/WTMS optional fields), and extended headers (AppID, ContextID, MSIN). Decodes verbose payload arguments (bool, int8–int64, uint8–uint64, float32/64, strings, raw data); non-verbose payloads are shown as `MsgID=0x… [hex]`. Emitted fields: `timestamp`, `level`, `type`, `AppID`, `ContextID`, `EcuID`, `MsgCtr`, optional `SessionID`, `info`.
- **Evlog parser** — native support for POSIX 1003.25 Enterprise Event Logging (`.evl`) binary files as produced by `evlogd`. Reads the 60-byte little-endian `posix_log_entry` header and variable payload. Payload formats: `STRING` (UTF-8 text), `PRINTF` (format string + binary varargs shown as hex), `BINARY` (hex dump or template-decoded). Emitted fields: `timestamp`, `level` (Emergency→Debug), `facility` (kern/user/…/local7), `event_type` (hex), `pid`, `uid`, `recid`, optional `cpu`, `flags`, `info`.
- **Evlog template support** — BINARY payloads can be decoded using evlog template files (`.t`/`.tmpl`/`.template`). Templates describe the structured binary layout of a specific `(facility, event_type)` combination with typed field declarations (`int`, `char[N]`, `string`, `float`, …) and an optional `%field%`-substitution format string. Use **File → Load Evlog Templates…** to point at a template directory; the decoder falls back to hex dump for records with no matching template.
- **Signal Browser panel** — dedicated left-dock tab alongside the Filters panel, showing the CAN frame and signal tree from a loaded DBC file. Replacing the earlier embedding inside the Filter tab, the panel now appears as its own resizable dock widget with independent show/hide control. Populated exclusively from DBC structure (no dependency on loaded log data).

### Refactoring

- **Signal Plot panel** — removed the embedded signal-selection tree. Signal selection is now driven entirely from the Signal Browser dock; `SignalPlotPanel::SetSelectedSignals()` replaces the internal tree iteration. The panel displays a "Select signals in the Signal Browser panel" placeholder when no signals are chosen.
- Signal Browser → Signal Plot wiring uses a `SignalSelectionChanged` Qt signal so the two panels remain decoupled.

## [1.6.0] — 2026-05-22

### New features

- **CAN / ASC log format support** — the application now parses Vector CANalyzer `.asc` files natively. Each frame is stored as a structured event with fields `CAN_ID`, `CAN_Channel`, `CAN_DLC`, `CAN_Data`, `CAN_IDE`, `type` (Rx/Tx/TxRq/ErrorFrame), and `timestamp` (float seconds). Error frames are captured as dedicated events.
- **DBC signal decoding** — load a `.dbc` CAN database alongside an ASC log (File → Load DBC…) to decode raw frame bytes into named signal values stored as `SIG:<name>` fields. Supports Intel (little-endian) and Motorola (big-endian) bit layouts.
- **Signal Plot panel** — new content tab showing how decoded `SIG:*` field values change over time. Left pane lists all discovered signals with checkboxes; right pane renders one `QLineSeries` per selected signal with automatic downsampling at 2 000 points to keep rendering fast. Supports both ASC float-second timestamps and ISO date timestamps.
- **Format-specific statistics (Strategy pattern)** — `StatsSummaryPanel` now selects a statistics strategy at refresh time based on data shape:
  - `CanStatisticsStrategy`: shows a *CAN Bus Summary* section (total frames, Rx/Tx/TxRq/error breakdown with %, unique IDs, channels, duration, frame rate) and a *Signal Ranges* section (min/max/avg for every `SIG:*` numeric field, up to 15 signals).
  - `GenericStatisticsStrategy`: no-op fallback for XML/CSV — the extra group box stays hidden.
  - Adding support for a new format requires only implementing `IStatisticsStrategy`.

### Bug fixes

- **DbcParser signal assignment** — `rbegin()->first` on a `std::map` returns the element with the **largest** key after insertion, not the newly inserted one. When DBC files contained messages in non-ascending ID order (e.g., 1, 2047, 179), `SG_` lines following the smaller `BO_` were silently attached to the wrong message. Fixed by storing the new ID before the move and using `db.messages[newId]` directly.
- **ThemeSwitcher include path** — `src/main/MyAppQt.cpp` used the stale flat path `"qt/ThemeSwitcher.hpp"` after the UI restructuring; updated to `"qt/utils/ThemeSwitcher.hpp"`.
- **Duplicate test source** — `BuiltinConversionPluginsTest.cpp` was listed twice in `tests/CMakeLists.txt` (once via `GLOB` and once explicitly), causing a duplicate-compilation error on some platforms. Removed the explicit entry.

### Refactoring

- **Qt UI source restructuring** — the `src/application/ui/qt/` directory was reorganised into four subdirectories to improve navigability (38 files moved with full history preserved via `git mv`):
  - `panels/` — all dock and content panels (FiltersPanel, StatsSummaryPanel, SignalPlotPanel, TimelineChartPanel, etc.)
  - `dialogs/` — modal dialogs (ConfigEditorDialog, FilterEditorDialog, LogFileLoadDialog, UpdateDialog, etc.)
  - `events/` — events table model and view (EventsTableModel, EventsTableView)
  - `utils/` — shared utilities (PanelUtils, ThemeSwitcher, ExportManager, TypeFilterView, UpdateChecker)
  - `target_include_directories` now exposes the `qt/` root so cross-subdir includes resolve with explicit prefixes (e.g. `"panels/FiltersPanel.hpp"`, `"utils/PanelUtils.hpp"`).

### Testing

- `AscParserTest` — 8 tests covering error frames, standard/extended frame parsing, DBC signal decoding (Intel and Motorola byte order), multi-channel files, and malformed input.
- `DbcParserTest` — 6 tests covering symbol parsing, Intel/Motorola signal layouts, out-of-order message IDs, and file-based round-trip parsing.

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
