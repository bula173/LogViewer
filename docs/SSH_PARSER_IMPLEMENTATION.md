# SSH Parser Implementation Summary

## Overview

Successfully implemented an optional SSH parser library for LogViewer that enables:
- SSH connectivity to remote test computers
- Real-time text log parsing from SSH terminal output
- Modular architecture as a separate library (customer-specific feature)

## Implementation Details

### 1. Library Structure

Created new library module: `libs/ssh_parser/`

**Files Created:**
- `CMakeLists.txt` - Build configuration with libssh dependency
- `SSHConnection.hpp/.cpp` - SSH connection management
- `SSHTextParser.hpp/.cpp` - Text log parsing with regex patterns
- `SSHLogSource.hpp/.cpp` - Integrated log source combining connection and parsing
- `README.md` - Complete documentation

### 2. Build System Integration

**Modified Files:**
- `CMakeLists.txt` - Added `ENABLE_SSH_PARSER` option
- `libs/CMakeLists.txt` - Conditional inclusion of ssh_parser subdirectory

**Build Option:**
```bash
cmake -DENABLE_SSH_PARSER=ON -S. -Bbuild
```

### 3. Key Features

#### SSHConnection Class
- **Authentication Methods:**
  - Password authentication
  - SSH public key authentication
  - SSH agent authentication
  - Support for encrypted keys with passphrases
  
- **Operations:**
  - Execute remote commands
  - Open interactive shell
  - Read/write shell I/O
  - Auto-reconnect support
  
- **Security:**
  - Host key verification
  - Configurable timeouts
  - Secure credential handling

#### SSHTextParser Class
- **Pattern Matching:**
  - Regex-based pattern recognition
  - Multiple pattern support
  - Custom pattern definition
  - Built-in common formats (syslog, timestamped, etc.)
  
- **Features:**
  - Real-time parsing
  - Multi-line log support
  - Field extraction
  - Progress tracking
  
- **Integration:**
  - Implements `IDataParser` interface
  - Observer pattern for event notification
  - Compatible with existing LogViewer architecture

#### SSHLogSource Class
- **Monitoring Modes:**
  - **CommandOutput**: Execute command and parse output
  - **TailFile**: Monitor remote log file (`tail -f`)
  - **InteractiveShell**: Parse all shell output
  
- **Features:**
  - Background monitoring thread
  - Line buffering and accumulation
  - Event batching for performance
  - Auto-reconnect on connection loss

### 4. Dependencies

**External:**
- libssh - SSH protocol implementation
  - macOS: `brew install libssh`
  - Linux: `apt-get install libssh-dev` or `dnf install libssh-devel`

**Internal:**
- `application_core` - Core LogViewer functionality
- `spdlog` - Logging
- Standard C++20 library

### 5. Architecture

```
┌─────────────────────────────────────┐
│          Application UI             │
│     (MainWindow, Controllers)       │
└──────────────┬──────────────────────┘
               │ Observer Pattern
               ↓
┌─────────────────────────────────────┐
│         SSHLogSource                │
│  ┌────────────────────────────────┐ │
│  │   Monitoring Thread            │ │
│  │   - Data acquisition           │ │
│  │   - Line buffering             │ │
│  │   - Event batching             │ │
│  └────────────────────────────────┘ │
└──────────────┬──────────────────────┘
               │
      ┌────────┴─────────┐
      ↓                  ↓
┌─────────────┐    ┌──────────────┐
│SSHConnection│    │ SSHTextParser│
│  - Auth     │    │  - Patterns  │
│  - Session  │    │  - Regex     │
│  - Channel  │    │  - Parsing   │
└─────────────┘    └──────────────┘
      │
      ↓
┌─────────────┐
│   libssh    │
│  (External) │
└─────────────┘
```

### 6. Usage Examples

#### Basic Connection
```cpp
ssh::SSHConnectionConfig config;
config.hostname = "192.168.1.100";
config.username = "testuser";
config.password = "password";
config.authMethod = ssh::SSHAuthMethod::Password;

ssh::SSHConnection conn(config);
if (conn.Connect()) {
    std::string output;
    conn.ExecuteCommand("tail -n 100 /var/log/app.log", output);
    conn.Disconnect();
}
```

#### Real-Time Monitoring
```cpp
ssh::SSHLogSourceConfig config;
config.connectionConfig.hostname = "test-server";
config.connectionConfig.username = "user";
config.mode = ssh::SSHLogSourceMode::TailFile;
config.logFilePath = "/var/log/application.log";

ssh::SSHLogSource source(config);
source.RegisterObserver(observer);
source.Start();  // Runs in background
// ... monitoring continues ...
source.Stop();
```

### 7. Security Considerations

- Credentials are stored in configuration objects (not persisted by default)
- Support for key-based authentication (more secure than passwords)
- Host key verification enabled by default
- Configurable connection timeouts
- SSH protocol encryption for all data transmission
- Logging of all SSH operations for audit trails

### 8. Testing

**Unit Testing:**
- SSH connection establishment
- Authentication methods
- Command execution
- Pattern matching
- Event generation

**Integration Testing:**
- Real SSH server connectivity
- Log file monitoring
- Multi-line parsing
- Auto-reconnect functionality

### 9. Documentation

**Created:**
- `libs/ssh_parser/README.md` - Complete user guide
  - Features and capabilities
  - Build instructions
  - API reference
  - Usage examples
  - Troubleshooting
  - Security guidelines

### 10. File Dialog Updates

**Updated:**
- Qt UI: Already includes CSV support
- wxWidgets UI: Added CSV to file filters

**File Filter:**
```
Log files (*.xml *.csv *.log *.txt)|*.xml;*.csv;*.log;*.txt
```

## Integration Points

### With Existing Parser System
- Implements `IDataParser` interface
- Uses same observer notification pattern
- Compatible with `EventsContainer`
- Integrates with UI progress indicators

### With LogViewer Core
- Uses `util::Logger` for diagnostics
- Uses `db::LogEvent` for event representation
- Uses `error::Error` for error handling
- Compatible with filtering system

## Deployment

### Customer-Specific Builds
1. **Without SSH Parser** (default):
   ```bash
   cmake -S. -Bbuild
   cmake --build build
   ```

2. **With SSH Parser**:
   ```bash
   cmake -DENABLE_SSH_PARSER=ON -S. -Bbuild
   cmake --build build
   ```

### Distribution
- SSH parser code resides in separate library
- Can be excluded from builds without SSH requirements
- Licensing can be controlled at build configuration level
- No runtime dependencies unless feature is enabled

## Performance Considerations

- Background thread prevents UI blocking
- Event batching reduces notification overhead (10 events/batch)
- Configurable polling interval (default: 100ms)
- Line buffering for efficient processing
- Regex compilation cached for performance

## Limitations

- Requires external libssh library
- Network latency affects real-time performance
- Text parsing only (binary logs not supported)
- Single connection per SSHLogSource instance
- SSH protocol overhead for each connection

## Future Enhancements

Possible improvements for future versions:
- Multiple simultaneous connections
- Connection pooling
- Advanced pattern editor UI
- Pattern library/marketplace
- Binary log support
- Compression support
- Connection statistics dashboard
- SSH tunnel support
- SFTP integration for file browsing

## Build Verification

To verify the implementation:

```bash
# 1. Install libssh
brew install libssh  # macOS

# 2. Configure with SSH parser enabled
cmake --preset macos-debug -DENABLE_SSH_PARSER=ON

# 3. Build
cmake --build --preset macos-debug-build

# 4. Verify
grep -r "SSH Parser: ENABLED" build/
```

## Summary

Successfully implemented a complete, production-ready SSH parser library that:
- ✅ Provides SSH connectivity to remote test computers
- ✅ Parses text logs from SSH terminal output
- ✅ Implemented as separate optional library
- ✅ Fully documented with examples
- ✅ Secure and performant
- ✅ Integrates seamlessly with LogViewer
- ✅ Ready for customer-specific deployments
