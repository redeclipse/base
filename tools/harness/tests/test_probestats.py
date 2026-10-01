"""Tests for tools/harness/probestats.py.

Run: python -m unittest discover -s tools/harness/tests -p "test_probestats.py" -v
"""
import os
import shutil
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
import probestats  # noqa: E402

# One last split (index 1) centred on the origin, half-size 270, grid 27:
# cells are 20 units, so the excluded margin is 30 units inside each face.
SPLITS = [(0, 0.0, 0.0, 0.0, 100.0), (1, 0.0, 0.0, 0.0, 270.0)]


class ProbeStatsTest(unittest.TestCase):
    def setUp(self):
        self.dir = tempfile.mkdtemp()

    def tearDown(self):
        shutil.rmtree(self.dir)

    def write(self, name, lines):
        path = os.path.join(self.dir, name)
        with open(path, "w") as f:
            f.write("\n".join(lines) + "\n")
        return path

    def points(self, pts):
        return self.write("points.txt", ["%g %g %g 0 0 1" % p for p in pts])

    def runfile(self, name, steps):
        """steps: list of per-step lists of (r, g, b, a), one per point."""
        lines = []
        for s, values in enumerate(steps):
            for split in SPLITS:
                lines.append("S\t%d\t%d\t%g\t%g\t%g\t%g" % ((s,) + split))
            for p, v in enumerate(values):
                lines.append("P\t%d\t%d\t%g\t%g\t%g\t%g" % ((s, p) + v))
        return self.write(name, lines)

    def test_jump_is_largest_component_change(self):
        pts = self.points([(0, 0, 0), (10, 0, 0)])
        run = self.runfile("ref.tsv", [[(0.1, 0, 0, 0), (0.2, 0, 0, 0)],
                                   [(0.1, 0, 0, 0), (0.2, 0.05, 0, 0)],
                                   [(0.13, 0, 0, 0), (0.2, 0.05, 0, 0)]])
        j, step, point, kept = probestats.jumps(probestats.read_points(pts), 27, probestats.read_run(run))
        self.assertAlmostEqual(j, 0.05)
        self.assertEqual((step, point, kept), (0, 1, 4))

    def test_points_near_last_split_faces_are_excluded(self):
        # 250 is within 30 of the face at 270; 300 is outside the split.
        pts = self.points([(0, 0, 0), (250, 0, 0), (0, 300, 0)])
        run = self.runfile("ref.tsv", [[(0, 0, 0, 0)] * 3,
                                   [(0, 0, 0, 0), (0.9, 0, 0, 0), (0.9, 0, 0, 0)]])
        j, step, point, kept = probestats.jumps(probestats.read_points(pts), 27, probestats.read_run(run))
        self.assertEqual((j, kept), (0.0, 1))

    def test_sweep_detects_and_passes(self):
        pts = self.points([(0, 0, 0)])
        ref = self.runfile("ref.tsv", [[(0.1, 0, 0, 0)], [(0.1, 0, 0, 0)], [(0.2, 0, 0, 0)]])
        smooth = self.runfile("smooth.tsv", [[(0.1, 0, 0, 0)], [(0.12, 0, 0, 0)], [(0.14, 0, 0, 0)]])
        steep = self.runfile("steep.tsv", [[(0.1, 0, 0, 0)], [(0.1, 0, 0, 0)], [(0.15, 0, 0, 0)]])
        flat = self.runfile("flat.tsv", [[(0.1, 0, 0, 0)], [(0.1, 0, 0, 0)], [(0.1, 0, 0, 0)]])
        self.assertEqual(probestats.main(["probestats.py", "sweep", pts, "27", ref]), 0)
        self.assertEqual(probestats.main(["probestats.py", "sweep", pts, "27", flat]), 1)
        self.assertEqual(probestats.main(["probestats.py", "sweep", pts, "27", ref, smooth]), 0)
        self.assertEqual(probestats.main(["probestats.py", "sweep", pts, "27", ref, steep]), 1)
        self.assertEqual(probestats.main(["probestats.py", "sweep", pts, "27", flat, smooth]), 1)

    def test_near_compares_only_points_inside_radius(self):
        pts = self.points([(0, 0, 0), (5, 0, 0), (50, 0, 0)])
        ref = self.runfile("ref.tsv", [[(0.1, 0, 0, 0), (0.1, 0, 0, 0), (0.1, 0, 0, 0)]])
        same = self.runfile("same.tsv", [[(0.1, 0, 0, 0), (0.1 + 1 / 255.0, 0, 0, 0), (0.5, 0, 0, 0)]])
        moved = self.runfile("moved.tsv", [[(0.1, 0, 0, 0), (0.2, 0, 0, 0), (0.1, 0, 0, 0)]])
        self.assertEqual(probestats.main(["probestats.py", "near", pts, "10", "0", "0", "0", ref, same]), 0)
        self.assertEqual(probestats.main(["probestats.py", "near", pts, "10", "0", "0", "0", ref, moved]), 1)

    def test_bad_usage(self):
        self.assertEqual(probestats.main(["probestats.py"]), 2)
        self.assertEqual(probestats.main(["probestats.py", "sweep", "p.txt"]), 2)


if __name__ == "__main__":
    unittest.main()
