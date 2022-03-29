from typing import Tuple
import tempfile
# from PySide6 import QtCore, QtWidgets, QtGui
# from PySide6.QtCore import Qt
from PyQt5 import QtCore, QtWidgets, QtGui
from PyQt5.QtCore import Qt
from Attribute import AttributeTab
from Model import *
from GuiConfig import *

class ProductionEditorModel(QtCore.QAbstractTableModel):
    def __init__(self, data) -> None:
        super().__init__()
        self._data = data
        self._header = ['Head', '', 'Body']
        self._item_font = QtGui.QFont()
        self._item_font.setPointSize(config.font.size.large)
        self._disabled = False

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
        if self._disabled:
            return Qt.ItemIsSelectable | Qt.ItemIsEnabled
        if index.column() != 1:
            return Qt.ItemIsSelectable | Qt.ItemIsEnabled | Qt.ItemIsEditable
        return Qt.ItemIsSelectable | Qt.ItemIsEnabled

    def setData(self, index, value, role):
        if role == Qt.EditRole:
            self._data[index.row()][index.column()] = value
            return True

    def disableEditing(self, disable: bool = True):
        self._disabled = disable


class CenterDelegate(QtWidgets.QStyledItemDelegate):
    def __init__(self) -> None:
        super().__init__()
        self._item_font = QtGui.QFont()
        self._item_font.setPointSize(config.font.size.large)

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
        # editorData = [['', '', ''], ['', '', ''], ['', '', '']]
        # TODO:
        editorData = [
            ['exp', '->', 'exp + term'],
            ['exp', '->', 'term'],
            ['term', '->', 'term * fac'],
            ['term', '->', 'fac'],
            ['fac', '->', 'ID'],
            ['fac', '->', '( exp )'],
        ]
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
        self._add_button = button
        font = button.font()
        font.setPointSize(config.font.size.large)
        button.setFont(font)
        button.setFixedWidth(28)
        button.setFixedHeight(28)
        tableButtons.addWidget(button)
        button = QtWidgets.QPushButton('-')
        button.setToolTip('Remove selected rows')
        self._remove_button = button

        button.clicked.connect(self.removeButtonClicked)
        font = button.font()
        font.setPointSize(config.font.size.large)
        button.setFont(font)
        button.setFixedWidth(28)
        button.setFixedHeight(28)
        tableButtons.addWidget(button)
        tableButtons.addStretch(1)

        tableLayout = QtWidgets.QHBoxLayout()
        tableLayout.addLayout(tableButtons)
        tableLayout.addWidget(table)

        self.setLayout(tableLayout)

    def removeButtonClicked(self) -> None:
        rowSet = set(index.row() for index in self._table.selectedIndexes())
        rows = sorted(rowSet, reverse=True)
        for row in rows:
            self.editorData.pop(row)
        self._model.layoutChanged.emit()
        self._table.clearSelection()

    def addButtonClicked(self) -> None:
        # row = len(self.editorData)
        self.editorData.append(['', '', ''])
        self._model.layoutChanged.emit()

    def disableEditing(self, disable: bool = True) -> None:
        self._remove_button.setDisabled(disable)
        self._add_button.setDisabled(disable)
        self._model.disableEditing(disable)


def grammarFormat(data) -> Tuple[str, Optional[str]]:
    if not isinstance(data, list):
        raise Exception('data is not an instance of list')

    forbidden = ['\\e', '$', '_e', '\\epsilon', '|', '%']
    violations = set()
    s = ''

    for i, entry in enumerate(data):
        head = None if isspace(entry[0]) else entry[0].strip()
        body = None if isspace(entry[2]) else entry[2].strip()

        if not head and body:
            err = 'Line {} is incorrect because no head is given'.format(i)
            return ('', err)
        elif head:
            check = head.split()

            if len(check) > 1:
                err = 'Head of line {} contains whitespaces'.format(i)
                return ('', err)

            if body:
                s += '{} -> {}\n'.format(head, body)
                check.extend(body.split())
            else:
                s += '{} -> _e\n'.format(head)

            for token in check:
                if token in forbidden:
                    violations.add(token)

    if len(violations) == 0:
        return (s, None)
    else:
        s = ', '.join(violations)
        err = 'Please remove those keywords for parsers to work:\n{}'.format(s)
        return ('', err)


class GrammarTab(QtWidgets.QWidget):
    def __init__(self, parent, opts: LRParserOptions) -> None:
        super().__init__()

        # self._tag = tag
        self._opts = opts
        self._lrwindow = parent
        self._disabled = False

        layout = QtWidgets.QVBoxLayout()

        table = EditorTable()
        self.table = table

        buttonLayout = QtWidgets.QHBoxLayout()
        button = QtWidgets.QPushButton('Start')
        button.setCheckable(False)
        button.clicked.connect(self.startButtonClicked)
        button.setFixedWidth(config.button.width)
        buttonLayout.addStretch(1)
        buttonLayout.addWidget(button)
        buttonLayout.addStretch(1)
        self._start_button = button

        info = QtWidgets.QLabel()
        self._normal_info = "Enter productions in the above table. \nAll consecutive symbols should be separated by whitespaces."
        self._disabled_info = 'Grammar is locked for further procedures.\nNow it is read-only.'
        info.setText(self._normal_info)
        font = info.font()
        font.setPointSize(config.font.size.normal)
        info.setFont(font)
        info.setAlignment(Qt.AlignCenter) # type: ignore
        self._info = info

        layout.addWidget(table)
        layout.addSpacing(16)
        layout.addWidget(info)
        layout.addSpacing(16)
        layout.addLayout(buttonLayout)

        self.setLayout(layout)

    def startButtonClicked(self) -> None:
        data = self.table.editorData
        s, err = grammarFormat(data)
        if not err:
            if isspace(s):
                dialog = TextDialog(self._lrwindow,
                                    'Please provide at least one production.')
                dialog.show()
            else:
                if '_temp_file' not in self.__dict__:
                    self._temp_file = tempfile.NamedTemporaryFile(delete=False)
                file = self._temp_file
                file.seek(0)  # Important for later writing
                file.truncate(0)
                file.write(s.encode('utf-8'))
                file.flush()
                self._opts.grammar = file.name
                # self._lrwindow.requestNext(self)
                attributeTab = AttributeTab(self._lrwindow, self._opts.copy())
                self._lrwindow.requestNext(attributeTab, 'Attributes')
                # self._lrwindow.requestNext(self._tag)
        else:
            # print('Error:', err)
            dialog = TextDialog(self._lrwindow, 'Error: {}'.format(err))
            dialog.show()

    # Should be called only once.
    def disableEditing(self, disable: bool = True) -> None:
        self._start_button.setDisabled(disable)
        self.table.disableEditing(disable)

    # def onRequestNextSuccess(self):
    #     self.disableEditing()
    #     self._disabled = True
    #     self._info.setText(self._disabled_info)

    # def onBecomingOnlyTab(self):
    #     self.disableEditing(False)
    #     self._info.setText(self._normal_info)
