import subprocess
import tempfile
import typing
from PyQt5 import QtGui, QtCore, QtWidgets
from PyQt5.QtCore import Qt
from GuiConfig import *
from io import StringIO
import re
import sys
from typing import Iterable, Optional
import clipboard
import shutil

from Model import TextDialog

def getlex(lines: Iterable[str]) -> str:
    lexfile = StringIO()
    lexfile.write(
    """%{
#include <ctype.h>
#include <stdio.h>
%}
""")

    tokens = set()
    banset = set()
    maxlen = 1
    pattern = re.compile('{\S+?}')

    for line in lines:
        if line.isspace() or len(line) == 0:
            continue

        line = line.rstrip() + '\n'
        lexfile.write(line)

        for word in pattern.findall(line):
            word = word[1:-1]
            banset.add(word)

        token = line.split()[0]
        maxlen = max(maxlen, len(token))
        tokens.add(token)

    tokens.difference_update(banset)

    lexfile.write('%%\n')
    for token in tokens:
        lexfile.write('{' + token + '}')
        lexfile.write(' ' * (maxlen-len(token)+1))
        lexfile.write(f'{{printf("%s ", "{token}");}}\n')
    lexfile.write('. ' + ' '*(maxlen+1) + '{printf("%s", yytext);}\n')

    lexfile.write(
    """%%
int yywrap() { return 1; }
int main()   { yylex(); printf("\\n"); return 0; }
""")

    lex = lexfile.getvalue()
    return lex

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
        splitter.setStretchFactor(1, 1)
        splitter.setMinimumHeight(440)

        ruleLabel = QtWidgets.QLabel("""<div>
<div align=center>Rules</div>
<div align=left><font size="-0.5">
e.g.<br/>
digit   [0-9]<br/>
number {digit}+(\.{digit}+)?(E[+-]+{digit}+)?</font>
</div>
</div>""")
        ruleEditor = QtWidgets.QPlainTextEdit()
        ruleLayout.addWidget(ruleLabel)
        ruleLayout.addWidget(ruleEditor)

        sourceLabel = QtWidgets.QLabel('Source')
        sourceLabel.setAlignment(Qt.AlignCenter)
        sourceEditor = QtWidgets.QPlainTextEdit()
        sourceEditor.setMinimumWidth(400)
        sourceLayout.addWidget(sourceLabel)
        sourceLayout.addWidget(sourceEditor)

        topLayout.addWidget(splitter, stretch=1)
        button = QtWidgets.QPushButton('Generate tokens')
        button.setMinimumWidth(config.button.width)
        button.setStyleSheet('padding: 4px 16px 4px 16px;')
        button.setToolTip('Generated tokens will be copied into clipboard')
        button.clicked.connect(self.generate)
        topLayout.addWidget(button, stretch=0, alignment=Qt.AlignCenter)

        self.setLayout(topLayout)

        self._ruleEditor = ruleEditor
        self._sourceEditor = sourceEditor
        self._generateButton = button

    def findCompiler(self) -> str:
        for compiler in ['gcc', 'clang']:
            filepath = shutil.which(compiler)
            if filepath and len(filepath.strip()) > 0:
                return filepath
        return ''

    def generate(self) -> None:
        compilerName = self.findCompiler()
        if compilerName.isspace():
            dialog = TextDialog(
                self,
                'Compiler is not found. Expects gcc or clang in PATH before using lexer.'
            )
            dialog.setModal(True)
            dialog.show()
            return

        lexerBinary = tempfile.NamedTemporaryFile()
        lexerBinary.close()

        self._generateButton.setEnabled(False)

        waitDialog = TextDialog(self, 'Process begins. Please wait...')
        waitDialog.show()

        class Slots(QtCore.QObject):
            success = QtCore.pyqtSignal(str)
            failure = QtCore.pyqtSignal(str)
        slots = Slots()

        class LexWorker(QtCore.QRunnable):
            def setSlots(self, slots: QtCore.QObject):
                self.slots = slots

            def setRules(self, rules: str):
                self.rules = rules
            
            def setSource(self, source: str):
                self.source = source

            def run(self):
                # rules = self._ruleEditor.toPlainText()
                rules = self.rules
                lexfile = getlex(rules.splitlines())
                flex = subprocess.Popen(
                    ['flex', '--stdout', '--nounistd',],
                    stdin=subprocess.PIPE,
                    stdout=subprocess.PIPE,
                )
                cc = subprocess.Popen(
                    [compilerName, '-xc', '-', '-o', lexerBinary.name],
                    stdin=subprocess.PIPE,
                    stdout=subprocess.PIPE,
                )
                flexout, _ = flex.communicate(lexfile.encode('UTF-8'))
                ccout, _ = cc.communicate(flexout)
                if cc.returncode != 0:
                    self.slots.failure.emit(f'Error in invoking {compilerName}.')
                else:
                    lexer = subprocess.Popen(
                        [lexerBinary.name],
                        stdin=subprocess.PIPE,
                        stdout=subprocess.PIPE,
                    )
                    source = self.source
                    lexerout, _ = lexer.communicate(source.encode('UTF-8'))
                    tokens = lexerout.decode('UTF-8')
                    clipboard.copy(tokens)
                    self.slots.success.emit(
                        f"""<p>
                        Success. Tokens are copied into clipboard:<br/>
                        <font face="JetBrains Mono">
                        {tokens}
                        </font>
                        </p>"""
                    )

        worker = LexWorker()
        worker.setRules(self._ruleEditor.toPlainText())
        worker.setSource(self._sourceEditor.toPlainText())
        worker.setSlots(slots)

        pool = QtCore.QThreadPool.globalInstance()
        pool.start(worker)

        def onResult(text: str):
            TextDialog(self, text).show()
            waitDialog.close()
            self._generateButton.setEnabled(True)

        slots.success.connect(onResult)
        slots.failure.connect(onResult)


