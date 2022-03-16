import signal
from typing import Any, List, Optional, Union
from PySide6 import QtWidgets, QtCore, QtGui
import PySide6
from PySide6.QtCore import Qt
import sys
import random
import math
from Model import LRParserOptions, ParsingEnv, GrowingList

signal.signal(signal.SIGINT, signal.SIG_DFL)


class StateItem(QtWidgets.QGraphicsEllipseItem):
    def __init__(
            self,
            centerX: float,
            centerY: float,
            radius: float,
            text: str,
            tooltip: Optional[str] = None,
            brush: Union[PySide6.QtGui.QBrush, PySide6.QtCore.Qt.BrushStyle,
                         PySide6.QtCore.Qt.GlobalColor, PySide6.QtGui.QColor,
                         PySide6.QtGui.QGradient, PySide6.QtGui.QImage,
                         PySide6.QtGui.QPixmap] = Qt.yellow,
            parent: Optional[PySide6.QtWidgets.QGraphicsItem] = None) -> None:
        super().__init__(centerX - radius, centerY - radius, radius * 2.0,
                         radius * 2.0, parent)
        # pos is initialized to (0, 0).
        # We do not change pos ourselves, but pos can change when item is dragged.
        self._center = QtCore.QPointF(centerX, centerY)
        self._radius = radius
        self._final = False
        self._start = False
        self.lines: List[QtWidgets.QGraphicsPathItem] = []

        self.setFlag(QtWidgets.QGraphicsItem.ItemIsMovable, True)
        self.setFlag(QtWidgets.QGraphicsItem.ItemSendsGeometryChanges, True)
        self.setBrush(brush)
        self.setToolTip(tooltip)  # type: ignore
        self.setAcceptDrops(True)

        label = QtWidgets.QGraphicsSimpleTextItem(parent=self)
        label.setText(text)
        charW, charH = 7, 13  # An estimate of default font size
        label.setPos(centerX - len(text) / 2.0 * charW, centerY - charH / 2.0)

        self._start_sign = QtWidgets.QGraphicsPathItem(parent=self)
        sign = self._start_sign
        a = QtCore.QPointF(centerX - radius, centerY)
        b = QtCore.QPointF(a.x() - 10, a.y() + 10)
        c = QtCore.QPointF(a.x() - 10, a.y() - 10)
        path = QtGui.QPainterPath()
        path.moveTo(a)
        path.lineTo(b)
        path.lineTo(c)
        path.lineTo(a)
        sign.setPath(path)
        pen = sign.pen()
        pen.setColor(Qt.transparent)
        sign.setPen(pen)

    def radius(self) -> float:
        return self._radius

    def center(self) -> QtCore.QPointF:
        return self._center

    def itemChange(self,
                   change: PySide6.QtWidgets.QGraphicsItem.GraphicsItemChange,
                   value: Any) -> Any:
        result = super().itemChange(change, value)

        if change == QtWidgets.QGraphicsItem.ItemPositionChange and self.scene(
        ):
            for line in self.lines:
                line.updatePath()

        return result

    def setFinal(self, final: bool = True) -> None:
        self._final = final

    def setStart(self, start: bool = True) -> None:
        self._start = start
        sign = self._start_sign
        pen = sign.pen()
        pen.setColor(Qt.black if start else Qt.transparent)
        sign.setPen(pen)

    def paint(self,
              painter: QtGui.QPainter,
              option: QtWidgets.QStyleOptionGraphicsItem,
              widget: Optional[QtWidgets.QWidget] = None) -> None:
        super().paint(painter, option, widget)

        if self._final:  # Draw double circle.
            painter.setBrush(self.brush())
            r = self.rect()
            distance = 8  # Constant.
            r0 = self._radius * 2.0 - distance
            r.setSize(QtCore.QSizeF(r0, r0))
            r.translate(QtCore.QPointF(distance / 2.0, distance / 2.0))
            painter.drawEllipse(r)


class EdgeItem(QtWidgets.QGraphicsPathItem):
    def __init__(self, src: StateItem, dest: StateItem, text: str='') -> None:
        super().__init__()
        self.src = src
        self.dest = dest
        self.text = text
        self.updatePath() 

        src.lines.append(self)
        dest.lines.append(self)

        self.setBrush(QtCore.Qt.black)
        pen = QtGui.QPen()
        pen.setStyle(QtCore.Qt.SolidLine)
        pen.setWidth(1)
        pen.setColor(Qt.black)
        self.setPen(pen)

    def setText(self, text) -> None:
        self.text = text
        self.updatePath()

    def updatePath(self) -> None:
        path = QtGui.QPainterPath()
        center = self.src.center()
        pos = self.src.pos()
        # Source position.
        a = QtCore.QPointF(center.x() + pos.x(), center.y() + pos.y())
        center = self.dest.center()
        pos = self.dest.pos()
        # Destination position.
        b = QtCore.QPointF(center.x() + pos.x(), center.y() + pos.y())
        deg = math.atan2(a.y() - b.y(), a.x() - b.x())  # b -> a

        # Drag a and b back a little, so arrow will not overlap with labels.
        r1 = self.src.radius()
        r2 = self.dest.radius()
        if (a.x() - b.x())**2 + (a.y() - b.y())**2 > (r1 + r2 + 10)**2:
            a.setX(a.x() - r1 * math.cos(deg))
            a.setY(a.y() - r1 * math.sin(deg))
            b.setX(b.x() + r2 * math.cos(deg))
            b.setY(b.y() + r2 * math.sin(deg))

        # Draw line (i.e. '-' in '->').
        path.moveTo(a)
        path.lineTo(b)

        # Draw arrow head (i.e. '>' in '->').
        leng = 8  # Constant.
        deg1 = deg - math.pi / 6.0
        deg2 = deg + math.pi / 6.0
        c1 = QtCore.QPointF(b.x() + math.cos(deg1) * leng,
                            b.y() + math.sin(deg1) * leng)
        c2 = QtCore.QPointF(b.x() + math.cos(deg2) * leng,
                            b.y() + math.sin(deg2) * leng)
        path.moveTo(b)
        path.lineTo(c1)
        path.moveTo(b)
        path.lineTo(c2)

        # Draw line label.
        x = (a.x() + b.x()) / 2.0
        y = (a.y() + b.y()) / 2.0
        p = QtCore.QPointF(x, y)
        font = QtGui.QFont()
        path.addText(p, font, self.text)

        self.setPath(path)

    def paint(self,
              painter: QtGui.QPainter,
              option: QtWidgets.QStyleOptionGraphicsItem,
              widget: Optional[QtWidgets.QWidget] = None) -> None:
        painter.setPen(self.pen())
        painter.setBrush(Qt.black)
        painter.drawPath(self.path())


class AutomatonView(QtWidgets.QGraphicsView):
    def __init__(self, *args, **kwargs) -> None:
        super().__init__(*args, **kwargs)

        scene = QtWidgets.QGraphicsScene()
        self.setScene(scene)
        self.setAlignment(Qt.AlignTop | Qt.AlignLeft)  # type: ignore
        self._states = GrowingList(lambda: None)
        self._edges: List[EdgeItem] = []

    def addState(self, index: int, description: str) -> int:
        radius = 22
        x = random.uniform(radius, 600)
        y = random.uniform(radius, 600)
        state = StateItem(x, y, radius, 's{}'.format(index), description)
        self._states[index] = state
        self.scene().addItem(state)
        return index

    def addEdge(self, a: int, b: int, label: str) -> int:
        A = self._states[a]
        B = self._states[b]
        edge = EdgeItem(A, B, label)
        self.scene().addItem(edge)
        index = len(self._edges)
        self._edges.append(edge)
        return index

    def setFinal(self, state: int, final: bool = True) -> None:
        S = self._states[state]
        S.setFinal(final)

    def setStart(self, state: int, start: bool = True) -> None:
        S = self._states[state]
        S.setStart(start)

class AutomatonTab(QtWidgets.QWidget):
    def __init__(self, parent: QtWidgets.QWidget, opts: LRParserOptions,
                 env: ParsingEnv) -> None:
        super().__init__(parent)  # type: ignore
        self._opts = opts
        self._env = env
        self._parent = parent

        automatonView = AutomatonView()

        label = QtWidgets.QLabel()
        font = label.font()
        font.setPointSize(12)
        label.setFont(font)
        label.setAlignment(Qt.AlignCenter)  # type: ignore
        label.setText('Label')

        buttons = QtWidgets.QHBoxLayout()
        continueButton = QtWidgets.QPushButton('Continue')
        continueButton.setFixedWidth(100)
        finishButton = QtWidgets.QPushButton('Next')
        finishButton.setFixedWidth(100)
        buttons.addStretch(1)
        buttons.addWidget(continueButton)
        buttons.addWidget(finishButton)
        buttons.addStretch(1)

        layout = QtWidgets.QVBoxLayout()
        layout.addWidget(automatonView)
        layout.addSpacing(24)
        layout.addWidget(label)
        layout.addSpacing(16)
        layout.addLayout(buttons)

        self.setLayout(layout)

        # Test
        m = automatonView
        m.addState(0, 'exp\' -> . exp')
        m.addState(1, 'exp\' -> exp .')
        m.addEdge(0, 1, 'exp')
        m.setStart(0)
        m.setFinal(1)

if __name__ == "__main__":
    app = QtWidgets.QApplication(sys.argv)

    view = AutomatonTab(None, None, None)
    view.resize(800, 600)
    view.show()

    # view = QtWebEngineWidgets.QWebEngineView()
    # view.setHtml("""<p>Hello, world</p><p><b>Yes?</b></p><button type='button' onclick='alert(11)'>Click Me!</button>""")
    # view.setContextMenuPolicy(Qt.NoContextMenu)

    # layout = QtWidgets.QVBoxLayout()
    # button = QtWidgets.QPushButton('Push me')
    # button.clicked.connect(lambda : layout.addWidget(view))
    # layout.addWidget(button)

    # widget = QtWidgets.QWidget()
    # widget.setLayout(layout)
    # widget.resize(800, 600)
    # widget.show()

    sys.exit(app.exec())