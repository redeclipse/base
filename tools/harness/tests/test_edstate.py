"""Unit tests for the EDSTATE parser (edstate.py). No game required.

The Python counterpart of edstate.tests.ps1:
  python3 -m unittest discover -s tools/harness/tests -p "test_*.py" -v
"""

import locale
import os
import sys
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
from edstate import parse_edstate  # noqa: E402

FULL = [
    'EDSTATE mode 1 3 8 2',
    'EDSTATE cam 512.00000 400.50000 600.00000 90.00000 -15.25000',
    'EDSTATE worldpos 512.00000 512.00000 512.00000',
    'EDSTATE cur 512 512 512 2',
    'EDSTATE sel 512 512 512 2 3 4 8 2 0 0 4 6 1 24 1',
    'EDSTATE ui 0 1234',
    'EDENT hover 17 playerstart 100.00000 200.00000 300.00000',
    'EDENT sel 17 playerstart 100.00000 200.00000 300.00000',
    'EDENT sel 18 mapmodel 110.00000 210.00000 310.00000',
    'EDSTATE end 3',
]


class FullDump(unittest.TestCase):
    def test_full(self):
        r = parse_edstate(FULL)
        self.assertTrue(r.complete)
        self.assertEqual(r.mode.edit_mode, 1)
        self.assertEqual(r.mode.grid_power, 3)
        self.assertEqual(r.mode.grid_size, 8)
        self.assertEqual(r.cam.x, 512)
        self.assertEqual(r.cam.pitch, -15.25)
        self.assertTrue(r.cur.x == 512 and r.cur.orient == 2)
        self.assertEqual((r.sel.sx, r.sel.sy, r.sel.sz), (2, 3, 4))
        self.assertEqual(r.sel.children, 24)
        self.assertEqual(r.sel.have_sel, 1)
        self.assertEqual(r.ui.free_cursor, 1234)
        self.assertEqual(len(r.hover), 1)
        self.assertEqual(r.hover[0].type, 'playerstart')
        self.assertEqual(len(r.ent_sel), 2)
        self.assertEqual(r.ent_sel[1].idx, 18)
        self.assertEqual(r.ent_count, 3)
        self.assertEqual(r.malformed, [])

    def test_timestamped(self):
        # The harness strips log timestamps, but the parser must not depend on that.
        s = parse_edstate(['2026-09-03 12:34.56 ' + l for l in FULL])
        self.assertTrue(s.complete and s.mode.edit_mode == 1)
        self.assertEqual(len(s.ent_sel), 2)


class Shapes(unittest.TestCase):
    def test_empty_selection(self):
        e = parse_edstate([
            'EDSTATE mode 1 3 8 0',
            'EDSTATE cam 0.00000 0.00000 0.00000 0.00000 0.00000',
            'EDSTATE worldpos 0.00000 0.00000 0.00000',
            'EDSTATE cur 0 0 0 0',
            'EDSTATE sel 0 0 0 0 0 0 8 0 0 0 0 0 0 0 0',
            'EDSTATE ui 0 0',
            'EDSTATE end 0',
        ])
        self.assertEqual(e.sel.have_sel, 0)
        self.assertEqual(e.hover, [])
        self.assertEqual(e.ent_sel, [])
        self.assertTrue(e.complete)

    def test_truncated_dump_is_incomplete(self):
        self.assertFalse(parse_edstate(['EDSTATE mode 1 3 8 0']).complete)

    def test_malformed_lines_are_captured(self):
        bad = parse_edstate(['EDSTATE mode 1 3 8 0', 'EDSTATE cur nonsense', 'EDENT hover 17', 'EDSTATE end 0'])
        self.assertEqual(len(bad.malformed), 2)
        self.assertIsNone(bad.cur)

    def test_unrelated_output_is_ignored(self):
        self.assertEqual(parse_edstate(['some other log line', 'EDSTATE mode 1 3 8 0', 'EDSTATE end 0']).malformed, [])

    def test_non_numeric_float_is_malformed_not_a_crash(self):
        # The float fields match \\S+, so validity is decided at conversion; a
        # torn read (mid-clearlog swap) must not take the caller down with it.
        r = parse_edstate(['EDSTATE mode 1 3 8 2', 'EDSTATE cam abc 483.99988 1024.00000 0.00000 0.00000', 'EDSTATE end 0'])
        self.assertEqual(len(r.malformed), 1)
        self.assertIsNone(r.cam)

    def test_overflowing_integer_is_malformed_not_a_crash(self):
        r = parse_edstate(['EDSTATE mode 1 3 8 2', 'EDSTATE cur 99999999999999 0 0 0', 'EDSTATE end 0'])
        self.assertEqual(len(r.malformed), 1)
        self.assertIsNone(r.cur)


class Locale(unittest.TestCase):
    """Coordinates must parse the same way whatever the locale. The pair only
    proves something where the hazard is real: the first assertion shows the
    locale-aware parse genuinely misreads a dot-decimal string under a comma
    locale, the second that parse_edstate is immune to it."""

    def test_comma_decimal_locale(self):
        old = locale.setlocale(locale.LC_NUMERIC)
        for name in ('de_DE.UTF-8', 'de_DE.utf8', 'de_DE'):
            try:
                locale.setlocale(locale.LC_NUMERIC, name)
                break
            except locale.Error:
                continue
        else:
            self.skipTest('no de_DE locale installed')
        try:
            try:
                misread = locale.atof('400.50000') != 400.5
            except ValueError:
                misread = True
            self.assertTrue(misread, 'de_DE sanity: a locale-aware parse should misread a dot decimal')
            self.assertEqual(parse_edstate(FULL).cam.y, 400.5)
        finally:
            locale.setlocale(locale.LC_NUMERIC, old)


class RealCaptures(unittest.TestCase):
    """Verbatim captures from the live engine, not an idealised format. The
    first has selchildcount negative ("... 0 -64 0"): accept it, don't fix it."""

    def test_capture_1(self):
        g = parse_edstate([
            'EDSTATE mode 1 3 8 2',
            'EDSTATE cam 497.99994 483.99988 1024.00000 0.00000 0.00000',
            'EDSTATE worldpos 498.00000 2532.00000 1024.00000',
            'EDSTATE cur 496 480 1024 2',
            'EDSTATE sel 496 480 1024 1 1 1 8 2 0 0 2 2 0 -64 0',
            'EDSTATE ui 0 1',
            'EDSTATE end 0',
        ])
        self.assertTrue(g.complete)
        self.assertEqual(g.sel.children, -64)
        self.assertEqual(g.sel.have_sel, 0)
        self.assertEqual(g.malformed, [])

    def test_capture_2(self):
        g = parse_edstate([
            'EDSTATE mode 1 3 8 5',
            'EDSTATE cam 504.99997 497.99994 1024.00000 260.62119 -56.78864',
            'EDSTATE worldpos 835.73645 443.36655 511.91632',
            'EDSTATE cur 832 440 504 5',
            'EDSTATE sel 832 440 504 0 0 0 8 5 0 0 2 2 0 0 0',
            'EDSTATE ui 0 1',
            'EDENT hover 0 playerstart 836.00000 444.00000 516.00000',
            'EDENT sel 0 playerstart 836.00000 444.00000 516.00000',
            'EDSTATE end 2',
        ])
        self.assertTrue(g.complete)
        self.assertEqual(g.ent_count, 2)
        self.assertEqual(len(g.hover), 1)
        self.assertEqual(len(g.ent_sel), 1)
        self.assertEqual(g.hover[0].idx, 0)
        self.assertEqual(g.malformed, [])


if __name__ == '__main__':
    unittest.main()
