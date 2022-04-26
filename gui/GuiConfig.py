from dataclasses import dataclass



class dotdict(dict):
    """dot.notation access to dictionary attributes"""
    __getattr__ = dict.get
    __setattr__ = dict.__setitem__
    __delattr__ = dict.__delitem__

# class GuiConfig(dotdict):
#     def __init__(self) -> None:
#         self.font = dotdict()
#         self.font.size = dotdict()
#         self.font.size.extrasmall = 10
#         self.font.size.small = 12
#         self.font.size.normal = 14
#         self.font.size.large = 16

#         self.win = dotdict()
#         self.win.width = 1000
#         self.win.height = 700

#         self.button = dotdict()
#         self.button.width = 135
#         self.button.square = dotdict()
#         self.button.square.width = 36

#         self.state = dotdict()
#         self.state.radius = 28

#         self.table = dotdict()
#         self.table.cell = dotdict()
#         self.table.cell.minwidth = 60

@dataclass
class GuiConfig:
    @dataclass
    class FontConfig:
        @dataclass
        class SizeConfig:
            extrasmall = 11
            small      = 12
            normal     = 13
            large      = 16
        size = SizeConfig()
        # name = 'Noto Sans SC'
        name = 'Murecho'
    font = FontConfig()

    @dataclass
    class WindowConfig:
        width  = 1100
        height = 750
    win = WindowConfig()

    @dataclass
    class ButtonConfig:
        width = 135
        @dataclass
        class ButtonSquareShape:
            width = 36
        square = ButtonSquareShape()
    button = ButtonConfig

    @dataclass
    class StateConfig:
        radius = 28
    state = StateConfig()

    @dataclass
    class TableConfig:
        @dataclass
        class CellConfig:
            minwidth = 60
        cell = CellConfig()
    table = TableConfig()

config = GuiConfig()
