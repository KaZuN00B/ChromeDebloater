# ChromeDebloater Pro — Project Memory & Architecture Context

## Project Overview
Pure native Windows C++20 software to harden, debloat, and optimize Chromium-based browsers (Google Chrome, Brave, Microsoft Edge) without AI, telemetry, or bloatware.

- **Executable**: `dist/ChromeDebloater.exe` (~402 KB native binary, zero external dependencies).
- **Compiler**: MSVC (C++20, `/O2`, `/MD`, `/utf-8`, `/DUNICODE`).
- **Build Script**: `powershell -ExecutionPolicy Bypass -File .\build.ps1`
- **UAC Elevation**: Embedded manifest requires administrator rights (`requireAdministrator`).
- **GitHub**: `https://github.com/KaZuN00B/ChromeDebloater.git`

## Core Engines
- `src/main.cpp`: CLI argument router (`--gaming`, `--shortcuts`, `--all`, `--shield`, `--clean`, `--audit`, `--low-resource`) & GUI window launcher.
- `src/ui/window.cpp` & `window.h`: High-res 1260x820 dark mode UI with 270° circular Health Score ring gauge, 3 metric cards, 5 Quick Actions, and 7 navigation pages.
- `src/ui/render_utils.cpp`: Double-buffered GDI+ rendering engine.
- `src/engine/shortcut_manager.cpp` & `shortcut_manager.h`: Manages `.lnk` flags and generates the Hardened Gaming Mode batch launcher (`Google Chrome Gaming Mode.bat`) and shortcuts.
- `src/engine/tweak_engine.cpp` & `tweak_engine.h`: 15 modular debloating subsystems covering AI, telemetry, low-resource clamps, privacy sandbox, and default browser prompts.
- `src/engine/network_shield.cpp` & `network_shield.h`: 16 Windows Defender Firewall outbound rules and 48 domains hosts sinkhole.
- `src/engine/audit_engine.cpp` & `audit_engine.h`: 12-subsystem live audit engine (0-100% score).
- `src/engine/backup_engine.cpp` & `backup_engine.h`: Automatic timestamped registry snapshot & rollback engine.
- `src/engine/update_checker.cpp` & `update_checker.h`: Live multi-browser upstream version queries.

## Key Operational Patterns
- **No External Dependencies**: Use Win32 APIs, GDI+, and Windows built-in `winsqlite3.dll`.
- **Default Browser Fix**: Dual HKLM/HKCU `DefaultBrowserSettingEnabled=0`, `HideFirstRunExperience=1`, sentinel `First Run` file, JSON preference patching.
- **Gaming Mode**: Low latency, unlocked FPS (`--disable-frame-rate-limit`), zero VSync (`--disable-gpu-vsync`), zero timer throttling (`--disable-background-timer-throttling`), 100MB cache limit.
