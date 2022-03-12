import tempfile
from model import *
import sys
import os
import re
from PySide6 import QtCore, QtWidgets, QtGui
from PySide6.QtCore import Qt
import subprocess
from model import *


class ProductionEditorModel(QtCore.QAbstractTableModel):
    def __init__(self, data) -> None:
        super().__init__()
        self._data = data
        self._header = ['Head', '', 'Body']
        self._item_font = QtGui.QFont()
        self._item_font.setPointSize(14)

    def rowCount(self, index):
        return len(self._data)

    def columnCount(self, index):
        return 3

    def data(self, index, role):
        if role == Qt.DisplayRole or role == Qt.EditRole:
            if index.column() == 1:
                return '→'
            return self._data[index.row()][index.column()]

        if role == Qt.TextAlignmentRole:
            return Qt.AlignCenter

        if role == Qt.FontRole:
            return self._item_font

    def headerData(self, section, orientation, role=Qt.DisplayRole):
        if orientation == Qt.Horizontal and role == Qt.DisplayRole:
            return self._header[section]
        elif orientation == Qt.Vertical and role == Qt.DisplayRole:
            return str(section)
        return super().headerData(section, orientation, role)

    def flags(self, index):
        if index.column() != 1:
            return Qt.ItemIsSelectable | Qt.ItemIsEnabled | Qt.ItemIsEditable
        return Qt.ItemIsSelectable | Qt.ItemIsEnabled

    def setData(self, index, value, role):
        if role == Qt.EditRole:
            self._data[index.row()][index.column()] = value
            return True

class CenterDelegate(QtWidgets.QStyledItemDelegate):
    def __init__(self) -> None:
        super().__init__()
        self._item_font = QtGui.QFont()
        self._item_font.setPointSize(14)

    def createEditor(self, parent, option, index):
        editor = QtWidgets.QStyledItemDelegate.createEditor(
            self, parent, option, index)
        editor.setAlignment(Qt.AlignCenter)
        editor.setFont(self._item_font)
        return editor

class EditorTable(QtWidgets.QWidget):
    def __init__(self) -> None:
        super().__init__()

        table = QtWidgets.QTableView()
        editorData = [['', '', ''], ['', '', ''], ['', '', '']]
        model = ProductionEditorModel(editorData)
        table.setModel(model)
        header = table.horizontalHeader()
        header.setSectionResizeMode(0, QtWidgets.QHeaderView.Stretch)
        header.setSectionResizeMode(1, QtWidgets.QHeaderView.ResizeToContents)
        header.setSectionResizeMode(2, QtWidgets.QHeaderView.Stretch)
        table.setHorizontalHeader(header)
        table.setItemDelegate(CenterDelegate())
        self.editorData = editorData
        self._model = model
        self._table = table

        tableButtons = QtWidgets.QVBoxLayout()
        button = QtWidgets.QPushButton('+')
        button.setToolTip('Append a new row')
        button.clicked.connect(self.addButtonClicked)
        font = button.font()
        font.setPointSize(14)
        button.setFont(font)
        button.setFixedWidth(28)
        button.setFixedHeight(28)
        tableButtons.addWidget(button)
        button = QtWidgets.QPushButton('-')
        button.setToolTip('Remove selected rows')

        button.clicked.connect(self.removeButtonClicked)
        font = button.font()
        font.setPointSize(14)
        button.setFont(font)
        button.setFixedWidth(28)
        button.setFixedHeight(28)
        tableButtons.addWidget(button)
        tableButtons.addStretch(1)

        tableLayout = QtWidgets.QHBoxLayout()
        tableLayout.addLayout(tableButtons)
        tableLayout.addWidget(table)

        self.setLayout(tableLayout)

    def removeButtonClicked(self):
        rows = set(index.row() for index in self._table.selectedIndexes())
        rows = sorted(rows, reverse=True)
        for row in rows:
            self.editorData.pop(row)
        self._model.layoutChanged.emit()
        self._table.clearSelection()

    def addButtonClicked(self):
        # row = len(self.editorData)
        self.editorData.append(['', '', ''])
        self._model.layoutChanged.emit()


def isspace(s):
    return s.isspace() or len(s) == 0

def gformat(data): # Returns a string
    if not isinstance(data, list):
        raise Exception('data is not an instance of list')
    s = ''
    for i, entry in enumerate(data):
        noHead = isspace(entry[0])
        noBody = isspace(entry[2])
        if noHead and not noBody:
            err = 'Line {} is incorrect because no head is given'.format(i)
            return ('', err)
        elif not noHead and noBody:
            s += '{} -> _e\n'.format(entry[0])
        elif not noHead:
            s += '{} -> {}\n'.format(entry[0], entry[2])
    return (s, None)

class GrammarEditorTab(QtWidgets.QWidget):
    def __init__(self, opts: LRParserOptions, lrwindow) -> None:
        super().__init__()
        self._opts = opts
        self._lrwindow = lrwindow

        layout = QtWidgets.QVBoxLayout()

        table = EditorTable()
        self.table = table

        buttonLayout = QtWidgets.QHBoxLayout()
        button = QtWidgets.QPushButton('Start')
        button.setCheckable(False)
        button.clicked.connect(self.startButtonClicked)
        button.setFixedWidth(100)
        buttonLayout.addStretch(1)
        buttonLayout.addWidget(button)
        buttonLayout.addStretch(1)

        info = QtWidgets.QLabel()
        info.setText(
            "Enter productions in the above table. \nAll consecutive symbols should be separated by whitespaces."
        )
        font = info.font()
        font.setPointSize(12)
        info.setFont(font)
        info.setAlignment(Qt.AlignCenter)

        layout.addWidget(table)
        layout.addSpacing(16)
        layout.addWidget(info)
        layout.addSpacing(16)
        layout.addLayout(buttonLayout)

        self.setLayout(layout)

    def startButtonClicked(self):
        data = self.table.editorData
        s, err = gformat(data)
        if not err and not isspace(s):
            print('Grammar rules:')
            print(s, end='')
            if '_temp_file' not in self.__dict__:
                self._temp_file = tempfile.NamedTemporaryFile(delete=False)
            file = self._temp_file
            file.truncate(0)
            file.write(s.encode('utf-8'))
            file.flush()
            self._opts.grammarFile = file.name
            self._lrwindow.nextButtonPressed(0)
            # TODO: What if there is already a panel? Then editing the grammer
            # would be of no use.
        elif err:
            print('Error:', err)