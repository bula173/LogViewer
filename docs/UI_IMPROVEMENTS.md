# UI Improvements and Layout Features

## Overview

LogViewer now features a flexible, dock-based UI layout that allows users to customize their workspace according to their preferences.

## Dock Widget System

### Available Panels

The application uses three dockable panels that can be freely arranged:

1. **Filters Panel** (Default: Left)
   - Extended Filters tab: Complex filtering with multiple conditions
   - Type Filters tab: Quick filtering by event type/level

2. **Item Details Panel** (Default: Right)
   - Shows detailed information for the selected event
   - Displays all fields in a structured format

3. **Tools Panel** (Default: Bottom)
   - Search tab: Full-text search across all events
   - AI Chat tab: Interactive conversation with AI about your logs

### Panel Features

Each dock panel supports:

- **Minimizable**: Click the close button to hide the panel
- **Movable**: Drag panels to different edges (left, right, top, bottom)
- **Floatable**: Drag away from the main window to create a floating window
- **Resizable**: Drag panel edges to adjust size

### View Menu

Access panel controls through **View** menu:

- Toggle visibility of each panel (Filters, Item Details, Tools)
- **Reset Layout**: Restore default panel positions

## Main Content Area

The central area uses tabs for primary content:

1. **Events Tab**
   - Table view of all log events
   - Column features:
     - **Movable columns**: Drag column headers to reorder
     - **Resizable columns**: Drag column separators
     - **Last column stretches**: Automatically fills remaining space
   - Multi-select support (Ctrl/Cmd + click)
   - Copy to clipboard (Ctrl/Cmd + C)

2. **AI Analysis Tab**
   - Predefined analysis types (Summary, Error Analysis, Pattern Detection, etc.)
   - Custom prompt support
   - Save/Load prompts (use non-native dialogs for better cross-platform support)
   - Filter-aware: Respects active filters when analyzing

## Configuration Dialog

Enhanced structured configuration accessible via **Edit → Config**:

### General Tab
- XML parser settings (root element, event element)
- **Type Filter Field**: Configure which field to use for filtering/coloring
- Log level selection

### Columns Tab
- View/edit column configuration
- Show/hide columns
- Set column widths
- Reorder columns (Move Up/Down buttons)

### Colors Tab
- **Dynamic column selection**: Dropdown populated with available columns
- Add color mappings for specific field values
- Background and foreground color pickers
- Live preview of color combinations
- Default to `typeFilterField` for consistency

### AI Tab
- Provider selection (Ollama, LM Studio, OpenAI, Anthropic, Google, xAI)
- API Key configuration (for cloud providers)
- Base URL and default model
- **Request Timeout**: Configurable timeout (30-3600 seconds) for slow machines

## File Dialogs

All file dialogs now use **non-native dialogs** (`QFileDialog::DontUseNativeDialog`) for consistent behavior across platforms, especially on macOS where native dialogs had path resolution issues.

Affected dialogs:
- Open Log File
- Load Prompt
- Save Prompt
- Load/Save Filters

All dialogs now properly use `QCoreApplication::applicationDirPath()` to resolve paths relative to the application bundle.

## Keyboard Shortcuts

| Action | Shortcut |
|--------|----------|
| Open File | Ctrl/Cmd + O |
| Clear Data | Ctrl/Cmd + Shift + L |
| Copy Selection | Ctrl/Cmd + C |
| Search | Enter (in search field) |
| Quit | Ctrl/Cmd + Q |

## Filter-Aware AI Analysis

When using AI Analysis:

1. **No filters active**: AI analyzes all events (up to max events limit with intelligent sampling)
2. **Filters active**: AI analyzes only the filtered results
   - Example: Filter to show only ERROR events → AI analyzes only errors
   - Respects type filters, extended filters, and search results

This allows focused analysis on specific subsets of your logs.

## Smart Event Sampling

For large log files, the AI analysis uses intelligent sampling:

- Default cap: 5,000 events
- Sampling is evenly distributed across the timeline
- Preserves representative sample of the entire log session
- Users can adjust max events in the AI Analysis panel

## Example Workflows

### Workflow 1: Error-Focused Analysis
1. Open log file
2. Go to Filters → Type Filters tab
3. Check only "ERROR"
4. Click "Apply Filter"
5. Switch to AI Analysis tab
6. Select "Error Analysis"
7. Click "Analyze Logs"
8. AI analyzes only the filtered ERROR events

### Workflow 2: Custom Layout for Large Monitors
1. Drag Filters panel to the left
2. Drag Item Details to the right
3. Drag Tools panel to bottom or make it float
4. Resize panels as needed
5. Layout is preserved between sessions

### Workflow 3: Multi-Column Analysis
1. View → Reset Layout (if needed)
2. Open log file
3. Edit → Config → Columns tab
4. Add/remove columns, adjust widths
5. Click Save
6. Back in Events tab, drag column headers to reorder
7. Columns configuration saved for next session

## Configuration Files

### Location

**macOS:** `~/Library/Application Support/LogViewer/config.json`
**Linux:** `~/.config/LogViewer/config.json`
**Windows:** `%APPDATA%\LogViewer\config.json`

### Structure

```json
{
  "logging": { "level": "debug" },
  "filters": { "typeFilterField": "level" },
  "aiConfig": {
    "provider": "ollama",
    "baseUrl": "http://localhost:11434",
    "defaultModel": "qwen2.5-coder:7b",
    "timeoutSeconds": 300
  },
  "parsers": {
    "xml": {
      "rootElement": "events",
      "eventElement": "event",
      "columns": [
        { "name": "id", "visible": true, "width": 50 },
        { "name": "timestamp", "visible": true, "width": 150 },
        { "name": "level", "visible": true, "width": 200 },
        { "name": "info", "visible": true, "width": 300 }
      ]
    }
  },
  "columnColors": {
    "level": {
      "ERROR": ["#ffffff", "#ff4200"],
      "WARN": ["#000000", "#ffcc00"],
      "INFO": ["#000000", "#90ee90"],
      "DEBUG": ["#000000", "#d3d3d3"]
    }
  }
}
```

## Tips and Best Practices

1. **Use Type Filter Field**: Set this to match your log format ("type", "level", "severity", etc.)
2. **Match Color Column**: Ensure `columnColors` uses the same key as `typeFilterField`
3. **Adjust Timeout**: For slow machines or complex AI queries, increase timeout to 600+ seconds
4. **Save Prompts**: Create and save custom analysis prompts for repeated use
5. **Filter Before Analysis**: Apply filters to focus AI on specific event types
6. **Organize Panels**: Arrange docks based on your workflow (e.g., float AI Chat for side-by-side viewing)
7. **Column Order**: Drag columns to put most important information first

## Future Enhancements

Planned improvements:
- Save/load custom layouts
- Keyboard shortcuts for panel toggling
- Column auto-sizing based on content
- Custom color themes
- Workspace presets for different log formats
