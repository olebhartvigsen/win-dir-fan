# PyInstaller spec for windows-dir-fan (onedir)
# Build: pyinstaller --clean --onedir pyinstaller.spec

block_cipher = None

from pathlib import Path
icon_datas = []
if Path('app icon.png').exists():
    icon_datas.append(('app icon.png', '.'))
if Path('app.ico').exists():
    icon_datas.append(('app.ico', '.'))

a = Analysis([
    'app.py',
], pathex=['.'],
             binaries=[],
             datas=icon_datas,
             hiddenimports=[],
             hookspath=[],
             runtime_hooks=[],
             excludes=[],
             win_no_prefer_redirects=False,
             win_private_assemblies=False,
             cipher=block_cipher)

pyz = PYZ(a.pure, a.zipped_data, cipher=block_cipher)

exe = EXE(pyz,
          a.scripts,
          [],
          exclude_binaries=True,
          name='windows-dir-fan',
          debug=False,
          bootloader_ignore_signals=False,
          strip=False,
          upx=True,
          console=False,
          icon='app.ico' if Path('app.ico').exists() else None )

coll = COLLECT(exe,
               a.binaries,
               a.zipfiles,
               a.datas,
               strip=False,
               upx=True,
               name='windows-dir-fan')
