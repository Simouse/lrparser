from PySide6 import QtCore, QtWidgets, QtGui
from PySide6.QtCore import Qt
from collections import deque
import sys

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

class GrowingList(list):
    def __init__(self, factory, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self._factory = factory

    def __setitem__(self, index, value):
        if index >= len(self):
            self.extend([self._factory() for _ in range(0, index + 1 - len(self))])
        list.__setitem__(self, index, value)

    def __getitem__(self, index):
        if index >= len(self):
            self.extend([self._factory() for _ in range(0, index + 1 - len(self))])
        return list.__getitem__(self, index)


def newEnv():
    symbol = GrowingList(Symbol.new)
    production = GrowingList(Production.new)
    table = GrowingList(lambda: GrowingList(lambda: set()))
    return {
        'symbol': symbol,
        'production': production,
        'table': table,
        'state_stack': deque(),
        'symbol_stack': deque(),
        'input_queue': deque(),
    }

class LRParserOptions:
    def __init__(self, outDir, exePath, grammarFile) -> None:
        self.outDir = outDir
        self.exePath = exePath
        self.grammarFile = grammarFile
        