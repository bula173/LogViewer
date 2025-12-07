# CSV Parser for LogViewer

## Overview

The CSV parser provides support for parsing comma-separated value (CSV) log files in the LogViewer application. It follows the same architecture as the existing XML parser and integrates seamlessly with the parser factory system.

## Features

- **Flexible Header Support**: Automatically detects and uses column headers
- **Quoted Field Handling**: Properly handles fields containing commas, quotes, and newlines
- **Custom Delimiters**: Support for comma, semicolon, tab, or any custom delimiter
- **Field Mapping**: Intelligent mapping of common field names to standard log event properties
- **Progress Tracking**: Reports parsing progress for large files
- **Batch Processing**: Processes events in batches for improved performance
- **Error Handling**: Gracefully handles malformed lines while continuing to parse

## Usage

### Automatic Parser Selection

The parser factory automatically selects the CSV parser for `.csv` files:

```cpp
#include "parser/ParserFactory.hpp"

// Create parser based on file extension
auto result = ParserFactory::CreateFromFile("logs.csv");
if (result.isOk()) {
    auto parser = result.unwrap();
    parser->RegisterObserver(observer);
    parser->ParseData("logs.csv");
}
```

### Direct Instantiation

You can also create a CSV parser directly:

```cpp
#include "csv/CsvParser.hpp"

CsvParser parser;
parser.SetDelimiter(',');  // Default is comma
parser.SetHasHeaders(true); // Default is true
parser.RegisterObserver(observer);
parser.ParseData("logs.csv");
```

### Custom Delimiter

For semicolon-separated values or tab-separated values:

```cpp
CsvParser parser;
parser.SetDelimiter(';');  // Semicolon
// or
parser.SetDelimiter('\t'); // Tab
parser.ParseData("logs.tsv");
```

## CSV Format

### Expected Format

The parser expects CSV files with the following structure:

```csv
id,timestamp,level,info,source,thread
1,2025-12-07 10:30:15,INFO,Application started,Main,1
2,2025-12-07 10:30:16,DEBUG,Loading configuration,Config,1
3,2025-12-07 10:30:17,ERROR,Failed to connect,Network,2
```

### Supported Field Names

The parser recognizes and maps the following field names (case-insensitive):

- **id**: Event identifier
- **timestamp**, **time**, **date**: Event timestamp
- **level**, **severity**, **priority**: Log level
- **info**, **message**, **msg**, **description**, **text**: Log message
- **source**, **logger**, **component**: Event source/component
- **thread**, **threadid**: Thread identifier
- **file**, **filename**: Source file name
- **line**, **lineno**: Source line number
- **function**, **func**: Function name

Additional fields are preserved as custom event properties.

### Quoted Fields

Fields containing special characters should be quoted:

```csv
id,timestamp,level,info
1,2025-12-07 10:00:00,INFO,"Message with, comma"
2,2025-12-07 10:00:01,DEBUG,"Message with ""quotes"""
```

## Implementation Details

### Architecture

- **CsvParser.hpp**: Header file with class definition
- **CsvParser.cpp**: Implementation with parsing logic
- **Integration**: Registered in `ParserFactory` for automatic selection

### Performance

- Uses batch processing (100 events per batch) for efficiency
- Minimal string allocations through move semantics
- Progress updates every batch for responsive UI

### Error Handling

- Malformed lines are logged and skipped
- Empty lines are automatically ignored
- Continues parsing even if individual lines fail

## Testing

Unit tests are available in `tests/CsvParserTest.cpp`:

```bash
# Run all tests
ctest --preset macos-debug-test

# Run CSV parser tests specifically
./tests/bin/LogViewer_tests --gtest_filter="CsvParserTest.*"
```

## Sample Data

A sample CSV log file is provided in `test_data/sample_logs.csv` for testing and demonstration purposes.

## Future Enhancements

Potential improvements for future versions:

- **Auto-detection**: Automatically detect delimiter and header presence
- **Type Inference**: Infer data types for numeric and boolean fields
- **Multi-line Fields**: Support for fields spanning multiple lines
- **Encoding Detection**: Automatic character encoding detection
- **Streaming**: Support for streaming large files without loading entirely in memory

## Files

- `src/application/csv/CsvParser.hpp` - Header file
- `src/application/csv/CsvParser.cpp` - Implementation
- `tests/CsvParserTest.cpp` - Unit tests
- `test_data/sample_logs.csv` - Sample data file

## Dependencies

The CSV parser uses only standard C++ libraries and depends on:

- `IDataParser` interface
- `LogEvent` class for event representation
- `Logger` utility for diagnostic output
- `Error` class for error handling
