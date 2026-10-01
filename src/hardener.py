"""
hardener.py — All hardening, privacy, performance, and debloat logic.
Each tweak is a small function that accepts a BrowserProfile and applies
idempotent registry writes or file operations.  Re-running is always safe.
"""
from __future__ import annotations
import os
import subprocess
import sqlite3
import shutil
import json
from pathlib import Path
from typing import Callable, TYPE_CHECKING

import winreg

if TYPE_CHECKING:
    from browsers import BrowserProfile


# ──────────────────────────────────────────────────────────────────────────────
# Registry helpers
# ──────────────────────────────────────────────────────────────────────────────

def _open_key(path: str, write: bool = True):
    """Open (and create if needed) an HKLM registry key."""
    access = winreg.KEY_WRITE | winreg.KEY_READ if write else winreg.KEY_READ
    return winreg.CreateKeyEx(winreg.HKEY_LOCAL_MACHINE, path, 0,
                              access | winreg.KEY_WOW64_64KEY)


def _set_dword(path: str, name: str, value: int):
    with _open_key(path) as k:
        winreg.SetValueEx(k, name, 0, winreg.REG_DWORD, value)


def _set_str(path: str, name: str, value: str):
    with _open_key(path) as k:
        winreg.SetValueEx(k, name, 0, winreg.REG_SZ, value)


def _set_list(path: str, name: str, values: list[str]):
    """Store a list as numbered sub-values (Chrome-style allowlist)."""
    sub = f"{path}\\{name}"
    with _open_key(sub) as k:
        for i, v in enumerate(values, start=1):
            winreg.SetValueEx(k, str(i), 0, winreg.REG_SZ, v)


def _delete_value_safe(path: str, name: str):
    try:
        with _open_key(path) as k:
            winreg.DeleteValue(k, name)
    except (FileNotFoundError, OSError):
        pass


# ──────────────────────────────────────────────────────────────────────────────
# Individual tweak functions
# Return (ok: bool, message: str)
# ──────────────────────────────────────────────────────────────────────────────

def tweak_disable_ai(b: "BrowserProfile") -> tuple[bool, str]:
    """Kill all AI / Gemini subsystems via enterprise policy."""
    p = b.policy_path
    try:
        _set_dword(p, "GenAiDefaultSettings", 2)
        _set_dword(p, "GeminiSettings", 1)
        _set_dword(p, "GeminiSparkSettings", 1)
        _set_dword(p, "GeminiActOnWebSettings", 1)
        _set_dword(p, "AIModeSettings", 1)
        _set_dword(p, "OptimizationGuideAllowed", 0)
        _set_dword(p, "OptimizationGuideFetchingEnabled", 0)
        _set_dword(p, "GenAILocalFoundationalModelSettings", 1)
        _set_dword(p, "BuiltInAIAPIsEnabled", 0)
        _set_dword(p, "PromptAPIAllowed", 2)
        _set_dword(p, "HelpMeWriteSettings", 2)
        _set_dword(p, "HelpMeReadSettings", 2)
        _set_dword(p, "HistorySearchSettings", 2)
        _set_dword(p, "TabOrganizerSettings", 2)
        _set_dword(p, "TabCompareSettings", 2)
        _set_dword(p, "CreateThemesSettings", 2)
        _set_dword(p, "DevToolsGenAiSettings", 2)
        _set_dword(p, "AutofillPredictionSettings", 2)
        _set_dword(p, "ChromeSuggestionsSettings", 2)
        _set_dword(p, "FindsSettings", 2)
        return True, "AI/Gemini subsystems disabled"
    except Exception as e:
        return False, f"AI disable error: {e}"


def tweak_delete_ai_models(b: "BrowserProfile") -> tuple[bool, str]:
    """Delete on-disk AI model files from User Data."""
    targets = [
        b.user_data_dir / "OnDeviceHeadSuggestModel",
        b.user_data_dir / "optimization_guide_model_store",
        b.user_data_dir / "OptimizationGuideModelsManifest",
        b.user_data_dir / "OptimizationHints",
        b.user_data_dir / "Default" / "AutofillAiModelCache",
    ]
    removed = 0
    for t in targets:
        if t.exists():
            try:
                if t.is_dir():
                    shutil.rmtree(t, ignore_errors=True)
                else:
                    t.unlink(missing_ok=True)
                removed += 1
            except Exception:
                pass
    return True, f"Removed {removed} AI model path(s)"


def tweak_disable_telemetry(b: "BrowserProfile") -> tuple[bool, str]:
    """Disable all telemetry, crash reporting, and feedback mechanisms."""
    p = b.policy_path
    try:
        _set_dword(p, "MetricsReportingEnabled", 0)
        _set_dword(p, "SafeBrowsingExtendedReportingEnabled", 0)
        _set_dword(p, "SpellCheckServiceEnabled", 0)
        _set_dword(p, "ChromeCleanupEnabled", 0)
        _set_dword(p, "ChromeCleanupReportingEnabled", 0)
        _set_dword(p, "UserFeedbackAllowed", 0)
        _set_dword(p, "ReportingEnabled", 0)
        _set_dword(p, "EnableMediaRouter", 0)
        _set_dword(p, "ShowCastIconInToolbar", 0)
        _set_dword(p, "CloudReportingEnabled", 0)
        _set_dword(p, "CloudProfileReportingEnabled", 0)
        _set_dword(p, "HeartbeatEnabled", 0)
        _set_dword(p, "SafeBrowsingEnabled", 0)
        return True, "Telemetry & reporting disabled"
    except Exception as e:
        return False, f"Telemetry error: {e}"


def tweak_privacy_hardening(b: "BrowserProfile") -> tuple[bool, str]:
    """Apply privacy-hardening policies (fingerprinting, tracking, cookies)."""
    p = b.policy_path
    try:
        # Block 3rd-party cookies in an allowlist (not blanket) — done via CookiesBlockedForUrls
        _set_dword(p, "BlockThirdPartyCookies", 0)   # allow but control at cookie level
        _set_dword(p, "DefaultCookiesSetting", 4)     # Session-only unless allowlisted
        _set_dword(p, "DefaultPopupsSetting", 2)       # Block popups
        _set_dword(p, "DefaultNotificationsSetting", 2) # Block notifications
        _set_dword(p, "DefaultGeolocationSetting", 2)  # Block geolocation
        _set_dword(p, "DefaultWebBluetoothGuardSetting", 2)
        _set_dword(p, "DefaultWebUsbGuardSetting", 2)
        _set_dword(p, "DefaultFileSystemReadGuardSetting", 2)
        _set_dword(p, "DefaultFileSystemWriteGuardSetting", 2)
        _set_dword(p, "DefaultSensorsSetting", 2)
        _set_dword(p, "DefaultSerialGuardSetting", 2)
        _set_dword(p, "DefaultWindowManagementSetting", 2)
        _set_dword(p, "DefaultLocalFontsAllowedForUrls", 2)
        _set_dword(p, "InsecurePrivateNetworkRequestsAllowed", 0)
        _set_dword(p, "DefaultInsecureContentSetting", 2)
        _set_dword(p, "ReduceAcceptLanguageEnabled", 1)
        _set_dword(p, "PostQuantumKeyAgreementEnabled", 1)
        _set_str(p, "SSLVersionMin", "tls1.2")
        _set_str(p, "HttpsOnlyMode", "force_enabled")
        _set_dword(p, "HSTSPinningBypassAllowed", 0)
        _set_dword(p, "WebRtcIPHandling", 2)   # Disable non-proxied UDP
        _set_dword(p, "RendererCodeIntegrityEnabled", 1)
        _set_dword(p, "ThirdPartyBlockingEnabled", 1)
        # DoH (Quad9) — Chrome only; Brave/Edge accept same policy name
        _set_str(p, "DnsOverHttpsMode", "automatic")
        _set_str(p, "DnsOverHttpsTemplates", "https://dns.quad9.net/dns-query")
        return True, "Privacy hardening applied"
    except Exception as e:
        return False, f"Privacy error: {e}"


def tweak_allowlist_google_auth(b: "BrowserProfile") -> tuple[bool, str]:
    """Allow Google auth cookies so Sync & Password Manager keep working (Chrome only)."""
    if b.key != "chrome":
        return True, "Skipped (not Chrome)"
    p = b.policy_path
    try:
        _set_list(p, "CookiesAllowedForUrls", [
            "[*.]google.com",
            "https://accounts.google.com",
            "[*.]googleusercontent.com",
            "[*.]gstatic.com",
            "[*.]apis.google.com",
            "[*.]passkeys.google.com",
            "https://myaccount.google.com",
        ])
        _set_dword(p, "BrowserSignin", 1)
        _set_dword(p, "BrowserAddPersonEnabled", 1)
        _set_dword(p, "PasswordManagerEnabled", 1)
        _set_dword(p, "PasswordLeakDetectionEnabled", 1)
        return True, "Google Sync & Password Manager allowlisted"
    except Exception as e:
        return False, f"Auth allowlist error: {e}"


def tweak_performance(b: "BrowserProfile") -> tuple[bool, str]:
    """Apply speed and low-resource performance policies."""
    p = b.policy_path
    try:
        _set_dword(p, "SitePerProcess", 0)                 # Collapse iframes → less RAM
        _set_dword(p, "HighEfficiencyModeEnabled", 1)       # Tab memory saver on
        _set_dword(p, "MemorySaverModeSavings", 2)          # Maximum aggressiveness
        _set_dword(p, "ThrottleJavaScriptTimers", 1)
        _set_dword(p, "IntensiveWakeUpThrottlingEnabled", 1)
        _set_dword(p, "HardwareAccelerationModeEnabled", 1)  # GPU offload
        _set_dword(p, "BuiltInDnsClientEnabled", 1)
        _set_dword(p, "DiskCacheSize", 268435456)            # 256 MB cache cap
        _set_dword(p, "TabHoverCards", 0)
        _set_dword(p, "BackgroundModeEnabled", 0)            # No background wake
        _set_dword(p, "NetworkPredictionOptions", 2)         # No preconnect
        _set_dword(p, "EnableMediaRouter", 0)
        return True, "Performance optimizations applied"
    except Exception as e:
        return False, f"Performance error: {e}"


def tweak_debloat_ui(b: "BrowserProfile") -> tuple[bool, str]:
    """Remove Cast, Sharing Hub, Clipboard, Web App install prompts from UI."""
    p = b.policy_path
    try:
        _set_dword(p, "ShowCastIconInToolbar", 0)
        _set_dword(p, "DesktopSharingHubEnabled", 0)
        _set_dword(p, "SharedClipboardEnabled", 0)
        _set_dword(p, "WebAppInstallByUserEnabled", 0)
        _set_dword(p, "UserFeedbackAllowed", 0)
        _set_dword(p, "AutofillAddressEnabled", 1)     # Keep address autofill
        _set_dword(p, "AutofillCreditCardEnabled", 0)  # Block CC autofill
        _set_dword(p, "TranslateEnabled", 0)           # Disable auto-translate nag
        _set_dword(p, "PrintingEnabled", 1)
        return True, "UI debloat applied (Cast/Share removed)"
    except Exception as e:
        return False, f"UI debloat error: {e}"


def tweak_search_engine(b: "BrowserProfile") -> tuple[bool, str]:
    """Set Brave Search as default search engine."""
    p = b.policy_path
    try:
        _set_dword(p, "DefaultSearchProviderEnabled", 1)
        _set_str(p, "DefaultSearchProviderName", "Brave Search")
        _set_str(p, "DefaultSearchProviderSearchURL",
                  "https://search.brave.com/search?q={searchTerms}")
        _set_str(p, "DefaultSearchProviderSuggestURL",
                  "https://search.brave.com/api/suggest?q={searchTerms}")
        return True, "Default search → Brave Search"
    except Exception as e:
        return False, f"Search engine error: {e}"


def tweak_flags(b: "BrowserProfile") -> tuple[bool, str]:
    """Inject performance chrome://flags into Local State (best-effort)."""
    local_state = b.user_data_dir / "Local State"
    if not local_state.exists():
        return True, "Local State not found — flags skipped (browser never launched?)"

    flags = {
        "enable-quic": "1",
        "back-forward-cache": "1",
        "smooth-scrolling": "1",
        "enable-scroll-prediction": "1",
        "canvas-oop-rasterization": "1",
        "enable-gpu-rasterization": "1",
        "enable-zero-copy": "1",
        "enable-parallel-downloading": "1",
    }
    try:
        raw = local_state.read_text(encoding="utf-8")
        data = json.loads(raw)
        browser_flags = data.setdefault("browser", {}).setdefault(
            "enabled_labs_experiments", []
        )
        existing = {f.split("@")[0] for f in browser_flags}
        added = 0
        for name, val in flags.items():
            if name not in existing:
                browser_flags.append(f"{name}@{val}")
                added += 1
        local_state.write_text(json.dumps(data, separators=(",", ":")), encoding="utf-8")
        return True, f"Injected {added} performance flags into Local State"
    except Exception as e:
        return False, f"Flags injection error: {e}"


def tweak_sqlite_vacuum(b: "BrowserProfile") -> tuple[bool, str]:
    """Vacuum/REINDEX SQLite profile databases to reclaim space and speed reads."""
    dbs = ["History", "Favicons", "Web Data", "Top Sites", "Shortcuts",
           "Network Action Predictor"]
    profile_dir = b.user_data_dir / "Default"
    total_saved = 0
    for db in dbs:
        path = profile_dir / db
        if not path.exists():
            continue
        before = path.stat().st_size
        try:
            con = sqlite3.connect(str(path))
            con.execute("VACUUM")
            con.execute("REINDEX")
            con.close()
            after = path.stat().st_size
            total_saved += max(0, before - after)
        except Exception:
            pass
    return True, f"SQLite vacuumed — {total_saved // 1024} KB reclaimed"


def tweak_purge_caches(b: "BrowserProfile") -> tuple[bool, str]:
    """Purge GPU/shader/crashpad caches from User Data."""
    caches = [
        b.user_data_dir / "Default" / "GPUCache",
        b.user_data_dir / "Default" / "DawnCache",
        b.user_data_dir / "GrShaderCache",
        b.user_data_dir / "ShaderCache",
        b.user_data_dir / "Crashpad",
    ]
    total = 0
    for c in caches:
        if c.exists():
            for f in c.rglob("*"):
                if f.is_file():
                    try:
                        f.unlink()
                        total += 1
                    except Exception:
                        pass
    return True, f"Purged {total} shader/GPU/crashpad cache files"


def tweak_flush_dns(_b: "BrowserProfile") -> tuple[bool, str]:
    """Flush Windows DNS resolver cache."""
    try:
        subprocess.run(["ipconfig", "/flushdns"], capture_output=True, check=True)
        return True, "Windows DNS cache flushed"
    except Exception as e:
        return False, f"DNS flush error: {e}"


def tweak_lock_updates_chrome(b: "BrowserProfile") -> tuple[bool, str]:
    """Freeze Chrome at its current version (Chrome only)."""
    if b.key != "chrome":
        return True, "Update freeze skipped (not Chrome)"
    up = b.update_policy_path
    try:
        _set_dword(up, "UpdateDefault", 0)
        _set_dword(up, "AutoUpdateCheckPeriodMinutes", 0)
        _set_dword(up, "DisableAutoUpdateChecksCheckboxValue", 1)
        _set_dword(up, "InstallDefault", 0)
        guid = b.app_guid
        _set_dword(up, f"Update{guid}", 0)
        _set_dword(up, f"Install{guid}", 0)
        # Detect current version
        exe = Path(r"C:\Program Files\Google\Chrome\Application\chrome.exe")
        if not exe.exists():
            exe = Path(r"C:\Program Files (x86)\Google\Chrome\Application\chrome.exe")
        if exe.exists():
            ver_cmd = subprocess.run(
                ["powershell", "-Command",
                 f"(Get-Item '{exe}').VersionInfo.ProductVersion"],
                capture_output=True, text=True
            )
            ver = ver_cmd.stdout.strip()
            if ver:
                _set_str(up, f"TargetVersionPrefix{guid}", ver)
        return True, "Chrome update freeze applied"
    except Exception as e:
        return False, f"Update lock error: {e}"


# ──────────────────────────────────────────────────────────────────────────────
# Master tweak list  — (id, label, function, applies_to)
# applies_to: None = all browsers, or list of keys
# ──────────────────────────────────────────────────────────────────────────────

TWEAKS: list[dict] = [
    {
        "id": "disable_ai",
        "label": "Disable AI / Gemini integrations",
        "category": "AI Removal",
        "fn": tweak_disable_ai,
        "applies_to": None,
    },
    {
        "id": "delete_ai_models",
        "label": "Delete on-disk AI model files",
        "category": "AI Removal",
        "fn": tweak_delete_ai_models,
        "applies_to": None,
    },
    {
        "id": "disable_telemetry",
        "label": "Disable telemetry & crash reporting",
        "category": "Privacy",
        "fn": tweak_disable_telemetry,
        "applies_to": None,
    },
    {
        "id": "privacy_hardening",
        "label": "Privacy hardening (DoH, HTTPS-only, WebRTC, TLS)",
        "category": "Privacy",
        "fn": tweak_privacy_hardening,
        "applies_to": None,
    },
    {
        "id": "search_engine",
        "label": "Set default search engine → Brave Search",
        "category": "Privacy",
        "fn": tweak_search_engine,
        "applies_to": None,
    },
    {
        "id": "allowlist_google_auth",
        "label": "Allowlist Google auth (keeps Sync & Passwords working)",
        "category": "Privacy",
        "fn": tweak_allowlist_google_auth,
        "applies_to": ["chrome"],
    },
    {
        "id": "performance",
        "label": "Performance policies (memory saver, GPU, throttle)",
        "category": "Performance",
        "fn": tweak_performance,
        "applies_to": None,
    },
    {
        "id": "flags",
        "label": "Inject performance flags (QUIC, parallel download, GPU raster)",
        "category": "Performance",
        "fn": tweak_flags,
        "applies_to": None,
    },
    {
        "id": "debloat_ui",
        "label": "Remove Cast, Share, Feedback, Web App prompts",
        "category": "Debloat",
        "fn": tweak_debloat_ui,
        "applies_to": None,
    },
    {
        "id": "sqlite_vacuum",
        "label": "Vacuum & reindex profile databases",
        "category": "Maintenance",
        "fn": tweak_sqlite_vacuum,
        "applies_to": None,
    },
    {
        "id": "purge_caches",
        "label": "Purge GPU / shader / crashpad caches",
        "category": "Maintenance",
        "fn": tweak_purge_caches,
        "applies_to": None,
    },
    {
        "id": "flush_dns",
        "label": "Flush Windows DNS resolver cache",
        "category": "Maintenance",
        "fn": tweak_flush_dns,
        "applies_to": None,
    },
    {
        "id": "lock_updates_chrome",
        "label": "Freeze Chrome at current version (no auto-updates)",
        "category": "Update Control",
        "fn": tweak_lock_updates_chrome,
        "applies_to": ["chrome"],
    },
]


def run_tweaks(
    browser: "BrowserProfile",
    tweak_ids: list[str],
    progress_cb: Callable[[str, bool, str], None] | None = None,
) -> list[dict]:
    """
    Execute selected tweaks against a browser.
    progress_cb(label, ok, message) is called after each tweak.
    Returns list of result dicts.
    """
    results = []
    for tw in TWEAKS:
        if tw["id"] not in tweak_ids:
            continue
        if tw["applies_to"] and browser.key not in tw["applies_to"]:
            msg = f"Skipped (not applicable to {browser.name})"
            if progress_cb:
                progress_cb(tw["label"], True, msg)
            results.append({"id": tw["id"], "label": tw["label"],
                            "ok": True, "message": msg})
            continue
        ok, msg = tw["fn"](browser)
        if progress_cb:
            progress_cb(tw["label"], ok, msg)
        results.append({"id": tw["id"], "label": tw["label"],
                        "ok": ok, "message": msg})
    return results
