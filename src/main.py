"""
main.py — ChromeDebloater main window (PySide6).
Run with administrator privileges (enforced via UAC manifest / elevation check).
"""
from __future__ import annotations
import sys
import os
import ctypes
import threading
from pathlib import Path

# ---------------------------------------------------------------------------
# Add src/ to path so imports work both from source and frozen bundle
# ---------------------------------------------------------------------------
_HERE = Path(__file__).resolve().parent
if str(_HERE) not in sys.path:
    sys.path.insert(0, str(_HERE))

from PySide6.QtWidgets import (
    QApplication, QMainWindow, QWidget, QVBoxLayout, QHBoxLayout,
    QLabel, QPushButton, QCheckBox, QGroupBox, QScrollArea,
    QComboBox, QTextEdit, QFrame, QSizePolicy, QProgressBar,
    QSplitter, QSpacerItem,
)
from PySide6.QtCore import Qt, QThread, Signal, QObject, QTimer
from PySide6.QtGui import QFont, QColor, QPalette, QIcon, QPixmap, QTextCursor

from browsers import BROWSERS, BrowserProfile
from hardener import TWEAKS, run_tweaks

# ──────────────────────────────────────────────────────────────────────────────
# UAC elevation
# ──────────────────────────────────────────────────────────────────────────────

def _is_admin() -> bool:
    try:
        return ctypes.windll.shell32.IsUserAnAdmin()
    except Exception:
        return False


def _elevate():
    """Re-launch self with UAC elevation."""
    exe = sys.executable
    params = " ".join([f'"{a}"' for a in sys.argv])
    ctypes.windll.shell32.ShellExecuteW(None, "runas", exe, params, None, 1)
    sys.exit(0)


# ──────────────────────────────────────────────────────────────────────────────
# Worker thread for running tweaks without freezing UI
# ──────────────────────────────────────────────────────────────────────────────

class HardenWorker(QObject):
    log_line = Signal(str, bool)    # (text, is_ok)
    progress = Signal(int)          # 0-100
    finished = Signal(bool)         # overall success

    def __init__(self, browser: BrowserProfile, tweak_ids: list[str]):
        super().__init__()
        self._browser = browser
        self._tweak_ids = tweak_ids

    def run(self):
        total = len(self._tweak_ids)
        done = [0]

        def on_progress(label: str, ok: bool, msg: str):
            done[0] += 1
            pct = int(done[0] / total * 100) if total else 100
            status = "✓" if ok else "✗"
            self.log_line.emit(f"  {status}  {label}\n     → {msg}", ok)
            self.progress.emit(pct)

        results = run_tweaks(self._browser, self._tweak_ids, on_progress)
        all_ok = all(r["ok"] for r in results)
        self.finished.emit(all_ok)


# ──────────────────────────────────────────────────────────────────────────────
# Stylesheet
# ──────────────────────────────────────────────────────────────────────────────

DARK_QSS = """
QMainWindow, QWidget#root {
    background: #0f1117;
}
QWidget {
    background: #0f1117;
    color: #e2e8f0;
    font-family: 'Segoe UI', 'Inter', sans-serif;
    font-size: 13px;
}
QGroupBox {
    border: 1px solid #2d3748;
    border-radius: 8px;
    margin-top: 14px;
    padding: 8px 10px;
    font-weight: 600;
    font-size: 12px;
    color: #94a3b8;
    letter-spacing: 0.5px;
    text-transform: uppercase;
}
QGroupBox::title {
    subcontrol-origin: margin;
    left: 10px;
    padding: 0 6px;
    background: #0f1117;
    color: #64748b;
}
QCheckBox {
    spacing: 8px;
    color: #cbd5e1;
    padding: 3px 0;
}
QCheckBox::indicator {
    width: 16px; height: 16px;
    border-radius: 4px;
    border: 1px solid #334155;
    background: #1e293b;
}
QCheckBox::indicator:checked {
    background: #3b82f6;
    border: 1px solid #3b82f6;
    image: none;
}
QCheckBox::indicator:hover { border-color: #60a5fa; }
QCheckBox:hover { color: #f1f5f9; }
QPushButton {
    background: #1e293b;
    color: #cbd5e1;
    border: 1px solid #334155;
    border-radius: 6px;
    padding: 8px 18px;
    font-weight: 500;
}
QPushButton:hover {
    background: #334155;
    color: #f1f5f9;
    border-color: #475569;
}
QPushButton#run_btn {
    background: qlineargradient(x1:0,y1:0,x2:1,y2:0,
        stop:0 #2563eb, stop:1 #7c3aed);
    color: #ffffff;
    border: none;
    border-radius: 8px;
    font-size: 14px;
    font-weight: 700;
    padding: 12px 28px;
    letter-spacing: 0.3px;
}
QPushButton#run_btn:hover {
    background: qlineargradient(x1:0,y1:0,x2:1,y2:0,
        stop:0 #1d4ed8, stop:1 #6d28d9);
    border: none;
}
QPushButton#run_btn:disabled {
    background: #1e293b;
    color: #475569;
    border: 1px solid #1e293b;
}
QComboBox {
    background: #1e293b;
    color: #e2e8f0;
    border: 1px solid #334155;
    border-radius: 6px;
    padding: 6px 10px;
    min-width: 200px;
}
QComboBox:hover { border-color: #475569; }
QComboBox::drop-down { border: none; width: 24px; }
QComboBox QAbstractItemView {
    background: #1e293b;
    color: #e2e8f0;
    border: 1px solid #334155;
    selection-background-color: #334155;
}
QTextEdit {
    background: #0a0d14;
    color: #94a3b8;
    border: 1px solid #1e293b;
    border-radius: 6px;
    font-family: 'Cascadia Code', 'Consolas', monospace;
    font-size: 12px;
    padding: 8px;
}
QScrollArea { border: none; }
QScrollBar:vertical {
    background: #0f1117;
    width: 8px;
    border-radius: 4px;
}
QScrollBar::handle:vertical {
    background: #334155;
    border-radius: 4px;
    min-height: 20px;
}
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
QProgressBar {
    background: #1e293b;
    border: none;
    border-radius: 4px;
    height: 6px;
    text-align: center;
    color: transparent;
}
QProgressBar::chunk {
    background: qlineargradient(x1:0,y1:0,x2:1,y2:0,
        stop:0 #2563eb, stop:1 #7c3aed);
    border-radius: 4px;
}
QSplitter::handle { background: #1e293b; width: 2px; }
QLabel#title {
    font-size: 22px;
    font-weight: 800;
    color: #f1f5f9;
    letter-spacing: -0.5px;
}
QLabel#subtitle {
    font-size: 12px;
    color: #475569;
}
QLabel#badge {
    background: #1e293b;
    color: #64748b;
    border-radius: 4px;
    padding: 2px 8px;
    font-size: 11px;
}
QLabel#section_lbl {
    font-size: 11px;
    font-weight: 600;
    color: #64748b;
    letter-spacing: 0.8px;
    text-transform: uppercase;
}
"""

# ──────────────────────────────────────────────────────────────────────────────
# Main Window
# ──────────────────────────────────────────────────────────────────────────────

class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("ChromeDebloater")
        self.setMinimumSize(860, 640)
        self.resize(960, 720)
        self._worker_thread: QThread | None = None
        self._checkboxes: dict[str, QCheckBox] = {}
        self._build_ui()

    # ── Build UI ──────────────────────────────────────────────────────────────

    def _build_ui(self):
        root = QWidget()
        root.setObjectName("root")
        self.setCentralWidget(root)
        layout = QVBoxLayout(root)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(0)

        # Header
        header = self._make_header()
        layout.addWidget(header)

        # Body split
        body = QSplitter(Qt.Horizontal)
        body.setHandleWidth(2)
        body.setChildrenCollapsible(False)

        # Left: options panel
        left = self._make_left_panel()
        body.addWidget(left)

        # Right: log panel
        right = self._make_right_panel()
        body.addWidget(right)

        body.setSizes([480, 400])
        layout.addWidget(body, 1)

        # Footer
        footer = self._make_footer()
        layout.addWidget(footer)

    def _make_header(self) -> QWidget:
        w = QWidget()
        w.setStyleSheet("background: #0a0d14; border-bottom: 1px solid #1e293b;")
        h = QHBoxLayout(w)
        h.setContentsMargins(24, 18, 24, 18)

        # Icon placeholder (colored box)
        icon_lbl = QLabel("🛡")
        icon_lbl.setStyleSheet("font-size: 32px; background: transparent;")
        h.addWidget(icon_lbl)

        h.addSpacing(12)

        col = QVBoxLayout()
        col.setSpacing(2)
        title = QLabel("ChromeDebloater")
        title.setObjectName("title")
        sub = QLabel("Harden & optimize Chromium browsers — Chrome · Brave · Edge")
        sub.setObjectName("subtitle")
        col.addWidget(title)
        col.addWidget(sub)
        h.addLayout(col)

        h.addStretch()

        admin_badge = QLabel("⚡ ADMIN" if _is_admin() else "⚠ NO ADMIN")
        admin_badge.setObjectName("badge")
        admin_badge.setStyleSheet(
            "background:#14532d; color:#4ade80; border-radius:4px; padding:3px 10px; font-size:11px; font-weight:700;"
            if _is_admin() else
            "background:#7f1d1d; color:#f87171; border-radius:4px; padding:3px 10px; font-size:11px; font-weight:700;"
        )
        h.addWidget(admin_badge)
        return w

    def _make_left_panel(self) -> QWidget:
        w = QWidget()
        w.setStyleSheet("background: #0f1117;")
        layout = QVBoxLayout(w)
        layout.setContentsMargins(20, 16, 12, 16)
        layout.setSpacing(12)

        # Browser picker
        brow_row = QHBoxLayout()
        lbl = QLabel("BROWSER")
        lbl.setObjectName("section_lbl")
        brow_row.addWidget(lbl)
        brow_row.addStretch()
        self._browser_combo = QComboBox()
        for b in BROWSERS:
            suffix = "  ✓" if b.installed else "  (not installed)"
            self._browser_combo.addItem(f"{b.name}{suffix}", userData=b.key)
            if not b.installed:
                idx = self._browser_combo.count() - 1
                self._browser_combo.model().item(idx).setEnabled(False)
        brow_row.addWidget(self._browser_combo)
        layout.addLayout(brow_row)

        # Select all / none row
        sel_row = QHBoxLayout()
        sel_lbl = QLabel("TWEAKS")
        sel_lbl.setObjectName("section_lbl")
        sel_row.addWidget(sel_lbl)
        sel_row.addStretch()
        all_btn = QPushButton("All")
        all_btn.setFixedWidth(52)
        all_btn.clicked.connect(lambda: self._select_all(True))
        none_btn = QPushButton("None")
        none_btn.setFixedWidth(52)
        none_btn.clicked.connect(lambda: self._select_all(False))
        sel_row.addWidget(all_btn)
        sel_row.addWidget(none_btn)
        layout.addLayout(sel_row)

        # Scrollable checkbox list grouped by category
        scroll = QScrollArea()
        scroll.setWidgetResizable(True)
        scroll.setHorizontalScrollBarPolicy(Qt.ScrollBarAlwaysOff)
        inner = QWidget()
        inner_layout = QVBoxLayout(inner)
        inner_layout.setContentsMargins(0, 0, 8, 0)
        inner_layout.setSpacing(10)

        categories: dict[str, list] = {}
        for tw in TWEAKS:
            categories.setdefault(tw["category"], []).append(tw)

        CAT_ICONS = {
            "AI Removal": "🤖",
            "Privacy": "🔒",
            "Performance": "⚡",
            "Debloat": "🧹",
            "Maintenance": "🔧",
            "Update Control": "📌",
        }

        for cat, tweaks in categories.items():
            icon = CAT_ICONS.get(cat, "•")
            grp = QGroupBox(f"{icon}  {cat}")
            grp_layout = QVBoxLayout(grp)
            grp_layout.setSpacing(2)
            grp_layout.setContentsMargins(8, 4, 8, 8)
            for tw in tweaks:
                cb = QCheckBox(tw["label"])
                cb.setChecked(True)
                self._checkboxes[tw["id"]] = cb
                grp_layout.addWidget(cb)
            inner_layout.addWidget(grp)

        inner_layout.addStretch()
        scroll.setWidget(inner)
        layout.addWidget(scroll, 1)
        return w

    def _make_right_panel(self) -> QWidget:
        w = QWidget()
        w.setStyleSheet("background: #0a0d14; border-left: 1px solid #1e293b;")
        layout = QVBoxLayout(w)
        layout.setContentsMargins(16, 16, 20, 16)
        layout.setSpacing(8)

        lbl = QLabel("OUTPUT")
        lbl.setObjectName("section_lbl")
        layout.addWidget(lbl)

        self._log = QTextEdit()
        self._log.setReadOnly(True)
        self._log.setPlaceholderText("Run tweaks to see output here…")
        layout.addWidget(self._log, 1)

        self._progress = QProgressBar()
        self._progress.setRange(0, 100)
        self._progress.setValue(0)
        self._progress.setFixedHeight(6)
        layout.addWidget(self._progress)

        clr_btn = QPushButton("Clear Log")
        clr_btn.setFixedWidth(90)
        clr_btn.clicked.connect(lambda: (self._log.clear(), self._progress.setValue(0)))
        clr_row = QHBoxLayout()
        clr_row.addStretch()
        clr_row.addWidget(clr_btn)
        layout.addLayout(clr_row)
        return w

    def _make_footer(self) -> QWidget:
        w = QWidget()
        w.setStyleSheet("background: #0a0d14; border-top: 1px solid #1e293b;")
        h = QHBoxLayout(w)
        h.setContentsMargins(24, 14, 24, 14)

        self._status_lbl = QLabel("Ready — select tweaks and click Run")
        self._status_lbl.setStyleSheet("color: #475569; font-size: 12px;")
        h.addWidget(self._status_lbl)
        h.addStretch()

        self._run_btn = QPushButton("▶  Run Selected Tweaks")
        self._run_btn.setObjectName("run_btn")
        self._run_btn.setFixedHeight(44)
        self._run_btn.clicked.connect(self._on_run)
        h.addWidget(self._run_btn)
        return w

    # ── Actions ───────────────────────────────────────────────────────────────

    def _select_all(self, checked: bool):
        for cb in self._checkboxes.values():
            cb.setChecked(checked)

    def _on_run(self):
        if not _is_admin():
            self._log_append("✗  Not running as administrator — please re-launch as admin.", ok=False)
            return

        selected = [tid for tid, cb in self._checkboxes.items() if cb.isChecked()]
        if not selected:
            self._log_append("⚠  No tweaks selected.", ok=True)
            return

        key = self._browser_combo.currentData()
        browser = next((b for b in BROWSERS if b.key == key), None)
        if browser is None:
            self._log_append("✗  Unknown browser selected.", ok=False)
            return

        self._run_btn.setEnabled(False)
        self._progress.setValue(0)
        self._status_lbl.setText(f"Applying {len(selected)} tweak(s) to {browser.name}…")
        self._log_append(
            f"\n── Running {len(selected)} tweaks on {browser.name} ──\n", ok=True
        )

        # Run in background thread
        worker = HardenWorker(browser, selected)
        thread = QThread()
        worker.moveToThread(thread)
        thread.started.connect(worker.run)
        worker.log_line.connect(self._log_append)
        worker.progress.connect(self._progress.setValue)
        worker.finished.connect(lambda ok: self._on_done(ok, thread))
        thread.start()
        self._worker_thread = thread

    def _on_done(self, all_ok: bool, thread: QThread):
        thread.quit()
        thread.wait()
        self._run_btn.setEnabled(True)
        msg = "All tweaks completed successfully." if all_ok else "Completed with some errors."
        self._status_lbl.setText(msg)
        banner = f"\n{'✓  ' + msg if all_ok else '✗  ' + msg}\n"
        self._log_append(banner, ok=all_ok)

    def _log_append(self, text: str, ok: bool = True):
        color = "#4ade80" if ok else "#f87171"
        # Escape HTML
        safe = text.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")
        safe = safe.replace("\n", "<br>").replace(" ", "&nbsp;")
        self._log.append(
            f'<span style="color:{color}; font-family:Consolas,monospace;">{safe}</span>'
        )
        self._log.moveCursor(QTextCursor.End)


# ──────────────────────────────────────────────────────────────────────────────
# Entry point
# ──────────────────────────────────────────────────────────────────────────────

def main():
    if not _is_admin():
        _elevate()

    app = QApplication(sys.argv)
    app.setApplicationName("ChromeDebloater")
    app.setOrganizationName("ChromeDebloater")
    app.setStyleSheet(DARK_QSS)

    # High-DPI
    app.setAttribute(Qt.AA_UseHighDpiPixmaps, True)

    win = MainWindow()
    win.show()
    sys.exit(app.exec())


if __name__ == "__main__":
    main()
