# LogViewer User Manual

**Version**: 1.7  
**Application**: LogViewer  

---

## Table of Contents

1. [Introduction](#1-introduction)
2. [Getting Started](#2-getting-started)
3. [Opening Log Files](#3-opening-log-files)
   - [Supported Formats](#31-supported-formats)
   - [Opening a File](#32-opening-a-file)
   - [Drag and Drop](#33-drag-and-drop)
   - [Recent Files](#34-recent-files)
   - [Loading a DBC File](#35-loading-a-dbc-file-can-signal-decoding)
   - [Loading Evlog Templates](#36-loading-evlog-templates)
4. [The Events Table](#4-the-events-table)
   - [Columns](#41-columns)
   - [Selecting and Navigating Events](#42-selecting-and-navigating-events)
   - [Copying Events](#43-copying-events)
   - [Jump to Timestamp](#44-jump-to-timestamp)
5. [Filtering](#5-filtering)
   - [Text Search](#51-text-search)
   - [Type Filter](#52-type-filter)
   - [Time Range Filter](#53-time-range-filter)
   - [Actor Filter](#54-actor-filter)
   - [Filter Profiles](#55-filter-profiles)
6. [Analysis Panels](#6-analysis-panels)
   - [Statistics](#61-statistics)
   - [Signal Plot](#62-signal-plot)
   - [Timeline Chart](#63-timeline-chart)
   - [Pattern Analysis](#64-pattern-analysis)
   - [Trace Viewer](#65-trace-viewer)
   - [Bookmarks](#66-bookmarks)
   - [Scenarios](#67-scenarios)
   - [Actors](#68-actors)
7. [Side-by-Side Comparison](#7-side-by-side-comparison)
   - [Opening the Panel](#71-opening-the-panel)
   - [Loading Files](#72-loading-files)
   - [Synchronisation Modes](#73-synchronisation-modes)
8. [Signal Browser and DBC Decoding](#8-signal-browser-and-dbc-decoding)
9. [AI-Assisted Analysis](#9-ai-assisted-analysis)
   - [Configuring an AI Provider](#91-configuring-an-ai-provider)
   - [Running an Analysis](#92-running-an-analysis)
10. [Named Layouts](#10-named-layouts)
11. [Export](#11-export)
12. [Themes](#12-themes)
13. [Configuration](#13-configuration)
14. [Keyboard Shortcuts](#14-keyboard-shortcuts)

---

## 1. Introduction

LogViewer is a desktop application for inspecting, filtering, and analysing log files. It supports multiple log formats — from plain XML/CSV to binary formats such as AUTOSAR DLT and POSIX Evlog — and provides a rich set of analysis panels, AI-assisted insights, and a side-by-side comparison view.

Key capabilities at a glance:

- Open log files in XML, CSV, JSON, CAN/ASC, DLT, or Evlog format
- Filter events by text, type, time range, or actor
- Visualise CAN signals, event timelines, and message patterns
- Compare two log files side-by-side with automatic or manual timestamp synchronisation
- Run AI analysis on filtered events using local or cloud providers
- Save named dock layouts and filter profiles for different workflows

---

## 2. Getting Started

When you first launch LogViewer you see the main window with:

- **Menu bar** — File, View, Tools, Help
- **Search bar** at the top — for quick text search (Ctrl+F)
- **Events table** in the centre — where loaded log entries appear
- **Left dock** — Filters panel and Signal Browser panel (tabbed)
- **Right content tabs** — Events, Statistics, Signals, Timeline, Patterns, Trace Viewer, Bookmarks, Scenarios, Actors, Side by Side
- **Details panel** (bottom or right dock) — full field view for the selected event
- **Bottom dock** — AI chat / plugin panels

All panels can be resized, floated, or hidden. Drag a panel's title bar to move it; double-click the title bar to float it as a separate window.

---

## 3. Opening Log Files

### 3.1 Supported Formats

| Extension | Format | Notes |
|-----------|--------|-------|
| `.xml` | Generic XML | Default when extension is unrecognised |
| `.csv` | Comma-Separated Values | |
| `.json` | JSON array or NDJSON | Array `[{…},…]` or one object per line |
| `.jsonl` | Newline-delimited JSON | Alias for NDJSON; same parser as `.json` |
| `.asc` | CAN/Vector CANalyzer | Optional DBC for signal decoding |
| `.dlt` | AUTOSAR DLT binary | Storage header + verbose/non-verbose payloads |
| `.evl` | POSIX 1003.25 Evlog binary | Optional template directory for BINARY payloads |

#### JSON log format

LogViewer accepts two JSON layouts:

- **Array** — a single JSON array at the top level: `[{"ts":"…","level":"INFO","msg":"…"}, …]`
- **NDJSON / JSONL** — one JSON object per line (format used by Winston, Bunyan, spdlog JSON sink, Loki, etc.)

Nested objects are flattened with dot notation: `{"ctx":{"host":"db"}}` becomes the key `ctx.host`. Arrays are flattened with index suffixes: `{"tags":["a","b"]}` becomes `tags.0` and `tags.1`.

If you open a file with an unknown or missing extension, LogViewer prompts you to pick the format before parsing begins.

### 3.2 Opening a File

**File > Open** (Ctrl+O) opens a file-picker dialog. The dialog filters by all supported extensions. After you select a file, the application parses it in the background and displays a progress indicator in the status bar. The UI remains interactive during loading.

Merging multiple files into one view is possible by opening them one after another — subsequent opens append events to the current dataset. Use **File > Clear** to start fresh.

### 3.3 Drag and Drop

Drag a log file from a file manager onto the main window. The application detects the extension and begins parsing immediately. If the extension is unrecognised, the format-picker dialog appears.

### 3.4 Recent Files

**File > Recent Files** lists the last ten files you opened. Click any entry to re-open it. Paths are stored per session; they are cleared if a file is deleted or moved.

### 3.5 Loading a DBC File (CAN Signal Decoding)

Before or after opening an `.asc` file, choose **File > Load DBC…** and select a `.dbc` CAN database. Once loaded:

- Every CAN frame whose ID matches a message in the DBC is decoded into named signals stored as `SIG:<name>` fields.
- The Signal Browser dock (left panel, second tab) is populated with the DBC frame and signal tree.
- The Signal Plot panel can plot any selected signal's value over time.

If you load an `.asc` file when a DBC is already loaded, decoding happens automatically. The active DBC path is saved in the session and reused on next launch.

### 3.6 Loading Evlog Templates

Evlog BINARY payloads can be decoded into structured fields using template files. Choose **File > Load Evlog Templates…** and select a directory containing `.t`, `.tmpl`, or `.template` files. Templates are keyed by `(facility, event_type)` and declare typed field layouts with an optional format string.

- Once a template directory is loaded, all subsequent `.evl` files are parsed with template decoding enabled.
- Records with no matching template fall back to a hex dump in the `info` field.
- The selected directory is remembered for the rest of the session and restored on the next launch.

Template file syntax example:

```
facility    8       # LOG_USER
event_type  42
description MyAppEvent
attributes {
    int32   error_code
    char[64] message
}
format "Error %error_code%: %message%"
```

---

## 4. The Events Table

### 4.1 Columns

The events table shows one row per log event. Default columns:

| Column | Description |
|--------|-------------|
| # | Sequential row number |
| Timestamp | Event time (float seconds for ASC/DLT/Evlog, ISO date for XML/CSV) |
| Level / Type | Severity level or event type (colour-coded based on configurable field) |
| Source / Actor | Originating component or actor (format-dependent) |
| Message / Info | The main event text |

Column visibility, order, and width are user-configurable in **Tools > Settings > Columns**. Column widths are persisted across sessions.

Rows are colour-coded based on the **type filter field**, which is auto-detected when a file is loaded. Colours are mapped in **Tools > Settings > Colors** (any column can have colour rules — the first matching column wins).

### 4.2 Selecting and Navigating Events

- Click a row to select it and display its full field list in the **Details panel**.
- Use the arrow keys to move between rows.
- The table uses virtual scrolling — rendering is O(1) regardless of dataset size.
- Click any column header to sort by that field.

### 4.3 Copying Events

| Action | Result |
|--------|--------|
| Ctrl+C | Copies selected rows as tab-separated plain text |
| Right-click > Copy as JSON | Copies selected events as a JSON array |
| Right-click > Copy as CSV | Copies selected events as CSV with a header row |

Multiple rows can be selected with Shift+Click or Ctrl+Click before copying.

### 4.4 Jump to Timestamp

Press **Ctrl+G** (or **Edit > Jump to Timestamp**) to open the Jump to Timestamp dialog. Enter a timestamp value in the format used by the current log file (e.g. `1234567.890` for ASC float seconds, or `2024-03-15 14:22:03.456` for XML/CSV ISO format). The table scrolls to and selects the event with the nearest matching timestamp.

---

## 5. Filtering

All filter controls are in the **Filters** panel on the left. Changes take effect immediately (debounced at 150 ms to avoid excessive recomputation on large datasets).

### 5.1 Text Search

The search bar at the top of the window (Ctrl+F to focus) supports four match modes selectable from the dropdown next to it:

| Mode | Description |
|------|-------------|
| Plain | Case-insensitive substring search |
| Regex | Full regular expression (RE2 syntax) |
| Fuzzy | Approximate matching — tolerates minor typos |
| Wildcard | Shell-style `*` and `?` patterns |

Press Enter or click **Search** to run. Matching rows are highlighted; use the up/down arrows next to the bar to navigate between results.

### 5.2 Type Filter

The Type Filter panel shows all distinct values of the **type filter field**. Check or uncheck values to show or hide event categories. **Select All** / **Deselect All** buttons are available.

**Auto-detection**: When a file is loaded, LogViewer automatically detects the type field by scanning the first 100 events for common names: `level`, `type`, `severity`, `priority`, `category`, and others. If none are found, a prompt asks for the field name — the answer is saved and used for future loads.

**Manual override**: Set a fixed field name in **Tools > Settings > General** ("Type Filter Field"). Leave the field empty to re-enable auto-detection. Setting it explicitly is useful when your logs use a non-standard field name or you have multiple file types with different naming conventions.

### 5.3 Time Range Filter

The **Time Range** panel lets you restrict visible events to a time window:

1. Enter a **Start** and **End** timestamp (or leave either blank for an open-ended range).
2. Click **Apply**.

The range is applied as an additional filter on top of any active text/type filters. Clear the fields and click Apply to remove the time constraint.

### 5.4 Actor Filter

The **Actors** panel in the left dock shows a tree of actor definitions. Enable or disable individual actors to include or exclude their events. Actors are defined in the **Actor Definitions** panel (content tab).

### 5.5 Filter Profiles

Filter profiles let you save and quickly switch between named filter states.

- **Save**: Click **Save Profile** in the Filter Profiles panel, give the profile a name, and the current text query, type filter, time range, and actor selection are stored.
- **Load**: Select a profile from the list and click **Load** to restore it.
- Profiles are persisted across sessions.

---

## 6. Analysis Panels

Switch between analysis views using the tab bar in the content area. Panels refresh lazily — computation only runs when the tab is visible, keeping the UI fast on large datasets.

### 6.1 Statistics

The **Statistics** tab displays aggregate metrics for the loaded data:

- Total event count, unique sources, time span
- Top-N most frequent values for key fields
- Per-field fill-rate (percentage of events that have each field populated)
- **CAN-specific** (when ASC data is loaded): total frame count, Rx / Tx / TxRq / ErrorFrame breakdown with percentages, unique CAN IDs, channels, recording duration, average frame rate, and min/max/avg for every decoded `SIG:*` signal

### 6.2 Signal Plot

The **Signals** tab plots numeric `SIG:*` field values over time as line series.

1. Load an ASC file with a DBC file loaded first.
2. In the **Signal Browser** dock (left panel, second tab), check one or more signals.
3. The Signal Plot updates immediately to show those signals.

Each series is automatically downsampled to 2 000 points when the dataset is larger, keeping the chart responsive. Zoom by scrolling; pan by clicking and dragging the axis.

### 6.3 Timeline Chart

The **Timeline** tab shows a bar histogram of event volume over time. Use it to identify bursts, quiet periods, and temporal clusters at a glance.

- **Zoom**: scroll wheel over the chart
- **Brush**: click and drag to select a time range; the main events table filters to that range
- Reset zoom with the Reset button in the toolbar

### 6.4 Pattern Analysis

The **Patterns** tab groups structurally similar log lines using template clustering. Messages that share the same structure but differ only in numeric or variable parts are collapsed into a single template with a match count.

- Useful for identifying repeated error conditions or high-frequency background noise
- Capped at 5 000 events per type to keep computation fast on large files
- Click a template row to filter the main table to matching events

### 6.5 Trace Viewer

The **Trace Viewer** tab shows a sequence-diagram-style timeline where each actor has a vertical lane and events are drawn as labelled markers. Use it to visualise message flows between system components.

Actors must be defined in the Actor Definitions panel before the Trace Viewer can render lanes.

### 6.6 Bookmarks

The **Bookmarks** tab lets you annotate important events:

1. Right-click any event in the events table and choose **Add Bookmark**, or press the star button in the toolbar.
2. Optionally add a note to the bookmark.
3. The bookmarks list shows the event text and your note; double-click a bookmark to navigate to that event.

Bookmarks are saved in the session file.

### 6.7 Scenarios

The **Scenarios** tab lets you define multi-step event patterns and search for them in the loaded log:

1. Create a new scenario and add steps, each specifying a field name and a match expression.
2. Click **Find** to locate all occurrences where the steps fire in sequence.
3. Results can be exported as JSON Lines for further processing.

### 6.8 Actors

The **Actors** tab displays an event attribution tree. Events are assigned to actors based on the actor definitions you configure in the **Actor Definitions** panel:

- Click **Discover…** to let the application scan loaded data and suggest actor fields based on cardinality and keyword heuristics.
- Review and import the suggestions, then the Actors panel tree updates automatically.

---

## 7. Side-by-Side Comparison

The Side-by-Side panel loads two log files independently and displays them in a split view, making it easy to compare what happened in two logs at the same point in time.

### 7.1 Opening the Panel

- Click the **Side by Side** tab in the content area, **or**
- Press **Ctrl+Shift+S**

### 7.2 Loading Files

Each side has its own **Open** button and a filename label. Click the button to open a file-picker dialog. Both sides support all log formats (XML, CSV, JSON, ASC, DLT, Evlog). File parsing runs on a background thread; the split view remains interactive while loading.

You can also **drag a file from the file manager** directly onto the Side by Side panel:

- Drop in the **left half** of the panel → loads into the left view
- Drop in the **right half** of the panel → loads into the right view

This is the same drag-and-drop gesture as the main window (which merges a file into the main events table), just targeted to one side of the comparison view.

### 7.3 Synchronisation Modes

Select a mode from the **Sync** dropdown in the toolbar:

#### Timestamp Sync (default)

When you click a row on either side, the application computes the timestamp of that event and scrolls the opposite side to the event with the nearest timestamp. This works best when both logs share a common time base (e.g. two DLT recordings from the same system clock).

#### Manual Sync

Use Manual sync when the two files have different time bases — for example, comparing a DLT log (epoch-based absolute seconds) with an ASC log (elapsed seconds from start of recording).

1. Select **Manual** from the Sync dropdown.
2. Find the same real-world moment in each log (e.g. a power-on event or a known error).
3. Click **Set Left Ref** with the matching event selected on the left side.
4. Click **Set Right Ref** with the matching event selected on the right side.
5. Once both reference points are set, the panel computes `offset = rightRefTimestamp − leftRefTimestamp`. All subsequent sync lookups apply this offset: `targetRight = leftTimestamp + offset`.

The sync status label below the toolbar shows whether reference points have been set and the computed offset value.

#### No Sync

Both views scroll completely independently. Useful when you only need to see two files simultaneously without needing them to stay aligned.

---

## 8. Signal Browser and DBC Decoding

The **Signal Browser** is a left-dock panel (second tab, alongside Filters). It shows the CAN frame and signal tree from a loaded DBC file.

- **Frames** are listed as top-level tree nodes with their CAN ID and name.
- **Signals** appear as children with checkboxes.
- Check a signal to add it to the Signal Plot chart; uncheck to remove it.
- The browser is populated from the DBC structure only — you do not need to load a log file first.

When no DBC is loaded, the panel shows a placeholder message. Load a DBC via **File > Load DBC…** to populate it.

---

## 9. AI-Assisted Analysis

### 9.1 Configuring an AI Provider

Open **Tools > Settings > AI** and configure:

| Setting | Description |
|---------|-------------|
| Provider | ollama, lmstudio, openai, anthropic, google, xai |
| Base URL | API endpoint (pre-filled with defaults; edit for local providers) |
| Model | Model name (e.g. `llama3.2`, `gpt-4o`, `claude-3-5-sonnet`) |
| API Key | Required for cloud providers; stored encrypted on disk |
| Timeout | Request timeout in seconds (30–3600) |

**Local providers (recommended for privacy):**
- **Ollama** — install from [ollama.com](https://ollama.com), run `ollama serve`, pull a model (`ollama pull llama3.2`), and set the URL to `http://localhost:11434`.
- **LM Studio** — download from [lmstudio.ai](https://lmstudio.ai), load a model, start the local server, and point LogViewer at `http://localhost:1234`.

### 9.2 Running an Analysis

1. Open the AI panel in the **bottom dock**.
2. Select an analysis type from the dropdown (Error Summary, Pattern Detection, Root Cause, Custom…) or type a custom prompt.
3. Click **Analyze**.

The analysis runs on a background thread. Results appear in the response area when complete.

**Filter-aware**: AI analysis respects the active filters. Only the events currently visible in the table are sent for analysis. On large datasets, a smart sampler caps at 5 000 events with even distribution across the timeline to stay within the model's context window.

---

## 10. Named Layouts

A layout captures the current dock geometry — which panels are visible, their sizes, and their positions. Layouts let you switch between different working configurations in one click.

**Saving a layout:**
1. Arrange panels to your preference.
2. **View > Layouts > Save Layout…**
3. Enter a name and click Save.

**Restoring a layout:**
- **View > Layouts > \<name\>** — click the layout name in the menu.

**Deleting a layout:**
- **View > Layouts > Delete Layout…** — select the layout to delete.

Two built-in layouts are always available:

| Layout | Optimised for |
|--------|--------------|
| XML / CSV Log | Generic log inspection: events table + filters + details |
| CAN Analysis | Signal Browser, Signal Plot, Statistics, and Timeline visible |

Named layouts are persisted in the application settings and survive restarts.

---

## 11. Export

**File > Export** sub-menu:

| Action | Output |
|--------|--------|
| Export as CSV… | Comma-separated file — all visible (filtered) events |
| Export as JSON… | JSON array of event objects |
| Export as XML… | XML file mirroring the input XML structure |

All export actions apply the current filter, so only visible rows are written. A Save As dialog lets you choose the output path; the last-used directory is remembered per export type.

For quick clipboard transfer without saving a file, use **right-click > Copy as JSON** or **Copy as CSV** in the events table.

---

## 12. Themes

Three colour themes are available under **View > Theme**:

| Theme | Description |
|-------|-------------|
| Light | White background, dark text |
| Dark | Dark background, light text; easier on the eyes in low-light environments |
| System | Follows the OS preference (light/dark mode toggle on macOS and Windows) |

The selected theme is applied instantly and persisted across sessions.

---

## 13. Configuration

Settings are stored per-platform:

| Platform | Location |
|----------|----------|
| macOS | `~/Library/Application Support/LogViewer/config.json` |
| Linux | `~/.config/LogViewer/config.json` |
| Windows | `%APPDATA%\LogViewer\config.json` |

Open the settings editor at **Tools > Settings**. The dialog has tabs:

- **General** — language, startup behaviour, update check interval
- **Columns** — add, remove, reorder, and rename columns; set visibility and default width
- **Colors** — map field values to foreground/background colours (e.g. `level=ERROR` → red background)
- **AI** — provider, model, API key, timeout
- **Plugins** — enable/disable installed plugins

You can also edit `config.json` directly; changes take effect on the next application start.

---

## 14. Keyboard Shortcuts

| Shortcut | Action |
|----------|--------|
| **Ctrl+O** | Open log file |
| **Ctrl+F** | Focus search bar |
| **Enter** (in search bar) | Run search |
| **Ctrl+G** | Jump to timestamp |
| **Ctrl+C** | Copy selected events (plain text) |
| **Ctrl+Shift+S** | Switch to Side by Side tab |
| **Ctrl+Z** | Undo last filter change |
| **Ctrl+S** | Save session |
| **Ctrl+Q** | Quit application |
| **F5** | Refresh / re-apply all filters |

Context-menu actions (right-click on the events table):

| Action | Description |
|--------|-------------|
| Copy as JSON | Selected events → JSON array to clipboard |
| Copy as CSV | Selected events → CSV with header to clipboard |
| Add Bookmark | Annotate the selected event |
| Jump to Event | Scroll to this event (when invoked from search results) |

---

*For build instructions and developer documentation see [ARCHITECTURE.md](ARCHITECTURE.md), [DEVELOPMENT.md](DEVELOPMENT.md), and [SDK_GETTING_STARTED.md](SDK_GETTING_STARTED.md).*
