from PySide6 import QtGui, QtWidgets
from pathlib import Path
import sys
app = QtWidgets.QApplication(sys.argv)
from PySide6 import QtGui
p = Path(r"C:\Windows\Web\Wallpaper\Windows\img0.jpg")
if p.exists():
    b = p.read_bytes()
    img = QtGui.QImage.fromData(b)
    pix = QtGui.QPixmap.fromImage(img)
    print(type(img), img.size(), type(pix))
else:
    print('no image')
