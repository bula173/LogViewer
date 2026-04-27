# LogViewer — Requirements Specification

**Document ID**: LV-REQ-001  
**Version**: 1.0  
**Status**: Active  
**Application version**: 1.2.x  

---

## Table of Contents

1. [Purpose and Scope](#1-purpose-and-scope)
2. [System Requirements (SYS)](#2-system-requirements-sys)
   - [2.1 Supported Operating Systems](#21-supported-operating-systems)
   - [2.2 Hardware](#22-hardware)
   - [2.3 Runtime Dependencies](#23-runtime-dependencies)
3. [Software Requirements (SW)](#3-software-requirements-sw)
   - [3.1 Functional Requirements](#31-functional-requirements)
   - [3.2 Performance Requirements](#32-performance-requirements)
   - [3.3 Usability Requirements](#33-usability-requirements)
   - [3.4 Reliability Requirements](#34-reliability-requirements)
   - [3.5 Security Requirements](#35-security-requirements)
   - [3.6 Portability Requirements](#36-portability-requirements)
   - [3.7 Plugin System Requirements](#37-plugin-system-requirements)
4. [Build Requirements](#4-build-requirements)
5. [Requirement Traceability](#5-requirement-traceability)

---

## 1. Purpose and Scope

This document defines the system and software requirements for **LogViewer**, a cross-platform desktop application for inspecting, filtering, and analysing log files. It covers:

- The minimum environment required to **run** the application (Section 2)
- The capabilities and constraints the application **must** satisfy (Section 3)
- The toolchain required to **build** the application from source (Section 4)

Requirements are identified by a unique ID in the form `SYS-xxx` (system) or `SW-xxx` (software) and a priority level:

| Priority | Meaning |
|----------|---------|
| **MUST** | Mandatory — the application cannot ship without this |
| **SHOULD** | Strongly recommended — omission requires explicit justification |
| **MAY** | Optional — nice-to-have |

---

## 2. System Requirements (SYS)

### 2.1 Supported Operating Systems

| ID | Requirement | Priority |
|----|-------------|----------|
| SYS-001 | The application MUST run on **Windows 10** (64-bit) or later | MUST |
| SYS-002 | The application MUST run on **macOS 12 Monterey** or later | MUST |
| SYS-003 | The application MUST run on **Ubuntu 22.04 LTS** (or equivalent glibc 2.35+) | MUST |
| SYS-004 | The application SHOULD run on other mainstream Linux distributions with glibc 2.35+ | SHOULD |
| SYS-005 | The application MAY run on earlier OS versions but this is not tested or guaranteed | MAY |

### 2.2 Hardware

| ID | Requirement | Priority |
|----|-------------|----------|
| SYS-010 | **CPU**: x86-64 (AMD64) processor | MUST |
| SYS-011 | **RAM**: Minimum 512 MB available to the application | MUST |
| SYS-012 | **RAM**: 1 GB or more recommended for log files exceeding 100 000 events | SHOULD |
| SYS-013 | **Disk**: 200 MB free disk space for installation | MUST |
| SYS-014 | **Display**: Minimum resolution 1280 × 720 pixels | MUST |
| SYS-015 | **Display**: High-DPI (HiDPI / Retina) screens SHOULD be supported with correct scaling | SHOULD |
| SYS-016 | **Network**: Internet access is required only for AI provider features; the core viewer works offline | MUST |

### 2.3 Runtime Dependencies

| ID | Requirement | Priority |
|----|-------------|----------|
| SYS-020 | On **Windows**, the installer MUST bundle all required Qt 6 DLLs and MinGW runtime libraries | MUST |
| SYS-021 | On **macOS**, the `.app` bundle MUST be self-contained (all Qt frameworks bundled via `macdeployqt`) | MUST |
| SYS-022 | On **Linux**, the application MAY depend on system-provided Qt 6 libraries (Qt 6.5+) or bundle them | MAY |
| SYS-023 | The application MUST NOT require administrator / root privileges for normal operation | MUST |
| SYS-024 | User configuration and application data MUST be stored in the platform-standard user data directory (`QStandardPaths`) | MUST |

---

## 3. Software Requirements (SW)

### 3.1 Functional Requirements

#### 3.1.1 Log File Loading

| ID | Requirement | Priority |
|----|-------------|----------|
| SW-001 | The application MUST be able to open log files via a file-open dialog | MUST |
| SW-002 | The application MUST support **XML** log format | MUST |
| SW-003 | The application MUST support **CSV** log format | MUST |
| SW-004 | The application SHOULD support **JSON** log format | SHOULD |
| SW-005 | Log file parsing MUST occur on a background thread; the UI MUST remain responsive during loading | MUST |
| SW-006 | The application MUST display a progress indicator during file loading | MUST |
| SW-007 | The application MUST handle malformed or truncated log files without crashing | MUST |
| SW-008 | Files up to **100 000 events** MUST load within 5 seconds on reference hardware (see SYS-010/011) | MUST |
| SW-009 | Files up to **1 000 000 events** SHOULD load within 30 seconds on reference hardware | SHOULD |

#### 3.1.2 Event Display

| ID | Requirement | Priority |
|----|-------------|----------|
| SW-010 | Events MUST be displayed in a table view with at minimum: timestamp, severity/type, source, message columns | MUST |
| SW-011 | The table MUST use a **virtual (lazy) rendering** model — only visible rows are rendered regardless of dataset size | MUST |
| SW-012 | The user MUST be able to select and view the full details of a single event in a detail panel | MUST |
| SW-013 | The application MUST support **light and dark themes** | MUST |
| SW-014 | Column widths MUST be user-adjustable and SHOULD persist across sessions | SHOULD |

#### 3.1.3 Filtering

| ID | Requirement | Priority |
|----|-------------|----------|
| SW-020 | The application MUST provide **text search** across event messages | MUST |
| SW-021 | Text search MUST support **regular expressions** | MUST |
| SW-022 | The application MUST provide filtering by event **type / severity** | MUST |
| SW-023 | The application MUST provide filtering by **actor / source** | MUST |
| SW-024 | Multiple filters MUST be combinable (AND logic) | MUST |
| SW-025 | Filter state SHOULD persist when a new file is loaded | SHOULD |
| SW-026 | The application MUST provide a **keyboard shortcut** (Ctrl+F) to focus the search bar | MUST |
| SW-027 | Search results MUST be highlighted in the table and navigable with next/previous controls | MUST |

#### 3.1.4 AI-Assisted Analysis

| ID | Requirement | Priority |
|----|-------------|----------|
| SW-030 | The application MUST support AI analysis via at least one **local** provider (Ollama or LM Studio) | MUST |
| SW-031 | The application SHOULD support AI analysis via at least one **cloud** provider (OpenAI, Anthropic, Gemini, or xAI) | SHOULD |
| SW-032 | AI requests MUST be executed asynchronously without blocking the UI | MUST |
| SW-033 | API keys for cloud providers MUST be stored encrypted on disk | MUST |
| SW-034 | The application MUST allow the user to configure AI provider URL, model name, and timeout | MUST |
| SW-035 | The application MUST allow the user to save and load custom AI analysis prompts | MUST |
| SW-036 | AI analysis MUST respect the active filter — only visible/filtered events are sent for analysis | MUST |

#### 3.1.5 Export

| ID | Requirement | Priority |
|----|-------------|----------|
| SW-040 | The application MUST support exporting the currently visible (filtered) events | MUST |
| SW-041 | Export SHOULD support at least CSV format | SHOULD |

#### 3.1.6 Configuration

| ID | Requirement | Priority |
|----|-------------|----------|
| SW-050 | Application settings MUST be persisted in a JSON configuration file | MUST |
| SW-051 | The user MUST be able to edit the configuration through the UI | MUST |
| SW-052 | The application MUST start with sensible defaults when no configuration file exists | MUST |

### 3.2 Performance Requirements

| ID | Requirement | Priority |
|----|-------------|----------|
| SW-060 | The UI MUST remain interactive (no freeze > 200 ms) during log file parsing | MUST |
| SW-061 | Scrolling through the event table MUST sustain 60 fps on reference hardware for files of any supported size | MUST |
| SW-062 | Filter changes MUST be applied and reflected in the table within 500 ms for datasets up to 1 000 000 events | SHOULD |
| SW-063 | Application startup to a usable state MUST take less than 3 seconds on reference hardware | SHOULD |
| SW-064 | Memory usage MUST NOT grow unboundedly when the same file is reloaded multiple times | MUST |

### 3.3 Usability Requirements

| ID | Requirement | Priority |
|----|-------------|----------|
| SW-070 | The application MUST use a dockable panel layout that the user can rearrange | MUST |
| SW-071 | Panel layout SHOULD be persisted and restored across sessions | SHOULD |
| SW-072 | All primary operations MUST be reachable via the menu bar | MUST |
| SW-073 | Common operations SHOULD have keyboard shortcuts | SHOULD |
| SW-074 | The application MUST display the current application version in the About dialog | MUST |

### 3.4 Reliability Requirements

| ID | Requirement | Priority |
|----|-------------|----------|
| SW-080 | The application MUST NOT crash when given an empty log file | MUST |
| SW-081 | The application MUST NOT crash when given a file that is not a valid log format | MUST |
| SW-082 | Failure to connect to an AI provider MUST be reported to the user with a clear error message; it MUST NOT crash the application | MUST |
| SW-083 | Plugin load failures MUST be logged and reported to the user; they MUST NOT prevent the core application from starting | MUST |

### 3.5 Security Requirements

| ID | Requirement | Priority |
|----|-------------|----------|
| SW-090 | API keys and credentials MUST be stored encrypted, not in plain text | MUST |
| SW-091 | The application MUST NOT transmit log data to any external service without explicit user action | MUST |
| SW-092 | The Windows binary SHOULD be signed with a code-signing certificate to prevent AV false positives | SHOULD |
| SW-093 | The macOS bundle SHOULD be signed and notarised with Apple | SHOULD |
| SW-094 | The application MUST enable platform binary hardening: ASLR, DEP/NX, and CFG where supported | MUST |
| SW-095 | Release builds MUST be verified with GitHub Sigstore build attestations | SHOULD |

### 3.6 Portability Requirements

| ID | Requirement | Priority |
|----|-------------|----------|
| SW-100 | The application MUST build and run on all three supported platforms (Windows, macOS, Linux) from the same source tree | MUST |
| SW-101 | Platform-specific code MUST be isolated behind preprocessor guards or abstraction layers | MUST |
| SW-102 | The build system MUST use **CMake ≥ 3.25** with CMakePresets for all supported platforms | MUST |

### 3.7 Plugin System Requirements

| ID | Requirement | Priority |
|----|-------------|----------|
| SW-110 | The application MUST expose a stable C ABI plugin interface versioned with major.minor | MUST |
| SW-111 | A plugin with an incompatible API major version MUST be rejected at load time with a clear error | MUST |
| SW-112 | Plugins MUST be loaded from a configurable directory without recompiling the host application | MUST |
| SW-113 | The SDK (headers + CMake config) MUST be installable independently of the host application | MUST |
| SW-114 | A plugin crash SHOULD NOT cause the host application to crash (fault isolation) | SHOULD |

---

## 4. Build Requirements

These are requirements on the development toolchain, not on the end-user system.

| ID | Requirement |
|----|-------------|
| BUILD-001 | **CMake** ≥ 3.25 |
| BUILD-002 | **C++20 compiler**: Clang ≥ 15, GCC ≥ 11, MSVC 2019, or Apple Clang ≥ 15 |
| BUILD-003 | **Qt** ≥ 6.5 (Core, Widgets, Charts, Network, Concurrent) |
| BUILD-004 | **Ninja** (recommended build generator) |
| BUILD-005 | **libexpat** — XML parsing |
| BUILD-006 | **libarchive** — archive support |
| BUILD-007 | **libssh** — SSH transport (optional) |
| BUILD-008 | **libcurl** — AI provider HTTP requests |
| BUILD-009 | **NSIS** — Windows installer packaging |
| BUILD-010 | **macdeployqt** — macOS bundle packaging |

Third-party libraries bundled via CMake `FetchContent` (no separate install required): `gtest`, `fmt`, `gflags`.

---

## 5. Requirement Traceability

| Requirement ID | Verified by |
|----------------|-------------|
| SYS-001 – SYS-005 | CI matrix: `windows-latest`, `macos-latest`, `ubuntu-latest` |
| SYS-010 – SYS-016 | Manual verification on reference hardware |
| SYS-020 – SYS-024 | CI release workflow (NSIS installer, macOS bundle) |
| SW-001 – SW-009 | `DataParserTest`, `CsvParserTest`, `XmlParserTest` |
| SW-010 – SW-014 | Manual UI testing |
| SW-020 – SW-027 | `ModelTest`, `EventsContainerTest` |
| SW-030 – SW-036 | Manual AI integration testing |
| SW-060 – SW-064 | Manual performance testing with `example_log_10k.xml` and generated logs |
| SW-080 – SW-083 | Unit tests + manual error injection |
| SW-090 – SW-095 | CI security workflow (CodeQL, Gitleaks, AV scan, attestation) |
| SW-110 – SW-114 | `ArchitectureImprovementsTests`, plugin load/reject tests |
| BUILD-001 – BUILD-010 | CI build matrix (`macos-debug-qt`, `macos-release-qt`, etc.) |
