#!/usr/bin/env python3
"""Task 5 (task5-smoke.ps1): harness.py's public behaviour, against a running harness."""

import os
import subprocess
import sys

from _live import Tally, core

HARNESS = os.path.join(core.HARNESS_DIR, 'harness.py')


def harness(*args):
    p = subprocess.run([sys.executable, HARNESS] + list(args), capture_output=True, text=True)
    return p.stdout + p.stderr


def main():
    t = Tally('task 5')
    t.run('core.py exists', lambda: os.path.exists(os.path.join(core.HARNESS_DIR, 'core.py')))
    t.run('harness.py imports core', lambda: 'import core' in open(HARNESS, encoding='utf-8').read())
    t.run('status runs', lambda: 'running' in harness('status'))
    t.run('send echoes', lambda: 'SMOKE_OK' in harness('send', 'echo "SMOKE_OK"'))

    def nav():
        harness('nav', 'ui_gameui_settings')
        return 'TOP=main' in harness('send', 'echo (concatword "TOP=" $uitopname)')
    t.run('nav opens a panel', nav)
    t.run('tree reports objects', lambda: 'aspect' in harness('tree', '--drawn'))
    t.run('shot writes a png', lambda: os.path.exists(harness('shot', 'task5smoke').strip().splitlines()[-1]))
    return t.finish()


if __name__ == '__main__':
    sys.exit(main())
