import typing
from PyQt5 import QtGui, QtCore, QtWidgets
from PyQt5.QtCore import Qt
from GuiConfig import *

class LexerDialog(QtWidgets.QDialog):
    def __init__(self, parent, **kwargs) -> None:
        super().__init__(parent, **kwargs)

        self.setWindowTitle('Lexer')

        topLayout = QtWidgets.QVBoxLayout()
        
        splitter = QtWidgets.QSplitter()
        ruleLayout = QtWidgets.QVBoxLayout()
        ruleWidget = QtWidgets.QWidget()
        ruleWidget.setLayout(ruleLayout)

        sourceLayout = QtWidgets.QVBoxLayout()
        sourceWidget = QtWidgets.QWidget()
        sourceWidget.setLayout(sourceLayout)

        splitter.addWidget(ruleWidget)
        splitter.addWidget(sourceWidget)
        splitter.setStretchFactor(0, 1)
        splitter.setStretchFactor(1, 2)
        splitter.setMinimumHeight(440)

        ruleLabel = QtWidgets.QLabel("""<div>
<div align=center>Rules</div>
<div align=left><font size="-0.5">
e.g.<br/>
digit   [0-9]<br/>
number {digit}+(\.{digit}+)?(E[+-]+{digit}+)?</font>
</div>
</div>""")
        ruleEditor = QtWidgets.QTextEdit()
        ruleLayout.addWidget(ruleLabel)
        ruleLayout.addWidget(ruleEditor)

        sourceLabel = QtWidgets.QLabel('Source')
        sourceLabel.setAlignment(Qt.AlignCenter)
        sourceEditor = QtWidgets.QTextEdit()
        sourceEditor.setMinimumWidth(320)
        sourceLayout.addWidget(sourceLabel)
        sourceLayout.addWidget(sourceEditor)

        topLayout.addWidget(splitter, stretch=1)
        button = QtWidgets.QPushButton('Generate tokens')
        button.setMinimumWidth(config.button.width)
        button.setStyleSheet('padding: 4px 16px 4px 16px;')
        button.setToolTip('Generated tokens will be copied into clipboard')
        topLayout.addWidget(button, stretch=0, alignment=Qt.AlignCenter)

        self.setLayout(topLayout)