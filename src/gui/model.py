from copy import copy, deepcopy
from typing_extensions import Protocol
from typing import Any, Generic, List, Optional, Tuple, TypeVar
from PySide6 import QtCore, QtWidgets, QtGui, QtStateMachine
from PySide6.QtCore import Qt
from collections import deque
import sys
import subprocess
import os
import re


class Symbol:
    def __init__(self, name: str, is_term: Optional[bool],
                 is_start: Optional[bool]):
        self.name = name
        self.is_term = is_term
        self.is_start = is_start
        self.productions: List[int] = []
        self.nullable: Optional[bool] = None
        self.first: set = set()
        self.follow: set = set()

    @staticmethod
    def new() -> 'Symbol':
        return Symbol('', None, None)


class Production:
    def __init__(self, head: int, body: List[int]):
        self.head = head
        self.body = body

    @staticmethod
    def new() -> 'Production':
        return Production(-1, [])

    def stringify(production: 'Production',
                  symbols: List[Symbol],
                  arrow: str = '->') -> str:
        head = int(production.head)
        s = '{} {} {}'.format(
            symbols[head].name, arrow,
            ' '.join([symbols[i].name for i in production.body]))
        return s


def isspace(s):
    return s == None or s.isspace() or len(s) == 0


class GrowingList(list):
    def __init__(self, factory=None, initial: int = 0):
        super().__init__()
        if factory:
            self._factory = factory
        else:
            self._factory = lambda: None
        assert isinstance(initial, int)
        if initial > 0:
            self.extend([self._factory() for _ in range(initial)])

    def __setitem__(self, index, value):
        if index >= len(self):
            self.extend(
                [self._factory() for _ in range(0, index + 1 - len(self))])
        list.__setitem__(self, index, value)

    def __getitem__(self, index):
        if index >= len(self):
            self.extend(
                [self._factory() for _ in range(0, index + 1 - len(self))])
        return list.__getitem__(self, index)


# class DequeMapper:
#     def __init__(self,
#                  proxy=None,
#                  mapMethod=lambda x: x,
#                  rmapMethod=lambda x: x) -> None:
#         self.proxy = proxy
#         self.mapMethod = mapMethod
#         self.rmapMethod = rmapMethod

#     def reset(self, proxy):
#         self.proxy = proxy

#     def append(self, item):
#         item = self.mapMethod(item)
#         return self.proxy.append(item)

#     def appendleft(self, item):
#         item = self.mapMethod(item)
#         return self.proxy.appendleft(item)

#     def pop(self):
#         item = self.proxy.pop()
#         return self.rmapMethod(item)

#     def popleft(self):
#         item = self.proxy.popleft()
#         return self.rmapMethod(item)

# def createEnv():
#     symbol = GrowingList(Symbol.new)
#     production = GrowingList(Production.new)

#     # table can only be accessed after symbol is established fully.
#     table = GrowingList(lambda: GrowingList(lambda: set()))

#     return {
#         'symbol': symbol,
#         'production': production,
#         'table': table,
#         'state_stack': None,
#         'symbol_stack': None,
#         'input_queue': None,
#     }


class DequeProtocol(Protocol):
    def append(self, x: int) -> None:
        raise NotImplementedError

    def appendleft(self, x: int) -> None:
        raise NotImplementedError

    def pop(self) -> Any:
        raise NotImplementedError

    def popleft(self) -> Any:
        raise NotImplementedError

    def clear(self) -> Any:
        raise NotImplementedError


class ParsingEnv():
    def __init__(self) -> None:
        self.symbol = GrowingList(Symbol.new)
        self.production = GrowingList(Production.new)
        self.table = GrowingList(lambda: GrowingList(lambda: set()))
        self.state_stack: Optional[DequeProtocol] = None
        self.symbol_stack: Optional[DequeProtocol] = None
        self.input_queue: Optional[DequeProtocol] = None
        self.show = lambda s: None
        self.section = lambda s: None
        self.addState = lambda a, s: None
        self.updateState = lambda a, s: None
        self.addEdge = lambda a, b, s: None
        self.setStart = lambda s: None
        self.setFinal = lambda s: None

    def copy(self) -> 'ParsingEnv':
        env = ParsingEnv()
        env.symbol = deepcopy(self.symbol)
        env.production = deepcopy(self.production)
        env.table = deepcopy(self.table)
        env.state_stack = deepcopy(self.state_stack)
        env.symbol_stack = deepcopy(self.symbol_stack)
        env.input_queue = deepcopy(self.input_queue)
        return env


class LRParserOptions:
    def __init__(self,
                 out: str,
                 bin: str,
                 grammar: str,
                 parser: str = 'SLR(1)') -> None:
        self.out = out  # Temporary directory
        self.bin = bin  # Executable file location
        self.grammar = grammar  # Grammar file path
        self.parser = parser

    def copy(self):
        return LRParserOptions(self.out, self.bin, self.grammar, self.parser)


class TextDialog(QtWidgets.QDialog):
    def __init__(self, parent, text, align=Qt.AlignCenter) -> None:
        super().__init__(parent)

        label = QtWidgets.QLabel()
        label.setText(text)
        label.setAlignment(align)

        labelLayout = QtWidgets.QHBoxLayout()
        # labelLayout.addSpacing(8)
        labelLayout.addWidget(label)
        # labelLayout.addSpacing(8)

        button = QtWidgets.QPushButton('OK')
        button.setCheckable(False)
        button.clicked.connect(self.close)
        button.setFixedWidth(100)

        buttonLayout = QtWidgets.QHBoxLayout()
        buttonLayout.addStretch(1)
        buttonLayout.addWidget(button)
        buttonLayout.addStretch(1)

        layout = QtWidgets.QVBoxLayout()
        # layout.addSpacing(16)
        layout.addLayout(labelLayout)
        layout.addSpacing(8)
        layout.addLayout(buttonLayout)
        # layout.addSpacing(16)

        self.setLayout(layout)
        self.setModal(True)
        self.setWindowTitle('Dialog')


# Returns (steps, err)
def getLRSteps(opts: LRParserOptions,
               test: str = '') -> Tuple[List[str], Optional[str]]:
    parser = {
        'LR(0)': 'lr0',
        'SLR(1)': 'slr',
        'LALR(1)': 'lalr',
        'LR(1)': 'lr1'
    }[opts.parser]

    cmd = [opts.bin, '-t', parser, '-o', opts.out, '-g', opts.grammar]

    if isspace(test):
        test = '$'

    print(' '.join(cmd))

    sub = subprocess.Popen(cmd,
                           stdin=subprocess.PIPE,
                           stdout=subprocess.DEVNULL)
    try:
        sub.communicate(input=test.encode('utf-8'), timeout=1.5)
    except subprocess.TimeoutExpired:
        err = 'Timed out'
        return ([], err)

    if sub.returncode != 0:
        err = 'Return code {}'.format(sub.returncode)
        return ([], err)

    # print('Output written to temporary directory', opts.outDir, sep=' ')

    with open(os.path.join(opts.out, 'steps.py'), mode='r',
              encoding='utf-8') as f:
        steps = f.read().strip().split(sep='\n')

    return (steps, None)


# Returns the line number at the stopping point.
def execNext(code: list, line: int, env: dict) -> int:
    leng = len(code)
    while line < leng:
        exec(code[line], env)
        if code[line].startswith('section('):
            break
        if code[line].startswith('show('):
            break
        line += 1
    return line


# Returns the line number at the stopping point.
def skipNext(code: list, line: int, env: dict) -> int:
    leng = len(code)
    while line < leng:
        if code[line].startswith('section('):
            exec(code[line], env)
            break
        if code[line].startswith('show('):
            break
        line += 1
    return line


# Returns the line number at the stopping point.
def execSection(code: list, line: int, env: dict):
    leng = len(code)
    while line < leng:
        exec(code[line], env)
        if code[line].startswith('section('):
            break
        line += 1
    return line


# Returns the line number at the stopping point.
def skipSection(code: list, line: int, env: dict):
    leng = len(code)
    while line < leng:
        if code[line].startswith('section('):
            exec(code[line], env)
            break
        line += 1
    return line