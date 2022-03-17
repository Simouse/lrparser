from getopt import error
import tempfile
import sys
import os
import re
from typing import Any, Callable, Dict, Tuple
from PySide6 import QtCore, QtWidgets, QtGui
from PySide6.QtCore import Qt
import subprocess
from Model import *

# TODO: Change all "Python3" titles to approperate ones.


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

        self._leftindex = 0
        self._deq: deque = deque()

        self.setScene(scene)
        self.setAlignment(Qt.AlignBottom)  # type: ignore
        self.setHorizontalScrollBarPolicy(Qt.ScrollBarAlwaysOff)

        # self.setTransformationAnchor(QtWidgets.QGraphicsView.NoAnchor)

    def appendText(self, text: str) -> None:
        textItem = self._scene.addText(text)
        index = len(self._deq) + self._leftindex
        textItem.setPos(*self.getPos(index))
        self._deq.append(textItem)

    def appendleftText(self, text: str) -> None:
        textItem = self._scene.addText(text)
        self._leftindex -= 1
        index = self._leftindex
        textItem.setPos(*self.getPos(index))
        self._deq.appendleft(textItem)
        # self.verticalScrollBar().setValue(self.verticalScrollBar().value() - 24);

    def popText(self) -> str:
        item = self._deq.pop()
        text = item.toPlainText()
        self._scene.removeItem(item)
        return text

    def popleftText(self) -> str:
        item = self._deq.popleft()
        self._leftindex += 1
        text = item.toPlainText()
        self._scene.removeItem(item)
        # self.verticalScrollBar().setValue(self.verticalScrollBar().value() + 24);
        return text

    def getPos(self, index) -> Tuple[int, int]:
        return (0, -24 * index)

    def clear(self) -> None:
        for item in self._deq:
            self._scene.removeItem(item)
        self._deq.clear()
        self._leftindex = 0


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


class TableTab(QtWidgets.QWidget):
    def __init__(self, parent, opts: LRParserOptions, env: ParsingEnv) -> None:
        super().__init__()

        self._lrwindow = parent
        self._opts = opts
        self._env = env
        self._symset = set()
        for sym in self._env.symbol:
            self._symset.add(sym.name)

        code, err = getLRSteps(self._opts)
        if err:
            print(err)
            raise Exception()
        self._code = code
        self._line = 0
        self._section = ''

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
        infoLabel.setAlignment(Qt.AlignCenter) # type: ignore
        self._info_label = infoLabel
        self._info_label.setText('Click "Start/Reset" button to start a test.')
        self._env.show = lambda s: self._info_label.setText(s)
        self._env.section = lambda s: self.setSection(s)

        # self._line = stepUntil(self._code, 0, self._env.__dict__, '#! Test')
        while self._line < len(self._code) and self._section != 'Parse Table':
            self._line = skipSection(self._code, self._line, self._env.__dict__)
            # codeLine = self._code[self._line]
            # if codeLine.startswith('section(') and 'Test' in codeLine:
            #     break
            self._line += 1
        while self._line < len(self._code) and self._section != 'Test':
            self._line = execSection(self._code, self._line, self._env.__dict__)
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

    def setSection(self, section: str) -> None:
        self._section = section

    def resetButtonClicked(self) -> None:
        dialog = InputDialog(self, self._opts, self.submitTest)
        dialog.show()

    # Returns line number of last line (returned by stepNext()).
    def continueButtonClicked(self) -> int:
        # print('Current line:', self._line)
        line = execNext(self._code, self._line, self._env.__dict__)
        # if line < len(self._code) and not re.match('#!', ):
        #     result = re.match('#\s*(.*)', self._code[line])
        #     if result:
        #         info = result.group(1)
        #         self._info_label.setText(info)
        self._line = line + 1
        if self._line >= len(self._code):
            self._continue_button.setEnabled(False)
            self._finish_button.setEnabled(False)
        return line

    def finishButtonClicked(self) -> None:
        # line = self._line
        # while line < len(self._code):
        #     line = self.continueButtonClicked()
        #     if line < len(self._code) and re.match('#!', self._code[line]):
        #         break
        line = execSection(self._code, self._line, self._env.__dict__)
        self._line = line + 1
        if self._line >= len(self._code):
            self._continue_button.setEnabled(False)
            self._finish_button.setEnabled(False)

    def submitTest(self, test: str) -> bool:
        # Check test string
        for token in test.strip().split():
            if token not in self._symset:
                dialog = TextDialog(self, 'Unknown token: {}'.format(token))
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
        layout.addLayout(subLayout)
        layout.addSpacing(8)
        stateStack, subLayout = self.createStackView('State Stack',
                                                     lambda i: str(i))
        self._env.state_stack = stateStack
        layout.addLayout(subLayout)
        layout.addSpacing(8)
        inputQueue, subLayout = self.createStackView(
            'Input Queue', lambda i: self._env.symbol[i].name)
        self._env.input_queue = inputQueue
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