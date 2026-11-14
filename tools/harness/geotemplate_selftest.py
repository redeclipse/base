#!/usr/bin/env python3
"""End-to-end self-test for geometry templates (geotemplate-selftest.ps1).

Boots a client in the harness home and drives template capture, live updates,
instances, rendering, shadows and collision through DEBUG_UTILS commands
(geotemplateinfo, geoinstancebb, edfillsel, geoinststats, edraycast). Writes
screenshots geot-*.png for review; the comment at each shot says what it must
show. Spec: doc/superpowers/specs/2026-10-02-geometry-templates-design.md

Pixel counts are scaled to a 1280x720 screenshot, where they were measured,
so a HiDPI window's larger screenshots compare the same.

  tools/harness/geotemplate_selftest.py [--keep-running]
"""

import argparse
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import core  # noqa: E402
import editor as ed  # noqa: E402
from selftest import Suite, dark_pixels, eval_cs, map_load, send  # noqa: E402

# 'newmap 12' is 4096 units, solid below z = 2048, with an 8-unit grid.
FLOOR, MID, GRID = 2048, 2048, 8
# The test block: 4x4x4 cubes of 8 at (2048, 2048, 2112), i.e.
# x 2048..2080, y 2048..2080, z 2112..2144.
BX, BY, BZ = 2048, 2048, 2112


class Info:
    def __init__(self, fields):
        self.box = ' '.join(str(v) for v in fields[0:6])
        self.verts, self.tris, self.instances, self.rebuilds = fields[6], fields[7], fields[8], fields[9]


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('--keep-running', action='store_true')
    a = p.parse_args(argv)
    t = Suite('geotemplate self-test', state_on_failure=True)
    ev, state = eval_cs, ed.get_state
    g = {}  # entity indices kept between steps

    def run(script, settle=250):
        return send(script, settle, suite=t)

    def info(tid):
        """geotemplateinfo <id>, parsed; None when there is no such template."""
        v = ev('(geotemplateinfo %d)' % tid)
        if not v:
            return None
        return Info([int(x) for x in v.split()])

    def shot(name):
        return core.invoke_shot(name, quiet=True)

    def drawn():
        return (ev('(geoinststats)') or '').split(' ')

    def hover_ids():
        return ' '.join(str(e.idx) for e in state().hover)

    def ray(what, origin, direction, expected):
        """edraycast from origin along direction: a hit at expected (a
        distance), or a miss when expected is None."""
        d = float(ev('(edraycast %s %s %s %s %s %s)' % tuple(core.format_coord(v) for v in list(origin) + list(direction))))
        if expected is None:
            t.expect_true('%s misses' % what, d < 0, 'got %s' % d)
        else:
            t.expect_true('%s hits at %s' % (what, expected), abs(d - expected) < 0.05, 'got %s, expected %s' % (d, expected))

    def collides(x, y, z, kind):
        """edcollide: a probe centred on x y z, kind 1 = ellipsoid, 2 = oriented box."""
        return ev('(edcollide %s %s %s %d 2 2)' % (core.format_coord(x), core.format_coord(y), core.format_coord(z), kind))

    core.stop()
    core.start()
    try:
        run('exec "tools/harness/tests/geotemplate.cfg" 0 0')

        # ==== Task 1: entity types and the map format ======================

        def old_map():
            with map_load():
                ed.open_map('atop')
            with open(os.path.join(core.HARNESS_DIR, 'tests', 'geotemplate-atop-types.txt'), encoding='utf-8') as f:
                expected = f.read().strip()
            t.expect('atop entity types', ev('(geot_hist)'), expected)
        t.step('a map from before geometry templates loads with every entity type intact', old_map)

        def types_survive_save():
            with map_load():
                ed.newmap(12)
            t.expect('edit mode', state().mode.edit_mode, 1)
            tt = ev('(geot_newent geotemplate "1 16 16 16" 2064 2064 2128)')
            ii = ev('(geot_newent geoinstance "1 90 0 0 50 0 0 0 0" 2300 2048 2200)')
            t.expect_true('newent geotemplate returned an index', bool(re.match(r'^\d+$', tt or '')), "got '%s'" % tt)
            t.expect_true('newent geoinstance returned an index', bool(re.match(r'^\d+$', ii or '')), "got '%s'" % ii)
            t.expect('one geotemplate', ev('(geot_count geotemplate)'), '1')
            t.expect('one geoinstance', ev('(geot_count geoinstance)'), '1')
            run('savemap harness_geot', 1500)
            with map_load():
                ed.open_map('harness_geot')
            # entities::numattrs pads every type to at least 5 attributes, so
            # the 4-attribute geotemplate reads back with one trailing 0.
            t.expect('geotemplate after reload', ev('(geot_get %s)' % tt), 'geotemplate 1 16 16 16 0')
            t.expect('geoinstance after reload', ev('(geot_get %s)' % ii), 'geoinstance 1 90 0 0 50 0 0 0 0')
        t.step('geotemplate and geoinstance entities exist and survive a save', types_survive_save)

        # ==== Task 3: capture, entity edits, save ==========================

        def capture():
            with map_load():
                ed.newmap(12)
            ed.sel(BX, BY, BZ, size=[4, 4, 4])
            run('edfillsel 1')
            # One cube straddling the box's +x face: 2080..2088 against a box ending at 2084
            ed.sel(BX + 32, BY, BZ, size=[1, 1, 1])
            run('edfillsel 1')
            g['T1'] = int(ev('(geot_newent geotemplate "1 20 20 20" 2064 2064 2128)'))
            i = info(1)
            t.expect('captured box', i.box, '2048 2048 2112 2080 2080 2144')
            t.expect_true('triangles built', i.tris > 0, 'tris %d' % i.tris)
            t.expect('geotemplateinfo of a missing id', ev('(geotemplateinfo 99)'), '')
        t.step('capture: whole cubes inside the requested box, straddlers excluded', capture)

        def off_grid():
            before = info(1)
            # Box x 2048.5..2088.5: the block's first column (x 2048) leaves, the straddler joins
            run('geot_moveent %d 2068.5 2064 2128' % g['T1'])
            after = info(1)
            t.expect('captured box after an off-grid move', after.box, '2056 2048 2112 2088 2080 2144')
            t.expect('one rebuild for the move', after.rebuilds, before.rebuilds + 1)
            run('geot_moveent %d 2064 2064 2128' % g['T1'])
            t.expect('captured box after moving back', info(1).box, '2048 2048 2112 2080 2080 2144')
        t.step('an off-grid template entity: the captured box snaps to the cubes it takes', off_grid)

        def reid():
            run('geot_setattr %d 0 5' % g['T1'])
            t.expect('the old id is gone', ev('(geotemplateinfo 1)'), '')
            t.expect('the new id has the same box', info(5).box, '2048 2048 2112 2080 2080 2144')
            dup = int(ev('(geot_newent geotemplate "5 8 8 8" 2300 2300 2300)'))
            t.expect('a duplicate id keeps the first definition', info(5).box, '2048 2048 2112 2080 2080 2144')
            run('geot_delent %d' % dup)
            run('geot_setattr %d 0 1' % g['T1'])
            t.expect('id 1 again', info(1).box, '2048 2048 2112 2080 2080 2144')
        t.step('re-id and duplicate ids', reid)

        def survive_reload():
            before = info(1)
            run('savemap harness_geot', 1500)
            with map_load():
                ed.open_map('harness_geot')
            after = info(1)
            t.expect('box after reload', after.box, before.box)
            t.expect('triangles after reload', after.tris, before.tris)
        t.step('templates survive a save and reload', survive_reload)

        # ==== Task 4: live geometry edits ==================================

        def edit_inside():
            before = info(1)
            ed.sel(BX, BY, BZ + 24, size=[1, 1, 1])  # the block's top corner cube
            run('edfillsel 0')
            after = info(1)
            t.expect('rebuilt once', after.rebuilds, before.rebuilds + 1)
            t.expect_true('triangle count changed', after.tris != before.tris, '%d -> %d' % (before.tris, after.tris))
            t.expect('captured box unchanged', after.box, before.box)
        t.step('an edit inside the box rebuilds the template on the same commit', edit_inside)

        def edit_outside():
            before = info(1)
            ed.sel(2400, 2400, BZ, size=[1, 1, 1])
            run('edfillsel 1')
            t.expect('not rebuilt', info(1).rebuilds, before.rebuilds)
        t.step('an edit outside every box does not rebuild', edit_outside)

        def overlapping():
            t2 = int(ev('(geot_newent geotemplate "2 20 20 20" 2048 2064 2128)'))
            x, y = info(1), info(2)
            ed.sel(2056, 2056, BZ, size=[1, 1, 1])  # inside both boxes
            run('edfillsel 0')
            t.expect('template 1 rebuilt', info(1).rebuilds, x.rebuilds + 1)
            t.expect('template 2 rebuilt', info(2).rebuilds, y.rebuilds + 1)
            # undo refuses (noedit()) unless the selection is in view, unlike
            # edfillsel, which needs no view: put the camera on it first
            ed.frame(2060, 2060, BZ + 4, dist=60, yaw=0, pitch=0)
            t.expect('undo succeeded', ev('(undo)'), '1')
            t.expect('undo rebuilds template 1 again', info(1).rebuilds, x.rebuilds + 2)
            t.expect('undo restores the triangles', info(1).tris, x.tris)
            run('geot_delent %d' % t2)
            t.expect('template 2 gone', ev('(geotemplateinfo 2)'), '')
        t.step('overlapping templates both rebuild; undo rebuilds again', overlapping)

        # ==== Task 5: instances in the octree ==============================

        def bounds():
            # Rebuild the block whole, so its captured box and pivot are
            # symmetric: pivot (2064,2064,2128), captured box +-16 around it
            ed.sel(BX, BY, BZ, size=[4, 4, 4])
            run('edfillsel 1')
            t.expect('block whole again', info(1).box, '2048 2048 2112 2080 2080 2144')
            g['I1'] = int(ev('(geot_newent geoinstance "1 90 0 0 0 0 0 0 0" 2300 2048 2200)'))
            t.expect('instance count', info(1).instances, 1)
            t.expect('bounds at yaw 90', ev('(geoinstancebb %d)' % g['I1']), '2284 2032 2184 2316 2064 2216')
            run('geot_setattr %d 4 50' % g['I1'])
            t.expect('bounds at scale 50', ev('(geoinstancebb %d)' % g['I1']), '2292 2040 2192 2308 2056 2208')
            run('geot_setattr %d 0 42' % g['I1'])
            t.expect('no bounds without a template', ev('(geoinstancebb %d)' % g['I1']), '')
            run('geot_setattr %d 0 1' % g['I1'])
            run('geot_setattr %d 4 0' % g['I1'])
            t.expect('bounds back', ev('(geoinstancebb %d)' % g['I1']), '2284 2032 2184 2316 2064 2216')
        t.step('instance bounds follow rotation and scale', bounds)

        def picked():
            ed.cursor('off')
            ed.lookatent(g['I1'])
            time.sleep(0.4)
            t.expect('hovered entity', hover_ids(), str(g['I1']))
            # A selected instance draws its full bounds, not the picking box:
            # check the shared selection-box function through those bounds
            run('geot_setattr %d 4 300' % g['I1'])
            t.expect('bounds at scale 300', ev('(geoinstancebb %d)' % g['I1']), '2252 2000 2152 2348 2096 2248')
            run('geot_setattr %d 4 0' % g['I1'])
            shot('geot-instance-box')  # the instance's selection box, hovered, at scale 100
        t.step('a registered instance is picked by the editor ray and its box follows scale', picked)

        def pivot_outside():
            # Box z 2112..2240 (centre 2176) holds the block (z 2112..2144): the
            # pivot is 32 units above the captured cubes, so the instance
            # position lies 32 above the transformed capture.
            t3 = int(ev('(geot_newent geotemplate "3 16 16 64" 2064 2064 2176)'))
            t.expect('captured box is the block', info(3).box, '2048 2048 2112 2080 2080 2144')
            i3 = int(ev('(geot_newent geoinstance "3 0 0 0 0 0 0 0 0" 2112 2064 2200)'))
            t.expect('bounds exclude the position', ev('(geoinstancebb %d)' % i3), '2096 2048 2136 2128 2080 2168')
            ed.cursor('off')
            ed.lookatent(i3)
            time.sleep(0.4)
            t.expect('hovered entity', hover_ids(), str(i3))
            run('geot_delent %d' % i3)
            run('geot_delent %d' % t3)
        t.step('an instance whose pivot lies outside its captured geometry is still picked at its position', pivot_outside)

        # ==== Task 7: G-buffer instances ===================================

        def gbuffer():
            g['I2'] = int(ev('(geot_newent geoinstance "1 45 0 0 150 0 0 0 0" 2360 2048 2200)'))
            ed.frame(2330, 2048, 2200, dist=220, yaw=0, pitch=-15)
            run('sleep 1 []', 400)
            st = drawn()
            t.expect('instances drawn', st[0], '2')
            t.expect('triangles drawn', st[1], str(2 * info(1).tris))
            # Must show two copies of the block in the air: one turned 90
            # degrees, one turned 45 degrees and 1.5x larger.
            shot('geot-instances')
        t.step('G-buffer: visible instances are drawn instanced', gbuffer)

        def source_edit():
            ed.sel(BX, BY, BZ + 24, size=[1, 1, 1])
            run('edfillsel 0')
            run('sleep 1 []', 400)
            t.expect('triangles drawn follow the edit', drawn()[1], str(2 * info(1).tris))
            # Both copies now miss the same top corner cube as the source.
            shot('geot-instances-edited')
            ed.sel(BX, BY, BZ + 24, size=[1, 1, 1])
            run('edfillsel 1')
        t.step('editing the source changes every instance at once', source_edit)

        def out_of_view():
            ed.goto(2330, 2048, 2600)
            ed.aim(0, 90)
            run('sleep 1 []', 400)
            t.expect('nothing drawn looking at the sky', drawn()[0], '0')
        t.step('instances out of view are not drawn', out_of_view)

        def distant():
            # The node's box query must not test against the instance itself:
            # drawn after it, the box's faces z-fight with the instance's and a
            # small box fails the pixel threshold.
            run('geot_setattr %d 0 42' % g['I1'])
            run('geot_setattr %d 0 42' % g['I2'])
            g['I3'] = int(ev('(geot_newent geoinstance "1 0 0 0 0 0 0 0 0" 2048 3900 2150)'))
            for dist in (1500, 2200, 3000):
                ed.frame(2048, 3900, 2150, dist=dist, yaw=0, pitch=0)
                run('sleep 1 []', 500)
                counts = []
                for _ in range(12):
                    run('sleep 1 []', 80)
                    counts.append(drawn()[0])
                t.expect('instances drawn at distance %d, 12 frames' % dist, ' '.join(counts), ' '.join(['1'] * 12))
        t.step('a small, distant instance is drawn on every frame', distant)

        def occlusion():
            run('oqinst 1')
            ed.frame(2048, 3900, 2150, dist=300, yaw=0, pitch=0)
            run('sleep 1 []', 600)
            t.expect('visible without a wall', drawn()[0], '1')
            # A wall 40x40 cells, one cell thick, 150 units in front of the instance
            ed.sel(1888, 3752, 2056, size=[40, 1, 40])
            run('edfillsel 1')
            ed.frame(2048, 3900, 2150, dist=300, yaw=0, pitch=0)
            for _ in range(3):
                run('sleep 1 []', 600)
            t.expect('culled behind the wall', drawn()[0], '0')
            shot('geot-instance-occluded')  # a wall filling the view; no block behind it showing through
            ed.sel(1888, 3752, 2056, size=[40, 1, 40])
            run('edfillsel 0')
            for _ in range(2):
                run('sleep 1 []', 600)
            t.expect('drawn again with the wall gone', drawn()[0], '1')
            run('geot_delent %d' % g['I3'])
            run('geot_setattr %d 0 1' % g['I1'])
            run('geot_setattr %d 0 1' % g['I2'])
        t.step('an instance behind world geometry is occlusion-culled, and drawn again once it is gone', occlusion)

        # ==== Task 8: editor boxes =========================================

        def boxes():
            ed.frame(2064, 2064, 2128, dist=140, yaw=30, pitch=-25)
            run('sleep 1 []', 400)
            # Must show the source block with a faint grey box (requested,
            # 2044..2084) around a bright cyan box tight on the block
            # (captured, 2048..2080).
            path = shot('geot-boxes')
            # The captured box is drawn at (0,160,160) with additive blending,
            # so on its edges over the dark teal ground the sum is R<40, G>150,
            # B>150; nothing else in this view matches. Measured on the
            # 1280x720 shot: ~684 matching pixels in the block's neighbourhood
            # (x 0.40..0.62, y 0.35..0.70 of the image) and 0 elsewhere.
            w, h, ch, rows = core.read_png(path)
            cyan = 0
            for y in range(int(h * 0.35), int(h * 0.70)):
                row = rows[y]
                for x in range(int(w * 0.40), int(w * 0.62)):
                    o = x * ch
                    if row[o] < 40 and row[o + 1] > 150 and row[o + 2] > 150:
                        cyan += 1
            cyan = cyan * (1280 * 720) / float(w * h)
            t.expect_true('captured box draws cyan edges', cyan >= 300,
                          'found %d cyan edge pixels around the block, as at 1280x720 (need >= 300, expect ~680)' % cyan)
        t.step('edit mode draws each template box', boxes)

        # ==== Task 9: shadows and GI =======================================

        def shadows():
            run('sunlightpitch 50; sunlightyaw 30', 400)
            ed.frame(2330, 2048, 2120, dist=260, yaw=200, pitch=-35)
            run('sleep 1 []', 500)
            # Must show both instances' shadows on the floor below them,
            # matching their shapes (one square-ish, one turned 45 degrees and larger).
            sun = dark_pixels(shot('geot-sun-shadow'), 0.44, 0.62, 0.40, 0.56, 30)
            # Control: the same view with both instances flagged no-shadow
            run('geot_setattr %d 5 1; geot_setattr %d 5 1' % (g['I1'], g['I2']), 500)
            run('sleep 1 []', 500)
            sun_control = dark_pixels(shot('geot-sun-noshadow'), 0.44, 0.62, 0.40, 0.56, 30)
            t.out('     counts: sun %d / control %d' % (sun, sun_control))
            t.expect_true('sun shadows of both instances on the floor', sun >= 3000 and sun_control <= 300,
                          'dark floor pixels: %d with shadows (need >= 3000), %d with no-shadow (need <= 300)' % (sun, sun_control))
            g['L1'] = int(ev('(geot_newent light "400 255 255 255" 2330 2120 2240)'))
            run('sunlight 0', 500)
            run('sleep 1 []', 500)
            point_control = dark_pixels(shot('geot-point-noshadow'), 0.30, 0.66, 0.42, 0.58, 20)
            run('geot_setattr %d 5 0; geot_setattr %d 5 0' % (g['I1'], g['I2']), 500)
            run('sleep 1 []', 500)
            # Must show point-light shadows of both instances on the floor, cast away from the light.
            point = dark_pixels(shot('geot-point-shadow'), 0.30, 0.66, 0.42, 0.58, 20)
            t.out('     counts: point %d / control %d' % (point, point_control))
            t.expect_true('point-light shadows of both instances on the floor', point >= 8000 and point_control <= 1500,
                          'dark floor pixels: %d with shadows (need >= 8000), %d with no-shadow (need <= 1500)' % (point, point_control))
            run('sunlight 0xA0A090', 300)
        t.step('instances cast sun and point-light shadows', shadows)

        def gi_bounce():
            pts = os.path.join(core.HOME_DIR, 'harness', 'geot-rh-points.txt')
            os.makedirs(os.path.dirname(pts), exist_ok=True)
            core.write_text(pts, '2300 2048 2049 0 0 1\n2330 2048 2049 0 0 1\n2360 2048 2049 0 0 1\n')
            run('geot_delent %d' % g['L1'], 300)
            run('sleep 1 []', 600)
            t.expect('probed points', ev('(rhprobe "harness/geot-rh-points.txt" "harness/geot-rh-a.txt")'), '3')
            run('geot_setattr %d 5 1; geot_setattr %d 5 1' % (g['I1'], g['I2']), 600)  # no-shadow: out of the RSM
            run('sleep 1 []', 600)
            ev('(rhprobe "harness/geot-rh-points.txt" "harness/geot-rh-b.txt")')
            with open(os.path.join(core.HOME_DIR, 'harness', 'geot-rh-a.txt'), encoding='utf-8') as f:
                x = f.read()
            with open(os.path.join(core.HOME_DIR, 'harness', 'geot-rh-b.txt'), encoding='utf-8') as f:
                y = f.read()
            t.expect_true('GI under the instances changes when they leave the RSM', x != y)
            run('geot_setattr %d 5 0; geot_setattr %d 5 0' % (g['I1'], g['I2']), 300)
        t.step('instances bounce light into the radiance hints', gi_bounce)

        # ==== Task 10: cached shadow meshes ================================

        def compare_shots(a_path, b_path):
            wa, ha, ca, ra = core.read_png(a_path)
            wb, hb, cb, rb = core.read_png(b_path)
            diff = total = 0
            for y in range(0, ha, 2):
                xa, xb = ra[y], rb[y]
                for x in range(0, wa, 2):
                    oa, ob = x * ca, x * cb
                    if abs(xa[oa] - xb[ob]) + abs(xa[oa + 1] - xb[ob + 1]) + abs(xa[oa + 2] - xb[ob + 2]) > 24:
                        diff += 1
                    total += 1
            return diff / float(total)

        def shadow_mesh():
            g['L2'] = int(ev('(geot_newent light "400 255 255 255" 2330 2120 2240)'))
            run('sunlight 0', 300)
            run('savemap harness_geot', 1500)
            with map_load():
                ed.open_map('harness_geot')
            # A loaded map's entities come back in a different order: find them again
            g['L2'] = int(ev('(geot_find light)'))
            g['I1'] = int(ev('(geot_find geoinstance 2 90)'))
            g['I2'] = int(ev('(geot_find geoinstance 2 45)'))
            t.out('     after reload: light %d, instances %d %d' % (g['L2'], g['I1'], g['I2']))
            t.expect_true('entities found again after the reload', g['L2'] >= 0 and g['I1'] >= 0 and g['I2'] >= 0)
            run('smmesh 1', 300)
            run('sunlight 1; sunlight 0', 300)  # sunlight's VARF clears the shadow-map cache (and the meshes)
            # allchanged() discards the meshes it builds at load, so build them now
            meshes = int(ev('(edgenshadowmeshes)'))
            t.expect_true('shadow meshes were generated', meshes > 0, '%d meshes' % meshes)
            ed.frame(2330, 2048, 2120, dist=260, yaw=200, pitch=-35)
            run('sleep 1 []', 600)
            t.expect_true('the meshes are still there when shot', int(ev('(edshadowmeshcount)')) > 0)
            on = shot('geot-mesh-on')
            run('smmesh 0', 300)  # clears the meshes: the live path
            run('sunlight 1; sunlight 0', 300)  # clears the shadow-map cache too
            run('sleep 1 []', 600)
            t.expect('no meshes on the live path', ev('(edshadowmeshcount)'), '0')
            off = shot('geot-mesh-off')
            frac = compare_shots(on, off)
            t.out('     mesh vs live: %.2f%% of pixels differ' % (frac * 100))
            t.expect_true('cached and live shadows agree', frac < 0.01, '%.2f%% of pixels differ' % (frac * 100))
            run('smmesh 1; sunlight 0xA0A090', 300)
            run('geot_delent %d' % g['L2'], 300)
        t.step('a cached shadow mesh includes the instances', shadow_mesh)

        # ==== Task 11: collision and raycasts ==============================

        def raycasts():
            # A 16-unit block, template 7 pivoted at its centre
            ed.sel(2048, 2400, 2112, size=[2, 2, 2])
            run('edfillsel 1')
            ev('(geot_newent geotemplate "7 12 12 12" 2056 2408 2120)')
            t.expect('template 7', info(7).box, '2048 2400 2112 2064 2416 2128')
            # yaw 45, scale 200: a 32-unit cube on its edge, centred at
            # (2400, 2600, 2300). Its -y corner is 16*sqrt(2) = 22.627 from the
            # centre; the ray runs 2 units off that vertical edge, where the
            # faces are y = -22.627 + |x|, so it hits at y = 2579.373.
            g['I7'] = int(ev('(geot_newent geoinstance "7 45 0 0 200 0 0 0 0" 2400 2600 2300)'))
            for what, cmd in (('hit distance', '(edraycast 2402 2500 2300 0 1 0)'), ('hit distance from the other side', '(edraycast 2402 2700 2300 0 -1 0)')):
                d = float(ev(cmd))
                t.expect_true(what, abs(d - 79.373) < 0.05, 'got %s, expected 79.373' % d)
            run('geot_setattr %d 5 2' % g['I7'])  # no-collide
            d = float(ev('(edraycast 2402 2500 2300 0 1 0)'))
            t.expect_true('a no-collide instance is not hit', d < 0, 'got %s' % d)
            run('geot_setattr %d 5 0' % g['I7'])
            run('geot_setattr %d 0 8' % g['I7'])  # no such template
            d = float(ev('(edraycast 2402 2500 2300 0 1 0)'))
            t.expect_true('an instance without a template is not hit', d < 0, 'got %s' % d)
            run('geot_setattr %d 0 7' % g['I7'])
        t.step('raycasts hit a rotated, scaled instance where its geometry is', raycasts)

        def several_arrays():
            # A 16-unit block straddling x = 2048, a top-level octree boundary:
            # the template gets one vertex array (one BIH mesh) per side, 5
            # faces (20 vertices) each; one array would have 24
            ed.sel(2040, 2760, 2200, size=[2, 2, 2])
            run('edfillsel 1')
            ev('(geot_newent geotemplate "10 12 12 12" 2048 2768 2208)')
            t.expect('template 10', info(10).box, '2040 2760 2200 2056 2776 2216')
            t.expect('two vertex arrays: 2 x 20 vertices', info(10).verts, 40)
            # Pivot at the block's centre, so the block is the instance's position +-8
            ev('(geot_newent geoinstance "10 0 0 0 0 0 0 0 0" 2400 3000 2300)')
            d = float(ev('(edraycast 2350 3003 2302 1 0 0)'))
            t.expect_true('near face from -x', abs(d - 42) < 0.05, 'got %s, expected 42' % d)
            d = float(ev('(edraycast 2450 3003 2302 -1 0 0)'))
            t.expect_true('near face from +x', abs(d - 42) < 0.05, 'got %s, expected 42' % d)
        t.step('a ray reports the nearest face when a template has several vertex arrays', several_arrays)

        def lopsided():
            # A 16 x 16 x 32 block lying to one side of its pivot: x 0..16,
            # y -16..0, z -16..16 from it. Its farthest corner is 27.7 from the
            # pivot, but the two diagonal corners are only 22.6 away.
            ed.sel(2056, 2624, 2200, size=[2, 2, 4])
            run('edfillsel 1')
            ev('(geot_newent geotemplate "9 16 16 16" 2056 2640 2216)')
            t.expect('template 9', info(9).box, '2056 2624 2200 2072 2640 2232')
            # Roll 180 turns it to x -16..0 around the instance. The ray runs
            # along (1, -1, 0), 26.9 from the pivot at its closest, and enters
            # the x = -16 face 1 unit inside the corner: 9.5*sqrt(2).
            ev('(geot_newent geoinstance "9 0 0 180 0 0 0 0 0" 2700 3300 2400)')
            d = float(ev('(edraycast 2674.5 3294.5 2415.5 1 -1 0)'))
            t.expect_true('hit distance', abs(d - 13.435) < 0.05, 'got %s, expected 13.435' % d)
        t.step('a ray grazing the far corner of a lopsided template is not rejected early', lopsided)

        # ==== Pitch and roll, end to end ===================================

        def pitch_roll():
            # Template 9, relative to its pivot: x 0..16, y -16..0, z -16..16.
            # Pitch 90 and roll 90 put a local (x, y, z) at (-z, -x, y) from
            # the instance, so the block occupies X -16..16, Y -16..0, Z -16..0
            # around P = (2700, 3500, 2400).
            px, py, pz = 2700, 3500, 2400
            ip = int(ev('(geot_newent geoinstance "9 0 90 90 0 0 0 0 0" %d %d %d)' % (px, py, pz)))
            t.expect('bounds', ev('(geoinstancebb %d)' % ip), '2684 3484 2384 2716 3500 2400')
            ray('down onto the top', (px + 3, py - 5, pz + 50), (0, 0, -1), 50)
            # Roll alone (Z 0..16) and Rx-first (Z 0..16) would miss; pitch alone would hit at 34
            ray('onto the +y face', (px + 3, py + 50, pz - 5), (0, -1, 0), 50)
            ray('onto the +x face', (px + 50, py - 5, pz - 5), (-1, 0, 0), 34)
            ray('onto the -y face', (px + 3, py - 50, pz - 5), (0, 1, 0), 34)
            # a ray at Y +8 passes beside the block (pitch alone has it reach Y +16)
            ray('beside the block', (px + 50, py + 8, pz - 5), (-1, 0, 0), None)
            # A probe of radius 2 centred 1 unit off a face: the ellipsoid and
            # the oriented box (the physics code takes a different path for each)
            for kind, name in ((1, 'ellipsoid'), (2, 'box')):
                t.expect('%s touching the +y face' % name, collides(px + 3, py + 1, pz - 5, kind), '1')
                t.expect('%s touching the +x face' % name, collides(px + 17, py - 5, pz - 5, kind), '1')
                t.expect('%s touching the -x face (X -16: pitch alone has no face there)' % name, collides(px - 17, py - 5, pz - 5, kind), '1')
                t.expect('%s touching the top face' % name, collides(px + 3, py - 5, pz + 1, kind), '1')
                t.expect('%s touching the bottom face' % name, collides(px + 3, py - 5, pz - 17, kind), '1')
                t.expect('%s touching the -y face' % name, collides(px + 3, py - 17, pz - 5, kind), '1')
                t.expect('%s past the +y face (Y 17: pitch alone has a face at 16)' % name, collides(px + 3, py + 17, pz - 5, kind), '0')
                t.expect('%s above the top face (Z 8: roll alone has a face at 0)' % name, collides(px + 3, py - 5, pz + 8, kind), '0')
                t.expect('%s inside, clear of every face' % name, collides(px, py - 8, pz - 8, kind), '0')
            # Beside the vertical edge X 16, Y 0, the probe centre 1.6 off both
            # faces: the ellipsoid (radius 2) is 2.26 from the edge and clear,
            # the box overlaps it
            t.expect('ellipsoid clear of the edge', collides(px + 17.6, py + 1.6, pz - 8, 1), '0')
            t.expect('box overlapping the edge', collides(px + 17.6, py + 1.6, pz - 8, 2), '1')
            ed.frame(px, py, pz, dist=90, yaw=30, pitch=20)
            run('sleep 1 []', 400)
            shot('geot-pitch-roll')  # a flat slab standing on its edge: 32 long in x, 16 in y and z
        t.step('a pitched and rolled instance is raycast and collided where the transform puts it', pitch_roll)

        # ==== Lifecycle with live instances ================================

        # Template 20: a 16-unit block, the entity at its centre, raised by dz.
        # Instances A (yaw 0) and B (yaw 180, equivalent: the block is
        # symmetric about the pivot in x and y) stand at the places below; D
        # is replaced in every state.
        life_pos = {'A': (3300, 2908, 2300), 'B': (3300, 3100, 2300), 'D': (3300, 3250, 2300)}
        life_block = (3000, 2904, 2112)  # the block's minimum corner
        life = {'T': -1, 'A': -1, 'B': -1, 'D': -1}

        def life_probe(name, idx, pos, present, dz):
            # The block spans, relative to the instance, x and y -8..8 and z -8-dz .. 8-dz
            bb = ''
            if present:
                bb = '%s %s %s %s %s %s' % tuple(core.format_coord(v) for v in (pos[0] - 8, pos[1] - 8, pos[2] - 8 - dz, pos[0] + 8, pos[1] + 8, pos[2] + 8 - dz))
            t.expect('%s bounds' % name, ev('(geoinstancebb %d)' % idx), bb)
            # Two rays along +x, 9.5 below and 7.5 above the instance: the
            # first hits once the block has dropped by 2 (z spans -10..6), the
            # second only while it has not (-8..8). 42 from 50 away to x = -8.
            ray('%s low ray' % name, (pos[0] - 50, pos[1] + 3, pos[2] - 9.5), (1, 0, 0), 42 if present and dz >= 2 else None)
            ray('%s high ray' % name, (pos[0] - 50, pos[1] + 3, pos[2] + 7.5), (1, 0, 0), 42 if present and dz <= 0 else None)

        def life_hover(name, idx, pos):
            ed.cursor('off')
            ed.goto(pos[0] - 100, pos[1], pos[2])
            ed.lookatent(idx)
            time.sleep(0.4)
            t.expect('%s hovered' % name, hover_ids(), str(idx))

        def life_new_instance(name, yaw):
            pos = life_pos[name]
            life[name] = int(ev('(geot_newent geoinstance "20 %s 0 0 0 0 0 0 0" %d %d %d)' % ((yaw,) + pos)))

        def life_state(label, present, dz):
            """The state every transition must leave: the template present or
            missing, raised by dz. Probes both live instances, hovers and moves
            one, deletes the instance an earlier state left behind and makes a
            new one."""
            pa, pb = life_pos['A'], life_pos['B']
            with_d = 3 if life['D'] >= 0 else 2
            if present:
                t.expect('%s template 20 instances' % label, info(20).instances, with_d)
            else:
                t.expect('%s template 20 absent' % label, ev('(geotemplateinfo 20)'), '')
            life_probe('%s A' % label, life['A'], pa, present, dz)
            life_probe('%s B' % label, life['B'], pb, present, dz)
            if life['D'] >= 0:
                life_probe('%s old D' % label, life['D'], life_pos['D'], present, dz)
                run('geot_delent %d' % life['D'])
                if present:
                    t.expect('%s instances after deleting D' % label, info(20).instances, 2)
                t.expect('%s D gone' % label, ev('(at (geot_get %d) 0)' % life['D']), 'none')
                life['D'] = -1
            life_hover('%s A' % label, life['A'], pa)
            life_hover('%s B' % label, life['B'], pb)
            moved = (pa[0], pa[1] + 40, pa[2])
            run('geot_moveent %d %d %d %d' % ((life['A'],) + moved))
            life_probe('%s A moved' % label, life['A'], moved, present, dz)
            life_hover('%s A moved' % label, life['A'], moved)
            run('geot_moveent %d %d %d %d' % ((life['A'],) + pa))
            life_probe('%s A back' % label, life['A'], pa, present, dz)
            life_new_instance('D', '90')
            life_probe('%s new D' % label, life['D'], life_pos['D'], present, dz)
            # Drawn: all three are in view when the template is there, none when it is not
            ed.frame(3300, 3050, 2300, dist=500, yaw=0, pitch=0)
            run('sleep 1 []', 600)
            run('sleep 1 []', 600)
            t.expect('%s instances drawn' % label, drawn()[0], '3' if present else '0')

        def life_baseline():
            with map_load():
                ed.newmap(12)
            run('oqinst 1')
            ed.sel(*life_block, size=[2, 2, 2])
            run('edfillsel 1')
            life['T'] = int(ev('(geot_newent geotemplate "20 12 12 12" 3008 2912 2120)'))
            t.expect('captured box', info(20).box, '3000 2904 2112 3016 2920 2128')
            life_new_instance('A', '0')
            life_new_instance('B', '180')
            life_new_instance('D', '90')
            life_state('baseline', True, 0)
        t.step('lifecycle: instances with a live template', life_baseline)

        def life_move():
            # Up by 2: the box is z 2110..2134 and still takes the whole block
            run('geot_moveent %d 3008 2912 2122' % life['T'])
            t.expect('captured box', info(20).box, '3000 2904 2112 3016 2920 2128')
            life_state('moved', True, 2)
        t.step('lifecycle: move the template entity', life_move)

        def life_reid():
            run('geot_setattr %d 0 21' % life['T'])
            t.expect('template 21 has no instances', info(21).instances, 0)
            life_state('missing id', False, 2)
        t.step('lifecycle: re-id the template, so the instances reference a missing id', life_reid)

        def life_reid_back():
            run('geot_setattr %d 0 20' % life['T'])
            life_state('id back', True, 2)
        t.step('lifecycle: re-id it back', life_reid_back)

        def life_recreate():
            run('geot_delent %d' % life['T'])
            life_state('template deleted', False, 2)
            life['T'] = int(ev('(geot_newent geotemplate "20 12 12 12" 3008 2912 2122)'))
            life_state('template recreated', True, 2)
        t.step('lifecycle: delete the template entity, then recreate it', life_recreate)

        def life_resetgl():
            run('resetgl', 5000)
            life_state('after resetgl', True, 2)
        t.step('lifecycle: resetgl (cleanupva, then allchanged)', life_resetgl)

        def life_maps():
            run('savemap harness_geot_life', 1500)
            with map_load():
                ed.newmap(12)
            t.expect('no template in the other map', ev('(geotemplateinfo 20)'), '')
            with map_load():
                ed.open_map('harness_geot_life')
            # A loaded map's entities come back in a different order: find them again
            life['T'] = int(ev('(geot_find geotemplate)'))
            life['A'] = int(ev('(geot_find geoinstance 2 0)'))
            life['B'] = int(ev('(geot_find geoinstance 2 180)'))
            life['D'] = int(ev('(geot_find geoinstance 2 90)'))
            t.out('     after reload: %d %d %d %d' % (life['T'], life['A'], life['B'], life['D']))
            t.expect_true('entities found again after the reload', min(life.values()) >= 0)
            run('oqinst 1')
            life_state('after reload', True, 2)
            run('geot_delent %d' % life['D'])
        t.step('lifecycle: switch maps and come back', life_maps)
    finally:
        if not a.keep_running:
            core.stop()
    return t.finish()


if __name__ == '__main__':
    sys.exit(main())
