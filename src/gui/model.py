from copy import deepcopy
from typing_extensions import Protocol
from typing import Any, Callable, Deque, Dict, List, Optional, Set, Tuple
# from PySide6 import QtCore, QtWidgets, QtGui, QtSvgWidgets
# from PySide6.QtCore import Qt
from PyQt5 import QtCore, QtWidgets, QtGui
from PyQt5.QtCore import Qt
from PyQt5.QtSvg import QSvgWidget
from collections import deque
from GuiConfig import *
import sys
import subprocess
import os
import graphviz

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


class TreeNode:
    def __init__(self, label: str) -> None:
        self.label = label
        self.children: Deque[TreeNode] = deque()

    @staticmethod
    def toJsonNode(obj: 'TreeNode', indentLevel: int, step: int) -> str:
        identStr = indentLevel * step * ' '
        s = identStr + '"{}" : {{'.format(obj.label)
        if len(obj.children) > 0:
            s += '\n'
            sub = map(
                lambda node: TreeNode.toJsonNode(node, indentLevel + 1, step),
                obj.children)
            s += (', \n').join(sub)
            s += '\n' + identStr
        s += '}'
        return s

    def toJson(self, indentLevel: int = 0, step: int = 4) -> str:
        outterIdent = indentLevel * step * ' '
        return '{}{{\n{}\n{}}}'.format(
            outterIdent, TreeNode.toJsonNode(self, indentLevel + 1, step),
            outterIdent)


class Forest:
    def __init__(self) -> None:
        self.nodes = GrowingList()
        self.trees: Set[TreeNode] = set()
        # self.labels: Dict[int, str] = {} # Used by networkx
        self._threadPool = QtCore.QThreadPool()

    def addNode(self, index: int, label: str) -> None:
        if self.nodes[index]:
            raise Exception('Node with index {} already exists'.format(index))
        node = TreeNode(label)
        self.nodes[index] = node
        self.trees.add(node)
        # self.labels[index] = label
        # node.index = index

    def setParent(self, child: int, parent: int) -> None:
        parentNode = self.nodes[parent]
        childNode = self.nodes[child]
        assert parentNode != None and isinstance(parentNode, TreeNode)
        assert childNode != None and isinstance(childNode, TreeNode)
        parentNode.children.append(childNode)
        self.trees.discard(childNode)

    def clear(self) -> None:
        self.nodes.clear()
        self.trees.clear()

    def toJson(self, indent: int = 4) -> str:
        return '[\n{}\n]'.format(',\n'.join(
            map(lambda node: node.toJson(1, indent),
                self.trees))) if len(self.trees) > 0 else '[]'

    def toSvgSync(self) -> bytes:
        g = graphviz.Graph()
        deq: Deque[TreeNode] = deque(self.trees)
        while len(deq) > 0:
            node = deq.popleft()
            g.node(str(id(node)), node.label)
            for child in node.children:
                deq.append(child)
                g.edge(str(id(node)), str(id(child)))
        g.node_attr['shape'] = 'circle'

        # Slow. 160 ~ 240 ms.
        # This is because graphviz on Windows is slow. On *unix, it should take
        # ~30 ms.
        data = g.pipe('svg') 
        return data

    def toSvg(self, callback: Callable[[bytes], None]) -> None:
        worker = Forest.Worker(self.toSvgSync)
        worker.signal.result.connect(callback)
        self._threadPool.start(worker)

    class WorkerSignal(QtCore.QObject):
        result = QtCore.pyqtSignal(bytes)

    class Worker(QtCore.QRunnable):
        def __init__(self, func, *args, **kwargs) -> None:
            super().__init__()
            self.func = func
            self.args = args
            self.kwargs = kwargs
            self.signal = Forest.WorkerSignal()

        # @QtCore.Slot()
        def run(self) -> None:
            data = self.func(*self.args, **self.kwargs)
            self.signal.result.emit(data)


if __name__ == '__main__':
    forest = Forest()
    astAddNode = lambda index, label: forest.addNode(index, label)
    astSetParent = lambda child, parent: forest.setParent(child, parent)
    astAddNode(0, "ID")
    astAddNode(1, "fac")
    astSetParent(0, 1)
    astAddNode(2, "term")
    astSetParent(1, 2)
    astAddNode(3, "*")
    astAddNode(4, "ID")
    astAddNode(5, "fac")
    astSetParent(4, 5)
    astAddNode(6, "term")
    astSetParent(5, 6)
    astSetParent(3, 6)
    astSetParent(2, 6)
    astAddNode(7, "*")
    astAddNode(8, "(")
    astAddNode(9, "ID")
    astAddNode(10, "fac")
    astSetParent(9, 10)
    astAddNode(11, "term")
    astSetParent(10, 11)
    astAddNode(12, "exp")
    astSetParent(11, 12)
    astAddNode(13, "+")
    astAddNode(14, "ID")
    astAddNode(15, "fac")
    astSetParent(14, 15)
    astAddNode(16, "term")
    astSetParent(15, 16)
    astAddNode(17, "exp")
    astSetParent(16, 17)
    astSetParent(13, 17)
    astSetParent(12, 17)
    astAddNode(18, ")")
    astAddNode(19, "fac")
    astSetParent(18, 19)
    astSetParent(17, 19)
    astSetParent(8, 19)
    astAddNode(20, "term")
    astSetParent(19, 20)
    astSetParent(7, 20)
    astSetParent(6, 20)
    astAddNode(21, "exp")
    astSetParent(20, 21)
    astAddNode(22, "+")
    astAddNode(23, "ID")
    astAddNode(24, "fac")
    astSetParent(23, 24)
    astAddNode(25, "term")
    astSetParent(24, 25)
    astAddNode(26, "exp")
    astSetParent(25, 26)
    astSetParent(22, 26)
    astSetParent(21, 26)
    print(forest.toJson(4))

    # forest.showImage()
    app = QtWidgets.QApplication([])
    svg = forest.toSvgSync()
    svgItem = QSvgWidget()
    svgItem.renderer().load(svg)
    svgItem.show()

    sys.exit(app.exec())
    

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
        self.astAddNode = lambda index, label: None
        self.astSetParent = lambda child, parent: None
        # self.astSetRoot = lambda index: None

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
        button.setFixedWidth(config.button.width)

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
        # self.resize(config.dialog.width, config.dialog.height)


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