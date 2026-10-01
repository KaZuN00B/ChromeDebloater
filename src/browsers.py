"""
browsers.py — Browser profile discovery for Chrome, Brave, Edge.
"""
from __future__ import annotations
import os
from pathlib import Path
from dataclasses import dataclass, field


@dataclass
class BrowserProfile:
    name: str                # Display name, e.g. "Google Chrome"
    key: str                 # Internal key: "chrome" | "brave" | "edge"
    policy_path: str         # Registry sub-key under HKLM\SOFTWARE\Policies\...
    update_policy_path: str  # Registry sub-key for update policy
    user_data_dir: Path      # Path to User Data folder
    executable: Path | None  # Path to browser binary
    app_guid: str = ""       # Google Update GUID (Chrome only)
    installed: bool = field(init=False, default=False)

    def __post_init__(self):
        self.installed = self.executable is not None and self.executable.exists()


def _env(var: str, *parts: str) -> Path:
    base = os.environ.get(var, "")
    return Path(base, *parts) if base else Path(*parts)


BROWSERS: list[BrowserProfile] = [
    BrowserProfile(
        name="Google Chrome",
        key="chrome",
        policy_path=r"SOFTWARE\Policies\Google\Chrome",
        update_policy_path=r"SOFTWARE\Policies\Google\Update",
        user_data_dir=_env("LOCALAPPDATA", "Google", "Chrome", "User Data"),
        executable=Path(r"C:\Program Files\Google\Chrome\Application\chrome.exe")
        if Path(r"C:\Program Files\Google\Chrome\Application\chrome.exe").exists()
        else Path(r"C:\Program Files (x86)\Google\Chrome\Application\chrome.exe"),
        app_guid="{8A69D345-D564-463C-AFF1-A69D9E530F96}",
    ),
    BrowserProfile(
        name="Brave Browser",
        key="brave",
        policy_path=r"SOFTWARE\Policies\BraveSoftware\Brave",
        update_policy_path=r"SOFTWARE\Policies\BraveSoftware\Update",
        user_data_dir=_env("LOCALAPPDATA", "BraveSoftware", "Brave-Browser", "User Data"),
        executable=Path(
            r"C:\Program Files\BraveSoftware\Brave-Browser\Application\brave.exe"
        ),
    ),
    BrowserProfile(
        name="Microsoft Edge",
        key="edge",
        policy_path=r"SOFTWARE\Policies\Microsoft\Edge",
        update_policy_path=r"SOFTWARE\Policies\Microsoft\EdgeUpdate",
        user_data_dir=_env("LOCALAPPDATA", "Microsoft", "Edge", "User Data"),
        executable=Path(
            r"C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe"
        ),
    ),
]


def get_browser(key: str) -> BrowserProfile | None:
    return next((b for b in BROWSERS if b.key == key), None)
