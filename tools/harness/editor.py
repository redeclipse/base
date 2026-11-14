#!/usr/bin/env python3
"""Drives the Red Eclipse map editor from outside, for agent/CI editor work.

The Linux/macOS counterpart of editor.ps1. Shares the running client, home dir
and command channel with harness.py: start the client with 'harness.py start',
then use 'open' or 'newmap' here to get into an editing session. The view
commands (goto, aim, lookat, lookatent, frame, frameent, nudge) do nothing
outside edit mode.

A fresh 'newmap 12' world is 4096 units: it centres on (2048, 2048) with the
floor surface at z = 2048, so that is where the examples aim. The engine side
needs a build with the editor test commands compiled in (-DDEBUG_UTILS).

  tools/harness/harness.py start
  tools/harness/editor.py newmap 12
  tools/harness/editor.py frame 2048 2048 2300 --dist 128 --yaw 45 --pitch 20
  tools/harness/editor.py seldrag 2052 2052 2048 2076 2076 2048
  tools/harness/editor.py state
  tools/harness/editor.py shot mysel
"""

import argparse
import json
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import core  # noqa: E402
from core import HarnessError, format_coord, invoke_batch, say, show_batch_result  # noqa: E402
from edstate import parse_edstate  # noqa: E402

COMMANDS = ('open', 'newmap', 'state', 'goto', 'aim', 'lookat', 'lookatent', 'frame', 'frameent', 'nudge', 'cursor', 'key',
            'entsel', 'sel', 'seldrag', 'shot')

TIMEOUT = 30


def ensure_editor_cfg(timeout=TIMEOUT):
    """editor.cfg is loaded on demand rather than from boot.cfg, so a UI-only
    session never pays for it. 'edh_ready' is unset on a cold session, and
    reading $edh_ready would log 'Unknown alias lookup', so ask identexists."""
    invoke_batch('if (! (identexists edh_ready)) [ exec "tools/harness/editor.cfg" 0 0 ]', 1, timeout)


def run(script, settle_ms, timeout=TIMEOUT):
    ensure_editor_cfg(timeout)
    return invoke_batch(script, settle_ms, timeout)


def coords(values, count, usage):
    values = [v for v in values if v != '']
    if len(values) < count:
        raise HarnessError('Usage: ' + usage)
    try:
        return [format_coord(core.to_float(v)) for v in values[:count]]
    except ValueError:
        raise HarnessError('Usage: ' + usage)


def _result(lines, failed):
    """Raises with failed's message when the batch echoed EDRESULT=0."""
    if any('EDRESULT=0' in line and re.search(r'EDRESULT=0$', core.strip_stamp(line)) for line in lines):
        raise HarnessError(failed)
    return lines


# ------------------------------------------------------------- state ----

def get_state(timeout=TIMEOUT):
    lines = run('eddumpstate', 100, timeout)
    state = parse_edstate(lines)
    if not state.complete:
        raise HarnessError('eddumpstate returned an incomplete dump. Raw output:\n' + '\n'.join(lines))
    if state.malformed:
        core.warn('Unparsed state lines:\n  ' + '\n  '.join(state.malformed))
    return state


def show_state(s, stream=None):
    def out(text, name=None):
        say(text, name, stream)
    out('mode      editing=%d gridpower=%d gridsize=%d orient=%d' % (s.mode.edit_mode, s.mode.grid_power, s.mode.grid_size, s.mode.orient), 'cyan')
    out('camera    (%s, %s, %s) yaw=%s pitch=%s' % tuple(format_coord(s.cam[k]) for k in ('x', 'y', 'z', 'yaw', 'pitch')))
    out('worldpos  (%s, %s, %s)' % tuple(format_coord(s.world_pos[k]) for k in ('x', 'y', 'z')))
    out('cursor    (%d, %d, %d) orient=%d' % (s.cur.x, s.cur.y, s.cur.z, s.cur.orient))
    if s.sel.have_sel:
        out('selection o=(%d, %d, %d) s=(%d, %d, %d) grid=%d orient=%d children=%d' % (
            s.sel.ox, s.sel.oy, s.sel.oz, s.sel.sx, s.sel.sy, s.sel.sz, s.sel.grid, s.sel.orient, s.sel.children), 'green')
    else:
        out('selection none', 'yellow')
    out('ui        cursorlock=%d freecursor=%d' % (s.ui.cursor_lock, s.ui.free_cursor))
    for e in s.hover:
        out('hover     [%d] %s (%s, %s, %s)' % (e.idx, e.type, format_coord(e.x), format_coord(e.y), format_coord(e.z)))
    for e in s.ent_sel:
        out('entsel    [%d] %s (%s, %s, %s)' % (e.idx, e.type, format_coord(e.x), format_coord(e.y), format_coord(e.z)), 'green')


def state_json(s):
    return json.dumps({'mode': s.mode, 'cam': s.cam, 'worldpos': s.world_pos, 'cur': s.cur, 'sel': s.sel, 'ui': s.ui,
                       'hover': s.hover, 'entsel': s.ent_sel, 'entcount': s.ent_count, 'malformed': s.malformed}, indent=2)


# ---------------------------------------------------------- commands ----
# Each returns the batch lines it produced (for the CLI to print); the ones
# that end in a state printout also return the state.

def open_map(name, settle=-1, timeout=TIMEOUT):
    """Enters an editing session on a shipped/downloaded map. 'edit' (the
    config/setup.cfg alias) sets G_EDITING and force-local-connects before
    loading the map: plain 'map' cannot get a cold client into edit mode.
    Map load is slow and clears IDF_MAP sleeps; give it room."""
    if not name:
        raise HarnessError('Usage: editor.py open <mapname>')
    lines = run('edit %s' % name, settle if settle >= 0 else 4000, timeout)
    lines += run('edh_enter', settle if settle >= 0 else 600, timeout)
    return lines


def newmap(size=12, settle=-1, timeout=TIMEOUT):
    """newmap (emptymap()) is legal only once edit mode is on, so 'edit' into a
    stable scratch map first, enter edit mode, then replace it with a
    deterministic empty world of the requested size."""
    lines = run('edit harness_scratch', settle if settle >= 0 else 4000, timeout)
    lines += run('edh_enter', settle if settle >= 0 else 600, timeout)
    lines += run('newmap %d' % int(size), settle if settle >= 0 else 3000, timeout)
    return lines


def goto(x, y, z, settle=150):
    a = coords([x, y, z], 3, 'editor.py goto <x> <y> <z>')
    return run('edgoto %s %s %s' % tuple(a), settle)


def aim(yaw, pitch, settle=150):
    a = coords([yaw, pitch], 2, 'editor.py aim <yaw> <pitch>')
    return run('edaim %s %s' % tuple(a), settle)


def lookat(x, y, z, settle=150):
    a = coords([x, y, z], 3, 'editor.py lookat <x> <y> <z>')
    return run('edlookat %s %s %s' % tuple(a), settle)


def lookatent(idx, settle=150):
    a = coords([idx], 1, 'editor.py lookatent <idx>')
    lines = run('echo (concatword "EDRESULT=" (edlookatent %s))' % a[0], settle)
    return _result(lines, 'No live entity at index %s, or not in edit mode.' % a[0])


def frame(x, y, z, dist=128, yaw=0, pitch=20, settle=200):
    a = coords([x, y, z], 3, 'editor.py frame <x> <y> <z> [--dist n] [--yaw n] [--pitch n]')
    return run('edframe %s %s %s %s %s %s' % (a[0], a[1], a[2], format_coord(dist), format_coord(yaw), format_coord(pitch)), settle)


def frameent(idx, dist=128, yaw=0, pitch=20, settle=200):
    a = coords([idx], 1, 'editor.py frameent <idx> [--dist n] [--yaw n] [--pitch n]')
    lines = run('echo (concatword "EDRESULT=" (edframeent %s %s %s %s))' % (a[0], format_coord(dist), format_coord(yaw), format_coord(pitch)), settle)
    return _result(lines, 'No live entity at index %s, or not in edit mode.' % a[0])


def nudge(fwd, right, up, settle=150):
    a = coords([fwd, right, up], 3, 'editor.py nudge <fwd> <right> <up>')
    return run('ednudge %s %s %s' % tuple(a), settle)


def cursor(mode, settle=500):
    """Absolute on/off drives the real TAB bind (edh_cursor), so repeated calls
    are safe; toggle taps TAB once."""
    script = {'on': 'edh_cursor 1', 'off': 'edh_cursor 0', 'toggle': 'edh_tap TAB'}.get((mode or '').strip().lower())
    if not script:
        raise HarnessError('Usage: editor.py cursor <on|off|toggle>')
    return run(script, settle)


def key(name, down=False, up=False, settle=300):
    """Synthetic input through the full bind path. The default taps once
    (press, report, release): pressing first and then calling edh_tap would
    press-press-release."""
    name = (name or '').strip()
    if not name:
        raise HarnessError('Usage: editor.py key <NAME> [--down] [--up]')
    if down and not up:
        script = 'echo (concatword "EDRESULT=" (gamekeypress %s 1))' % name
    elif up and not down:
        script = 'echo (concatword "EDRESULT=" (gamekeypress %s 0))' % name
    else:
        script = 'echo (concatword "EDRESULT=" (gamekeypress %s 1))\nsleep 50 (concat gamekeypress %s 0)' % (name, name)
    # gamekeypress returns 0 only when the name resolves to no key code.
    return _result(run(script, settle), "Unknown key name '%s'." % name)


def entsel(x, y, z, radius=32, hover=False, settle=400):
    a = coords([x, y, z], 3, 'editor.py entsel <x> <y> <z> [--radius n] [--hover]')
    if hover:
        # The path a user takes: aim at it, then add what is hovered.
        script = 'edlookat %s %s %s\nsleep 150 [ entadd ]' % tuple(a)
    else:
        script = 'echo (concatword "EDRESULT=" (edentnear %s %s %s %s))' % (a[0], a[1], a[2], format_coord(radius))
    return run(script, settle)


def sel(x, y, z, size=None, settle=300):
    a = coords([x, y, z], 3, 'editor.py sel <x> <y> <z> [--size sx,sy,sz]')
    # --size given with the wrong arity must not silently fall back to a
    # single cube: the caller asked for a box.
    if size is not None:
        size = list(size)
        if len(size) != 3:
            raise HarnessError('Usage: editor.py sel <x> <y> <z> [--size sx,sy,sz] -- --size needs exactly 3 values, got %d.' % len(size))
        script = 'edselbox %s %s %s %d %d %d' % (a[0], a[1], a[2], int(size[0]), int(size[1]), int(size[2]))
    else:
        script = 'edselcube %s %s %s' % tuple(a)
    return run(script, settle)


def seldrag(x1, y1, z1, x2, y2, z2, settle=900):
    a = coords([x1, y1, z1, x2, y2, z2], 6, 'editor.py seldrag <x1> <y1> <z1> <x2> <y2> <z2>')
    return run('edh_dragsel %s %s %s %s %s %s' % tuple(a), settle)


def shot(name, settle=300, quiet=True):
    ensure_editor_cfg()
    return core.invoke_shot(name, settle, TIMEOUT, quiet=quiet)


# --------------------------------------------------------------- main -----

def _size(text):
    try:
        return [int(v) for v in text.split(',') if v.strip() != '']
    except ValueError:
        raise argparse.ArgumentTypeError('--size takes integers, as sx,sy,sz')


def main(argv=None):
    global TIMEOUT
    p = argparse.ArgumentParser(prog='editor.py', description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter, allow_abbrev=False)
    p.add_argument('command', nargs='?', default='state', choices=COMMANDS)
    p.add_argument('rest', nargs='*')
    p.add_argument('--dist', type=float, default=128)
    p.add_argument('--yaw', type=float, default=0)
    p.add_argument('--pitch', type=float, default=20)
    p.add_argument('--radius', type=float, default=32)
    p.add_argument('--size', type=_size, help='sel: box size in grid cubes, sx,sy,sz')
    p.add_argument('--settle', type=int, default=-1)
    p.add_argument('--timeout', type=int, default=30)
    p.add_argument('--down', action='store_true')
    p.add_argument('--up', action='store_true')
    p.add_argument('--hover', action='store_true')
    p.add_argument('--json', action='store_true', help='state: print the parsed state as JSON')
    a = p.parse_args(argv)
    TIMEOUT = a.timeout
    r = a.rest

    def st(default):
        return a.settle if a.settle >= 0 else default

    try:
        c = a.command
        if c == 'open':
            show_batch_result(open_map(' '.join(r).strip(), a.settle))
            show_state(get_state())
        elif c == 'newmap':
            show_batch_result(newmap(int(r[0]) if r and r[0] else 12, a.settle))
            show_state(get_state())
        elif c == 'state':
            s = get_state()
            if a.json:
                print(state_json(s))
            else:
                show_state(s, sys.stdout)
        elif c == 'goto':
            show_batch_result(goto(*(r + [''] * 3)[:3], settle=st(150)))
        elif c == 'aim':
            show_batch_result(aim(*(r + [''] * 2)[:2], settle=st(150)))
        elif c == 'lookat':
            show_batch_result(lookat(*(r + [''] * 3)[:3], settle=st(150)))
        elif c == 'lookatent':
            show_batch_result(lookatent((r + [''])[0], settle=st(150)))
        elif c == 'frame':
            show_batch_result(frame(*(r + [''] * 3)[:3], dist=a.dist, yaw=a.yaw, pitch=a.pitch, settle=st(200)))
        elif c == 'frameent':
            show_batch_result(frameent((r + [''])[0], dist=a.dist, yaw=a.yaw, pitch=a.pitch, settle=st(200)))
        elif c == 'nudge':
            show_batch_result(nudge(*(r + [''] * 3)[:3], settle=st(150)))
        elif c == 'cursor':
            show_batch_result(cursor(''.join(r), settle=st(500)))
        elif c == 'key':
            show_batch_result(key(''.join(r), a.down, a.up, settle=st(300)))
        elif c == 'entsel':
            show_batch_result(entsel(*(r + [''] * 3)[:3], radius=a.radius, hover=a.hover, settle=st(400)))
            show_state(get_state())
        elif c == 'sel':
            show_batch_result(sel(*(r + [''] * 3)[:3], size=a.size, settle=st(300)))
            show_state(get_state())
        elif c == 'seldrag':
            show_batch_result(seldrag(*(r + [''] * 6)[:6], settle=st(900)))
            show_state(get_state())
        elif c == 'shot':
            print(shot('_'.join(r).strip(), st(300), quiet=False))
    except HarnessError as e:
        say(str(e), 'red')
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
