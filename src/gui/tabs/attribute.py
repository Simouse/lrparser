import sys
import os
import re
from PySide6 import QtCore, QtWidgets, QtGui
from PySide6.QtCore import Qt
import subprocess
from model import *


class SymbolTableModel(QtCore.QAbstractTableModel):
    def __init__(self, symvec):
        super().__init__()
        self._symvec = symvec
        self._colnames = ['name', 'is_term', 'nullable', 'first', 'follow']
        self._colheader = ['Name', 'Terminal', 'Nullable', 'First', 'Follow']

    def data(self, index, role):
        if role == Qt.DisplayRole:
            attr = self._colnames[index.column()]
            value = self._symvec[index.row()].__dict__[attr]

            if isinstance(value, set):
                s = ''
                for k, v in enumerate(value):
                    if k > 0:
                        s += ' '
                    s += self._symvec[v].name
                value = s

            return value

        if role == Qt.TextAlignmentRole:
            return Qt.AlignCenter

    def rowCount(self, index):
        return len(self._symvec)

    def columnCount(self, index):
        return len(self._colnames)

    def headerData(self, section, orientation, role=Qt.DisplayRole):
        if orientation == Qt.Horizontal and role == Qt.DisplayRole:
            return self._colheader[section]
        elif orientation == Qt.Vertical and role == Qt.DisplayRole:
            return str(section)
        return super().headerData(section, orientation, role)


class ProductionTableModel(QtCore.QAbstractTableModel):
    def __init__(self, symvec, prods):
        super().__init__()
        self._prods = prods
        self._symvec = symvec
        self._colnames = ['head', 'body']
        self._colheader = ['Head', 'Body']

    def data(self, index, role):
        if role == Qt.DisplayRole:
            attr = self._colnames[index.column()]
            value = self._prods[index.row()].__dict__[attr]

            if value == None:
                return ''

            if isinstance(value, int):
                l = []
                l.append(value)
                value = l

            s = ''
            if len(value) == 0:
                s += 'ε'
            else:
                for i in range(0, len(value)):
                    if i > 0:
                        s += ' '
                    s += self._symvec[value[i]].name
            return s

        if role == Qt.TextAlignmentRole:
            return Qt.AlignCenter

    def rowCount(self, index):
        return len(self._prods)

    def columnCount(self, index):
        return 2

    def headerData(self, section, orientation, role=Qt.DisplayRole):
        if orientation == Qt.Horizontal and role == Qt.DisplayRole:
            return self._colheader[section]
        elif orientation == Qt.Vertical and role == Qt.DisplayRole:
            return str(section)
        return super().headerData(section, orientation, role)

class SymbolTable(QtWidgets.QWidget):
    def __init__(self, symvec, *args) -> None:
        super().__init__(*args)
        self._data = symvec
        self._model = SymbolTableModel(self._data)

        table = QtWidgets.QTableView()
        table.setModel(self._model)
        header = table.horizontalHeader()
        header.setSectionResizeMode(0, QtWidgets.QHeaderView.Stretch)
        header.setSectionResizeMode(1, QtWidgets.QHeaderView.Stretch)
        header.setSectionResizeMode(2, QtWidgets.QHeaderView.Stretch)
        header.setSectionResizeMode(3, QtWidgets.QHeaderView.Stretch)
        header.setSectionResizeMode(4, QtWidgets.QHeaderView.Stretch)
        table.setHorizontalHeader(header)

        layout = QtWidgets.QHBoxLayout()
        label = QtWidgets.QLabel()
        label.setText('Symbols')
        font = label.font()
        font.setBold(True)
        font.setPointSize(10)
        label.setFont(font)
        layout.addStretch(1)
        layout.addWidget(label)
        layout.addStretch(1)

        vlayout = QtWidgets.QVBoxLayout()
        vlayout.addLayout(layout)
        vlayout.addSpacing(4)
        vlayout.addWidget(table)
        self.setLayout(vlayout)

        self.refresh()

    def refresh(self):
        self._model.layoutChanged.emit()


class ProductionTable(QtWidgets.QWidget):
    def __init__(self, symvec, prods, *args) -> None:
        super().__init__(*args)
        self._data = (symvec, prods)
        self._model = ProductionTableModel(*self._data)

        table = QtWidgets.QTableView()
        table.setModel(self._model)
        header = table.horizontalHeader()
        header.setSectionResizeMode(0, QtWidgets.QHeaderView.Stretch)
        header.setSectionResizeMode(1, QtWidgets.QHeaderView.Stretch)
        table.setHorizontalHeader(header)

        layout = QtWidgets.QHBoxLayout()
        label = QtWidgets.QLabel()
        label.setText('Productions')
        font = label.font()
        font.setBold(True)
        font.setPointSize(10)
        label.setFont(font)
        layout.addStretch(1)
        layout.addWidget(label)
        layout.addStretch(1)

        vlayout = QtWidgets.QVBoxLayout()
        vlayout.addLayout(layout)
        vlayout.addSpacing(4)
        vlayout.addWidget(table)

        self.setLayout(vlayout)

        self.refresh()

    def refresh(self):
        self._model.layoutChanged.emit()


class AttributeTab(QtWidgets.QWidget):
    def __init__(self, opt: LRParserOptions, lrwindow) -> None:
        super().__init__()

        self.opt = opt
        self.lrwindow = lrwindow

        # Get initial parser status
        cmd = ' '.join([
            self.opt.exePath, '-o', self.opt.outDir, '-g',
            self.opt.grammarFile, '--no-test'
        ])
        print(cmd)
        # status = subprocess.call(cmd, timeout=2.0, stdout=subprocess.DEVNULL)
        status = subprocess.call(cmd, timeout=2.0)

        if status != 0:
            raise Exception('Cannot launch LR parser')
        with open(os.path.join(opt.outDir, 'steps.py'),
                  mode='r',
                  encoding='utf-8') as f:
            self._exec_content = f.read().split(sep='\n')
        self._exec_line = 0
        print('Output written to temporary directory', opt.outDir, sep=' ')

        result = re.match('#!nsym=(\d+),nprod=(\d+)', self._exec_content[0])
        if result:
            nsym = int(result.group(1))
            nprod = int(result.group(2))
            self._exec_line += 1
        else:
            print('Error: cannot get arguments: [nsym, nprod]')
            sys.exit(1)

        env = createExecEnv(nsym, nprod, 2048)
        self.opt.env = env
        self.productionTable = ProductionTable(env['symbol'],
                                               env['production'])
        self.symbolTable = SymbolTable(env['symbol'])

        hLayout = QtWidgets.QHBoxLayout()
        hLayout.addWidget(self.productionTable)
        hLayout.addWidget(self.symbolTable)

        button = QtWidgets.QPushButton('Continue')
        button.clicked.connect(self.continueButtonClicked)
        button.setCheckable(False)
        button.setFixedWidth(100)
        self._continue_button = button
        button = QtWidgets.QPushButton('Finish')
        button.setCheckable(False)
        button.setFixedWidth(100)
        button.clicked.connect(self.finishButtonClicked)
        self._finish_button = button

        buttons = QtWidgets.QHBoxLayout()
        buttons.addStretch(1)
        buttons.addWidget(self._continue_button)
        buttons.addWidget(self._finish_button)
        buttons.addStretch(1)
        self._button_layout = buttons

        infoText = QtWidgets.QLabel()
        font = infoText.font()
        font.setPointSize(12)
        infoText.setFont(font)
        infoText.setAlignment(Qt.AlignCenter)
        infoText.setText('Press button "Continue" to start')
        self._info_text = infoText

        vLayout = QtWidgets.QVBoxLayout()
        vLayout.addLayout(hLayout)
        vLayout.addSpacing(16)
        vLayout.addWidget(infoText)
        vLayout.addSpacing(16)
        vLayout.addLayout(buttons)

        self.setLayout(vLayout)
        self.continueButtonClicked()

    def continueButtonClicked(self):
        self.nextLine()
        self.symbolTable.refresh()
        self.productionTable.refresh()

    def finishButtonClicked(self):
        self.finish()
        self.symbolTable.refresh()
        self.productionTable.refresh()

    def nextButtonClicked(self):
        self.lrwindow.nextButtonPressed(0)

    def finish(self):
        # print('Finish')
        line = self._exec_line
        text = self._exec_content
        leng = len(text)
        info = ''
        s = ''
        while line < leng:
            result = re.match('#\s*Done\s*\.?\s*', text[line])
            if result:
                info = text[line]
                line += 1
                break
            s += text[line]
            s += '\n'
            line += 1
        # print(s)
        try:
            exec(s, self.opt.env)
        except:
            print('Exception happens between line {} and line {}'.format(
                self._exec_line, line))
            raise
        if not info.isspace():
            result = re.match('#\s*(.*)', info)
            if result:
                s = result.group(1)
                # print(s)
                self._info_text.setText(s)

        self._exec_line = line
        self.prepareNextPart()

    def prepareNextPart(self):
        self._continue_button.setDisabled(True)
        self._finish_button.setDisabled(True)
        self._button_layout.removeWidget(self._continue_button)
        self._button_layout.removeWidget(self._finish_button)
        self._continue_button.deleteLater()
        self._finish_button.deleteLater()
        self._continue_button = None
        self._finish_button = None
        button = QtWidgets.QPushButton('Next')
        button.setCheckable(False)
        button.setFixedWidth(100)
        button.clicked.connect(self.nextButtonClicked)

        # Two stretches still exist, so the index should be 1
        self._button_layout.insertWidget(1, button)

    def nextLine(self):
        line = self._exec_line
        text = self._exec_content
        leng = len(text)
        info = ''
        s = ''
        while line < leng:
            if re.match('#\s*Done\s*', text[line]):
                info = text[line]
                line = leng
                break
            elif str(text[line]).startswith('#'):
                info = text[line]
                line += 1
                break
            s += text[line]
            s += '\n'
            line += 1
        # print(s)
        try:
            exec(s, self.opt.env)
        except:
            print('Exception happens between line {} and line {}'.format(
                self._exec_line, line))
            raise
        if not info.isspace():
            result = re.match('#\s*(.*)', info)
            if result:
                s = result.group(1)
                # print(s)
                self._info_text.setText(s)

        self._exec_line = line
        if line >= leng:
            self.prepareNextPart()