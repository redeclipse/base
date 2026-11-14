#!/usr/bin/env python3
"""Drives a running Red Eclipse client from outside, for agent/CI UI work.

The Linux/macOS counterpart of harness.ps1. Launches the game against an
isolated home dir with a CubeScript command channel installed
(tools/harness/boot.cfg), then sends CubeScript batches and reads their output
back. The UI is CubeScript, so editing config/ui/**.cfg and calling 'reload'
applies the change live: no rebuild, no restart.

  tools/harness/harness.py start
  tools/harness/harness.py nav ui_gameui_settings_graphics
  tools/harness/harness.py shot graphics
  tools/harness/harness.py reload config/ui/game/settings.cfg
  tools/harness/harness.py send 'echo "top=" $uitopname'
  tools/harness/harness.py stop
"""

import argparse
import math
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import core  # noqa: E402
from core import HarnessError, format_coord, invoke_batch, say, show_batch_result  # noqa: E402

COMMANDS = ('start', 'stop', 'status', 'send', 'shot', 'reload', 'nav', 'log', 'tree', 'find', 'click')

# ------------------------------------------------------- widget queries ----
#
# uidumptree reports object rects with x in ASPECT space (0 .. hudw/hudh) while
# the cursor is in screen fractions (0..1 across the full width), so clicking a
# widget means dividing its x by the aspect ratio.


def tree_float(value):
    """An empty scroll area divides 0 by 0 in the engine (ScrollBar::vscale), so
    uidumptree can print 'nan', '-nan', 'inf' (or MSVC's '-nan(ind)') for a
    rect. One such widget must not take the whole tree down."""
    low = value.lower()
    if re.match(r'^[+-]?nan', low):
        return math.nan
    if re.match(r'^\+?inf', low):
        return math.inf
    if low.startswith('-inf'):
        return -math.inf
    return core.to_float(value)


class Node:
    __slots__ = ('depth', 'drawn', 'type', 'x', 'y', 'w', 'h', 'tag', 'text', 'cursor_x', 'cursor_y')

    def __init__(self, depth, drawn, type_, x, y, w, h, tag, text):
        self.depth, self.drawn, self.type = depth, drawn, type_
        self.x, self.y, self.w, self.h = x, y, w, h
        self.tag, self.text = tag, text
        self.cursor_x = self.cursor_y = None


class UiTree:
    def __init__(self, aspect, nodes):
        self.aspect, self.nodes = aspect, nodes


TREE_LINE = re.compile(r'^UITREE (\d+) ([01]) (\S+) (\S+) (\S+) (\S+) (\S+) (\S+)\s?(.*)$')


def parse_ui_tree(lines):
    aspect, nodes = 16 / 9, []
    for raw in lines:
        t = core.strip_stamp(raw)
        m = re.match(r'^UIASPECT\s+([0-9.]+)', t)
        if m:
            aspect = core.to_float(m.group(1))
            continue
        m = TREE_LINE.match(t)
        if m:
            nodes.append(Node(int(m.group(1)), m.group(2) == '1', m.group(3), tree_float(m.group(4)), tree_float(m.group(5)),
                              tree_float(m.group(6)), tree_float(m.group(7)), m.group(8), m.group(9).strip()))
    return UiTree(aspect, nodes)


def get_ui_tree(timeout=30):
    return parse_ui_tree(invoke_batch('echo "UIASPECT " $uiaspect\nuidumptree', 1, timeout))


def find_widget(tree, label):
    """Drawn widgets carrying text, exact match on label first, then substring."""
    def usable(n):
        total = n.x + n.y + n.w + n.h
        return n.drawn and n.text and n.w > 0 and n.h > 0 and math.isfinite(total)
    candidates = [n for n in tree.nodes if usable(n)]
    hits = [n for n in candidates if n.text == label]
    if not hits:
        hits = [n for n in candidates if label in n.text]
    return hits


def add_click_point(tree, nodes):
    for n in nodes:
        n.cursor_x = (n.x + n.w / 2) / tree.aspect
        n.cursor_y = n.y + n.h / 2
    return nodes


def format_node(n, indent=True):
    rect = '[%s %s %s %s]' % (format_coord(n.x) if math.isfinite(n.x) else n.x, format_coord(n.y) if math.isfinite(n.y) else n.y,
                              format_coord(n.w) if math.isfinite(n.w) else n.w, format_coord(n.h) if math.isfinite(n.h) else n.h)
    click = 'click(%s,%s)' % (_coord(n.cursor_x), _coord(n.cursor_y))
    if indent:
        return '%s%s %s %s %s %s' % (' ' * n.depth, n.type, 'drawn' if n.drawn else 'hidden', rect, click, n.text)
    return '%s %s %s %s' % (n.type, rect, click, n.text)


def _coord(v):
    return format_coord(v) if v is not None and math.isfinite(v) else str(v)


def click_script(cx, cy):
    # Move first, let a frame register the hover, then press and release.
    return 'uisetcursor %s %s\nsleep 100 [\n    uikeypress -1 1\n    sleep 60 [ uikeypress -1 0 ]\n]' % (cx, cy)


def click(label, settle_ms=500, timeout=30):
    """Clicks the first drawn widget whose text matches label; returns its node
    and the batch's lines."""
    tree = get_ui_tree(timeout)
    hits = add_click_point(tree, find_widget(tree, label))
    if not hits:
        raise HarnessError("No drawn widget matching '%s'. Try: harness.py tree --text" % label)
    if len(hits) > 1:
        core.warn("%d widgets match '%s'; clicking the first (%s). Use 'find' to disambiguate." % (len(hits), label, hits[0].text))
    n = hits[0]
    return n, invoke_batch(click_script(format_coord(n.cursor_x), format_coord(n.cursor_y)), settle_ms, timeout)


# --------------------------------------------------------------- main -----

def main(argv=None):
    p = argparse.ArgumentParser(prog='harness.py', description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter, allow_abbrev=False)
    p.add_argument('command', nargs='?', default='status', choices=COMMANDS)
    p.add_argument('rest', nargs='*', help='the script, panel, file, screenshot name or label')
    p.add_argument('--width', type=int, default=1280, help='start: window width (1280)')
    p.add_argument('--height', type=int, default=720, help='start: window height (720)')
    p.add_argument('--focus', action='store_true', help='start: let the window take focus')
    p.add_argument('--exe', help='start: the client to run (default $RE_HARNESS_EXE or bin/<arch>/redeclipse_<os>)')
    p.add_argument('--settle', type=int, default=-1, help='ms to wait before a batch counts as finished')
    p.add_argument('--timeout', type=int, default=30, help='seconds to wait for a batch (30)')
    p.add_argument('--file', help='send: read the script from a file')
    p.add_argument('--drawn', action='store_true', help='tree: only objects rendered this frame')
    p.add_argument('--text', action='store_true', help='tree: only objects carrying text')
    a = p.parse_args(argv)

    def settle(default):
        return a.settle if a.settle >= 0 else default

    rest = ' '.join(a.rest).strip()
    try:
        if a.command == 'start':
            core.start(a.width, a.height, a.focus, a.exe)
        elif a.command == 'stop':
            core.stop()
        elif a.command == 'status':
            pid = core.harness_pid()
            if pid:
                say('running   PID %d' % pid, 'green')
                say('home      %s' % core.HOME_DIR)
                say('batches   %d sent' % core.next_seq())
                say('shots     %s' % core.SHOT_DIR)
                print('running')
            else:
                say('not running', 'yellow')
                say('start with: tools/harness/harness.py start')
                print('not running')
        elif a.command == 'send':
            if a.file:
                with open(a.file, encoding='utf-8') as f:
                    script = f.read()
            else:
                script = ' '.join(a.rest)
            if not script:
                raise HarnessError('Nothing to send. Pass CubeScript as an argument, or use --file <path>.')
            show_batch_result(invoke_batch(script, settle(1), a.timeout))
        elif a.command == 'nav':
            if not rest:
                raise HarnessError('Usage: harness.py nav ui_gameui_settings_graphics')
            show_batch_result(invoke_batch('gameui_open %s' % rest, settle(400), a.timeout))
        elif a.command == 'reload':
            if not rest:
                raise HarnessError('Usage: harness.py reload config/ui/game/settings.cfg')
            cfg = rest.replace('\\', '/')
            if not os.path.exists(os.path.join(core.REPO_ROOT, cfg)):
                core.warn('No such file under repo root: %s' % cfg)
            show_batch_result(invoke_batch('exec "%s" 0 0' % cfg, settle(200), a.timeout))
        elif a.command == 'shot':
            print(core.invoke_shot('_'.join(a.rest).strip(), settle(300), a.timeout))
        elif a.command == 'tree':
            tree = get_ui_tree(a.timeout)
            nodes = tree.nodes
            if a.drawn:
                nodes = [n for n in nodes if n.drawn]
            if a.text:
                nodes = [n for n in nodes if n.text]
            say('aspect %s, %d objects' % (format_coord(tree.aspect), len(tree.nodes)), 'cyan')
            for n in add_click_point(tree, nodes):
                print(format_node(n))
        elif a.command == 'find':
            if not rest:
                raise HarnessError('Usage: harness.py find Apply')
            tree = get_ui_tree(a.timeout)
            hits = add_click_point(tree, find_widget(tree, rest))
            if not hits:
                say("No drawn widget matching '%s'." % rest, 'yellow')
                return 0
            for n in hits:
                print(format_node(n, indent=False))
        elif a.command == 'click':
            if not rest:
                raise HarnessError('Usage: harness.py click Back')
            tree = get_ui_tree(a.timeout)
            hits = add_click_point(tree, find_widget(tree, rest))
            if not hits:
                raise HarnessError("No drawn widget matching '%s'. Try: harness.py tree --text" % rest)
            if len(hits) > 1:
                core.warn("%d widgets match '%s'; clicking the first (%s). Use 'find' to disambiguate." % (len(hits), rest, hits[0].text))
            n = hits[0]
            cx, cy = format_coord(n.cursor_x), format_coord(n.cursor_y)
            say("clicking '%s' at (%s,%s)" % (n.text, cx, cy), 'cyan')
            show_batch_result(invoke_batch(click_script(cx, cy), settle(500), a.timeout))
        elif a.command == 'log':
            if os.path.exists(core.LOG_FILE):
                sys.stdout.write(core.read_log())
            else:
                say('No log yet.')
    except HarnessError as e:
        say(str(e), 'red')
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
