"""Parser for the structured output of the engine's eddumpstate command.

The Python counterpart of edstate.ps1. A pure function over lines, no game
required, so tests/test_edstate.py unit tests it directly.
"""

import re

_STAMP = re.compile(r'^\d{4}-\d{2}-\d{2} \d{2}:\d{2}\.\d{2} ')


class Record(dict):
    """A dict whose keys also read as attributes: state.cam.x, state.sel.have_sel."""

    def __getattr__(self, name):
        try:
            return self[name]
        except KeyError:
            raise AttributeError(name)


class EdState:
    def __init__(self):
        self.mode = None       # edit_mode, grid_power, grid_size, orient
        self.cam = None        # x, y, z, yaw, pitch
        self.world_pos = None  # x, y, z
        self.cur = None        # x, y, z, orient
        self.sel = None        # ox oy oz sx sy sz grid orient cx cy cxs cys corner children have_sel
        self.ui = None         # cursor_lock, free_cursor
        self.hover = []        # idx, type, x, y, z
        self.ent_sel = []
        self.malformed = []
        self.complete = False
        self.ent_count = 0


_INT32 = (-2 ** 31, 2 ** 31 - 1)


def _int(text):
    """An int32, as [int] parses one: a field outside that range is malformed."""
    v = int(text)
    if not _INT32[0] <= v <= _INT32[1]:
        raise OverflowError(text)
    return v


def _float(text):
    # Python's float() is locale-independent; 'nan'/'inf' are not coordinates.
    v = float(text)
    if v != v or v in (float('inf'), float('-inf')):
        raise ValueError(text)
    return v


_SEL_FIELDS = ('ox', 'oy', 'oz', 'sx', 'sy', 'sz', 'grid', 'orient', 'cx', 'cy', 'cxs', 'cys', 'corner', 'children', 'have_sel')


def parse_edstate(lines):
    result = EdState()
    for raw in lines:
        if raw is None:
            continue
        t = _STAMP.sub('', raw).strip()
        # Anything that is not one of our tags is ordinary log output.
        if not re.match(r'^(EDSTATE|EDENT)\s', t):
            continue
        # The regexes only constrain shape (\S+ for floats); a torn or garbage
        # token (a log read landing mid-clearlog) fails the conversion, and that
        # lands the line in .malformed instead of taking the parser down.
        try:
            m = re.match(r'^EDSTATE mode (-?\d+) (-?\d+) (-?\d+) (-?\d+)$', t)
            if m:
                result.mode = Record(edit_mode=_int(m.group(1)), grid_power=_int(m.group(2)), grid_size=_int(m.group(3)), orient=_int(m.group(4)))
                continue
            m = re.match(r'^EDSTATE cam (\S+) (\S+) (\S+) (\S+) (\S+)$', t)
            if m:
                result.cam = Record(x=_float(m.group(1)), y=_float(m.group(2)), z=_float(m.group(3)), yaw=_float(m.group(4)), pitch=_float(m.group(5)))
                continue
            m = re.match(r'^EDSTATE worldpos (\S+) (\S+) (\S+)$', t)
            if m:
                result.world_pos = Record(x=_float(m.group(1)), y=_float(m.group(2)), z=_float(m.group(3)))
                continue
            m = re.match(r'^EDSTATE cur (-?\d+) (-?\d+) (-?\d+) (-?\d+)$', t)
            if m:
                result.cur = Record(x=_int(m.group(1)), y=_int(m.group(2)), z=_int(m.group(3)), orient=_int(m.group(4)))
                continue
            m = re.match(r'^EDSTATE sel ((?:-?\d+ ){14})(-?\d+)$', t)
            if m:
                f = [_int(v) for v in (m.group(1) + m.group(2)).split()]
                result.sel = Record(zip(_SEL_FIELDS, f))
                continue
            m = re.match(r'^EDSTATE ui (-?\d+) (-?\d+)$', t)
            if m:
                result.ui = Record(cursor_lock=_int(m.group(1)), free_cursor=_int(m.group(2)))
                continue
            m = re.match(r'^EDENT (hover|sel) (-?\d+) (\S+) (\S+) (\S+) (\S+)$', t)
            if m:
                ent = Record(idx=_int(m.group(2)), type=m.group(3), x=_float(m.group(4)), y=_float(m.group(5)), z=_float(m.group(6)))
                (result.hover if m.group(1) == 'hover' else result.ent_sel).append(ent)
                continue
            m = re.match(r'^EDSTATE end (-?\d+)$', t)
            if m:
                result.ent_count = _int(m.group(1))
                result.complete = True
                continue
        except (ValueError, OverflowError):
            pass
        # A tagged line we could not read is a real problem: surface it rather
        # than silently returning a half-populated state.
        result.malformed.append(t)
    return result
