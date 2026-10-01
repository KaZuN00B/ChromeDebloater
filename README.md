# ChromeDebloater

**Ultra-lightweight, native Windows C++ tool to harden, debloat, and optimize Chromium-based browsers without AI.**

Supports: **Google Chrome**, **Brave Browser**, **Microsoft Edge**

![ChromeDebloater UI](assets/ui_preview.jpg)

---

## Highlights

- **Pure Native C++20**: Zero runtime dependencies. No Python, no Qt, no webview, no electron. Single standalone executable (~122 KB).
- **Direct Win32 & Registry API**: Directly enforces Windows enterprise policies, cleans browser models, vacuums profile databases, and controls Windows services.
- **Embedded UAC Elevation**: Automatically requests administrator rights on launch with native Windows UAC prompt.
- **Modern Dark UI**: Native Windows 10/11 immersive dark mode with Segoe UI typography and live execution console.
- **Re-runnable & Idempotent**: Safely execute optimizations anytime for ongoing browser maintenance.

---

## Hardening & Optimization Modules

| Module | Category | Description |
|---|---|---|
| 🤖 **AI & Gemini Elimination** | Security | Enforces 25 enterprise policies disabling Gemini, OptimizationGuide, PromptAPI, etc., and deletes local on-disk model stores. |
| 🔒 **Privacy & Content Hardening** | Privacy | Quad9 DNS-over-HTTPS, HTTPS-Only mode, WebRTC IP leakage mitigation, TLS 1.2 minimum, anti-tracking, blocks unnecessary permissions (sensors, geo, notifications). |
| 📡 **Telemetry Suppression** | Privacy | Turns off Metrics reporting, SafeBrowsing extended reporting, Chrome cleanup scanner, and user feedback telemetry. |
| 🔑 **Authentication Preservation** | Compatibility | Explicitly allowlists Google auth cookies to keep **Google Sync** and **Google Password Manager** operational (Chrome only). |
| ⚡ **Performance & Low-RAM** | Performance | Clamps iframe process proliferation (`SitePerProcess=0`), enables maximum tab Memory Saver, enables GPU hardware offload, throttles background JS timers. |
| 🚀 **Performance Flags** | Performance | Injects QUIC, parallel downloading, GPU rasterization, zero-copy, and back-forward cache into `Local State`. |
| 🧹 **UI Debloat** | Interface | Strips Cast icon from toolbar, Desktop Sharing Hub, shared clipboard, and web app install promotions. |
| 🔍 **Search Provider** | Privacy | Configures private Brave Search as the default search engine. |
| 🔧 **Deep Maintenance** | Health | Vacuums and reindexes SQLite databases (`History`, `Favicons`, `Web Data`) via Windows native SQLite, cleans shader/GPU/crashpad caches, and flushes Windows DNS cache. |
| 📌 **Permanent Version Freeze** | Control | 4-layer update lockdown for Google Chrome: enterprise update policies, GoogleUpdater services disabled, scheduled tasks disabled. |

---

## Quick Start (Run Portable Binary)

1. Download **`ChromeDebloater.exe`** from the [Releases](https://github.com/KaZuN00B/ChromeDebloater/releases) page or from `dist/ChromeDebloater.exe`.
2. Double-click the executable.
3. Accept the Windows UAC Administrator prompt.
4. Select your target browser (**Google Chrome**, **Brave**, or **Microsoft Edge**).
5. Choose your desired tweaks and click **⚡ RUN OPTIMIZATIONS**.

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

The compiled binary will be placed at `dist\ChromeDebloater.exe` (~122 KB).

### Building with CMake
```bat
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
```

---

## Project Structure

```
ChromeDebloater/
├── src/
│   ├── main.cpp          # wWinMain entry point, UAC elevation check, message loop
│   ├── ui.h / ui.cpp     # Native Win32 modern dark GUI, controls, worker thread
│   ├── engine.h / .cpp   # Hardening engine (Registry, files, SCM, processes)
│   ├── browser.h         # Chromium browser detection (Chrome, Brave, Edge)
│   ├── sqlite_helper.h   # Dynamic Windows native winsqlite3.dll wrapper
│   ├── app.manifest      # UAC requireAdministrator + ComCtl32 v6 visual styles
│   └── app.rc            # Windows resource script
├── dist/
│   └── ChromeDebloater.exe # Standalone compiled portable executable (122 KB)
├── CMakeLists.txt        # CMake build configuration
├── build.ps1             # Native MSVC command-line build script
├── LICENSE               # MIT License
└── README.md
```

---

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
