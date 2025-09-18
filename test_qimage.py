from PySide6 import QtGui
from pathlib import Path
p = Path(r"C:\Windows\Web\Wallpaper\Windows\img0.jpg")
if p.exists():
    b = p.read_bytes()
    img = QtGui.QImage.fromData(b)
    pix = QtGui.QPixmap.fromImage(img)
    print(type(img), img.size(), type(pix))
else:
    print('no image')
