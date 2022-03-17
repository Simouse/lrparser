# Tested on Python 3.7
import sys
from typing import Set
from PySide6 import QtCore, QtWidgets, QtGui
from PySide6.QtCore import Qt
import tempfile
import signal
from Attribute import *
from ParseTable import *
from Model import *
from Editor import *

# For faster debugging
signal.signal(signal.SIGINT, signal.SIG_DFL)

# opts = LRParserOptions(tempfile.mkdtemp(), './build/lrparser', grammar='grammar.txt')
# # code = open('out/steps.py', encoding='utf-8').read()
# # exec(code, opts.env)
# app = QtWidgets.QApplication([])
# window = TableTab(None, opts)
# window.resize(900, 600)
# window.show()
# sys.exit(app.exec())


class ParserWindow(QtWidgets.QTabWidget):
    def __init__(self) -> None:
        super().__init__()
        self.setTabPosition(QtWidgets.QTabWidget.North)
        self.setMovable(False)
        self.setWindowTitle('Grammar')
        self.resize(900, 600)
        self._tabs: List[QtWidgets.QWidget] = []

        self.setTabsClosable(True)
        self.tabBar().tabCloseRequested.connect(self.closeTab)
        opts = LRParserOptions(tempfile.mkdtemp(),
                               './build/lrparser',
                               grammar='')
        # self.createTab(0, opts)
        tab = GrammarTab(self, opts)
        self.requestNext(tab, 'Grammar', False)

    def closeTab(self, index: int) -> None:
        # print('Trying to close tab', index)
        widget = self.widget(index)
        if widget:
            widget.deleteLater()
        self.removeTab(index)
        # self._tab_count -= 1
        self._tabs.pop(index)

    def requestNext(self, tab: QtWidgets.QWidget, title: str, withCloseButton: bool = True) -> None:
        self.addTab(tab, title)
        index = len(self._tabs) # index of next tab
        self._tabs.append(tab)
        if not withCloseButton:
            tabBar = self.tabBar()
            tabBar.setTabButton(index, QtWidgets.QTabBar.RightSide, None) # type: ignore
            tabBar.setTabButton(index, QtWidgets.QTabBar.LeftSide, None) # type: ignore
        # print(index)
        self.setCurrentIndex(index)

        try:
            tab.onRequestNextSuccess()
        except AttributeError:
            pass  # Ignore it


class MainWindow(QtWidgets.QMainWindow):
    def __init__(self) -> None:
        super().__init__()
        # self._prevent_gc = []
        self.setWindowTitle('Home')

        self._windows: Set[QtWidgets.QWidget] = set()

        lrButton = QtWidgets.QPushButton('Grammar')
        lrButton.setCheckable(False)
        lrButton.clicked.connect(self.startParser)
        lrButton.setFixedWidth(160)

        dummyButton = QtWidgets.QPushButton('Dummy')
        dummyButton.setCheckable(False)
        dummyButton.setDisabled(True)
        dummyButton.setFixedWidth(160)

        layout = QtWidgets.QVBoxLayout()
        layout.addStretch(1)
        layout.addWidget(lrButton)
        layout.addWidget(dummyButton)
        layout.addStretch(1)

        hLayout = QtWidgets.QHBoxLayout()
        hLayout.addSpacing(16)
        hLayout.addLayout(layout)
        hLayout.addSpacing(16)

        widget = QtWidgets.QWidget()
        widget.setLayout(hLayout)
        self.setCentralWidget(widget)

    def startParser(self) -> None:
        window = ParserWindow()
        window.show()
        self._windows.add(window)


if __name__ == "__main__":
    app = QtWidgets.QApplication([])

    window = MainWindow()
    window.show()

    sys.exit(app.exec())