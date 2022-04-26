import asyncio
from ctypes import alignment
from getopt import error
import tempfile
import signal
import sys
import os
import re
import threading
from typing import Any, Callable, Dict, Set, Tuple
# from PySide6 import QtCore, QtWidgets, QtGui
# from PySide6.QtCore import Qt
from PyQt5 import QtCore, QtWidgets, QtGui, QtSvg
from PyQt5.QtCore import Qt
from graphviz import render
from Model import *
from GuiConfig import *
from Lexer import LexerDialog


class ParseTableModel(DiffTableModel):
    def __init__(self, symvec, prod, table) -> None:
        super().__init__()
        self._symvec = symvec
        self._prod = prod
        self._table = table

        T = []
        N = []
        leng = len(self._symvec)
        for i in range(leng):
            sym = self._symvec[i]
            if sym.is_term:
                T.append(i)
            else:
                N.append(i)
        self._term_count = len(T)
        T.extend(N)
        self._resorted = T

    def data(self, index, role):
        if role == Qt.DisplayRole:
            row = index.row()
            col = self._resorted[index.column()]
            value = self._table[row][col]
            if value == None or len(value) == 0:
                return ''
            assert isinstance(value, set)
            return ', '.join(value)

        if role == Qt.TextAlignmentRole:
            return Qt.AlignCenter

        if role == Qt.ToolTipRole:
            row = index.row()
            col = self._resorted[index.column()]
            value = self._table[row][col]
            if value == None or len(value) == 0:
                return ''
            assert isinstance(value, set)
            tip = []
            for item in value:
                assert isinstance(item, str)
                result = re.match('s(\d+)', item)
                if result:
                    state = result.group(1)
                    tip.append('Shift and goto state {}'.format(state))
                    continue
                result = re.match('r(\d+)', item)
                if result:
                    i = result.group(1)
                    production = self._prod[int(i)]
                    s = Production.stringify(production, self._symvec, '→')
                    tip.append('Reduce by production: {}'.format(s))
                    continue
                if item == 'acc':
                    tip.append('Success')
                    continue
                tip.append('Goto state {}'.format(item))
            s = '\n'.join(tip)
            if len(tip) > 1:
                s += '\n[Conflicts]'
            return s

        if role == Qt.BackgroundRole:
            return self.highlightCellByChanges(index)

        return super().data(index, role)

    def rowCount(self, index):
        return len(self._table)

    def columnCount(self, index):
        return len(self._symvec)

    def headerData(self, section, orientation, role=Qt.DisplayRole):
        if role == Qt.DisplayRole:
            if orientation == Qt.Vertical:
                return str(section)
            if orientation == Qt.Horizontal:
                return self._symvec[self._resorted[section]].name
        return super().headerData(section, orientation, role)


class ParseTableView(QtWidgets.QTableView):
    def __init__(self, symvec, prod, table) -> None:
        super().__init__()
        self._model = ParseTableModel(symvec, prod, table)
        self.setModel(self._model)

        header = self.horizontalHeader()
        header.setSectionResizeMode(QtWidgets.QHeaderView.Stretch)
        header.setMinimumSectionSize(config.table.cell.minwidth)
        self.setHorizontalHeader(header)

        # Hide epsilon column.
        self.hideColumn(0)

    def emitLayoutChanged(self):
        self._model.layoutChanged.emit()


class InputDialog(QtWidgets.QDialog):
    def __init__(self, parent, opts: LRParserOptions,
                 submitText: Callable[[str], bool]) -> None:
        super().__init__(parent)

        self.setModal(True)
        self.resize(400, 300)
        self._opts = opts
        # Returns whether the dialog should be closed.
        self._submitText = submitText

        self.setWindowTitle('Dialog')

        topLayout = QtWidgets.QVBoxLayout()
        label = QtWidgets.QLabel()
        label.setText('Input space-separated tokens below:')
        font = label.font()
        font.setPointSize(config.font.size.small)
        label.setFont(font)
        topLayout.addWidget(label)
        textEdit = QtWidgets.QPlainTextEdit()
        self._textEdit = textEdit
        topLayout.addWidget(textEdit, 2)

        buttonGroup = QtWidgets.QHBoxLayout()
        buttonGroup.addStretch(1)
        # Token as input
        button = QtWidgets.QPushButton('Test')
        button.clicked.connect(self.testButtonPressed)
        button.setCheckable(False)
        button.setMinimumWidth(config.button.width)
        buttonGroup.addWidget(button)
        # buttonStack.addWidget(button)

        # Source as input
        button = QtWidgets.QPushButton('From source...')
        button.clicked.connect(self.convertButtonPressed)
        button.setCheckable(False)
        button.setMinimumWidth(config.button.width)
        buttonGroup.addSpacing(8)
        buttonGroup.addWidget(button)
        buttonGroup.addStretch(1)

        topLayout.addLayout(buttonGroup)

        self.setLayout(topLayout)

    def testButtonPressed(self):
        text = self._textEdit.toPlainText()
        text += ' $'
        if self._submitText(text):
            self.close()

    def convertButtonPressed(self):
        w = LexerDialog(self)
        w.show()

# Symbol stack -> [] | [] <- Input queue
# State  stack -> []

class DequeWidget(QtWidgets.QLabel):
    def __init__(self, usedAsStack: bool, symvec: Optional[List[Symbol]] = None) -> None:
        super().__init__()
        self._symvec = symvec
        self._deq: Deque[int] = deque()
        self._is_stack = usedAsStack
        self._underline_range = (-1, -1)

    def append(self, item: int) -> None:
        self._deq.append(item)
        self.refreshView()

    def appendleft(self, item: int) -> None:
        self._deq.appendleft(item)
        self.refreshView()

    def pop(self) -> None:
        self._deq.pop()
        self.refreshView()

    def popleft(self) -> None:
        self._deq.popleft()
        self.refreshView()
        
    def refreshView(self) -> None:
        vec = self._symvec
        mapper = lambda i: vec[i].name if vec else str(i)
        items = self._deq if self._is_stack else reversed(self._deq)
        words = [mapper(a) for a in items]

        start, end = self._underline_range
        if start >= 0 and start < len(self._deq):
            words[start] = '<u>' + words[start]
            words[end] = words[end] + '</u>'
        text = ' '.join(words)

        # text = ' '.join()
        self.setText(text)

    def clear(self) -> None:
        self._deq.clear()
        self.refreshView()

    def underline(self, i: int, j: int = -1) -> None:
        self._underline_range = (i, j)
        self.refreshView()

class SvgView(QtWidgets.QScrollArea):
    def __init__(self, parent = None) -> None:
        super().__init__(parent)

        svgWidget = QtSvg.QSvgWidget()
        svgWidget.renderer().setAspectRatioMode(Qt.KeepAspectRatio)
        self._svg_widget = svgWidget
        self.setWidget(svgWidget)
        self.setAlignment(Qt.AlignCenter)

    def loadSvg(self, svg) -> None:
        w = self._svg_widget
        if svg:
            w.load(svg)
            hint = w.sizeHint()
            w.resize(int(hint.width() * 1.2), int(hint.height() * 1.2))
        else:
            w.load(b'')


class TableTab(QtWidgets.QWidget):
    def __init__(self, parent, opts: LRParserOptions, env: ParsingEnv) -> None:
        super().__init__()

        self._parent = parent
        self._opts = opts
        self._env = env
        self._ast = Forest()

        self._symdict: Dict[str, Symbol] = {}
        for sym in self._env.symbol:
            self._symdict[sym.name] = sym

        code, err = getLRSteps(self._opts)
        if err:
            print(err)
            raise Exception()
        self._code = code
        self._line = 0
        self._section = ''

        table = self.createTableLayout()
        tableWidget = QtWidgets.QWidget()
        tableWidget.setLayout(table)

        ast = SvgView(self)
        self._ast_view = ast
        astLayout = QtWidgets.QVBoxLayout()
        astLayout.addWidget(self.createTitle('AST'))
        astLayout.addSpacing(4)
        astLayout.addWidget(ast)
        astWidget = QtWidgets.QWidget()
        astWidget.setLayout(astLayout)

        splitter = QtWidgets.QSplitter()
        tableWidget.sizePolicy().setHorizontalStretch(3)
        splitter.addWidget(tableWidget)
        astWidget.sizePolicy().setHorizontalStretch(1)
        splitter.addWidget(astWidget)

        hLayout = QtWidgets.QHBoxLayout()
        hLayout.addSpacing(16)
        hLayout.addWidget(splitter)
        hLayout.addSpacing(16)

        infoLabel = QtWidgets.QLabel()
        font = infoLabel.font()
        font.setPointSize(config.font.size.normal)
        infoLabel.setFont(font)
        infoLabel.setAlignment(Qt.AlignCenter)  # type: ignore
        self._info_label = infoLabel
        self._info_label.setText('Click "Start/Reset" button to start a test.')
        self._env.show = lambda s: self._info_label.setText(s)
        self._env.section = lambda s: self.setSection(s)

        while self._line < len(self._code) and self._section != 'Parse Table':
            self._line = skipSection(self._code, self._line,
                                     self._env.__dict__)
            self._line += 1
        while self._line < len(self._code) and self._section != 'Test':
            self._line = execSection(self._code, self._line,
                                     self._env.__dict__)
            self._line += 1
        self._table.emitLayoutChanged()

        buttons = QtWidgets.QHBoxLayout()
        buttons.addStretch(1)
        button = QtWidgets.QPushButton('Start/Reset')
        button.clicked.connect(self.resetButtonClicked)
        button.setFixedWidth(config.button.width)
        buttons.addWidget(button)
        self._reset_button = button
        button = QtWidgets.QPushButton('Continue')
        button.clicked.connect(self.continueButtonClicked)
        button.setFixedWidth(config.button.width)
        buttons.addWidget(button)
        self._continue_button = button
        button = QtWidgets.QPushButton('Finish')
        button.clicked.connect(self.finishButtonClicked)
        button.setFixedWidth(config.button.width)
        buttons.addWidget(button)
        self._finish_button = button
        buttons.addStretch(1)

        self._reset_button.setDisabled(False)
        self._continue_button.setDisabled(True)
        self._finish_button.setDisabled(True)

        layout = QtWidgets.QVBoxLayout()
        layout.addLayout(hLayout, 1)
        stackLayout = self.createStacks()
        layout.addLayout(stackLayout)
        layout.addSpacing(8)
        layout.addWidget(infoLabel)
        layout.addSpacing(16)
        layout.addLayout(buttons)
        # layout.addSpacing(16)
        self.setLayout(layout)

        self._env.astAddNode = lambda index, label: self._ast.addNode(
            index, label)
        self._env.astSetParent = lambda child, parent: self._ast.setParent(
            child, parent)

    def setSection(self, section: str) -> None:
        self._section = section

    def resetButtonClicked(self) -> None:
        dialog = InputDialog(self, self._opts, self.submitTest)
        dialog.show()
        self.tryCaching()

    def refreshAST(self) -> None:
        def onSvgPrepared(svg: bytes):
            self._ast_view.loadSvg(svg)
        self._ast.toSvg(onSvgPrepared)

    # Returns line number of last line (returned by stepNext()).
    def continueButtonClicked(self) -> int:
        line = execNext(self._code, self._line, self._env.__dict__)
        self._line = line + 1
        if self._line >= len(self._code):
            self._continue_button.setEnabled(False)
            self._finish_button.setEnabled(False)
        # print(self._ast.toJson())

        find_hint = False
        for i in range(self._line, len(self._code)):
            result = re.match('reduce_hint\((\d+)\)', self._code[i])
            if result:
                find_hint = True
                start = int(result.group(1))
                self._symbolStackWidget.underline(start)
                break

        if not find_hint:
            self._symbolStackWidget.underline(-1)

        self._symbolStackWidget.refreshView()

        self.refreshAST()
        return line

    # Called by ParserWindow.
    def onTabClosed(self) -> None:
        # print('TableTab: onTabClosed()')
        self._ast_view.close()
        self._ast_view.deleteLater()

    # If this widget is in an individual window, this is called.
    # So it will be easier to test...
    def closeEvent(self, event: QtGui.QCloseEvent) -> None:
        self.onTabClosed()
        return super().closeEvent(event)

    def finishButtonClicked(self) -> None:
        line = execSection(self._code, self._line, self._env.__dict__)
        self._line = line + 1
        if self._line >= len(self._code):
            self._continue_button.setEnabled(False)
            self._finish_button.setEnabled(False)
        self.refreshAST()

    def submitTest(self, test: str) -> bool:
        # Check test string
        for token in test.strip().split():
            if token not in self._symdict.keys():
                msg = 'Error: Unknown token: {}'.format(token)
                dialog = TextDialog(self, msg)
                dialog.show()
                return False
            sym = self._symdict[token]
            if not sym.is_term:
                msg = 'Error: {} is not terminal, and cannot be in input sequence.'.format(
                    token)
                dialog = TextDialog(self, msg)
                dialog.show()
                return False

        steps, err = getLRSteps(self._opts, test)
        if err:
            dialog = TextDialog(self, 'Error: {}'.format(err))
            dialog.show()
            return False

        # Reset widget states
        assert self._env.symbol_stack
        assert self._env.state_stack
        assert self._env.input_queue
        self._env.symbol_stack.clear()
        self._env.state_stack.clear()
        self._env.input_queue.clear()

        line = 0
        while line < len(steps) and self._section != 'Test':
            line = skipSection(steps, line, self._env.__dict__)
            line += 1
        while line < len(steps) and self._section != 'Init Test':
            line = execSection(steps, line, self._env.__dict__)
            line += 1
        self._line = line
        self._code = steps

        self._finish_button.setEnabled(True)
        self._continue_button.setEnabled(True)

        # execSection() may change the label. So we reset label text here.
        self._info_label.setText('Click "Continue" button to begin the test.')
        
        self._ast.clear()
        self._ast_view.loadSvg(None)

        return True

    # Not helpful.
    def tryCaching(self) -> None:
        # ast = Forest()
        # ast.addNode(0, 'please')
        # ast.addNode(1, 'cache')
        # ast.setParent(0, 1)
        # def receiveSvg(svg: bytes) -> None:
        #     self._ast_view.loadSvg(svg)
        # ast.toSvg(lambda svg: receiveSvg(svg))
        pass

    def createTableLayout(self) -> QtWidgets.QVBoxLayout:
        layout = QtWidgets.QVBoxLayout()
        layout.addWidget(self.createTitle('Parse Table'))
        layout.addSpacing(4)
        symvec = self._env.symbol
        prod = self._env.production
        table = self._env.table
        table = ParseTableView(symvec, prod, table)
        layout.addWidget(table)
        self._table = table
        return layout

    def createTitle(self, title: str) -> QtWidgets.QLabel:
        label = QtWidgets.QLabel()
        label.setText(title)
        font = label.font()
        font.setBold(True)
        font.setPointSize(config.font.size.small)
        label.setFont(font)
        label.setAlignment(Qt.AlignCenter)  # type: ignore
        return label

    def createStacks(self) -> QtWidgets.QBoxLayout:
        padding = 24
        labelWidth = 130

        layout = QtWidgets.QHBoxLayout()
        layout.addSpacing(padding)
        label = QtWidgets.QLabel('<b>Symbol stack</b>')
        label.setFixedWidth(labelWidth)
        layout.addWidget(label)
        layout.addSpacing(8)
        symbolStackWidget = DequeWidget(True, self._env.symbol)
        self._symbolStackWidget = symbolStackWidget
        layout.addWidget(symbolStackWidget)
        layout.addStretch(1)
        inputQueueWidget = DequeWidget(False, self._env.symbol)
        layout.addWidget(inputQueueWidget)
        layout.addSpacing(8)
        label = QtWidgets.QLabel('<b>Input Queue</b>')
        label.setFixedWidth(labelWidth)
        label.setAlignment(Qt.AlignRight)
        layout.addWidget(label)
        layout.addSpacing(padding)

        layout2 = QtWidgets.QHBoxLayout()
        layout2.addSpacing(padding)
        label = QtWidgets.QLabel('<b>State stack</b>')
        label.setFixedWidth(labelWidth)
        layout2.addWidget(label)
        layout2.addSpacing(8)
        stateStackWidget = DequeWidget(True)
        layout2.addWidget(stateStackWidget)
        layout2.addStretch(1)
        
        layout3 = QtWidgets.QVBoxLayout()
        layout3.addLayout(layout)
        layout3.addLayout(layout2)

        self._env.input_queue = inputQueueWidget
        self._env.state_stack = stateStackWidget
        self._env.symbol_stack = symbolStackWidget

        return layout3


# if __name__ == '__main__':
#     # For faster debugging
#     signal.signal(signal.SIGINT, signal.SIG_DFL)
#     app = QtWidgets.QApplication([])

#     fontpath_list = [
#         './src/gui/resources/Lato-Black.ttf',
#         './src/gui/resources/Lato-BlackItalic.ttf',
#         './src/gui/resources/Lato-Bold.ttf',
#         './src/gui/resources/Lato-BoldItalic.ttf',
#         './src/gui/resources/Lato-Italic.ttf',
#         './src/gui/resources/Lato-Light.ttf',
#         './src/gui/resources/Lato-LightItalic.ttf',
#         './src/gui/resources/Lato-Regular.ttf',
#         './src/gui/resources/Lato-Thin.ttf',
#         './src/gui/resources/Lato-ThinItalic.ttf',
#     ]
#     for fontpath in fontpath_list:
#         QtGui.QFontDatabase.addApplicationFont(fontpath)

#     font_extra_small = QtGui.QFont('Lato')
#     font_extra_small.setPointSize(config.font.size.extrasmall)
#     font_extra_small.setHintingPreference(QtGui.QFont.PreferNoHinting)
#     font_small = QtGui.QFont('Lato')
#     font_small.setPointSize(config.font.size.small)
#     font_small.setHintingPreference(QtGui.QFont.PreferNoHinting)
#     font_normal = QtGui.QFont('Lato')
#     font_normal.setPointSize(config.font.size.normal)
#     font_normal.setHintingPreference(QtGui.QFont.PreferNoHinting)
#     font_large = QtGui.QFont('Lato')
#     font_large.setPointSize(config.font.size.large)
#     font_large.setHintingPreference(QtGui.QFont.PreferNoHinting)

#     app.setFont(font_extra_small)
#     app.setFont(font_small, 'QGraphicsSimpleTextItem')
#     app.setFont(font_small, "QLabel")

#     opts = LRParserOptions(out=tempfile.mkdtemp(),
#                            bin='./build/lrparser',
#                            grammar='./grammar.txt',
#                            parser='SLR(1)')
#     env = ParsingEnv()

#     class Section:
#         def __init__(self) -> None:
#             self.section = ''

#         def setSection(self, newSection) -> None:
#             self.section = newSection

#     section = Section()
#     env.section = lambda s: section.setSection(s)

#     steps, err = getLRSteps(opts)
#     if err:
#         raise Exception(err)
#     line = 0
#     while line < len(steps) and section.section != 'Parse Table':
#         line = execSection(steps, line, env.__dict__)
#         line += 1

#     window = TableTab(parent=None, opts=opts, env=env)
#     window.resize(config.win.width, config.win.height)
#     window.show()

#     sys.exit(app.exec())
