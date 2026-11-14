#!/usr/bin/env python3
"""End-to-end self-test for the map editor harness (editor-selftest.ps1).

Boots a client, drives every editor capability the harness claims to support,
and asserts the resulting editor state. Selection is exercised through the
REAL input path (gamekeypress -> execbind -> drag/onrelease); the
coordinate-addressed wrappers are used only for deterministic setup.

Every one of editor.py's 16 subcommands is used at least once:
  open newmap state goto aim lookat lookatent frame frameent nudge
  cursor key entsel sel seldrag shot

  tools/harness/editor_selftest.py [--keep-running] [--map atop]
"""

import argparse
import math
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import core  # noqa: E402
import editor as ed  # noqa: E402
from selftest import Suite, eval_cs, map_load, send  # noqa: E402

# 'newmap 12' builds a 4096-unit world whose bottom four octants are solid
# (emptymap(), src/engine/world.cpp), so the floor surface is the z = 2048
# plane and everything above it is air. Default gridpower is 3, an 8-unit grid.
FLOOR = 2048  # z of the floor surface
MID = 2048    # x/y of the world centre
GRID = 8      # gridsize at the default gridpower 3
# O_TOP in the orient enum, src/engine/octa.h (LEFT, RIGHT, BACK, FRONT,
# BOTTOM, TOP = 0..5): the face a ray coming down onto the floor strikes.
O_TOP = 5


def cube_at(v):
    """The gridsize-aligned cube containing coordinate v: the engine's own rule
    for cur (lookupcube + normalizelookupcube, octaedit.cpp): floor to the grid."""
    return int(math.floor(v / GRID) * GRID)


def get_var(name):
    return eval_cs('$' + name, prefix='SELFTEST=' + name) or ''


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('--keep-running', action='store_true')
    # Which shipped map the 'open' step loads. Only that step uses it: every
    # other step works in the deterministic 'newmap 12' scratch world.
    p.add_argument('--map', default='atop')
    a = p.parse_args(argv)

    t = Suite('editor self-test', state_on_failure=True)
    state = ed.get_state
    ent = {}

    core.stop()
    core.start()

    def open_step():
        with map_load():
            ed.open_map(a.map)
        s = state()
        t.expect('edit mode', s.mode.edit_mode, 1)
        t.expect_true('grid size is positive', s.mode.grid_size > 0, 'gridsize=%d' % s.mode.grid_size)
        name = get_var('mapname')
        t.expect_true("the requested map is loaded (mapname='%s')" % name, name.endswith(a.map), "expected a name ending in '%s'" % a.map)
    t.step('open: load a shipped map into an editing session', open_step)

    def newmap_step():
        with map_load():
            ed.newmap(12)
        s = state()
        t.expect('edit mode', s.mode.edit_mode, 1)
        t.expect('grid size', s.mode.grid_size, GRID)
        t.expect('world size', get_var('mapsize'), '4096')
    t.step('newmap: build the deterministic scratch world', newmap_step)

    def frame_step():
        # Yaw 0 / pitch 0 puts the camera dist units on the -Y side of the
        # target, looking back along +Y (orbitpos, src/engine/world.cpp).
        ed.frame(MID, MID, 2300, dist=100, yaw=0, pitch=0)
        s = state()
        t.expect_near('camera x', s.cam.x, MID)
        t.expect_near('camera y', s.cam.y, MID - 100)
        t.expect_near('camera z', s.cam.z, 2300)
        t.expect_near('camera yaw', s.cam.yaw, 0)
        t.expect_near('camera pitch', s.cam.pitch, 0)
    t.step('frame: one call fully determines the view', frame_step)

    def goto_aim_step():
        ed.goto(MID, 2000, 2400)
        ed.aim(45, -30)
        s = state()
        t.expect_near('camera x', s.cam.x, MID)
        t.expect_near('camera y', s.cam.y, 2000)
        t.expect_near('camera z', s.cam.z, 2400)
        t.expect_near('camera yaw', s.cam.yaw, 45)
        t.expect_near('camera pitch', s.cam.pitch, -30)
    t.step('goto / aim: both are absolute', goto_aim_step)

    def lookat_step():
        ed.goto(MID, MID, 2300)
        # 100 out along +Y and 100 down: straight ahead at yaw 0, 45 degrees down.
        ed.lookat(MID, MID + 100, 2200)
        s = state()
        t.expect_near('camera position is unchanged', s.cam.y, MID)
        t.expect_near('yaw towards +Y', s.cam.yaw, 0)
        t.expect_near('pitch 45 degrees down', s.cam.pitch, -45)
        # A second, different target: otherwise a lookat that silently did
        # nothing would still pass the yaw-0 check above.
        ed.lookat(MID + 100, MID, 2300)
        s = state()
        t.expect_near('yaw towards +X', s.cam.yaw, 270)
        t.expect_near('pitch is level', s.cam.pitch, 0)
    t.step('lookat: aiming at a point yields the angles to it', lookat_step)

    def nudge_step():
        ed.goto(MID, MID, 2300)
        ed.aim(0, 0)
        # At yaw 0 the facing is +Y, so "right" is -X: forward 100 -> +Y,
        # right 64 -> -X, up 32 -> +Z.
        ed.nudge(100, 64, 32)
        s = state()
        t.expect_near('moved right along -X', s.cam.x, MID - 64)
        t.expect_near('moved forward along +Y', s.cam.y, MID + 100)
        t.expect_near('moved up along +Z', s.cam.z, 2332)
    t.step('nudge: relative to the facing, not to the axes', nudge_step)

    def cursor_step():
        # ui_freecursor boots as 1, which edh_cursor already reads as "on", so
        # 'cursor on' from boot presses nothing. Drive it off first, so the 'on'
        # below has to go through the TAB bind.
        ed.cursor('off')
        t.expect('freecursor cleared before the on-test', state().ui.free_cursor, 0)
        # The only writer is ta_cursor_mode (config/tool/tooledit.cfg), whose
        # "on" value is $clockmillis: > 1 cannot be met by the boot default.
        ed.cursor('on')
        on = state().ui.free_cursor
        t.expect_true('TAB set freecursor to a clockmillis timestamp', on > 1, 'freecursor=%d' % on)
        # Absolute, so a second 'on' must not toggle it back off.
        ed.cursor('on')
        again = state().ui.free_cursor
        t.expect_true('cursor on is idempotent', again > 1, 'freecursor=%d' % again)
        ed.cursor('off')
        t.expect('freecursor cleared', state().ui.free_cursor, 0)
    t.step('cursor: lock and unlock through the real TAB bind', cursor_step)

    def seldrag_step():
        # Stand above the floor looking down at it, so the crosshair ray strikes
        # world geometry at both drag corners. Both corners are aimed at the
        # middle of a grid cell, so sub-unit ray jitter cannot round the cursor
        # into the neighbouring cube.
        ed.goto(MID, MID - 148, 2300)
        near = MID + 4                 # inside the cell at 2048
        far = MID + 3 * GRID + 4       # inside the cell at 2072
        # rendereditcursor() steps 0.05 past the ray's hit point and sets cur
        # to the gridsize cube containing that point; aimed down onto the
        # floor, that is just inside the solid below it.
        ed.lookat(near, near, FLOOR)
        s = state()
        t.expect('cur x under the near corner', s.cur.x, cube_at(near))
        t.expect('cur y under the near corner', s.cur.y, cube_at(near))
        t.expect('cur z (the cube under the surface)', s.cur.z, FLOOR - GRID)
        t.expect('cur orient (top face)', s.cur.orient, O_TOP)

        ed.seldrag(near, near, FLOOR, far, far, FLOOR)
        s = state()
        # The drag ends aimed at the far corner, so cur must have followed it.
        t.expect('cur x under the far corner', s.cur.x, cube_at(far))
        t.expect('cur y under the far corner', s.cur.y, cube_at(far))
        t.expect('cur z after the drag', s.cur.z, FLOOR - GRID)
        t.expect('cur orient after the drag', s.cur.orient, O_TOP)
        t.expect('a selection exists', s.sel.have_sel, 1)
        t.expect('selection grid', s.sel.grid, GRID)
        t.expect('selection origin x', s.sel.ox, MID)
        t.expect('selection origin y', s.sel.oy, MID)
        t.expect('selection origin z (the cube under the surface)', s.sel.oz, FLOOR - GRID)
        # The drag spanned 3 whole cells, so 4 cubes wide in both dragged axes;
        # 1x1 would mean the release landed on the press corner.
        t.expect('selection spans the drag in x', s.sel.sx, 4)
        t.expect('selection spans the drag in y', s.sel.sy, 4)
        t.expect('selection is one cube deep', s.sel.sz, 1)
        # One untouched 2048-unit solid octant: mag = 2048/8, so -256.
        t.expect('selchildcount (one 2048 leaf at grid 8 -> -2048/8)', s.sel.children, -256)
    t.step('seldrag: geometry selection through the real MOUSE1 path', seldrag_step)

    def key_step():
        t.expect_true('a selection is in place to cancel', state().sel.have_sel == 1)
        # SPACE is bound to [ entmoving 0; cancelsel ] (config/setup.cfg), so a
        # real press must clear the selection. Nothing but the bind can do that.
        out = ed.key('SPACE')
        t.expect_true('the key was dispatched', any('EDRESULT=1' in line for line in out), 'output: ' + ' | '.join(out))
        t.expect('the SPACE editbind cancelled the selection', state().sel.have_sel, 0)
        # Key names resolve case-insensitively, so a lower-case name reaches the same bind.
        ed.sel(MID, MID, FLOOR - GRID)
        t.expect_true('a selection is in place for the lower-case press', state().sel.have_sel == 1)
        out = ed.key('space')
        t.expect_true('the lower-case key was dispatched', any('EDRESULT=1' in line for line in out), 'output: ' + ' | '.join(out))
        t.expect('the lower-case space reached the same editbind', state().sel.have_sel, 0)
        # An unknown name is an error, like lookatent/frameent on a dead index.
        err = ''
        try:
            ed.key('NOTAKEY')
        except core.HarnessError as e:
            err = str(e)
        t.expect_true('an unknown key name is rejected with an error', "Unknown key name 'NOTAKEY'" in err, "error: '%s'" % err)
    t.step('key: a synthetic keypress reaches its editbind', key_step)

    def sel_step():
        # NOT the path under test (that is seldrag above); this only confirms
        # the deterministic setup wrapper still places the box it is asked for.
        ed.sel(MID, MID, FLOOR - GRID, size=[2, 2, 1])
        s = state()
        t.expect('a selection exists', s.sel.have_sel, 1)
        t.expect('origin x', s.sel.ox, MID)
        t.expect('origin y', s.sel.oy, MID)
        t.expect('origin z', s.sel.oz, FLOOR - GRID)
        t.expect('size x', s.sel.sx, 2)
        t.expect('size y', s.sel.sy, 2)
        t.expect('size z', s.sel.sz, 1)
        err = ''
        try:
            ed.sel(MID, MID, FLOOR - GRID, size=[2, 2])
        except core.HarnessError as e:
            err = str(e)
        t.expect_true('sel --size with 2 values is rejected', '--size needs exactly 3 values' in err, "error: '%s'" % err)
    t.step('sel: the coordinate-addressed setup shortcut', sel_step)

    def entity_step():
        # newent drops the entity relative to the current selection, so pin it
        # afterwards: newent implicitly selects what it made, and entpos edits
        # the selected group.
        ex, ey, ez = MID, MID, 2100
        send('newent "playerstart" ""', 400)
        send('entpos %d %d %d' % (ex, ey, ez), 400)
        s = state()
        t.expect_true('the entity was created and implicitly selected', len(s.ent_sel) == 1, 'entsel count=%d' % len(s.ent_sel))
        if len(s.ent_sel) != 1:
            return
        idx = s.ent_sel[0].idx
        ent['idx'], ent['pos'] = idx, (ex, ey, ez)
        t.expect('it is a playerstart', s.ent_sel[0].type, 'playerstart')
        t.expect_near('entity z is where entpos put it', s.ent_sel[0].z, ez, 0.01)
        send('entcancel', 300)
        t.expect_true('the implicit selection is cleared', len(state().ent_sel) == 0)

        # lookatent: aim only, and let the editor recompute the hover.
        ed.goto(ex, ey - 96, ez + 40)
        ed.lookatent(idx)
        s = state()
        t.expect_near('lookatent yaw towards +Y', s.cam.yaw, 0)
        # atan2(-40, 96) = -22.62 degrees.
        t.expect_near('lookatent pitch down onto the entity', s.cam.pitch, -22.62, 0.1)
        t.expect_true('the entity is hovered', len([e for e in s.hover if e.idx == idx]) == 1, 'hover=%d' % len(s.hover))
        t.expect_true('hovering alone does not select', len(s.ent_sel) == 0, 'entsel count=%d' % len(s.ent_sel))

        # entsel --hover: aim, then add what the editor says is hovered (the
        # user's path, not the edentnear shortcut).
        ed.entsel(ex, ey, ez, hover=True)
        s = state()
        t.expect_true('the hovered entity is now selected', len([e for e in s.ent_sel if e.idx == idx]) == 1, 'entsel=%d' % len(s.ent_sel))

        # frameent: orbit the camera around it.
        dist, yaw, pitch = 96.0, 30.0, -25.0
        ed.frameent(idx, dist=dist, yaw=yaw, pitch=pitch)
        s = state()
        # orbitpos(): camera = target - dir(yaw,pitch)*dist, with
        # dir = (-sin(yaw)cos(pitch), cos(yaw)cos(pitch), sin(pitch)).
        ry, rp = math.radians(yaw), math.radians(pitch)
        t.expect_near('frameent camera x', s.cam.x, ex + math.sin(ry) * math.cos(rp) * dist, 0.01)
        t.expect_near('frameent camera y', s.cam.y, ey - math.cos(ry) * math.cos(rp) * dist, 0.01)
        t.expect_near('frameent camera z', s.cam.z, ez - math.sin(rp) * dist, 0.01)
        t.expect_near('frameent camera yaw', s.cam.yaw, yaw, 0.01)
        t.expect_near('frameent camera pitch', s.cam.pitch, pitch, 0.01)
        t.expect_true('the entity is still hovered from the new angle', len([e for e in s.hover if e.idx == idx]) == 1, 'hover=%d' % len(s.hover))
    t.step('lookatent / entsel / frameent: entity selection through the real hover path', entity_step)

    def entsel_shortcut_step():
        # NOT the entity selection test (that is 'entsel --hover' above): this
        # confirms the edentnear shortcut still selects what it is pointed at.
        t.expect_true('the entity from the previous step exists', 'idx' in ent)
        if 'idx' not in ent:
            return
        idx = ent['idx']
        ex, ey, ez = ent['pos']
        send('entcancel', 300)
        t.expect_true('the selection is cleared first', len(state().ent_sel) == 0)
        # 5 units off the entity, inside a 16-unit radius.
        out = ed.entsel(ex + 5, ey, ez, radius=16)
        t.expect_true('edentnear reported index %d' % idx, any(re.search(r'EDRESULT=%d$' % idx, core.strip_stamp(line)) for line in out),
                      'output: ' + ' | '.join(out))
        s = state()
        t.expect_true('entity %d is selected' % idx, len([e for e in s.ent_sel if e.idx == idx]) == 1,
                      'entsel=' + ','.join(str(e.idx) for e in s.ent_sel))
        t.expect_true('nothing else is selected', len(s.ent_sel) == 1, 'entsel count=%d' % len(s.ent_sel))
        # A point ~1400 units from the only entity with an 8-unit radius must
        # select nothing, and report it as -1 rather than throwing.
        send('entcancel', 300)
        out, err = [], ''
        try:
            out = ed.entsel(ex + 1000, ey + 1000, ez, radius=8)
        except core.HarnessError as e:
            err = str(e)
        t.expect_true('a miss does not throw', err == '', "error: '%s'" % err)
        t.expect_true('a miss reports -1', any(re.search(r'EDRESULT=-1$', core.strip_stamp(line)) for line in out), 'output: ' + ' | '.join(out))
        t.expect('a miss selects nothing', len(state().ent_sel), 0)
    t.step('entsel (no --hover): the coordinate-addressed setup shortcut', entsel_shortcut_step)

    def shot_step():
        png = ed.shot('editor-selftest')
        t.expect_true('screenshot written to %s' % png, png and os.path.exists(png), 'shot returned: %s' % png)
    t.step('shot: a screenshot of the framed entity is written', shot_step)

    if not a.keep_running:
        core.stop()
    return t.finish()


if __name__ == '__main__':
    sys.exit(main())
