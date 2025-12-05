# Ollama Deployment Options for LogViewer

## Overview

LogViewer's AI analysis features use Ollama, which needs to be installed separately. Here are your deployment options:

---

## ✅ Option 1: Separate Installation (Recommended)

**Pros:**
- Simplest to implement ✓
- Users control their Ollama version
- No licensing concerns
- Smaller package size
- Automatic updates via Ollama

**Cons:**
- Users must install separately
- Extra setup step

**Implementation:**
Currently implemented! LogViewer includes:
- Auto-detection of Ollama installation
- Setup dialog with instructions (Tools → AI Analysis Setup)
- Links to download page
- Status checking

**User Experience:**
1. Install LogViewer normally
2. Click "AI Analysis" tab
3. If Ollama missing, click setup link
4. Install Ollama (5 minutes)
5. Done!

---

## Option 2: Bundle Ollama Binary

**Pros:**
- One-click install for users
- Guaranteed compatible version

**Cons:**
- Large installer (~500MB+)
- Must bundle for each OS
- Update management complexity
- Licensing considerations

**Implementation for macOS:**

```cmake
# In packaging/CMakeLists.txt

if(APPLE)
    # Download Ollama binary
    set(OLLAMA_VERSION "0.1.17")
    set(OLLAMA_URL "https://github.com/ollama/ollama/releases/download/v${OLLAMA_VERSION}/ollama-darwin")
    
    file(DOWNLOAD ${OLLAMA_URL} 
         ${CMAKE_BINARY_DIR}/ollama
         SHOW_PROGRESS)
    
    # Bundle in .app
    install(PROGRAMS ${CMAKE_BINARY_DIR}/ollama
            DESTINATION LogViewer.app/Contents/Resources/bin
            COMPONENT Runtime)
    
    # Install model
    install(CODE "
        execute_process(
            COMMAND ${CMAKE_INSTALL_PREFIX}/LogViewer.app/Contents/Resources/bin/ollama pull llama3.2:1b
        )
    ")
endif()
```

**Installer size impact:**
- Ollama binary: ~50MB
- llama3.2:1b model: ~1.3GB
- llama3.2 model: ~4GB

---

## Option 3: Download on First Use

**Pros:**
- Small initial installer
- User downloads only if needed

**Cons:**
- Network required on first use
- Download UX complexity

**Implementation:**

Add download manager to OllamaSetupDialog:

```cpp
class OllamaDownloader : public QObject
{
    Q_OBJECT
public:
    void DownloadOllama();
    void DownloadModel(const QString& model);
    
signals:
    void Progress(int percentage);
    void Finished();
    void Error(const QString& message);
};
```

---

## Option 4: GitHub CI/CD Pre-packaging

**Pros:**
- Automated deployment
- Consistent across platforms

**Cons:**
- Long CI/CD build times
- Storage costs for large artifacts

**GitHub Actions Workflow:**

```yaml
# .github/workflows/release.yml

name: Release with Ollama

on:
  release:
    types: [created]

jobs:
  build-macos:
    runs-on: macos-latest
    steps:
      - uses: actions/checkout@v3
      
      - name: Install Ollama
        run: brew install ollama
      
      - name: Pull AI Model
        run: ollama pull llama3.2:1b
      
      - name: Build LogViewer
        run: |
          cmake --preset macos-release
          cmake --build --preset macos-release-build
          cpack --preset macos-release-package
      
      - name: Bundle Ollama
        run: |
          cp $(which ollama) dist/Release/LogViewer.app/Contents/Resources/bin/
          cp -r ~/.ollama/models dist/Release/LogViewer.app/Contents/Resources/
      
      - name: Create DMG
        run: |
          hdiutil create -volname LogViewer \
                         -srcfolder dist/Release/LogViewer.app \
                         -ov -format UDZO \
                         LogViewer-with-AI.dmg
      
      - name: Upload Release Asset
        uses: actions/upload-release-asset@v1
        with:
          asset_path: ./LogViewer-with-AI.dmg
          asset_name: LogViewer-macOS-AI-${{ github.ref_name }}.dmg
```

---

## Recommendation

**For MVP and Ease of Use: Option 1 (Current Implementation)**

Reasons:
1. ✅ Already working
2. ✅ Clean separation of concerns
3. ✅ Users comfortable with "system requirements"
4. ✅ Similar to Python requiring pip packages
5. ✅ Easy to test and maintain

**For Premium Experience: Option 2 or 3**

If you want maximum convenience:
- **macOS**: Bundle small model (llama3.2:1b) = +1.3GB installer
- **Windows/Linux**: Provide download-on-first-use

---

## Installation Instructions to Include in README

```markdown
## AI Analysis Features (Optional)

LogViewer includes AI-powered log analysis using local LLMs.

### Setup (5 minutes):

1. **Install Ollama**
   - macOS: `brew install ollama`
   - Linux: `curl -fsSL https://ollama.ai/install.sh | sh`
   - Windows: Download from https://ollama.ai

2. **Start Ollama**
   ```bash
   ollama serve
   ```

3. **Download Model**
   ```bash
   ollama pull llama3.2    # 4GB, best quality
   # OR
   ollama pull llama3.2:1b # 1.3GB, faster
   ```

4. **Use in LogViewer**
   - Load a log file
   - Click "AI Analysis" tab
   - Select analysis type and click "Analyze"

### Note
AI analysis runs completely on your machine. No data is sent to external servers.
```

---

## Current Status

✅ Implemented:
- Auto-detection
- Setup dialog with instructions
- Status checking
- Error handling

📝 To Add (if desired):
- Download-on-demand option
- Model selection UI
- Bundled installer variant

Your current setup is **production-ready** for users comfortable with installing system dependencies!
