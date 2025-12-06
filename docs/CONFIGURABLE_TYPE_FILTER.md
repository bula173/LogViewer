# Configurable Type Filter Field

## Overview
The type filter now uses a configurable field name instead of hardcoded "type", allowing LogViewer to work with different log formats that use different field names for categorization.

## Changes

### Configuration (Config.hpp/cpp)
- Added `typeFilterField` member variable with default value `"type"`
- Added `GetFilterConfig()` method to load the filter configuration from JSON
- `SaveConfig()` now writes: `j["filters"]["typeFilterField"] = typeFilterField;`
- `LoadConfig()` now reads the `filters.typeFilterField` setting from JSON

### Filter Logic (MainWindowPresenter.cpp)
- `UpdateTypeFilters()`: Now uses `config.typeFilterField` instead of hardcoded `"type"`
- `ApplySelectedTypeFilters()`: Now uses `config.typeFilterField` instead of hardcoded `"type"`

### Color Logic
- **EventsTableModel.cpp**: Foreground and background colors now use `config.typeFilterField`
- **EventsContainerAdapter.cpp** (wxWidgets): Row coloring now uses `config.typeFilterField`
- **AIChatPanel.cpp**: Event formatting now uses `config.typeFilterField`
- **columnColors configuration**: The key in `columnColors` should match your `typeFilterField` value

### Default Configuration (etc/default_config.json)
- Added `"filters"` section with `"typeFilterField": "level"`
- This matches the example log structure which uses "level" for event types

### UI Configuration Dialog
- Added to Edit → Config → General tab
- Field: "Type Filter Field" with placeholder text showing examples
- Users can change without editing JSON manually

### Color Configuration Dialog
- Edit → Config → Colors tab
- Column dropdown now auto-populates with available columns
- Defaults to the `typeFilterField` value for consistency
- Users can add color mappings for different field values

## Configuration Example

```json
{
  "filters": {
    "typeFilterField": "level"
  },
  "columnColors": {
    "level": {
      "ERROR": ["#ffffff", "#ff4200"],
      "WARN":  ["#000000", "#ffcc00"],
      "INFO":  ["#000000", "#90ee90"],
      "DEBUG": ["#000000", "#d3d3d3"]
    }
  }
}
```

**Important**: The key in `columnColors` (e.g., `"level"`) must match the value of `typeFilterField`. This ensures that filtering and coloring use the same field consistently.

## Supported Values
The `typeFilterField` can be set to any attribute/field name in your log events:
- `"type"` - for logs that use `<event type="ERROR">`
- `"level"` - for logs that use `<event level="ERROR">`
- `"severity"` - for logs that use `<event severity="ERROR">`
- `"priority"` - for logs that use `<event priority="ERROR">`
- `"category"` - for logs that use `<event category="ERROR">`

## Benefits
1. **Flexibility**: Works with various log formats without code changes
2. **Consistency**: All filter operations use the same field
3. **Configurability**: Users can adapt LogViewer to their log structure
4. **Default behavior**: Defaults to "type" for backward compatibility

## Testing
To test with different field names:
1. Edit `config.json` (or `etc/default_config.json`)
2. Set `"filters": { "typeFilterField": "level" }` (or other field name)
3. Load a log file that uses that field for categorization
4. The type filter should now show values from that field

## Future Enhancements
- UI configuration dialog to set `typeFilterField` without manual JSON editing
- Auto-detection of common field names on log load
- Multiple filter fields (type + severity + module, etc.)
