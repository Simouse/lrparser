# Python 3.7
import json
import sys
import os
import re
from collections import deque
from PySide6 import QtCore, QtWidgets, QtGui
import PySide6
from PySide6.QtCore import Qt

# if os.environ.get('OS','') == 'Windows_NT':
#     os.system('chcp 65001')

# fsenc = sys.getfilesystemencoding()
# print(fsenc)
# sys.exit(0)

class Symbol:
    def __init__(self, name: str, is_term: bool, is_start: bool):
        self.name = name
        self.is_term = is_term
        self.is_start = is_start
        self.productions = []
        self.nullable = None
        self.first = set()
        self.follow = set()

    def new():
        return Symbol('', None, None)


class Production:
    def __init__(self, head: int, body: list):
        self.head = head
        self.body = body

    def new():
        return Production(None, None)


symbol = [Symbol.new() for _ in range(0, 10)]
production = [Production.new() for _ in range(0, 6)]
table = [[set() for _ in range(0, 10)] for _ in range(0, 2048)]
symbol_stack = deque()
state_stack = deque()
input_queue = deque()


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
            for i in range(0, len(value)):
                if i > 0:
                    s += ' '
                s += self._symvec[value[i]].name
            return s

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


class SymbolTable(QtWidgets.QTableView):
    def __init__(self, symvec, *args) -> None:
        super().__init__(*args)
        self._data = symvec
        self.refresh()

    def refresh(self):
        self.setModel(SymbolTableModel(self._data))
        self.resizeColumnsToContents()
        self.resizeRowsToContents()


class ProductionTable(QtWidgets.QTableView):
    def __init__(self, symvec, prods, *args) -> None:
        super().__init__(*args)
        self._data = (symvec, prods)
        self.refresh()

    def refresh(self):
        self.setModel(ProductionTableModel(*self._data))
        self.resizeColumnsToContents()
        self.resizeRowsToContents()

class MainWindow(QtWidgets.QMainWindow):
    def __init__(self) -> None:
        super().__init__()
        self.setWindowTitle('Parser')

        # self.table = SymbolTable(symbol)
        self.productionTable = ProductionTable(symbol, production)
        self.symbolTable = SymbolTable(symbol)

        sublayout = QtWidgets.QHBoxLayout()
        sublayout.addWidget(self.productionTable)
        sublayout.addWidget(self.symbolTable)

        button = QtWidgets.QPushButton('Next')
        button.setCheckable(True)
        button.clicked.connect(self.button_clicked)

        # infoText = QtWidgets.QTextEdit()
        # infoText.setText('Hi')
        # infoText.setPlaceholderText('')
        # infoText.setReadOnly(True)
        # font = infoText.font()
        # font.setPointSize(16)
        # infoText.setAlignment()
        # infoText.setFont(font)
        infoText = QtWidgets.QLabel()
        # font = infoText.font()
        font = QtGui.QFont('Times New Roman', 14)
        # font.setPixelSize(18)
        infoText.setFont(font)
        infoText.setAlignment(Qt.AlignCenter)
        infoText.setText('Hello')
        infoText.setFixedHeight(120)
        self.infoText = infoText

        layout = QtWidgets.QVBoxLayout()
        layout.addLayout(sublayout)
        layout.addWidget(infoText)
        layout.addWidget(button)

        widget = QtWidgets.QWidget()
        widget.setLayout(layout)
        self.setCentralWidget(widget)

        self._exec_content = open('out/steps.py', mode='r',
                                  encoding='utf-8').read().split(sep='\n')
        self._exec_next_line = 0
        # print(self._exec_content)

    def button_clicked(self):
        self.advance()
        # symbol[0].name = symbol[0].name + '!'
        # symbol[0].name='ε'
        self.symbolTable.refresh()
        self.productionTable.refresh()

    def advance(self):
        line = self._exec_next_line
        text = self._exec_content
        leng = len(text)
        info = ''
        s = ''
        while line < leng:
            if str(text[line]).startswith('#'):
                info = text[line]
                line += 1
                break
            s += text[line]
            s += '\n'
            line += 1
        # print(s)
        try:
            exec(s)
        except:
            print('Exception happens between line {} and line {}'.format(
                self._exec_next_line, line))
            raise
        if not info.isspace():
            result = re.match('#\s*(.*)', info)
            if result:
                s = result.group(1)
                print(s)
                self.infoText.setText(s)
                
        self._exec_next_line = line

if __name__ == "__main__":
    app = QtWidgets.QApplication([])

    window = MainWindow()
    window.resize(800, 600)
    window.show()

    sys.exit(app.exec())