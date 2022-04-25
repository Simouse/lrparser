# Tested on Python 3.7
import sys
from typing import Set
import typing
# from PySide6 import QtCore, QtWidgets, QtGui
# from PySide6.QtCore import Qt
from PyQt5 import QtCore, QtWidgets, QtGui
from PyQt5.QtCore import Qt
import tempfile
import signal
from Attribute import *
from ParseTable import *
from Model import *
from Editor import *
from GuiConfig import *
# import asyncio

# For faster debugging
signal.signal(signal.SIGINT, signal.SIG_DFL)

lrparser_path = 'lrparser'

class ParserWindow(QtWidgets.QTabWidget):
    def __init__(self) -> None:
        super().__init__()
        self.setTabPosition(QtWidgets.QTabWidget.North)
        self.setMovable(False)
        self.setWindowTitle('Parser')
        self.resize(config.win.width, config.win.height)
        self._tabs: List[QtWidgets.QWidget] = []
        self._detachedTabs: List[QtWidgets.QWidget] = []
        self.setTabsClosable(True)
        self.tabBar().tabCloseRequested.connect(self.closeTab)
        self.tabBar().tabBarDoubleClicked.connect(self.detachTab)
        self.window().setAttribute(Qt.WA_AlwaysShowToolTips, True)

        opts = LRParserOptions(tempfile.mkdtemp(),
                               lrparser_path,
                               grammar='')
        tab = GrammarTab(self, opts)
        self.requestNext(tab, 'Grammar', False)

    def detachTab(self, index: int) -> None:
        # Ignore the first tab
        if index == 0:
            return

        widget = self.widget(index)       # w: widget to detach
        title = self.tabText(index)
        self.removeTab(self.currentIndex())
        self._detachedTabs.append(widget) # Keep alive
        self._tabs.pop(index)

        widget.setParent(None)            # type: ignore
        widget.setWindowTitle(title)
        widget.show()

    def closeTab(self, index: int) -> None:
        # print(f'closeTab({self}, {index})')
        widget = self.widget(index)
        if widget:
            widget.deleteLater()
        self.removeTab(index)
        tab = self._tabs.pop(index)
        try:
            tab.onTabClosed()
        except AttributeError:
            pass # ignore

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
        if index > 0:
            self.setTabToolTip(index, 'Double click to detach')

        try:
            tab.onRequestNextSuccess()
        except AttributeError:
            pass  # Ignore it

    def closeEvent(self, event: QtGui.QCloseEvent) -> None:
        print('ParserWindow: closeEvent() called')
        
        for tab in self._tabs:
            try:
                tab.onTabClosed()
            except AttributeError:
                pass # ignore

        return super().closeEvent(event)


class MainWindow(QtWidgets.QMainWindow):
    def __init__(self) -> None:
        super().__init__()
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


def main():
    app = QtWidgets.QApplication([])

    fontpath_list = [
        './src/gui/resources/Noto_Sans_SC/NotoSansSC-Black.otf',
        './src/gui/resources/Noto_Sans_SC/NotoSansSC-Bold.otf',
        './src/gui/resources/Noto_Sans_SC/NotoSansSC-Light.otf',
        './src/gui/resources/Noto_Sans_SC/NotoSansSC-Medium.otf',
        './src/gui/resources/Noto_Sans_SC/NotoSansSC-Regular.otf',
        './src/gui/resources/Noto_Sans_SC/NotoSansSC-Thin.otf',
    ]
    for fontpath in fontpath_list:
        QtGui.QFontDatabase.addApplicationFont(fontpath)

    fontname = config.font.name
    font_extra_small = QtGui.QFont(fontname)
    font_extra_small.setPointSize(config.font.size.extrasmall)
    font_extra_small.setHintingPreference(QtGui.QFont.PreferNoHinting)
    font_small = QtGui.QFont(fontname)
    font_small.setPointSize(config.font.size.small)
    font_small.setHintingPreference(QtGui.QFont.PreferNoHinting)
    font_normal = QtGui.QFont(fontname)
    font_normal.setPointSize(config.font.size.normal)
    font_normal.setHintingPreference(QtGui.QFont.PreferNoHinting)
    font_large = QtGui.QFont(fontname)
    font_large.setPointSize(config.font.size.large)
    font_large.setHintingPreference(QtGui.QFont.PreferNoHinting)

    app.setFont(font_extra_small)
    app.setFont(font_small, 'QGraphicsSimpleTextItem')
    app.setFont(font_small, "QLabel")

    window = ParserWindow()
    window.show()
    # self._windows.add(window)
    # window = MainWindow()
    # window.show()

    sys.exit(app.exec())

if __name__ == "__main__":
    main()