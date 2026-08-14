# LogViewer Installation Manifest System

## Overview

The installation manifest (`LogViewer_install_manifest.json` / `.logviewer_manifest.json`) is a metadata file that tracks installed libraries, versions, and build information. This prevents unintended overwriting of up-to-date files when upgrading LogViewer to a new version.

## File Location

- **Windows**: `<InstallDir>/LogViewer_install_manifest.json`
- **macOS**: `<InstallDir>/.logviewer_manifest.json` (hidden file)
- **Linux**: `/etc/LogViewer/.logviewer_manifest.json`

## File Format

The manifest is a JSON file containing:

> **Note:** The `libraries` list below matches what `packaging/CMakeLists.txt` currently generates (`Qt5Core`, `Qt5Gui`, …), which is stale — the application has used Qt 6 since the Qt5→Qt6 migration. This is a build-system bug, not a doc error; the manifest generator's `LIBRARIES` argument needs updating to the `Qt6*` names.

```json
{
  "version": "1.4.0",
  "installationDate": "2026-05-19T12:34:56Z",
  "installationPrefix": "/path/to/installation",
  "buildInfo": {
    "cmakeVersion": "3.25.0",
    "cmakeGenerator": "Unix Makefiles",
    "buildType": "Release",
    "cxxStandard": "20",
    "compiler": "Clang 14.0.0"
  },
  "libraries": {
    "Qt5Core": {
      "version": "5.15.2"
    },
    "Qt5Gui": {
      "version": "5.15.2"
    },
    "Qt5Widgets": {
      "version": "5.15.2"
    },
    "Qt5Network": {
      "version": "5.15.2"
    },
    "Qt5Xml": {
      "version": "5.15.2"
    },
    "Qt5Concurrent": {
      "version": "5.15.2"
    }
  },
  "metadata": {
    "description": "LogViewer installation manifest - prevents overwriting during upgrades",
    "format_version": "1.0"
  }
}
```

## Usage During Installation/Upgrade

### Installation Script Flow

1. **Check for existing manifest**
   ```bash
   if [ -f "$INSTALL_DIR/.logviewer_manifest.json" ]; then
       OLD_VERSION=$(jq -r '.version' "$INSTALL_DIR/.logviewer_manifest.json")
       NEW_VERSION="1.4.1"
   fi
   ```

2. **Compare versions**
   - If new version > old version: Proceed with upgrade
   - If new version == old version: Skip file overwriting (already installed)
   - If new version < old version: Warn user about downgrade

3. **Preserve user files** (optional)
   - Use manifest to identify only system library files
   - Preserve user configuration and log files
   - Update only changed binaries and libraries

4. **Update manifest**
   - Replace old manifest with new one after successful installation
   - Preserve original in backup (e.g., `.logviewer_manifest.json.bak`)

### Example Installation Script (Bash)

```bash
#!/bin/bash

INSTALL_DIR="${1:-.}"
MANIFEST_FILE="$INSTALL_DIR/.logviewer_manifest.json"
NEW_MANIFEST="./LogViewer_install_manifest.json"

# Load old version if exists
if [ -f "$MANIFEST_FILE" ]; then
    OLD_VERSION=$(jq -r '.version' "$MANIFEST_FILE")
    INSTALL_DATE=$(jq -r '.installationDate' "$MANIFEST_FILE")
    echo "Existing installation found: v$OLD_VERSION (installed: $INSTALL_DATE)"
    
    # Backup old manifest
    cp "$MANIFEST_FILE" "$MANIFEST_FILE.bak"
else
    OLD_VERSION="0.0.0"
    echo "No existing installation found"
fi

# Get new version
NEW_VERSION=$(jq -r '.version' "$NEW_MANIFEST")
echo "Installing version: v$NEW_VERSION"

# Version comparison
if [ "$OLD_VERSION" = "$NEW_VERSION" ]; then
    echo "Same version already installed. Skipping file overwrite."
    exit 0
fi

# Proceed with installation
echo "Installing files..."
# ... installation commands ...

# Update manifest
cp "$NEW_MANIFEST" "$MANIFEST_FILE"
echo "Installation complete. Manifest updated."
```

### Example Installation Script (NSIS - Windows)

```nsis
; Check for existing manifest
IfFileExists "$INSTDIR\.logviewer_manifest.json" HaveManifest NoManifest

HaveManifest:
  ; Read old version from manifest
  nsJSON::Set /file "$INSTDIR\.logviewer_manifest.json" /tree manifest
  nsJSON::Get /tree manifest /key "version" /end
  Pop $OldVersion
  ${If} $OldVersion == $NewVersion
    MessageBox MB_YESNO "Version $NewVersion already installed.$\nSkip file overwriting?" IDYES SkipFiles
  ${EndIf}

NoManifest:
  ; Proceed with installation
  SetOverwrite try
  SetOutPath "$INSTDIR"
  File "LogViewer.exe"
  File "LogViewer_install_manifest.json"
  Goto Done

SkipFiles:
  ; Only update manifest, skip other files
  SetOverwrite off
  SetOutPath "$INSTDIR"
  File "LogViewer_install_manifest.json"

Done:
  DetailPrint "Installation complete"
```

## Extending the Manifest

To add more libraries or metadata:

1. **Modify `GenerateInstallManifest.cmake`** in `cmake/GenerateInstallManifest.cmake`
2. **Update library list** in `packaging/CMakeLists.txt`:
   ```cmake
   generate_install_manifest(
       OUTPUT_FILE "${MANIFEST_OUTPUT_FILE}"
       INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}"
       LIBRARIES "Qt5Core;Qt5Gui;Qt5Widgets;Qt5Network;Qt5Xml;Qt5Concurrent;customlib"
   )
   ```

## Version Handling

The manifest includes semantic versioning (MAJOR.MINOR.PATCH):
- **Major version change** (1.x → 2.x): Likely breaking changes, consider full reinstall
- **Minor version change** (1.4 → 1.5): New features, safe upgrade
- **Patch version change** (1.4.0 → 1.4.1): Bug fixes, safe upgrade

## Backward Compatibility

- Manifest format is versioned (`"format_version": "1.0"`)
- Future versions can handle older manifest formats
- Scripts should gracefully handle missing manifest file (first installation)

## Benefits

✅ **Prevents unnecessary file overwrites** during upgrades
✅ **Tracks installation metadata** for diagnostics
✅ **Enables intelligent installers** that preserve user files
✅ **Simplifies version management** for maintenance tools
✅ **Provides audit trail** of installation history
