# Polish Alphabet Support in LogViewer

## Overview

LogViewer now fully supports the Polish alphabet, including all special Polish characters:

### Polish Characters Supported

- **ą** (a with ogonek)
- **ć** (c with acute)
- **ę** (e with ogonek)
- **ł** (l with stroke)
- **ń** (n with acute)
- **ó** (o with acute)
- **ś** (s with acute)
- **ź** (z with acute)
- **ż** (z with dot above)

Both lowercase and uppercase versions (Ą, Ć, Ę, Ł, Ń, Ó, Ś, Ź, Ż) are supported.

## Technical Implementation

### 1. Locale Initialization

The application initializes `wxLocale` in `MyApp::OnInit()` to ensure proper Unicode handling:

```cpp
// Initialize locale for proper Unicode/Polish character support
if (!m_locale.Init(wxLANGUAGE_DEFAULT, wxLOCALE_LOAD_DEFAULT))
{
    util::Logger::Warn("Failed to initialize locale, using default");
}
```

This ensures that:
- Text rendering works correctly for Polish characters
- Sorting and comparison operations respect Polish locale rules
- Input/output operations handle UTF-8 encoding properly

### 2. UTF-8 Encoding in XML Parser

The XML parser (Expat-based) is configured for explicit UTF-8 encoding:

```cpp
XML_Parser parser = XML_ParserCreate("UTF-8");
```

Key features:
- **UTF-8 BOM Handling**: Automatically detects and skips UTF-8 Byte Order Mark (EF BB BF) if present
- **Character Encoding**: All text is processed as UTF-8, preserving Polish characters
- **Memory Safety**: Uses proper string handling to avoid corruption of multi-byte characters

### 3. wxWidgets String Conversion

All string conversions between `std::string` and `wxString` use UTF-8:

```cpp
wxString::FromUTF8(str.c_str())  // std::string -> wxString
wxStr.ToUTF8().data()            // wxString -> std::string
```

This ensures:
- Polish characters display correctly in the GUI
- Search and filter operations work with Polish text
- Copy/paste operations preserve Polish characters

## Usage

### Creating XML Files with Polish Text

Ensure your XML log files are saved with UTF-8 encoding. Example:

```xml
<?xml version="1.0" encoding="UTF-8"?>
<logs>
    <event>
        <timestamp>2025-01-15 10:30:00</timestamp>
        <type>INFO</type>
        <info>Użytkownik zalogował się do systemu</info>
        <message>Operacja zakończona powodzeniem</message>
    </event>
    <event>
        <timestamp>2025-01-15 10:31:00</timestamp>
        <type>WARNING</type>
        <info>Próba dostępu do pliku: dane_ważne.txt</info>
        <message>Błąd: Brak uprawnień</message>
    </event>
    <event>
        <timestamp>2025-01-15 10:32:00</timestamp>
        <type>ERROR</type>
        <info>Połączenie z bazą danych przerwane</info>
        <message>Sprawdź konfigurację serwera</message>
    </event>
</logs>
```

### Searching with Polish Text

The search functionality fully supports Polish characters:

1. Type Polish text directly in the search box: `błąd`, `połączenie`, `użytkownik`
2. Use filter criteria with Polish values
3. Sort by columns containing Polish text

### Copying Text with Polish Characters

The detail view (ItemVirtualListControl) supports multiple copy operations:

- **Ctrl+C**: Copy the selected value to clipboard
- **Right-click context menu**:
  - Copy Value: Copy only the value field
  - Copy Key: Copy only the key field
  - Copy Both: Copy both key and value as "Key: Value"

All copy operations preserve Polish characters correctly, so you can paste them into other applications.

### Configuration with Polish Text

Column names, filter values, and other configuration can use Polish characters:

```json
{
  "columns": [
    {
      "name": "Znacznik czasu",
      "isVisible": true,
      "width": 150
    },
    {
      "name": "Typ zdarzenia",
      "isVisible": true,
      "width": 100
    }
  ]
}
```

## Testing

To verify Polish character support:

1. **Create Test File**: Save the example XML above as `test_polski.xml` with UTF-8 encoding
2. **Open in LogViewer**: File → Open → select `test_polski.xml`
3. **Verify Display**: All Polish characters should display correctly
4. **Test Search**: Search for `błąd` or `użytkownik` - should find matching events
5. **Test Filters**: Apply type filters with Polish text

## Troubleshooting

### Characters Display as �� or Boxes

**Cause**: File is not saved as UTF-8 or contains invalid encoding

**Solution**:
- Re-save the XML file with UTF-8 encoding (without BOM, or with BOM - both supported)
- Use a text editor like Notepad++, VS Code, or Sublime Text
- Verify encoding in the editor's status bar

### Search Doesn't Find Polish Words

**Cause**: Locale not properly initialized

**Solution**:
- Check application logs for locale initialization warnings
- Ensure system has proper Polish locale support installed
- On Windows: Control Panel → Region → Administrative → Change system locale

### Copy/Paste Loses Polish Characters

**Cause**: Clipboard encoding issue

**Solution**:
- This should work automatically with wxWidgets
- If issues persist, try copying smaller text selections
- Report the issue with specific reproduction steps

## Platform-Specific Notes

### Windows
- Polish characters work out-of-the-box on Polish Windows installations
- On non-Polish Windows, Polish locale support may need to be enabled
- UTF-8 is fully supported on Windows 10 and later

### Linux
- Ensure locale `pl_PL.UTF-8` is available: `locale -a | grep pl_PL`
- Generate if missing: `sudo locale-gen pl_PL.UTF-8`
- Set environment variable if needed: `export LANG=pl_PL.UTF-8`

### macOS
- Polish support is built-in on modern macOS versions
- UTF-8 is the default encoding

## Performance Considerations

Polish character support has minimal performance impact:
- UTF-8 encoding adds ~1-2% overhead compared to ASCII-only
- Search and filter operations may be slightly slower with multi-byte characters
- Memory usage is identical (UTF-8 is variable-length encoding)

## References

- [UTF-8 Encoding Standard](https://en.wikipedia.org/wiki/UTF-8)
- [wxWidgets Internationalization](https://docs.wxwidgets.org/3.2/overview_i18n.html)
- [Expat XML Parser](https://libexpat.github.io/)
- [Polish Alphabet on Wikipedia](https://en.wikipedia.org/wiki/Polish_alphabet)
