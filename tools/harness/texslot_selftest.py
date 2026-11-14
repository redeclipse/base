#!/usr/bin/env python3
"""Regression self-test for the map editor's texture slot editor (texslot-selftest.ps1).

Boots a client in the harness home, opens the shipped map 'atop' in the editor
and drives the slot editor the way its panel does (tool_tex_editslot ->
ui_tool_texeditslot_on_open -> tool_tex_editslot_apply), plus the engine's
editslot and cloneslot commands directly. Each step is one of the defects in
doc/texture-slot-editing-findings.md.

Slot indices are discovered on the loaded map rather than hardcoded, but the
test does rely on atop having a glowdecal decal and a loaded 512px world slot,
and on appleflap/danger.jpg being 64x64.

  tools/harness/texslot_selftest.py [--keep-running]
"""

import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import core  # noqa: E402
import editor as ed  # noqa: E402
from selftest import Suite, eval_cs, map_load, send  # noqa: E402

# getvtexname reports the path as the engine stored it, with path()'s
# separator: '\' on Windows, '/' here.
SEP = '\\' if os.name == 'nt' else '/'


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('--keep-running', action='store_true')
    a = p.parse_args(argv)
    t = Suite('texture slot self-test')
    ev = eval_cs

    core.start()
    try:
        def setup():
            with map_load():
                ed.open_map('atop')
            t.expect('edit mode', ev('$editing'), '1')
        t.step('setup: atop is open in the editor', setup)

        def decal_shader():
            d = ev('(listfind i (loopconcat j $numdecalslots [result $j]) [=s (getvshadername $i 1) glowdecal])')
            if int(d) < 0:
                raise core.HarnessError('atop has no glowdecal decal slot')
            send('tool_tex_cur = 7; tool_tex_editslot %s 1; tool_tex_editslot_apply' % d, 400)
            t.expect('decal %s shader' % d, ev('(getvshadername %s 1)' % d), 'glowdecal')
            t.expect('editing a decal leaves the world texture brush alone', ev('$tool_tex_cur'), '7')
            send('toolpanel_close tool_texeditslot', 200)
        t.step('an unchanged apply keeps a non-bump decal shader (glowdecal fell back to stddecal)', decal_shader)

        def rescale():
            s = ev('(listfind i (loopconcat j $numslots [result $j]) [&& [> $i 1] [texloaded $i] [= (getvteximgwidth (getslottex $i) 0) 512]])')
            if int(s) < 0:
                raise core.HarnessError('atop has no loaded 512px slot')
            v = ev('(getslottex %s)' % s)
            scale = core.to_float(ev('(getvscale %s)' % v))
            want = min(8.0, max(0.125, scale * 8.0))
            send('tool_tex_editslot %s 0; tool_tex_editslot_apply_rescale = 1; tool_texeditslot_tex0 = "appleflap/danger.jpg"; tool_tex_editslot_apply' % v, 400)
            t.expect('slot %s scale after 512 -> 64' % s, core.to_float(ev('(getvscale %s)' % v)), want)
            send('tool_texeditslot_tex0 = "textures/default"; tool_tex_editslot_apply', 400)
            t.expect('slot %s scale after 64 -> 1024' % s, core.to_float(ev('(getvscale %s)' % v)), want / 16.0)
            send('toolpanel_close tool_texeditslot', 200)
        t.step('shrinking the diffuse with compensate scale gives a finite, clamped scale (was inf)', rescale)

        def bound_slot():
            before = ev('(getvtexname 6 0)')
            send('tool_tex_editslot 5 0; tool_focus_tex 6; tool_tex_editslot_apply', 400)
            t.expect('slot 6 diffuse', ev('(getvtexname 6 0)'), before)
            send('toolpanel_close tool_texeditslot', 200)
        t.step('apply writes to the slot the editor was opened on, not the current texture', bound_slot)

        def reopen():
            send('tool_tex_editslot 5 0; tool_tex_editslot 7 0', 400)
            t.expect('diffuse field', ev('$tool_texeditslot_tex0'), ev('(fixpathslashes (getvtexname 7 0))'))
            send('toolpanel_close tool_texeditslot', 200)
        t.step('reopening the editor on another slot re-reads its fields', reopen)

        def changed_under():
            orig = ev('(getvtexname 8 0)')
            send('tool_tex_editslot 8 0; editslot 8 [ texture 0 "textures/default" ] 0 0; tool_texeditslot_tex0 = "appleflap/danger.jpg"; '
                 'tool_tex_editslot_apply', 400)
            t.expect('slot 8 diffuse', ev('(getvtexname 8 0)'), 'textures' + SEP + 'default')
            send('toolpanel_close tool_texeditslot; editslot 8 [ texture 0 "%s" ] 0 0' % orig.replace('\\', '/'), 300)
        t.step('apply is refused when the slot changed under an open editor', changed_under)

        def out_of_range():
            geom = ev('(getvtexname 1 0)')
            send('editslot 99999 [ texture 0 "appleflap/danger.jpg" ] 0 0', 300)
            t.expect('default geometry slot diffuse', ev('(getvtexname 1 0)'), geom)
            send('editslot 99999 [ texture 0 "appleflap/danger.jpg" ] 0 1', 300)
            t.expect('dummy decal slot diffuse', ev('(getvtexname 99999 0 1)'), '')
        t.step('editslot refuses an out of range index instead of editing the fallback slot', out_of_range)

        def clone():
            first = ev('(cloneslot 5)')
            item = ev('(getslottex %s)' % first)
            slots = int(ev('$numslots'))
            second = ev('(cloneslot (getvindex %s))' % item)
            t.expect('numslots', ev('$numslots'), slots + 1)
            t.expect('the second clone diffuse', ev('(getvtexname (getslottex %s) 0)' % second), ev('(getvtexname 5 0)'))
        t.step('a clone can be cloned by slot index (the conversion the texture list menu now does; the menu wiring itself is not driven)', clone)

        def hint_sets():
            decal = {'': 'stddecal', 'g': 'glowdecal', 'gG': 'pulseglowdecal', 'v': 'dispdecal', 'erg': 'envglowdecal',
                     's': 'specdecal', 'er': 'envdecal', 'n': 'bumpdecal', 'nesrg': 'bumpenvspecglowdecal'}
            for h, want in decal.items():
                t.expect("decal hints '%s'" % h, ev('(tool_tex_editslot_decal = 1; at $decalshaders (tool_tex_editslot_findshader "%s") 0)' % h), want)
            world = {'': 'stdworld', 'ngers': 'bumpenvspecglowworld', 'ngerG': 'bumpenvpulseglowworld', 'nsS': 'bumpspecmapworld',
                     'Td': 'triplanardetailworld'}
            for h, want in world.items():
                t.expect("world hints '%s'" % h, ev('(tool_tex_editslot_decal = 0; at $worldshaders (tool_tex_editslot_findshader "%s") 0)' % h), want)
        t.step('the slot editor finds a shader for every hint set it can produce', hint_sets)
    finally:
        if not a.keep_running:
            core.stop()
    return t.finish()


if __name__ == '__main__':
    sys.exit(main())
