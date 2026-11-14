#!/usr/bin/env python3
"""In-game self-test for drilling projectiles against BIH geometry (drill-selftest.ps1).

Boots a client, builds a scratch map with geometry instances in front of a
world wall, leaves edit mode and fires the SMG (drill 2) through the real
MOUSE1 bind. Each shot is classified by the stain buffer its first stain lands
in (dbgstain): "mapmodel" is a stain on an instance, "opaque" one on plain
world geometry, and "transparent" one on the wall behind the targets, which
has alpha material so that it can be told from them.

A BIH mesh is a hollow shell: sphere collision only finds its triangles near
the surface, so the drill test once let a bullet that was merely inside a
thick instance carry on through it, without a stain, whenever the hit was
within about 60 degrees of the normal. Thin instances and thin world geometry
must still be drilled through.

  tools/harness/drill_selftest.py [--keep-running]
"""

import argparse
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import core  # noqa: E402
import editor as ed  # noqa: E402
from selftest import Suite, eval_cs, send  # noqa: E402

W_SMG = 4
SHOTS = 3


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('--keep-running', action='store_true')
    a = p.parse_args(argv)
    t = Suite('drill self-test')
    ev = eval_cs

    def run(script, settle=250):
        return send(script, settle, suite=t)

    def set_editing(on):
        run('if (!= $editing %d) [edittoggle]' % on, 900)

    def stand(pos, look, weapon):
        """Stands the player at pos looking at look (edit mode places and aims
        the view, which is the player), then leaves edit mode with the weapon out."""
        set_editing(1)
        run('edgoto %s; edlookat %s' % (pos, look), 300)
        set_editing(0)
        t.expect('editing', ev('$editing'), '0')
        run('weapon %d 1' % weapon, 1500)

    def fire():
        """One shot through the real bind; the stain buffer of its first stain,
        or 'none'. The settle is long enough for a ricochet to land."""
        for line in run('gamekeypress MOUSE1 1; sleep 30 [gamekeypress MOUSE1 0]', 1500):
            m = re.match(r'^tris = \d+, verts = \d+, total tris = \d+, (\w+)', line)
            if m:
                return m.group(1)
        return 'none'

    def expect_shots(expected):
        for i in range(1, SHOTS + 1):
            t.expect('shot %d first stain' % i, fire(), expected)

    core.stop()
    core.start()
    try:
        run('exec "tools/harness/tests/geotemplate.cfg" 0 0')

        def scene():
            run('entediting 0', 300)
            ed.newmap(12)
            run('entediting 1', 300)
            # Template 1: a solid 32-unit block, x 1504..1536, y 1504..1536, z 2048..2080
            ed.sel(1504, 1504, 2048, size=[4, 4, 4])
            run('edfillsel 1')
            # The wall behind the targets: x 1984..2368, y 2200..2208, z 2048..2112
            ed.sel(1984, 2200, 2048, size=[48, 1, 8])
            run('edfillsel 1')
            # editmat, unlike edfillsel, refuses a selection that is not in view
            ed.frame(2176, 2200, 2080, dist=300, yaw=0, pitch=0)
            out = run('editmat alpha', 300)
            t.expect('editmat took the wall', len([l for l in out if re.search(r'not in view|Unknown material', l)]), '0')
            # Template 2: a slab one unit thick, x 1600..1632, y 1504..1505, z 2048..2080
            run('gridpower 0')
            ed.sel(1600, 1504, 2048, size=[32, 1, 32])
            run('edfillsel 1')
            # A world slab one unit thick: x 2342..2358, y 2100..2101, z 2048..2080
            ed.sel(2342, 2100, 2048, size=[16, 1, 32])
            run('edfillsel 1')
            # Template 3: a slab two units thick, x 1700..1732, y 1504..1506, z 2048..2080
            ed.sel(1700, 1504, 2048, size=[32, 2, 32])
            run('edfillsel 1')
            # A world slab two units thick: x 2310..2326, y 2100..2102, z 2048..2080
            ed.sel(2310, 2100, 2048, size=[16, 2, 32])
            run('edfillsel 1')
            run('gridpower 3; cancelsel')
            run('geot_newent geotemplate "1 20 20 18" 1520 1520 2066')
            run('geot_newent geotemplate "2 18 2 18" 1616 1504.5 2064')
            run('geot_newent geotemplate "3 18 3 18" 1716 1505 2064')
            box = lambda i: ' '.join((ev('(geotemplateinfo %d)' % i) or '').split(' ')[0:6])  # noqa: E731
            t.expect('template 1 box', box(1), '1504 1504 2048 1536 1536 2080')
            t.expect('template 2 box', box(2), '1600 1504 2048 1632 1505 2080')
            # Thick: x 2032..2064, y 2084..2116, z 2062..2094
            thick = ev('(geot_newent geoinstance "1 0 0 0 0 0 0 0 0" 2048 2100 2080)')
            # Thin, at half size: x 2192..2208, y 2099.75..2100.25, z 2064..2080
            ev('(geot_newent geoinstance "2 0 0 0 50 0 0 0 0" 2200 2100 2072)')
            # Thin, at full size (as thick as the world slab): x 2104..2136, y 2099.5..2100.5, z 2056..2088
            slab = ev('(geot_newent geoinstance "2 0 0 0 0 0 0 0 0" 2120 2100 2072)')
            t.expect('one-unit instance bounds (rounded out)', ev('(geoinstancebb %s)' % slab), '2104 2099 2056 2136 2101 2088')
            t.expect('ray to the one-unit instance', ev('(edraycast 2120 2000 2072 0 1 0)'), '99.5')
            # Two units thick: x 2264..2296, y 2099..2101, z 2056..2088
            run('geot_newent geoinstance "3 0 0 0 0 0 0 0 0" 2280 2100 2072')
            t.expect('template 3 box', box(3), '1700 1504 2048 1732 1506 2080')
            t.expect('ray to the two-unit instance', ev('(edraycast 2280 2000 2072 0 1 0)'), '99')
            t.expect('ray to the two-unit world slab', ev('(edraycast 2318 2000 2066 0 1 0)'), '100.1')
            t.expect('thick instance bounds', ev('(geoinstancebb %s)' % thick), '2032 2084 2062 2064 2116 2094')
            t.expect('ray to the thick front face', ev('(edraycast 2048 2000 2072 0 1 0)'), '84.0')
            t.expect('ray to the thin slab', ev('(edraycast 2200 2000 2072 0 1 0)'), '99.75')
            t.expect('ray to the world slab', ev('(edraycast 2350 2000 2066 0 1 0)'), '100.1')
            run('dbgstain 1')
        t.step('scene: a thick and a thin geometry instance in front of a world wall', scene)

        cases = [
            ('thick instance, head-on: the bullet stops on the front face', '2048 2010 2066', '2048 2084 2072', 'mapmodel'),
            # Reaches x = 2032 at y ~ 2076, 32 degrees off the -x face's normal
            ('thick instance, side face at about 32 degrees: the bullet stops on it', '1990 2050 2066', '2045 2084 2068', 'mapmodel'),
            ('thick instance, front face at about 70 degrees: the bullet stops on it', '1993.6 2063.5 2066', '2050 2084 2066', 'mapmodel'),
            ('thin instance (0.5 units), head-on: drilled through onto the wall', '2200 2030 2066', '2200 2100 2072', 'transparent'),
            ('thin instance (1 unit), head-on: drilled through onto the wall, as the world slab is', '2120 2030 2066', '2120 2100 2072', 'transparent'),
            ('thin world slab (1 unit), head-on: drilled through onto the wall', '2350 2030 2066', '2350 2100 2068', 'transparent'),
            ('world slab (2 units), head-on: too thick to drill', '2318 2030 2066', '2318 2100 2068', 'opaque'),
            ('instance slab (2 units), head-on: too thick to drill, as the world slab is', '2280 2030 2066', '2280 2100 2072', 'mapmodel'),
        ]
        for name, pos, look, want in cases:
            def case(pos=pos, look=look, want=want):
                stand(pos, look, W_SMG)
                expect_shots(want)
            t.step(name, case)
    finally:
        if not a.keep_running:
            core.stop()
    return t.finish()


if __name__ == '__main__':
    sys.exit(main())
