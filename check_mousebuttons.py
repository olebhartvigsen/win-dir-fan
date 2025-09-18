from PySide6 import QtGui, QtCore, QtWidgets
import sys
app = QtWidgets.QApplication(sys.argv)
b = QtGui.QGuiApplication.mouseButtons()
print('type', type(b), 'repr', repr(b))
try:
    print('int(b)=', int(b))
except Exception as e:
    print('int failed', e)
print('Qt.LeftButton value', QtCore.Qt.LeftButton, type(QtCore.Qt.LeftButton))
print('int(Qt.LeftButton)=', int(QtCore.Qt.LeftButton))
