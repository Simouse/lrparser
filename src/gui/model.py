from PySide6 import QtCore, QtWidgets, QtGui
from PySide6.QtCore import Qt
from collections import deque

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


def createExecEnv(nsym: int, nprod: int, maxstate: int):
    return {
        'symbol': [Symbol.new() for _ in range(0, nsym)],
        'production': [Production.new() for _ in range(0, nprod)],
        'table': [[set() for _ in range(0, 10)] for _ in range(0, maxstate)],
        'state_stack': deque(),
        'symbol_stack': deque(),
        'input_queue': deque(),
    }

class LRParserOptions:
    def __init__(self, outDir, exePath, grammarFile) -> None:
        self.outDir = outDir
        self.exePath = exePath
        self.grammarFile = grammarFile
        