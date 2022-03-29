import re
from typing import List
# from PySide6 import QtCore, QtWidgets, QtGui
# from PySide6.QtCore import Qt
from PyQt5 import QtCore, QtWidgets, QtGui
from ParseTable import *
from Model import *
from Automaton import AutomatonTab
from GuiConfig import *

class SymbolTableModel(QtCore.QAbstractTableModel):
    def __init__(self, symvec: List[Symbol]) -> None:
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

        if role == Qt.BackgroundRole:
            sym = self._symvec[index.row()]
            if index.column() == len(self._colheader) - 1 and sym.is_term:
                return QtGui.QColor(240, 240, 240)

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

    def flags(self, index):
        sym = self._symvec[index.row()]
        if index.column() == len(self._colheader) - 1 and sym.is_term:
            return Qt.NoItemFlags
        else:
            return Qt.ItemIsEnabled | Qt.ItemIsSelectable


class ProductionTableModel(QtCore.QAbstractTableModel):
    def __init__(self, symvec: List[Symbol], prods: List[Production]) -> None:
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
    def __init__(self, symvec: List[Symbol], *args) -> None:
        super().__init__(*args)
        self._data = symvec
        self._model = SymbolTableModel(self._data)

        table = QtWidgets.QTableView()
        table.setModel(self._model)
        header = table.horizontalHeader()
        header.setSectionResizeMode(QtWidgets.QHeaderView.Stretch)
        table.setHorizontalHeader(header)

        layout = QtWidgets.QHBoxLayout()
        label = QtWidgets.QLabel()
        label.setText('Symbols')
        font = label.font()
        font.setBold(True)
        font.setPointSize(config.font.size.small)
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

    def refresh(self) -> None:
        self._model.layoutChanged.emit()


class ProductionTable(QtWidgets.QWidget):
    def __init__(self, symvec: List[Symbol], prods: List[Production],
                 *args) -> None:
        super().__init__(*args)
        # self._data = (symvec, prods)
        self._model = ProductionTableModel(symvec, prods)

        table = QtWidgets.QTableView()
        table.setModel(self._model)
        header = table.horizontalHeader()
        header.setSectionResizeMode(QtWidgets.QHeaderView.Stretch)
        table.setHorizontalHeader(header)

        layout = QtWidgets.QHBoxLayout()
        label = QtWidgets.QLabel()
        label.setText('Productions')
        font = label.font()
        font.setBold(True)
        font.setPointSize(config.font.size.small)
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

    def refresh(self) -> None:
        self._model.layoutChanged.emit()


class AttributeTab(QtWidgets.QWidget):
    def __init__(self, parent: QtWidgets.QWidget,
                 opts: LRParserOptions) -> None:
        super().__init__()

        # self._tag = tag
        self._opts = opts
        self._parent = parent

        code, err = getLRSteps(self._opts)
        if err:
            raise Exception(err)
        self._code = code
        self._line = 0
        self._section = None

        # print(self._code)

        env = ParsingEnv()
        self._env = env
        self.productionTable = ProductionTable(env.symbol, env.production)
        self.symbolTable = SymbolTable(env.symbol)

        hLayout = QtWidgets.QHBoxLayout()
        hLayout.addWidget(self.productionTable)
        hLayout.addWidget(self.symbolTable)

        button = QtWidgets.QPushButton('Continue')
        button.clicked.connect(self.continueButtonClicked)
        button.setCheckable(False)
        button.setFixedWidth(config.button.width)
        self._continue_button = button
        button = QtWidgets.QPushButton('Finish')
        button.setCheckable(False)
        button.setFixedWidth(config.button.width)
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
        font.setPointSize(config.font.size.normal)
        infoText.setFont(font)
        infoText.setAlignment(Qt.AlignCenter)  # type: ignore
        infoText.setText('Press button "Continue" to start')
        self._info_text = infoText
        self._env.show = lambda s: self._info_text.setText(s)
        self._env.section = lambda s: self.setSection(s)

        vLayout = QtWidgets.QVBoxLayout()
        vLayout.addLayout(hLayout)
        vLayout.addSpacing(16)
        vLayout.addWidget(infoText)
        vLayout.addSpacing(16)
        vLayout.addLayout(buttons)

        self.setLayout(vLayout)

        while self._line < len(self._code) and self._section != 'Attributes':
            self._line = execSection(self._code, self._line,
                                     self._env.__dict__)
            self._line += 1

        self.symbolTable.refresh()
        self.productionTable.refresh()

    def continueButtonClicked(self) -> None:
        self.nextLine()
        self.symbolTable.refresh()
        self.productionTable.refresh()

    def finishButtonClicked(self) -> None:
        self.finish()
        self.symbolTable.refresh()
        self.productionTable.refresh()

    def setSection(self, section) -> None:
        self._section = section

    def nextButtonClicked(self) -> None:
        items = [
            'Recursive Descent', 'LL(1)', 'LR(0)', 'SLR(1)', 'LALR(1)', 'LR(1)'
        ]
        item, ok = QtWidgets.QInputDialog.getItem(
            self, 'Parsers', 'Choose a parser from below:', items, 0, False)
        if ok and item:
            if item in [
                'LR(0)',
                'SLR(1)',
                'LALR(1)',
                'LR(1)',
            ]:
                # print('User chose parser:', item)
                self._opts.parser = item
                opts = self._opts.copy()
                env = self._env.copy()
                # tab = TableTab(self._lrwindow, opts, env)
                tab = AutomatonTab(self._parent, opts, env)
                self._parent.requestNext(tab, item + ' DFA')
            else:
                dialog = TextDialog(self, 'Not implemented')
                dialog.show()

    def finish(self) -> None:
        while True:
            if self._line >= len(self._code):
                break
            limitFlag = self.nextLine()
            if limitFlag:
                break

    def prepareNextPart(self) -> None:
        self._continue_button.setDisabled(True)
        self._finish_button.setDisabled(True)
        self._button_layout.removeWidget(self._continue_button)
        self._button_layout.removeWidget(self._finish_button)
        self._continue_button.deleteLater()
        self._finish_button.deleteLater()
        del self._continue_button
        del self._finish_button
        button = QtWidgets.QPushButton('Next')
        button.setCheckable(False)
        button.setFixedWidth(config.button.width)
        button.clicked.connect(self.nextButtonClicked)
        self._next_button = button
        self._info_text.setText('Done.')
        # Two stretches still exist, so the index should be 1
        self._button_layout.insertWidget(1, button)

    # Returns if the stop is caused by limit.
    def nextLine(self) -> bool:
        line = execNext(self._code, self._line, self._env.__dict__)
        leng = len(self._code)
        flag = line >= leng or self._section != 'Attributes'
        # if line < leng:
        #     code = self._code[line]
        #     if re.match('#!', code):
        #         line += 1
        #         flag = True
        #     else:
        #         result = re.match('#\s*(.*)', code)
        #         if result:
        #             s = result.group(1)
        #             self._info_text.setText(s)
        #             line += 1
        if flag:
            self.prepareNextPart()
        self._line = line + 1
        return flag