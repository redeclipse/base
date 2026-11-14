"""Shared bits of the live tests/ scripts (task*.py, shadersource.py): the
import path and the PASS/FAIL tally the PowerShell versions printed."""

import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
import core  # noqa: E402,F401


class Tally:
    def __init__(self, name):
        self.name, self.failures = name, 0

    def check(self, what, condition):
        print(core.colour('  PASS  ' + what, 'green') if condition else core.colour('  FAIL  ' + what, 'red'), flush=True)
        if not condition:
            self.failures += 1

    def run(self, what, body):
        """body() is the condition; an exception fails it with the message."""
        try:
            self.check(what, bool(body()))
        except Exception as e:  # noqa: BLE001
            print(core.colour('  FAIL  %s -- %s' % (what, e), 'red'), flush=True)
            self.failures += 1

    def finish(self):
        if self.failures:
            print(core.colour('%d check(s) failed' % self.failures, 'red'))
            return 1
        print(core.colour('%s: all checks passed' % self.name, 'green'))
        return 0
