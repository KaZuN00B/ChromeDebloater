# ChromeDebloater

**Portable Windows tool to harden, debloat and optimize Chromium-based browsers without AI.**

![ChromeDebloater UI](assets/ui_preview.jpg)

Supports: **Google Chrome**, **Brave Browser**, **Microsoft Edge**

---

## Features

| Category | What it does |
|---|---|
| 🤖 **AI Removal** | Kills all Gemini / GenAI / OptimizationGuide subsystems via enterprise policy; deletes on-disk model files |
| 🔒 **Privacy** | DoH (Quad9), HTTPS-Only, WebRTC leak fix, TLS 1.2 min, disable telemetry / crash reporting, block notifications / geolocation / Bluetooth |
| ⚡ **Performance** | Memory Saver (max), SitePerProcess=0, GPU hardware acceleration, JS timer throttle, QUIC + parallel download flags |
| 🧹 **Debloat** | Remove Cast icon, Desktop Sharing Hub, Feedback, Web App install prompts |
| 🔧 **Maintenance** | SQLite VACUUM/REINDEX on profile databases, purge GPU/shader/crashpad caches, flush DNS |
| 📌 **Update Control** | Freeze Chrome at current version (4-layer: policy + services + tasks + binary) |

All tweaks are **idempotent** — re-running is always safe.

---

## Requirements

- Windows 10 / 11
- Administrator privileges (UAC prompt fires automatically)
- Python 3.10+ with PySide6 (for running from source)

---

## Run from Source

```bat
cd C:\Users\Admin\Documents\ChromeDebloater
python src\main.py
```

---

## Build Portable EXE

```bat
cd C:\Users\Admin\Documents\ChromeDebloater
pyinstaller ChromeDebloater.spec --clean
```

Output: `dist\ChromeDebloater.exe` — single portable file, no installer needed.

---

## Project Layout

```
ChromeDebloater/
├── src/
│   ├── main.py         # PySide6 UI + entry point
│   ├── browsers.py     # Browser profile definitions
│   └── hardener.py     # All tweak logic
├── ChromeDebloater.spec
├── build.ps1
└── README.md
```

---

## Disclaimer

These tweaks apply Windows enterprise registry policies. They are designed to be
non-destructive and re-runnable. Run at your own risk; test in a VM first if unsure.
