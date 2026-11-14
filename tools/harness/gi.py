#!/usr/bin/env python3
"""GI (radiance hints) stability checks, driven through the map editor.

The Linux/macOS counterpart of gi.ps1. Needs a running harness client in edit
mode on a map with sunlight and GI:
    tools/harness/harness.py start --width 1600 --height 900
    tools/harness/editor.py open park

zoom    Animates edzoom (a DEBUG_UTILS stand-in for a weapon zoom) from
        $editfov down to --fov and back over --frames frames each way, and
        counts RH split resizes ($rhsplitresets). PASS when there are none.
sweep   Probes a lattice of world points (rhprobe: the real getrhlight) while
        the camera moves --step units per step along its facing (--kind
        translate; choose --yaw 0/90/180/270) or turns --step degrees (--kind
        rotate). Always runs once at rhblend 0; with --blend > 0, again at that
        rhblend. probestats.py then reports J, the largest per-step change at
        any fixed point. Reference only: DETECTED when today's lookup pops.
        With --blend: PASS when the crossfade cuts J to a quarter or less.
near    Probes a fine lattice around the camera at rhblend 0 and at --blend
        (required, > 0). PASS when the points within --radius agree to 2/255.

Lattice: points every --spacing units over +-extent around (x, y) at heights
z - spacing, z and z + spacing, normal up. Defaults: sweep 600/40, near 40/8.
Files go to home/uitest/harness/ (rhprobe-points.txt, <name>_<kind>_b<blend>.tsv).

The view is made deterministic first: HUD, editor cursor, outlines and entity
markers off, giscale raised to --giscale so GI changes stand out.

  tools/harness/gi.py zoom --fov 30 --frames 20
  tools/harness/gi.py sweep --kind translate --x 700 --y 1000 --z 1120 --yaw 90 --name t
"""

import argparse
import math
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import core  # noqa: E402
from core import HarnessError, format_coord, invoke_batch, say, show_batch_result  # noqa: E402

PROBESTATS = os.path.join(core.HARNESS_DIR, 'probestats.py')
POINTS_REL = 'harness/rhprobe-points.txt'
OUT_REL = 'harness/rhprobe-out.txt'


class Gi:
    def __init__(self, args):
        self.a = args

    def value(self, expr):
        """A CubeScript expression's value as a string."""
        lines = invoke_batch('echo (concatword "GIVALUE=" %s)' % expr, 1, self.a.timeout)
        value = None
        for line in lines:
            m = re.search(r'GIVALUE=(\S*)', line)
            if m:
                value = m.group(1)
        if value is None:
            raise HarnessError("No value for '%s'. Output:\n%s" % (expr, '\n'.join(lines)))
        return value

    def set_view(self):
        show_batch_result(invoke_batch('showhud 0; editinhibit 1; outline 0; entediting 0; giscale %s' % format_coord(self.a.giscale), 200, self.a.timeout))
        if self.value('$editing') != '1':
            raise HarnessError('Not in edit mode: run tools/harness/editor.py open <map> first.')

    def assert_pose(self):
        for name in ('x', 'y', 'z'):
            if getattr(self.a, name) is None:
                raise HarnessError('--%s is required (read a start pose with tools/harness/editor.py state).' % name)

    def set_blend(self, value):
        if self.value('(identexists rhblend)') != '1':
            if value > 0:
                raise HarnessError('rhblend does not exist in this build.')
            return
        show_batch_result(invoke_batch('rhblend %s' % format_coord(value), 300, self.a.timeout))

    def write_lattice(self, ext, gap):
        a, lines = self.a, []
        n = int(math.floor(ext / gap))
        for k in (-1, 0, 1):
            for i in range(-n, n + 1):
                for j in range(-n, n + 1):
                    lines.append('%s %s %s 0 0 1' % (format_coord(a.x + i * gap), format_coord(a.y + j * gap), format_coord(a.z + k * gap)))
        path = os.path.join(core.HOME_DIR, POINTS_REL)
        core.write_text(path, '\n'.join(lines) + '\n')
        return path, len(lines)

    def probe_step(self, index, px, py, pz, pyaw, tsv, count):
        """Places the camera, lets a frame render, probes, and appends the
        step's S and P lines to tsv."""
        invoke_batch('edgoto %s %s %s; edaim %s 0' % (format_coord(px), format_coord(py), format_coord(pz), format_coord(pyaw)), 100, self.a.timeout)
        got = self.value('(rhprobe "%s" "%s")' % (POINTS_REL, OUT_REL))
        if int(got) != count:
            raise HarnessError('rhprobe returned %s, expected %d (GI off, or not rendered yet?)' % (got, count))
        rows = []
        with open(os.path.join(core.HOME_DIR, OUT_REL), encoding='utf-8') as f:
            for line in f.read().splitlines():
                fields = line.split(' ')
                if fields[0] == 'RHSPLIT':
                    rows.append('S\t%d\t' % index + '\t'.join(fields[1:6]))
                elif fields[0] == 'RHPROBE':
                    rows.append('P\t%d\t' % index + '\t'.join(fields[1:6]))
        with open(tsv, 'a', encoding='ascii', newline='\n') as f:
            f.write(''.join(r + '\n' for r in rows))

    def probe_run(self, blend, tag, count, pose_at, step_count):
        self.set_blend(blend)
        tsv = os.path.join(core.CMD_DIR, '%s_%s_b%s.tsv' % (self.a.name, tag, format_coord(blend)))
        if os.path.exists(tsv):
            os.remove(tsv)
        for i in range(step_count + 1):
            self.probe_step(i, *pose_at(i), tsv, count)
        return tsv

    # ------------------------------------------------------------ commands ----

    def zoom(self):
        a = self.a
        if a.frames < 1:
            raise HarnessError('--frames must be at least 1 (with no frames there is no zoom to check).')
        self.set_view()
        base = core.to_float(self.value('$editfov'))
        before = int(self.value('$rhsplitresets'))
        fovs = [base + (a.fov - base) * i / a.frames for i in range(1, a.frames + 1)]
        fovs += [base + (a.fov - base) * i / a.frames for i in range(a.frames - 1, -1, -1)]
        # One batch per step; the settle makes sure a frame renders at each fov.
        for f in fovs:
            invoke_batch('edzoom %s' % format_coord(f), 50, a.timeout)
        invoke_batch('edzoom 0', 100, a.timeout)
        after = int(self.value('$rhsplitresets'))
        delta = after - before
        print('ZOOM base=%s min=%s frames=%d rhsplitresets %d -> %d (+%d)' % (format_coord(base), format_coord(a.fov), len(fovs), before, after, delta))
        print('PASS' if delta == 0 else 'FAIL')
        return 0 if delta == 0 else 1

    def sweep(self):
        a = self.a
        self.assert_pose()
        self.set_view()
        points, count = self.write_lattice(a.extent if a.extent > 0 else 600, a.spacing if a.spacing > 0 else 40)
        grid = self.value('$rhgrid')
        # The engine's facing (vec(yaw, 0), src/shared/geom.h): x = -sin(yaw), y = cos(yaw).
        dx, dy = -math.sin(math.radians(a.yaw)), math.cos(math.radians(a.yaw))
        if a.kind == 'translate':
            def pose_at(i):
                return a.x + dx * i * a.step, a.y + dy * i * a.step, a.z, a.yaw
        else:
            def pose_at(i):
                return a.x, a.y, a.z, a.yaw + i * a.step
        runs = [self.probe_run(0, a.kind, count, pose_at, a.steps)]
        if a.blend > 0:
            runs.append(self.probe_run(a.blend, a.kind, count, pose_at, a.steps))
            self.set_blend(0)
        return subprocess.call([sys.executable, PROBESTATS, 'sweep', points, grid] + runs)

    def near(self):
        a = self.a
        if a.blend <= 0:
            raise HarnessError('--blend must be > 0 for near (rhblend 0 against itself always agrees).')
        self.assert_pose()
        self.set_view()
        points, count = self.write_lattice(a.extent if a.extent > 0 else 40, a.spacing if a.spacing > 0 else 8)

        def pose_at(i):
            return a.x, a.y, a.z, a.yaw
        ref = self.probe_run(0, 'near', count, pose_at, 0)
        cand = self.probe_run(a.blend, 'near', count, pose_at, 0)
        self.set_blend(0)
        return subprocess.call([sys.executable, PROBESTATS, 'near', points, format_coord(a.radius), format_coord(a.x), format_coord(a.y),
                                format_coord(a.z), ref, cand])


def main(argv=None):
    p = argparse.ArgumentParser(prog='gi.py', description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter, allow_abbrev=False)
    p.add_argument('command', nargs='?', default='zoom', choices=('zoom', 'sweep', 'near'))
    p.add_argument('--fov', type=float, default=30)
    p.add_argument('--frames', type=int, default=20)
    p.add_argument('--kind', choices=('translate', 'rotate'), default='translate')
    p.add_argument('--x', type=float)
    p.add_argument('--y', type=float)
    p.add_argument('--z', type=float)
    p.add_argument('--yaw', type=float, default=0)
    p.add_argument('--steps', type=int, default=72)
    p.add_argument('--step', type=float, default=1)
    p.add_argument('--blend', type=float, default=0)
    p.add_argument('--extent', type=float, default=-1)
    p.add_argument('--spacing', type=float, default=-1)
    p.add_argument('--radius', type=float, default=30)
    p.add_argument('--name', default='gi')
    p.add_argument('--giscale', type=float, default=8)
    p.add_argument('--timeout', type=int, default=30)
    a = p.parse_args(argv)
    try:
        return getattr(Gi(a), a.command)()
    except HarnessError as e:
        say(str(e), 'red')
        return 1


if __name__ == '__main__':
    sys.exit(main())
