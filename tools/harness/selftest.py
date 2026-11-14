"""Reporting and driving helpers shared by the *_selftest.py scripts.

The PowerShell self-tests each carry a copy of Step/Expect/ExpectTrue and of
the Send/Eval/Invoke-MapLoad wrappers; here they live once.
"""

import math
import os
import re
import sys
import traceback

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import core  # noqa: E402
import editor  # noqa: E402


def ps_str(value):
    """value as PowerShell's "$value" would print it, which is what the
    original assertions compared: 512.0 -> '512', None -> '', lists joined."""
    if value is None:
        return ''
    if isinstance(value, bool):
        return 'True' if value else 'False'
    if isinstance(value, float):
        if math.isfinite(value) and value == int(value):
            return str(int(value))
        return repr(value)
    if isinstance(value, (list, tuple)):
        return ' '.join(ps_str(v) for v in value)
    return str(value)


class Suite:
    """Counts failures across steps; a step that raises is one failure."""

    def __init__(self, name, state_on_failure=False):
        self.name = name
        self.failures = 0
        self.steps = 0
        self.state_on_failure = state_on_failure

    def out(self, text, colour=None):
        print(core.colour(text, colour), flush=True)

    def step(self, name, body):
        self.steps += 1
        before = self.failures
        self.out('')
        self.out('[%d] %s' % (self.steps, name), 'cyan')
        try:
            body()
        except core.HarnessError as e:
            self.fail(str(e))
        except Exception as e:  # a bug in the step itself is a failure too, with its trace
            self.fail('%s: %s' % (type(e).__name__, e))
            for line in traceback.format_exc().rstrip().splitlines():
                self.out('           ' + line, 'grey')
        # A red step is not debuggable without the state that produced it.
        if self.state_on_failure and self.failures > before:
            self.out('     --- editor state at failure ---', 'yellow')
            try:
                editor.show_state(editor.get_state(), sys.stdout)
            except Exception as e:
                self.out('     (state unavailable: %s)' % e, 'yellow')

    def ok(self, text):
        self.out('     ok    ' + text, 'green')

    def fail(self, text):
        self.out('     FAIL  ' + text, 'red')
        self.failures += 1

    def expect(self, what, actual, expected):
        """Case-sensitive comparison of the two as strings (PowerShell -ceq)."""
        a, e = ps_str(actual), ps_str(expected)
        if a == e:
            self.ok("%s = '%s'" % (what, a))
        else:
            self.fail("%s -- expected '%s', got '%s'" % (what, e, a))

    def expect_near(self, what, actual, expected, tolerance=2.0):
        if actual is not None and abs(float(actual) - float(expected)) <= tolerance:
            self.ok('%s = %s (~%s)' % (what, ps_str(actual), ps_str(expected)))
        else:
            self.fail('%s -- expected ~%s (+/-%s), got %s' % (what, ps_str(expected), ps_str(tolerance), ps_str(actual)))

    def expect_true(self, what, condition, detail=''):
        if condition:
            self.ok(what)
        else:
            self.fail(what + (' -- ' + detail if detail else ''))

    def finish(self):
        self.out('')
        if self.failures:
            self.out('%d check(s) failed' % self.failures, 'red')
            return 1
        self.out('%s: all checks passed' % self.name, 'green')
        return 0


# ------------------------------------------------------------- driving ----

ENGINE_ERROR = re.compile(r'Unknown (command|alias)|assert|exception')


def send(script, settle_ms=250, suite=None, timeout=30):
    """Runs a batch and returns its lines, stamps stripped. With a suite, an
    engine error in the output counts as a failure (a script error never fails
    a command by itself: it only reaches the log)."""
    lines = [core.strip_stamp(line) for line in core.invoke_batch(script, settle_ms, timeout)]
    if suite is not None:
        for line in lines:
            if ENGINE_ERROR.search(line):
                suite.fail('engine reported an error: ' + line)
    return lines


def eval_cs(expr, prefix='SELFTEST_EVAL', timeout=30):
    """A CubeScript expression's value as text, or None if it did not echo."""
    for line in send('echo (concatword "%s=" %s)' % (prefix, expr), 1, timeout=timeout):
        m = re.search(prefix + r'=(.*)$', line)
        if m:
            return m.group(1).strip()
    return None


class map_load:
    """Loading a map while an entity is hovered trips an unguarded enthover
    read (world.cpp, see the README's known issue). Turning entity editing off
    first empties enthover; it is turned back on even if the load fails."""

    def __enter__(self):
        send('entediting 0', 300)

    def __exit__(self, *exc):
        send('entediting 1', 300)
        return False


def dark_pixels(path, x0, x1, y0, y1, green_below):
    """Pixels inside a rectangle given as fractions of the image whose green
    channel is below green_below, scaled to a 1280x720 image so thresholds
    measured there hold for a HiDPI (2x) screenshot too."""
    w, h, ch, rows = core.read_png(path)
    count = 0
    for y in range(int(h * y0), int(h * y1)):
        row = rows[y]
        for x in range(int(w * x0), int(w * x1)):
            if row[x * ch + 1] < green_below:
                count += 1
    return count * (1280 * 720) / float(w * h)
