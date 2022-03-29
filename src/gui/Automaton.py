import signal
from typing import Any, List, Optional, Union
# from PySide6 import QtWidgets, QtCore, QtGui
# from PySide6.QtCore import Qt
from PyQt5 import QtWidgets, QtCore, QtGui
from PyQt5.QtCore import Qt
import sys
import random
import math
from Model import *
from ParseTable import *
from GuiConfig import *

signal.signal(signal.SIGINT, signal.SIG_DFL)


class StateItem(QtWidgets.QGraphicsEllipseItem):
    def __init__(
            self,
            centerX: float,
            centerY: float,
            radius: float,
            text: str,
            description: Optional[str] = None,
            brush: Union[QtGui.QBrush, QtCore.Qt.BrushStyle,
                         QtCore.Qt.GlobalColor, QtGui.QColor,
                         QtGui.QGradient, QtGui.QImage,
                         QtGui.QPixmap] = Qt.yellow,
            parent: Optional[QtWidgets.QGraphicsItem] = None) -> None:
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
        self.setDescription(description)  # type: ignore
        self.setAcceptDrops(True)

        label = QtWidgets.QGraphicsSimpleTextItem(parent=self)
        label.setText(text)
        font = label.font()
        font.setPointSize(config.font.size.small)
        font.setFamily('Lato')
        label.setFont(font)
        metrics = QtGui.QFontMetrics(font)
        w = metrics.width(text)
        h = metrics.height()
        # charW, charH = 7, 13  # An estimate of default font size
        # label.setPos(centerX - len(text) / 2.0 * charW, centerY - charH / 2.0)
        label.setPos(centerX - w / 2, centerY - h / 2)

        self._start_sign = QtWidgets.QGraphicsPathItem(parent=self) # ⊳
        sign = self._start_sign
        a = QtCore.QPointF(centerX - radius, centerY)
        off = radius * 0.6
        b = QtCore.QPointF(a.x() - off, a.y() + off)
        c = QtCore.QPointF(a.x() - off, a.y() - off)
        path = QtGui.QPainterPath()
        path.moveTo(a)
        path.lineTo(b)
        path.lineTo(c)
        path.lineTo(a)
        sign.setPath(path)
        pen = sign.pen()
        pen.setColor(Qt.transparent)
        sign.setPen(pen)

    def setDescription(self, description: str) -> None:
        tooltip = description.splitlines()
        tooltip = sorted(tooltip)
        self.setToolTip('\n'.join(tooltip))

    def radius(self) -> float:
        return self._radius

    def center(self) -> QtCore.QPointF:
        return self._center

    def itemChange(self,
                   change: QtWidgets.QGraphicsItem.GraphicsItemChange,
                   value: Any) -> Any:
        result = super().itemChange(change, value)

        if change == QtWidgets.QGraphicsItem.ItemPositionChange and self.scene(
        ):
            self.prepareGeometryChange()
            for line in self.lines:
                line.updatePath()

        return result

    def setFinal(self, final: bool = True) -> None:
        self.prepareGeometryChange()
        self._final = final

    def setStart(self, start: bool = True) -> None:
        self.prepareGeometryChange()
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
            distance = 4.0  # Constant.
            width = (self._radius - distance) * 2.0
            r.setSize(QtCore.QSizeF(width, width))
            r.translate(QtCore.QPointF(distance, distance))
            painter.drawEllipse(r)

        # painter.setBrush(Qt.transparent)
        # painter.drawRect(self.rect())


class EdgeItem(QtWidgets.QGraphicsPathItem):
    def __init__(self,
                 src: StateItem,
                 dest: StateItem,
                 text: str = '') -> None:
        super().__init__()
        self.src = src
        self.dest = dest
        self.text = text
        label = QtWidgets.QGraphicsSimpleTextItem(self)
        font = label.font()
        font.setPointSize(config.font.size.small)
        font.setFamily('Lato')
        label.setFont(font)
        self.label = label
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

    def boundingRect(self) -> QtCore.QRectF:
        # Include label.
        r = super().boundingRect()
        w = len(self.text) * 7.0
        h = 20.0
        r.setWidth(r.width() + w * 2)
        r.setHeight(r.height() + h)
        r.translate(-w, -h)
        return r

    def updatePath(self) -> None:
        self.prepareGeometryChange()

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
        self.label.setText(self.text)
        self.label.setPos(x, y - 18.0)

        self.setPath(path)

    def paint(self,
              painter: QtGui.QPainter,
              option: QtWidgets.QStyleOptionGraphicsItem,
              widget: Optional[QtWidgets.QWidget] = None) -> None:
        painter.setPen(self.pen())
        painter.setRenderHint(QtGui.QPainter.TextAntialiasing, True)
        painter.setRenderHint(QtGui.QPainter.Antialiasing, True)
        painter.setBrush(Qt.black)
        painter.drawPath(self.path())

        # painter.setBrush(Qt.transparent)
        # painter.drawRect(self.boundingRect())


class AutomatonView(QtWidgets.QGraphicsView):
    def __init__(self, *args, **kwargs) -> None:
        super().__init__(*args, **kwargs)

        scene = QtWidgets.QGraphicsScene()
        self.setScene(scene)
        self.setAlignment(Qt.AlignTop | Qt.AlignLeft)  # type: ignore
        self._states = GrowingList(lambda: None)
        self._edges: List[EdgeItem] = []

    def addState(self, index: int, description: str) -> int:
        radius = config.state.radius
        x = random.uniform(radius, self.rect().width() - radius)
        y = random.uniform(radius, self.rect().height() - radius)
        state = StateItem(x, y, radius, 's{}'.format(index), description)
        self._states[index] = state
        self.scene().addItem(state)
        return index

    def updateState(self, index: int, description: str) -> None:
        stateItem: StateItem = self._states[index]
        stateItem.setToolTip(description)

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

    def setDescription(self, state: int, description: str) -> None:
        stateItem: StateItem = self._states[state]
        stateItem.setDescription(description)


class AutomatonTab(QtWidgets.QWidget):
    def __init__(self, parent: QtWidgets.QWidget, opts: LRParserOptions, env: ParsingEnv) -> None:
        super().__init__(parent)  # type: ignore
        # self._opts = opts
        self._env = env
        self._parent = parent
        self._section = ''
        self._opts = opts

        self._line = 0
        self._code, err = getLRSteps(opts)
        if err:
            print(err)
            raise Exception()

        automatonView = AutomatonView()

        label = QtWidgets.QLabel()
        font = label.font()
        font.setPointSize(config.font.size.normal)
        label.setFont(font)
        label.setAlignment(Qt.AlignCenter)  # type: ignore
        label.setText('Press "Continue" button to start.')
        self._label = label

        buttons = QtWidgets.QHBoxLayout()
        continueButton = QtWidgets.QPushButton('Continue')
        self.continueButton = continueButton
        continueButton.clicked.connect(self.continueButtonClicked)
        continueButton.setFixedWidth(config.button.width)
        finishButton = QtWidgets.QPushButton('Finish')
        finishButton.setFixedWidth(config.button.width)
        finishButton.clicked.connect(self.finishButtonClicked)
        self.finishButton = finishButton
        buttons.addStretch(1)
        buttons.addWidget(continueButton)
        buttons.addWidget(finishButton)
        buttons.addStretch(1)
        self.buttonsLayout = buttons

        layout = QtWidgets.QVBoxLayout()
        layout.addWidget(automatonView)
        layout.addSpacing(24)
        layout.addWidget(label)
        layout.addSpacing(16)
        layout.addLayout(buttons)

        self.setLayout(layout)

        m = automatonView
        self._env.addEdge = lambda a, b, s: m.addEdge(a, b, s)  # type: ignore
        self._env.addState = lambda a, s: m.addState(a, s)  # type: ignore
        self._env.updateState = lambda a, s: m.updateState(a, s)  # type: ignore
        self._env.setFinal = lambda s: m.setFinal(s, True)
        self._env.setStart = lambda s: m.setStart(s, True)
        self._env.section = lambda s: self.setSection(s)
        self._env.show = lambda s: label.setText(s)

        while self._line < len(self._code) and self._section != 'DFA':
            self._line = skipSection(self._code, self._line, self._env.__dict__)
            self._line += 1
        
    def setSection(self, section) -> None:
        self._section = section

    def continueButtonClicked(self) -> None:
        if self._line < len(self._code):
            self._line = execNext(self._code, self._line, self._env.__dict__)
            self._line += 1
        if self._line >= len(self._code) or self._section != 'DFA':
            self.finish()

    def finishButtonClicked(self) -> None:
        while self._line < len(self._code) and self._section == 'DFA':
            self._line = execSection(self._code, self._line, self._env.__dict__)
            self._line += 1
        self.finish()

    def finish(self) -> None:
        self.continueButton.setDisabled(True)
        self.finishButton.setDisabled(True)
        self.buttonsLayout.removeWidget(self.continueButton)
        self.buttonsLayout.removeWidget(self.finishButton)
        self.continueButton.deleteLater()
        self.finishButton.deleteLater()
        next = QtWidgets.QPushButton('Next')
        next.setFixedWidth(config.button.width)
        next.clicked.connect(self.nextButtonPressed)
        self.buttonsLayout.insertWidget(1, next)
        self._label.setText('DFA is built.')

    def nextButtonPressed(self) -> None:
        tab = TableTab(self._parent, self._opts.copy(), self._env.copy())
        self._parent.requestNext(tab, self._opts.parser + ' Table')