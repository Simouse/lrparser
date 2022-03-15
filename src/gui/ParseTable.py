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
                 onStepsGenerated: Callable[[List[str]], None]) -> None:
        super().__init__(parent)

        self.setModal(True)
        self.resize(400, 300)
        self._opts = opts
        self._onStepsGenerated = onStepsGenerated

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
        print(self._opts.__dict__)
        text = self._textEdit.toPlainText()
        text += ' $'
        code, err = getLRSteps(self._opts, test=text)
        if not err:
            self._onStepsGenerated(code)
            self.close()
        else:
            dialog = TextDialog(self, 'Error: {}'.format(err))
            dialog.show()

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
        return text

    def getPos(self, index) -> Tuple:
        return (0, -24 * index)


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
        self._env = env  # Everything except parser states are used.

        code, err = getLRSteps(self._opts)
        if err:
            print(err)
            raise Exception()
        self._code = code
        self._line = stepUntil(self._code, 0, self._env.__dict__, '#! Test')

        hLayout = QtWidgets.QHBoxLayout()
        hLayout.addSpacing(16)
        hLayout.addLayout(self.createTableLayout(), 3)
        hLayout.addSpacing(8)
        # After stacks layout is created, data reference in self._opts.env
        # will be replaced.
        hLayout.addLayout(self.createStacksLayout(), 1)
        hLayout.addSpacing(16)

        buttons = QtWidgets.QHBoxLayout()
        buttons.addStretch(1)
        button = QtWidgets.QPushButton('Start/Reset')
        button.clicked.connect(self.resetButtonClicked)
        button.setFixedWidth(100)
        buttons.addWidget(button)
        self._reset_button = button
        button = QtWidgets.QPushButton('Continue')
        button.setFixedWidth(100)
        buttons.addWidget(button)
        self._continue_button = button
        button = QtWidgets.QPushButton('Finish')
        button.setFixedWidth(100)
        buttons.addWidget(button)
        self._finish_button = button
        buttons.addStretch(1)

        self._reset_button.setDisabled(False)
        self._continue_button.setDisabled(True)
        self._finish_button.setDisabled(True)

        layout = QtWidgets.QVBoxLayout()
        layout.addLayout(hLayout)
        layout.addSpacing(16)
        layout.addLayout(buttons)
        layout.addSpacing(16)

        self.setLayout(layout)
        # self._line = stepUntil(self._code, 0, self._opts.env, '#!!')

        # self._table.emitLayoutChanged()

    def resetButtonClicked(self) -> None:
        dialog = InputDialog(self, self._opts, self.reset)
        dialog.show()

    def reset(self, steps: List[str]) -> None:
        # TODO: reset stack view states
        line = skipUntil(steps, 0, '#! Test')
        line = stepUntil(steps, line + 1, self._env.__dict__, '#! Init Test')
        self._line = line + 1

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