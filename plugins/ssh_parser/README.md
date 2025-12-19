# SSH Parser Library

## Overview

The SSH Parser is an optional library module for LogViewer that provides SSH connectivity and real-time text log parsing from remote test computers. This feature is available only for specific customers with licensing agreements.

## Features

- **SSH Connection Management**
  - Password, public key, and SSH agent authentication
  - Auto-reconnect on connection loss
  - Secure connection with host key verification

- **Real-Time Log Monitoring**
  - Execute remote commands and parse output
  - Tail remote log files (`tail -f`)
  - Parse interactive shell output

- **Text Log Parsing**
  - Multiple regex pattern support
  - Common log format recognition (syslog, timestamped, etc.)
  - Custom pattern definition
  - Multi-line log entry support

- **Integration**
  - Observer pattern for event notification
  - Compatible with existing LogViewer architecture
  - Seamless integration with UI components

## Building

The SSH parser is **disabled by default** and must be explicitly enabled during build configuration.

### Prerequisites

Install libssh development library:

**macOS:**
```bash
brew install libssh
```

**Linux (Ubuntu/Debian):**
```bash
sudo apt-get install libssh-dev
```

**Linux (Fedora/RHEL):**
```bash
sudo dnf install libssh-devel
```

### Build Configuration

Enable SSH parser during CMake configuration:

```bash
cmake -DENABLE_SSH_PARSER=ON -S. -Bbuild
cmake --build build
```

Or using presets:
```bash
cmake --preset macos-debug -DENABLE_SSH_PARSER=ON
cmake --build --preset macos-debug-build
```

## Usage

### Basic SSH Connection

```cpp
#include "ssh_parser/SSHConnection.hpp"

ssh::SSHConnectionConfig config;
config.hostname = "192.168.1.100";
config.port = 22;
config.username = "testuser";
config.password = "password";
config.authMethod = ssh::SSHAuthMethod::Password;

ssh::SSHConnection connection(config);
if (connection.Connect()) {
    std::string output;
    connection.ExecuteCommand("cat /var/log/app.log", output);
    // Process output...
    connection.Disconnect();
}
```

### Real-Time Log Monitoring

```cpp
#include "ssh_parser/SSHLogSource.hpp"

ssh::SSHLogSourceConfig config;
config.connectionConfig.hostname = "192.168.1.100";
config.connectionConfig.username = "testuser";
config.connectionConfig.password = "password";
config.connectionConfig.authMethod = ssh::SSHAuthMethod::Password;
config.mode = ssh::SSHLogSourceMode::TailFile;
config.logFilePath = "/var/log/application.log";

ssh::SSHLogSource logSource(config);

// Register observer to receive parsed events
logSource.RegisterObserver(myObserver);

// Start monitoring
if (logSource.Start()) {
    // Monitoring runs in background thread
    // Events are delivered to observer as they arrive
    
    // Stop when done
    logSource.Stop();
}
```

### Custom Log Patterns

```cpp
ssh::SSHTextParser parser;

// Add custom pattern for application-specific logs
ssh::TextLogPattern customPattern;
customPattern.name = "app_log";
customPattern.pattern = std::regex(
    R"(^\[(\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2})\] \[(\w+)\] \[(\w+)\] (.*)$)");
customPattern.captureGroups = {"timestamp", "level", "component", "info"};

parser.AddPattern(customPattern);
```

### SSH Key Authentication

```cpp
ssh::SSHConnectionConfig config;
config.hostname = "testserver.example.com";
config.username = "testuser";
config.privateKeyPath = "/home/user/.ssh/id_rsa";
config.publicKeyPath = "/home/user/.ssh/id_rsa.pub";
config.passphrase = "key_passphrase";  // If key is encrypted
config.authMethod = ssh::SSHAuthMethod::PublicKey;

ssh::SSHConnection connection(config);
connection.Connect();
```

## Architecture

### Class Hierarchy

```
ssh::SSHConnection
├── Authentication (Password, PublicKey, Agent)
├── Session Management
└── Channel Operations (Execute, Shell)

ssh::SSHTextParser : public parser::IDataParser
├── Pattern Matching (Regex)
├── Field Extraction
└── Event Generation

ssh::SSHLogSource
├── SSHConnection (Connection management)
├── SSHTextParser (Text parsing)
└── Monitoring Thread (Real-time processing)
```

### Data Flow

```
Remote Host → SSH Connection → Shell/Command Output
    ↓
Line Buffer → Text Parser → Pattern Matching
    ↓
Event Items → Observer Notification → LogViewer UI
```

## Configuration

### Connection Modes

1. **CommandOutput**: Execute a command and parse its output
   ```cpp
   config.mode = ssh::SSHLogSourceMode::CommandOutput;
   config.command = "journalctl -f";
   ```

2. **TailFile**: Monitor a remote log file in real-time
   ```cpp
   config.mode = ssh::SSHLogSourceMode::TailFile;
   config.logFilePath = "/var/log/syslog";
   ```

3. **InteractiveShell**: Parse all output from an interactive shell
   ```cpp
   config.mode = ssh::SSHLogSourceMode::InteractiveShell;
   ```

### Default Log Patterns

The parser includes built-in patterns for:

- **Syslog Format**: `Jan 1 10:30:15 hostname program[pid]: message`
- **Timestamped**: `[2025-12-07 10:30:15] [LEVEL] message`
- **Simple**: `2025-12-07 10:30:15 message`
- **Level-Prefixed**: `ERROR: message` or `INFO: message`

## Security Considerations

- Store credentials securely (use key-based auth when possible)
- Enable strict host key checking for production
- Use encrypted keys with passphrases
- Limit SSH user permissions on remote hosts
- Consider using SSH agent for key management
- Implement connection timeouts
- Log all SSH operations for audit trails

## Limitations

- Requires libssh library (external dependency)
- Network latency affects real-time performance
- Large log volumes may impact performance
- SSH protocol overhead for each connection
- Limited to text-based log formats

## Licensing

This module is available only for customers with appropriate licensing agreements. Contact sales for information about enabling SSH parser capabilities.

## Troubleshooting

### libssh not found

```
Error: libssh not found. SSH parser will not be built.
```

**Solution**: Install libssh development package (see Prerequisites).

### Connection timeouts

Increase timeout in configuration:
```cpp
config.connectionConfig.timeoutSeconds = 60;
```

### Authentication failures

- Verify credentials
- Check SSH server configuration
- Ensure user has appropriate permissions
- For key auth, verify key file paths and permissions

### Pattern not matching

Enable debug logging to see pattern matching attempts:
```cpp
util::Logger::SetLevel(util::LogLevel::Debug);
```

## API Reference

See header files for detailed API documentation:
- `SSHConnection.hpp` - SSH connection management
- `SSHTextParser.hpp` - Text log parsing
- `SSHLogSource.hpp` - Integrated log source

## Examples

Complete examples are available in:
- `docs/examples/ssh_basic_connection.cpp`
- `docs/examples/ssh_log_monitoring.cpp`
- `docs/examples/ssh_custom_patterns.cpp`

## Support

For technical support or licensing inquiries:
- Email: support@logviewer.example.com
- Documentation: https://docs.logviewer.example.com/ssh-parser
