# GitHub Workflow Testing Scripts

This directory contains scripts to test GitHub workflows locally using the `act` tool.

## Prerequisites

### Install act
- **Windows**: `winget install nektos.act`
- **Linux/macOS**: `curl https://raw.githubusercontent.com/nektos/act/master/install.sh | sudo bash`
- **Alternative**: Download from [GitHub releases](https://github.com/nektos/act/releases)

### Optional: Docker
- For full workflow execution (without Docker, scripts run in dry-run mode)
- Install Docker Desktop or Docker CE

## Available Scripts

### 1. test-workflows.ps1 (PowerShell)
Advanced PowerShell script with comprehensive options and colored output.

**Usage:**
```powershell
# Test all workflows
.\scripts\test-workflows.ps1

# Test specific platform
.\scripts\test-workflows.ps1 -Target windows
.\scripts\test-workflows.ps1 -Target linux
.\scripts\test-workflows.ps1 -Target macos

# Test specific framework
.\scripts\test-workflows.ps1 -Target wx
.\scripts\test-workflows.ps1 -Target qt

# Release build testing
.\scripts\test-workflows.ps1 -Target all -BuildType Release

# Dry-run mode (syntax validation only)
.\scripts\test-workflows.ps1 -DryRun

# Verbose output
.\scripts\test-workflows.ps1 -Target wx -Verbose
```

**Parameters:**
- `-Target`: `all`, `windows`, `linux`, `macos`, `wx`, `qt`
- `-BuildType`: `Debug`, `Release`
- `-DryRun`: Run in dry-run mode (syntax validation only)
- `-Verbose`: Enable verbose output

### 2. test-workflows.bat (Batch)
Simple batch script with menu-driven interface.

**Usage:**
```cmd
REM Interactive menu
scripts\test-workflows.bat

REM Direct option selection
scripts\test-workflows.bat 1  (Windows workflow)
scripts\test-workflows.bat 2  (Linux workflow)
scripts\test-workflows.bat 4  (All workflows)
```

**Menu Options:**
1. Test Windows workflow (wx + qt)
2. Test Linux workflow (wx + qt, gcc + clang)
3. Test macOS workflow (wx + qt)
4. Test all workflows
5. Test wxWidgets framework only
6. Test Qt framework only
7. Validate workflow syntax only
8. Custom test (interactive)

### 3. test-workflows.sh (Bash)
Unix shell script with command-line options.

**Usage:**
```bash
# Make executable
chmod +x scripts/test-workflows.sh

# Test all workflows
./scripts/test-workflows.sh

# Test specific platform
./scripts/test-workflows.sh windows
./scripts/test-workflows.sh linux
./scripts/test-workflows.sh macos

# Test specific framework
./scripts/test-workflows.sh wx
./scripts/test-workflows.sh qt

# With options
./scripts/test-workflows.sh --dry-run qt
./scripts/test-workflows.sh --verbose --build-type Release wx
```

**Options:**
- `-d, --dry-run`: Run in dry-run mode
- `-v, --verbose`: Enable verbose output
- `-b, --build-type`: Build type (Debug|Release)
- `-h, --help`: Show help message

## Testing Matrix

All scripts test the following combinations:

### Windows Workflow
- Framework: wx, qt
- Build Type: Debug, Release
- Matrix: `gui_framework × build_type`

### Linux Workflow  
- Framework: wx, qt
- Compiler: gcc, clang
- Build Type: Debug, Release
- Matrix: `gui_framework × compiler × build_type`

### macOS Workflow
- Framework: wx, qt
- Build Type: Debug, Release
- Matrix: `gui_framework × build_type`

## Example Output

```
🧪 LogViewer Workflow Tester
============================================================
Target: all | Build Type: Debug | Dry Run: false

🔍 Checking prerequisites...
✅ act is installed: act version 0.2.82
⚠️  Docker is not available. Using dry-run mode only.

🪟 Testing Windows Workflows
==================================================
📝 Validating workflow syntax: .github/workflows/cmake-windows.yml
✅ Workflow syntax is valid

  Testing framework: wx
🚀 Testing workflow: cmake-windows.yml
   Running in dry-run mode
   Command: act -n -W .github/workflows/cmake-windows.yml --matrix gui_framework:wx --matrix build_type:Debug -v
✅ Workflow test passed: cmake-windows.yml
```

## Troubleshooting

### Common Issues

1. **act not found**
   - Install act using the methods above
   - Ensure act is in your system PATH

2. **Docker not available**
   - Scripts automatically switch to dry-run mode
   - Install Docker for full workflow execution

3. **Workflow syntax errors**
   - Check YAML indentation in workflow files
   - Validate against GitHub Actions schema

4. **Matrix parameter errors**
   - Ensure matrix parameters match workflow definitions
   - Check for typos in framework names (wx/qt)

### Verification Commands

```bash
# Verify act installation
act --version

# List available workflows
act --list

# Check specific workflow syntax
act -n -W .github/workflows/cmake-windows.yml

# Test with verbose output for debugging
act -n -W .github/workflows/cmake-windows.yml --matrix gui_framework:wx -v
```

## Integration with Development Workflow

1. **Pre-commit testing**: Run syntax validation before committing
2. **Feature branch testing**: Test specific frameworks when working on GUI changes
3. **Release testing**: Use Release build type before creating releases
4. **CI/CD validation**: Verify workflow changes locally before pushing

## Configuration Files

- `.actrc`: Local act configuration (platform mappings)
- `.github/workflows/`: Actual workflow files being tested

The scripts automatically handle matrix parameters and provide comprehensive coverage of all build configurations supported by your CI/CD pipeline.
