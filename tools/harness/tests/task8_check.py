#!/usr/bin/env python3
"""Task 8 (task8-check.ps1): an unchanged build checks clean against itself."""

import contextlib
import io
import sys

from _live import Tally
import shaders


def main():
    t = Tally('task 8')
    t.check('record exited cleanly', shaders.main(['record', '--run', 't8', '--sids', 's00', '--no-maps']) == 0)
    res = shaders.Recorder().check('t8', ['s00'], True)
    t.check('check produced results', len(res) > 300)
    t.check('every configuration is hash-identical', all(r.status == 'PASS-TEXT' for r in res))
    buf = io.StringIO()
    with contextlib.redirect_stdout(buf):
        code = shaders.main(['check', '--run', 't8', '--sids', 's00', '--no-maps', '--filter', 'hud*'])
    t.check('check exits 0 when clean', code == 0)
    t.check('a summary line is printed', len([l for l in buf.getvalue().splitlines() if l.startswith('== ') and ' configs:' in l]) == 1)
    t.check('diff shows the contract is identical', any('contract: identical' in l for l in shaders.diff('hud', 's00', 't8')))
    return t.finish()


if __name__ == '__main__':
    sys.exit(main())
