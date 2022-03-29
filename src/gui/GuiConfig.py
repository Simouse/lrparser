class dotdict(dict):
    """dot.notation access to dictionary attributes"""
    __getattr__ = dict.get
    __setattr__ = dict.__setitem__
    __delattr__ = dict.__delitem__

class GuiConfig(dotdict):
    def __init__(self) -> None:
        self.font = dotdict()
        self.font.size = dotdict()
        self.font.size.extrasmall = 10
        self.font.size.small = 12
        self.font.size.normal = 14
        self.font.size.large = 16

        self.win = dotdict()
        self.win.width = 1000
        self.win.height = 700

        self.button = dotdict()
        self.button.width = 135

        self.state = dotdict()
        self.state.radius = 28

        self.table = dotdict()
        self.table.cell = dotdict()
        self.table.cell.minwidth = 60

config = GuiConfig()
