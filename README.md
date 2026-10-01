# ChromeDebloater Pro

**Next-generation, ultra-lightweight Windows C++ software to harden, debloat, and optimize Chromium-based browsers without AI.**

Supports: **Google Chrome**, **Brave Browser**, **Microsoft Edge**

![ChromeDebloater Pro UI](assets/ui_preview.jpg)

---

## What Makes ChromeDebloater Pro Different?

- 🎨 **Redesigned Modern Dark UI**: Built with a custom double-buffered GDI+ rendering engine. No old-school Win32 controls, no Electron, no webview, no Python/Qt bloat. Smooth, responsive 60fps desktop experience in deep obsidian `#0B0E14` with glowing status badges and Segoe UI typography.
- 🔍 **Live System Audit Engine**: Automatically inspects browser registry configurations and on-disk model caches to calculate your **Hardening & Optimization Score (0% – 100%)** before making changes.
- 💾 **1-Click Snapshot & Rollback**: Automatically takes a timestamped registry backup before applying changes. Restore to any previous point or reset all policies to factory defaults with one click.
- 🧹 **Deep Profile Cleaner**: Compacts and defragments SQLite databases (`History`, `Favicons`, `Web Data`) via Windows built-in `winsqlite3.dll`, sweeps stale GPU/shader caches, and flushes Windows DNS cache.
- ⚡ **Pure Native Windows Binary**: Only **~201 KB**. Single standalone executable. Zero runtime dependencies.
- 🛡 **Native UAC Elevation**: Embedded `requireAdministrator` manifest ensures seamless administrator privilege handling.
- 🖥 **Dual-Mode (GUI + Headless CLI)**: Double-click to launch the graphical dashboard, or run headlessly via terminal / scripts with `--all`, `--audit`, or `--clean`.

---

## Software Architecture

```
ChromeDebloater/
├── src/
│   ├── main.cpp                  # wWinMain entry point, CLI parser, elevation check, message loop
│   ├── ui/
│   │   ├── window.h / .cpp       # Modern main window, view controller, thread dispatcher
│   │   ├── theme.h               # Obsidian dark palette, colors, fonts
│   │   ├── render_utils.h / .cpp # GDI+ double-buffered renderer (cards, gauges, toggles, badges)
│   │   └── navigation.h          # Sidebar view routing (Dashboard, Tweaks, Cleaner, Backups, Logs)
│   ├── engine/
│   │   ├── browser_target.h      # Chromium browser discovery & version detection
│   │   ├── audit_engine.h / .cpp # Real-time system scanner & 0-100% hardening score calculator
│   │   ├── tweak_engine.h / .cpp # Enterprise policy manager, flag injector, SCM service controller
│   │   ├── backup_engine.h / .cpp# Automated registry snapshots & rollback manager
│   │   └── sqlite_cleaner.h      # Dynamic winsqlite3.dll VACUUM and REINDEX engine
│   ├── app.manifest              # UAC requireAdministrator + ComCtl32 v6 visual styles
│   └── app.rc                    # Windows resource script
├── dist/
│   └── ChromeDebloater.exe       # Standalone compiled portable executable (~201 KB)
├── assets/
│   └── ui_preview.jpg            # Application interface preview
├── CMakeLists.txt                # CMake build configuration
├── build.ps1                     # Native MSVC command-line build script
├── LICENSE                       # MIT License
└── README.md
```

---

## Included Modules

| Subsystem | Category | Description |
|---|---|---|
| 🤖 **AI & Gemini Elimination** | Security | Enforces 25 enterprise policies disabling Gemini, OptimizationGuide, PromptAPI, etc., and deletes local foundational models. |
| 🔒 **Privacy & Content Hardening** | Privacy | Enforces Quad9 DNS-over-HTTPS, HTTPS-Only mode, WebRTC IP leakage mitigation, TLS 1.2 minimum, anti-tracking, and blocks intrusive device sensors. |
| 📡 **Telemetry Suppression** | Privacy | Shuts down metrics reporting, SafeBrowsing extended telemetry, background cleanup scanner, and user feedback hooks. |
| 🔑 **Authentication Preservation** | Compatibility | Explicitly allowlists Google auth cookies to keep **Google Sync** and **Google Password Manager** operational (Chrome only). |
| ⚡ **Low-RAM & Performance** | Performance | Clamps process inflation (`SitePerProcess=0`), enables maximum tab Memory Saver, enables GPU hardware offload, throttles background JS timers. |
| 🚀 **Performance Flags** | Performance | Injects QUIC, parallel downloading, GPU rasterization, zero-copy, and back-forward cache into `Local State`. |
| 🧹 **UI Debloat** | Interface | Strips Cast icon from toolbar, Desktop Sharing Hub, shared clipboard, and web app install promotions. |
| 🔍 **Search Provider** | Privacy | Configures private Brave Search as the default search engine. |
| 🔧 **Deep Maintenance** | Health | Vacuums and reindexes SQLite databases (`History`, `Favicons`, `Web Data`) via Windows native SQLite, cleans shader/GPU caches, and flushes DNS. |
| 📌 **Permanent Version Freeze** | Control | 4-layer update lockdown for Google Chrome: enterprise update policies, GoogleUpdater services disabled, scheduled tasks disabled. |

---

## Quick Start (Portable Binary)

1. Download **`ChromeDebloater.exe`** from the [Releases](https://github.com/KaZuN00B/ChromeDebloater/releases) page or from `dist/ChromeDebloater.exe`.
2. Double-click the executable and accept the Windows UAC Administrator prompt.
3. Use the sidebar to switch between:
   - 📊 **Dashboard**: View your Hardening Score and run 1-click optimizations.
   - ⚡ **Optimizations**: Choose from **Maximum**, **Balanced**, or **Privacy** presets, or toggle individual modules.
   - 🧹 **Deep Cleaner**: Vacuum databases and clean shader bloat.
   - 🔄 **Backups & Restore**: Create restore points or rollback to previous registry states.
   - 📜 **Activity Console**: Monitor live execution events.

---

## Command-Line / Scripting Usage

```cmd
:: Run a full system audit and view your score
ChromeDebloater.exe --audit

:: Apply all 10 optimizations headlessly (auto-creates a safety restore point first)
ChromeDebloater.exe --all --browser chrome

:: Deep clean SQLite profile databases, purge caches, and flush DNS
ChromeDebloater.exe --clean

:: Display all command-line options
ChromeDebloater.exe --help
```

---

## Building from Source

### Prerequisites
- Windows 10 / 11 (x64)
- Visual Studio 2022 / 2026 or Visual Studio C++ Build Tools (with C++20 support)

### One-Click PowerShell Build
```powershell
cd C:\Users\Admin\Documents\ChromeDebloater
.\build.ps1 -Clean
```

The compiled binary will be placed at `dist\ChromeDebloater.exe` (~201 KB).

---

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
