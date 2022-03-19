from getopt import error
import tempfile
import signal
import sys
import os
import re
from typing import Any, Callable, Dict, Set, Tuple
from PySide6 import QtCore, QtWidgets, QtGui
from PySide6.QtCore import Qt
from Model import *


class ParseTableModel(QtCore.QAbstractTableModel):
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
        self.setHorizontalHeader(header)

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

        layout = QtWidgets.QVBoxLayout()
        label = QtWidgets.QLabel()
        label.setText('Choose input mode from below:')
        layout.addWidget(label)
        combo = QtWidgets.QComboBox(self)
        combo.addItems(['Tokens', 'Source'])
        combo.setCurrentIndex(0)
        combo.currentIndexChanged.connect(self.comboBoxIndexChanged)
        layout.addWidget(combo)
        textEdit = QtWidgets.QPlainTextEdit()
        self._textEdit = textEdit
        layout.addWidget(textEdit, 2)

        buttonStack = QtWidgets.QStackedWidget(self)
        button = QtWidgets.QPushButton('Test')
        button.clicked.connect(self.testButtonPressed)
        button.setCheckable(False)
        button.setFixedWidth(100)
        buttonStack.addWidget(button)
        button = QtWidgets.QPushButton('Convert')
        button.clicked.connect(self.convertButtonPressed)
        button.setCheckable(False)
        button.setFixedWidth(100)
        buttonStack.addWidget(button)
        buttonStack.setCurrentIndex(0)

        self._buttonStack = buttonStack

        buttonLayout = QtWidgets.QHBoxLayout()
        buttonLayout.addStretch(1)
        buttonLayout.addWidget(buttonStack)
        buttonLayout.addStretch(1)

        layout.addLayout(buttonLayout)

        self.setLayout(layout)

    def comboBoxIndexChanged(self, index):
        self._buttonStack.setCurrentIndex(index)

    def testButtonPressed(self):
        # print(self._opts.__dict__)
        text = self._textEdit.toPlainText()
        text += ' $'
        if self._submitText(text):
            self.close()

    def convertButtonPressed(self):
        text = self._textEdit.toPlainText()
        # print('Convert button pressed')
        dialog = TextDialog(self, 'Not implemented')
        dialog.show()
        # TODO: Convert the source into tokens and go back to Test mode


class BottomUpDequeView(QtWidgets.QGraphicsView):
    def __init__(self) -> None:
        super().__init__()

        scene = QtWidgets.QGraphicsScene()
        self._scene = scene

        font = QtGui.QFont()
        font.setPointSize(11)
        self._font = font

        self._leftIndex = 0
        self._deq: deque = deque()
        self._itemHeight = 24

        self.setScene(scene)
        self.setAlignment(Qt.AlignBottom | Qt.AlignHCenter)  # type: ignore
        self.setHorizontalScrollBarPolicy(Qt.ScrollBarAlwaysOff)

    def shrink(self) -> None:
        self.setSceneRect(self._scene.itemsBoundingRect())

    def newLabel(self, text: str) -> QtWidgets.QLabel:
        label = QtWidgets.QLabel(text=text)
        w = self.width()
        label.setFixedWidth(w)
        label.setFixedHeight(self._itemHeight)
        label.setAlignment(Qt.AlignCenter)  # type: ignore
        return label

    def appendText(self, text: str) -> None:
        textItem = self._scene.addWidget(self.newLabel(text))
        index = len(self._deq) + self._leftIndex
        textItem.setPos(-textItem.widget().width() / 2.0, self.getPosY(index))
        self._deq.append(textItem)
        self.shrink()

    def appendleftText(self, text: str) -> None:
        textItem = self._scene.addWidget(self.newLabel(text))
        self._leftIndex -= 1
        index = self._leftIndex
        textItem.setPos(-textItem.widget().width() / 2.0, self.getPosY(index))
        self._deq.appendleft(textItem)
        self.shrink()

    def popText(self) -> str:
        item = self._deq.pop()
        # text = item.toPlainText()
        label: QtWidgets.QLabel = item.widget()
        text = label.text()
        self._scene.removeItem(item)
        self.shrink()
        return text

    def popleftText(self) -> str:
        item = self._deq.popleft()
        self._leftIndex += 1
        label: QtWidgets.QLabel = item.widget()
        text = label.text()
        self._scene.removeItem(item)
        self.shrink()
        return text

    def getPosY(self, index) -> float:
        return -self._itemHeight * index

    def clear(self) -> None:
        for item in self._deq:
            self._scene.removeItem(item)
        self._deq.clear()
        self._leftIndex = 0
        self._scene.clear()


class MappedBottomUpDequeView(BottomUpDequeView):
    def __init__(self, mapper: Callable[[int], str],
                 reverseMapper: Callable[[str], int]) -> None:
        super().__init__()
        self._mapper = mapper
        self._rmapper = reverseMapper

    def append(self, item: int) -> None:
        text = self._mapper(item)
        return super().appendText(text)

    def appendleft(self, item: int) -> None:
        text = self._mapper(item)
        return super().appendleftText(text)

    def pop(self) -> int:
        item = super().popText()
        return self._rmapper(item)

    def popleft(self) -> int:
        item = super().popleftText()
        return self._rmapper(item)


# Only used inside TableTab.
class ASTWindow(QtSvgWidgets.QSvgWidget):
    def __init__(self, *args, **kwargs) -> None:
        super().__init__(*args, **kwargs)

        self.offset: Optional[QtCore.QPointF] = None
        
        self.setWindowFlag(Qt.FramelessWindowHint, True)
        self.setWindowFlag(Qt.WindowStaysOnTopHint, True)
        self.setMouseTracking(True)

    def svgWidget(self) -> QtSvgWidgets.QSvgWidget:
        # return self._svgWidget
        return self

    def closeEvent(self, event: QtGui.QCloseEvent) -> None:
        if event.spontaneous():
            event.ignore()
        else:
            super().closeEvent(event)

    def mousePressEvent(self, event: QtGui.QMouseEvent) -> None:
        self.offset = event.position()
        event.accept()

    def mouseMoveEvent(self, event: QtGui.QMouseEvent) -> None:
        if self.offset:
            pos = event.globalPosition()
            x = self.offset.x()
            y = self.offset.y()
            # Does not work on WSLg.
            self.move(int(pos.x() - x), int(pos.y() - y))
        event.accept()

    def mouseReleaseEvent(self, event: QtGui.QMouseEvent) -> None:
        self.offset = None
        event.accept()


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

        self._ast_view = ASTWindow()

        hLayout = QtWidgets.QHBoxLayout()
        hLayout.addSpacing(16)
        hLayout.addLayout(self.createTableLayout(), 3)
        hLayout.addSpacing(8)
        # After stacks layout is created, data reference in self._opts.env
        # will be replaced.
        hLayout.addLayout(self.createStacksLayout(), 1)
        hLayout.addSpacing(16)

        infoLabel = QtWidgets.QLabel()
        font = infoLabel.font()
        font.setPointSize(12)
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
        button.setFixedWidth(100)
        buttons.addWidget(button)
        self._reset_button = button
        button = QtWidgets.QPushButton('Continue')
        button.clicked.connect(self.continueButtonClicked)
        button.setFixedWidth(100)
        buttons.addWidget(button)
        self._continue_button = button
        button = QtWidgets.QPushButton('Finish')
        button.clicked.connect(self.finishButtonClicked)
        button.setFixedWidth(100)
        buttons.addWidget(button)
        self._finish_button = button
        buttons.addStretch(1)

        self._reset_button.setDisabled(False)
        self._continue_button.setDisabled(True)
        self._finish_button.setDisabled(True)

        layout = QtWidgets.QVBoxLayout()
        layout.addLayout(hLayout)
        layout.addSpacing(24)
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

    def refreshAST(self) -> None:
        def onSvgPrepared(svg: bytes):
            self._ast_view.svgWidget().renderer().load(svg)
            self._ast_view.svgWidget().adjustSize()
            self._ast_view.show()

        self._ast.toSvg(onSvgPrepared)

    # Returns line number of last line (returned by stepNext()).
    def continueButtonClicked(self) -> int:
        line = execNext(self._code, self._line, self._env.__dict__)
        self._line = line + 1
        if self._line >= len(self._code):
            self._continue_button.setEnabled(False)
            self._finish_button.setEnabled(False)
        # print(self._ast.toJson())
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
                msg = 'Error: Unknown token: {}.'.format(token)
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
        self._ast_view.hide()
        return True

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
        font.setPointSize(10)
        label.setFont(font)
        label.setAlignment(Qt.AlignCenter)  # type: ignore
        return label

    # Returns (BottomUpDequeView, QBoxLayout).
    def createStackView(
        self, title: str, mapper: Callable[[int], str]
    ) -> Tuple[MappedBottomUpDequeView, QtWidgets.QVBoxLayout]:

        table = MappedBottomUpDequeView(mapper, lambda s: 0)
        table.setFixedWidth(65)
        layout = QtWidgets.QHBoxLayout()
        layout.addStretch(1)
        layout.addWidget(table)
        layout.addStretch(1)
        tableLayout = layout

        layout = QtWidgets.QVBoxLayout()
        layout.addWidget(self.createTitle(title))
        layout.addSpacing(4)
        layout.addLayout(tableLayout)

        return (table, layout)

    def createStacksLayout(self) -> QtWidgets.QHBoxLayout:
        layout = QtWidgets.QHBoxLayout()
        layout.addStretch(1)
        layout.addLayout(self.createStackIndicators())
        layout.addSpacing(-4)
        symbolStack, subLayout = self.createStackView(
            'Symbol Stack', lambda i: self._env.symbol[i].name)
        self._env.symbol_stack = symbolStack
        self.symbolStackView = symbolStack
        layout.addLayout(subLayout)
        layout.addSpacing(8)
        stateStack, subLayout = self.createStackView('State Stack',
                                                     lambda i: str(i))
        self._env.state_stack = stateStack
        self.stateStackView = stateStack
        layout.addLayout(subLayout)
        layout.addSpacing(8)
        inputQueue, subLayout = self.createStackView(
            'Input Queue', lambda i: self._env.symbol[i].name)
        self._env.input_queue = inputQueue
        self.inputQueueView = inputQueue
        layout.addLayout(subLayout)
        layout.addSpacing(-4)
        layout.addLayout(self.createQueueIndicators())
        return layout

    def createStackIndicators(self) -> QtWidgets.QVBoxLayout:
        layout = QtWidgets.QVBoxLayout()
        layout.addSpacing(28)
        label = QtWidgets.QLabel()
        label.setText('Top →')
        label.setAlignment(Qt.AlignTrailing)  # type: ignore

        layout.addWidget(label)
        layout.addStretch(1)
        label = QtWidgets.QLabel()
        label.setText('Bottom →')
        label.setAlignment(Qt.AlignTrailing)  # type: ignore

        layout.addWidget(label)

        return layout

    def createQueueIndicators(self) -> QtWidgets.QVBoxLayout:
        layout = QtWidgets.QVBoxLayout()
        layout.addSpacing(28)
        label = QtWidgets.QLabel()
        label.setText('← Front')
        label.setAlignment(Qt.AlignLeading)  # type: ignore

        layout.addWidget(label)
        layout.addStretch(1)
        label = QtWidgets.QLabel()
        label.setText('← End')
        label.setAlignment(Qt.AlignLeading)  # type: ignore

        layout.addWidget(label)

        return layout


if __name__ == '__main__':
    # For faster debugging
    signal.signal(signal.SIGINT, signal.SIG_DFL)
    app = QtWidgets.QApplication([])

    opts = LRParserOptions(out=tempfile.mkdtemp(),
                           bin='./build/lrparser',
                           grammar='./grammar.txt',
                           parser='SLR(1)')
    env = ParsingEnv()

    class Section:
        def __init__(self) -> None:
            self.section = ''

        def setSection(self, newSection) -> None:
            self.section = newSection

    section = Section()
    env.section = lambda s: section.setSection(s)

    steps, err = getLRSteps(opts)
    if err:
        raise Exception(err)
    line = 0
    while line < len(steps) and section.section != 'Parse Table':
        line = execSection(steps, line, env.__dict__)
        line += 1

    window = TableTab(parent=None, opts=opts, env=env)
    window.resize(900, 600)
    window.show()

    sys.exit(app.exec())
