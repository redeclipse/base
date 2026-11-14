#!/usr/bin/env python3
"""Acceptance tests for the shader equivalence harness (shaders-selftest.ps1).

Records a one-point baseline, then checks deliberate mutations of real shader
configs and expects each to land on the right tier:
  unchanged                   -> all PASS-TEXT          (determinism)
  --filter stdworld           -> includes <variant:0,0>stdworld
  a changed registry entry    -> FAIL, registry         (tier 0, under any filter)
  a changed param default     -> FAIL, contract         (tier 0)
  a comment in hud            -> PASS-TEXT              (tier 1)
  a renamed local in hud      -> PASS-SPIRV/PASS-PIXEL  (tier 2, or 3 without glslang)
  a changed constant in hud   -> FAIL, pixel            (tier 3)
Every mutation is reverted byte-for-byte in a finally block.

  tools/harness/shaders_selftest.py [--keep-running]
"""

import argparse
import os
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import core  # noqa: E402
import shadercorpus as sc  # noqa: E402
import shaders  # noqa: E402
from selftest import Suite  # noqa: E402

RUN = 'selftest-base'
HUD_LINE = 'vec4 diffuse = texture2D(tex0, texcoord0), color = diffuse * colorscale;'


class mutation:
    """Replaces the first occurrence of find in a repo file for the duration
    of the with-block, then restores the original bytes exactly. unique also
    requires it to be the only occurrence."""

    def __init__(self, rel, find, replace, unique=False):
        self.path = os.path.join(core.REPO_ROOT, rel)
        self.rel, self.find, self.replace, self.unique = rel, find, replace, unique

    def __enter__(self):
        with open(self.path, 'rb') as f:
            self.original = f.read()
        text = self.original.decode('utf-8')
        i = text.find(self.find)
        if i < 0:
            raise core.HarnessError('Mutation anchor not found in %s: %s' % (self.rel, self.find))
        if self.unique and text.find(self.find, i + 1) >= 0:
            raise core.HarnessError('Mutation anchor is not unique in %s: %s' % (self.rel, self.find))
        try:
            with open(self.path, 'wb') as f:
                f.write((text[:i] + self.replace + text[i + len(self.find):]).encode('utf-8'))
        except BaseException:
            self.__exit__()
            raise
        return self

    def __exit__(self, *exc):
        with open(self.path, 'wb') as f:
            f.write(self.original)
        return False


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('--keep-running', action='store_true')
    a = p.parse_args(argv)
    t = Suite('shader harness self-test')

    def check(filter):
        return shaders.Recorder().check(RUN, ['s00'], True, filter)

    def result(results, name):
        for r in results:
            if r.name == name and r.sid == 's00':
                return r
        return sc.Result('(none)', name, 's00')

    def expect(what, condition, got):
        t.expect_true(what, condition, 'got: %s' % got)

    core.start()
    try:
        def record():
            points = [x for x in sc.read_sweep(shaders.SWEEP_FILE) if x.id == 's00']
            problems = shaders.Recorder().record(RUN, points, [], sc.read_sweep(shaders.SWEEP_FILE))
            expect('record reports no leaks or palette gaps', problems == 0, '%d problem(s)' % problems)
            d = os.path.join(shaders.CORPUS_ROOT, RUN)
            with open(os.path.join(d, 'registry.txt'), encoding='utf-8', newline='') as f:
                gaps = sc.palette_coverage(sc.read_manifest(d), f.read().splitlines(), ['s00'])
            expect('every registered world/decal shader is in the corpus', not gaps, ', '.join(gaps))
        t.step('record a one-point baseline', record)

        def determinism():
            res = check('*')
            bad = [r for r in res if r.status != 'PASS-TEXT']
            expect('all %d configurations PASS-TEXT' % len(res), len(res) > 300 and not bad, ' | '.join(sc.format_result(r) for r in bad[:5]))
        t.step('an unchanged build checks clean (determinism)', determinism)

        def filter_variants():
            res = check('stdworld')
            expect('stdworld is checked', result(res, 'stdworld').status == 'PASS-TEXT', result(res, 'stdworld').status)
            v = result(res, '<variant:0,0>stdworld')
            expect('<variant:0,0>stdworld is checked under the stdworld filter', v.status == 'PASS-TEXT', v.status)
        t.step('a filter includes the variants of its shader', filter_variants)

        def registry():
            with mutation('config/glsl/world.cfg', 'worldshader "specworld" "s"', 'worldshader "specworld" "sS"', unique=True):
                reg = [r for r in check('hud') if r.name == 'registry']
                ok = len(reg) == 1 and reg[0].status == 'FAIL' and '-world specworld s;' in reg[0].detail and '+world specworld sS' in reg[0].detail
                expect('one registry FAIL naming specworld', ok, ' | '.join(sc.format_result(r) for r in reg))
        t.step('a changed registry entry fails, whatever the filter', registry)

        def param_default():
            with mutation('config/glsl/world.cfg', 'defuniformparam "gloss" 1 // glossiness', 'defuniformparam "gloss" 2 // glossiness'):
                r = result(check('stdworld'), 'stdworld')
                expect('stdworld FAIL naming gloss', r.status == 'FAIL' and 'gloss' in r.detail, '%s %s' % (r.status, r.detail))
        t.step('a changed param default fails the contract', param_default)

        def comment():
            with mutation('config/glsl/init.cfg', HUD_LINE, HUD_LINE + ' // harness selftest'):
                r = result(check('hud'), 'hud')
                expect('hud PASS-TEXT', r.status == 'PASS-TEXT', '%s %s' % (r.status, r.detail))
        t.step('a comment is textually equivalent', comment)

        def renamed_local():
            with mutation('config/glsl/init.cfg', HUD_LINE, 'vec4 texel = texture2D(tex0, texcoord0), color = texel * colorscale;'):
                r = result(check('hud'), 'hud')
                expect('hud PASS-SPIRV (or PASS-PIXEL without glslang)', r.status in ('PASS-SPIRV', 'PASS-PIXEL'), '%s %s' % (r.status, r.detail))
        t.step('a renamed local is equivalent past the text tier', renamed_local)

        def constant():
            with mutation('config/glsl/init.cfg', HUD_LINE, 'vec4 diffuse = texture2D(tex0, texcoord0), color = diffuse * colorscale * 0.5;'):
                r = result(check('hud'), 'hud')
                expect('hud FAIL with a pixel error', r.status == 'FAIL' and 'maxerr=' in r.detail, '%s %s' % (r.status, r.detail))
                code = shaders.main(['check', '--run', RUN, '--sids', 's00', '--no-maps', '--filter', 'hud'])
                expect('check exits non-zero', code != 0, 'exit %d' % code)
        t.step('a changed constant fails at the pixel tier', constant)

        # Assumes these two files were clean when the test started; if git
        # already showed them modified beforehand, this fails for that alone.
        def restored():
            out = subprocess.run(['git', '-C', core.REPO_ROOT, 'status', '--porcelain', '--', 'config/glsl/init.cfg', 'config/glsl/world.cfg'],
                                 capture_output=True, text=True).stdout.strip()
            expect('git sees no change', not out, out)
        t.step('the mutated files are restored', restored)
    finally:
        if not a.keep_running:
            core.stop()
    return t.finish()


if __name__ == '__main__':
    sys.exit(main())
