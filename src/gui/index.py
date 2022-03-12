# Tested on Python 3.7
from tabs.grammar import *
from model import *
import json
import sys
import os
import re
from collections import deque
from PySide6 import QtCore, QtWidgets, QtGui
from PySide6.QtCore import Qt
import tempfile
import signal
from tabs.attribute import *

# For faster debugging
signal.signal(signal.SIGINT, signal.SIG_DFL)

class LRParseTab(QtWidgets.QWidget):
    def __init__(self, opts, lrwindow) -> None:
        super().__init__()

        layout = QtWidgets.QVBoxLayout()
        label = QtWidgets.QLabel()
        label.setText('This is a label')
        layout.addWidget(label)
        self.setLayout(layout)


class LRWindow(QtWidgets.QTabWidget):
    def __init__(self) -> None:
        super().__init__()
        self.setTabPosition(QtWidgets.QTabWidget.North)
        self.setMovable(False)
        self.setWindowTitle('LR Parser')
        self.resize(800, 600)
        self._tab_classes = [GrammarEditorTab, AttributeTab, LRParseTab]
        self._tab_titles = ['Grammar Editor', 'Attributes', 'Parsing']
        self._tab_count = 0

        opts = LRParserOptions(tempfile.mkdtemp(), './build/lrparser.exe',
                               None)
        self._opts = opts
        
        self.nextButtonPressed(-1);

        # if __debug__:
        #     self.nextButtonPressed(0)

    # Called by subviews.
    def nextButtonPressed(self, tabIndex):
        # print('Next button pressed in tab {}'.format(tabIndex))
        nextTab = tabIndex + 1
        if nextTab < len(self._tab_classes):  # Valid tab index
            if nextTab >= self._tab_count:  # Need to create this one
                clazz = self._tab_classes[nextTab]
                self.addTab(clazz(self._opts, self), self._tab_titles[nextTab])
                self._tab_count += 1
            self.setCurrentIndex(nextTab)


class MainWindow(QtWidgets.QMainWindow):
    def __init__(self) -> None:
        super().__init__()
        self._prevent_gc = []

        lrButton = QtWidgets.QPushButton('LR Parsing')
        lrButton.setCheckable(False)
        lrButton.clicked.connect(self.startLR)

        dummyButton = QtWidgets.QPushButton('Dummy')
        dummyButton.setCheckable(False)
        dummyButton.setDisabled(True)

        layout = QtWidgets.QVBoxLayout()
        layout.addStretch(1)
        layout.addWidget(lrButton)
        layout.addWidget(dummyButton)
        layout.addStretch(1)

        widget = QtWidgets.QWidget()
        widget.setLayout(layout)
        self.setCentralWidget(widget)

    def startLR(self):
        window = LRWindow()
        window.show()
        self._prevent_gc.append(window)


if __name__ == "__main__":
    app = QtWidgets.QApplication([])

    window = MainWindow()
    window.show()

    sys.exit(app.exec())