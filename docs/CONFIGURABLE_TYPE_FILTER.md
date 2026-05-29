# Type Filter Field — Auto-Detection and Override

## Overview

LogViewer automatically detects which field in your log events represents the event type or severity level. No manual configuration is required for common log formats. You can still pin a specific field name if your logs use a non-standard naming convention.

## Auto-Detection

When a file is loaded, `UpdateTypeFilters()` scans the first 100 events for these field names (in priority order):

```
level → type → severity → priority → category →
loglevel → log_level → logLevel → eventtype → event_type
```

The first field that is non-empty in any of the sampled events becomes the active type filter field for that session. The detected value is held in memory but **not written to config**, so auto-detection runs fresh on each file load.

## Fallback: User Prompt

If none of the candidate field names match any event in the first 100 rows, a modal dialog asks the user to enter the field name manually. The provided value is:

- Immediately applied to the current session.
- **Saved to `config.json`** so it is used automatically for future loads without prompting again.

## Manual Override (Settings)

Open **Tools > Settings > General** and set the **Type Filter Field**:

- **Non-empty value** — auto-detection is skipped entirely; the given field is always used.
- **Empty value** — auto-detection runs on every file load (default behaviour).

Saving an explicit value persists it to `config.json`:

```json
{
  "filters": {
    "typeFilterField": "severity"
  }
}
```

Clearing the field and saving restores auto-detection.

## Row Colouring

Row colours are driven by `columnColors` in the configuration, **not** limited to the type filter field. Any column can have colour rules; the first column (alphabetically by key) whose value matches an event is applied:

```json
{
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

Configure colours in **Tools > Settings > Colors**.

## XML Structure Auto-Discovery

The XML parser also requires no configuration. It discovers structure by element depth:

| Depth | Role |
|-------|------|
| 1 | Root element (any tag name, e.g. `<events>`, `<log>`) |
| 2 | Event record (any tag name, e.g. `<event>`, `<record>`) |
| 3+ | Field — tag name becomes the key, text content the value |

XML attributes on depth-2 elements are also captured as key/value pairs. No `rootElement` or `eventElement` configuration keys are needed or read.
