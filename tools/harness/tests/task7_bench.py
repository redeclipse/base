#!/usr/bin/env python3
"""Task 7 (task7-bench.ps1): shaderbench against the running game."""

import os
import re
import shutil
import sys

from _live import Tally, core
import shadercorpus as sc


def bench(script):
    hit = [l for l in core.invoke_batch(script, 1, 120) if 'SHADERBENCH ' in l]
    return re.sub(r'^.*SHADERBENCH ', '', hit[0]) if hit else ''


def main():
    t = Tally('task 7')
    d = os.path.join(core.HOME_DIR, 'shadercorpus', 't7')
    shutil.rmtree(d, ignore_errors=True)
    core.invoke_batch('shaderdumpall t7 s00 0', 1, 300)
    rows = sc.read_manifest(d)
    hud = [r for r in rows if r.name == 'hud'][0]
    plain = [r for r in rows if r.name == 'hudnotexture'][0]
    same = bench('shaderbench t7 %s "hud" 2' % hud.hash)
    t.check('hud against its own blob passes exactly (%s)' % same, re.match(r'^hud PASS maxerr=0 ', same) is not None)
    m = re.search(r'cov=(\d+)', same)
    if m:
        t.check('hud covers most of the target', int(m.group(1)) >= 50)
    other = bench('shaderbench t7 %s "hud" 2' % plain.hash)
    t.check('a different shader fails (%s)' % other, other.startswith('hud FAIL '))
    nosrc = bench('shaderbench t7 0000000000000000 "hud" 1')
    t.check('a missing blob fails (%s)' % nosrc, re.match(r'^hud FAIL .*reason=oldsource', nosrc) is not None)
    nolive = bench('shaderbench t7 %s "nosuchshader" 1' % hud.hash)
    t.check('a missing live shader fails (%s)' % nolive, re.match(r'^nosuchshader FAIL .*reason=missing', nolive) is not None)
    t.check('the bench is repeatable', bench('shaderbench t7 %s "hud" 2' % hud.hash) == same)
    t.check('the game still renders afterwards (read the PNG)', os.path.exists(core.invoke_shot('task7', quiet=True)))
    return t.finish()


if __name__ == '__main__':
    sys.exit(main())
