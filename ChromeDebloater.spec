# ChromeDebloater.spec - PyInstaller spec file for one-file portable exe
# Build with: pyinstaller ChromeDebloater.spec --clean

block_cipher = None

a = Analysis(
    ['src/main.py'],
    pathex=['.', 'src'],
    binaries=[],
    datas=[],
    hiddenimports=[
        'browsers',
        'hardener',
        'winreg',
        'ctypes',
        'sqlite3',
        'subprocess',
        'shutil',
        'json',
        'PySide6.QtWidgets',
        'PySide6.QtCore',
        'PySide6.QtGui',
    ],
    hookspath=[],
    hooksconfig={},
    runtime_hooks=[],
    excludes=[
        'torch', 'numpy', 'scipy', 'pandas', 'sklearn',
        'transformers', 'tokenizers', 'cv2', 'PIL',
        'matplotlib', 'IPython', 'notebook',
        'openai', 'anthropic',
    ],
    win_no_prefer_redirects=False,
    win_private_assemblies=False,
    cipher=block_cipher,
    noarchive=False,
)

pyz = PYZ(a.pure, a.zipped_data, cipher=block_cipher)

exe = EXE(
    pyz,
    a.scripts,
    a.binaries,
    a.zipfiles,
    a.datas,
    [],
    name='ChromeDebloater',
    debug=False,
    bootloader_ignore_signals=False,
    strip=False,
    upx=True,
    upx_exclude=[],
    runtime_tmpdir=None,
    console=False,          # No console window
    disable_windowed_traceback=False,
    argv_emulation=False,
    target_arch=None,
    codesign_identity=None,
    entitlements_file=None,
    uac_admin=True,         # Force UAC elevation prompt on launch
    icon=None,
    version=None,
    onefile=True,
)
