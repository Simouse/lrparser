# Tested on Python 3.7
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
from tabs.grammar import *
from model import *

# For faster debugging
signal.signal(signal.SIGINT, signal.SIG_DFL)

class LRParseTab(QtWidgets.QWidget):
    def __init__(self, tag, opts, lrwindow) -> None:
        super().__init__()

        layout = QtWidgets.QVBoxLayout()
        label = QtWidgets.QLabel()
        label.setText('This is a label')
        layout.addWidget(label)
        self.setLayout(layout)


class ParserWindow(QtWidgets.QTabWidget):
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

        self.switchTabByIndex(0)

    # Called by subviews.
    def switchTabByIndex(self, index):
        if index < len(self._tab_classes):  # Valid tab index
            if index >= self._tab_count:  # Need to create this one
                clazz = self._tab_classes[index]
                self.addTab(clazz(self._tab_titles[index], self._opts, self),
                            self._tab_titles[index])
                self._tab_count += 1
            self.setCurrentIndex(index)

    def requestNext(self, currentTag):
        if currentTag in self._tab_titles:
            index = self._tab_titles.index(currentTag)
            self.switchTabByIndex(index + 1)
        else:
            raise Exception(
                'This is a programming error. Check argument `currentTag`.')


class MainWindow(QtWidgets.QMainWindow):
    def __init__(self) -> None:
        super().__init__()
        self._prevent_gc = []
        self.setWindowTitle('Home')

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
        window = ParserWindow()
        window.show()
        self._prevent_gc.append(window)

    def CloseEvent(self, event):
        print("Close event: {}".format(event))


if __name__ == "__main__":
    app = QtWidgets.QApplication([])

    window = MainWindow()
    window.show()

    sys.exit(app.exec())