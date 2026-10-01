"""Statistics over the rhprobe sweeps of tools/harness/gi.ps1.

  python probestats.py sweep <points> <grid> <ref.tsv> [<cand.tsv>]
      For each run, J: the largest change of any probe value (r, g, b or a)
      between consecutive steps. Probes outside the last split, or within 1.5
      of its cells of its faces, at either step are left out: the last split
      keeps today's hard edge, which this work doesn't change. With only
      <ref.tsv>: DETECTED (exit 0) when J >= DETECT, else NOT DETECTED
      (exit 1). With <cand.tsv>: PASS (exit 0) when J_ref >= DETECT and
      J_cand <= J_ref / RATIO, else FAIL (exit 1).
  python probestats.py near <points> <radius> <x> <y> <z> <ref.tsv> <cand.tsv>
      The largest difference between the two runs' step-0 values over the
      probes within <radius> of (x, y, z). PASS (exit 0) when <= NEAR.

<points>: one "x y z nx ny nz" line per probe, indexed from 0. Run files
are tab-separated "S step split cx cy cz half" and "P step point r g b a".
"""
import sys

DETECT = 0.01
RATIO = 4.0
NEAR = 2.0 / 255.0


def read_points(path):
    with open(path) as f:
        return [tuple(float(v) for v in line.split()[:3]) for line in f if line.strip()]


def read_run(path):
    """Returns (splits, probes): splits[step][split] = (cx, cy, cz, half),
    probes[step][point] = (r, g, b, a)."""
    splits, probes = {}, {}
    with open(path) as f:
        for line in f:
            fields = line.split()
            if not fields:
                continue
            step, index = int(fields[1]), int(fields[2])
            values = tuple(float(v) for v in fields[3:7])
            if fields[0] == "S":
                splits.setdefault(step, {})[index] = values
            elif fields[0] == "P":
                probes.setdefault(step, {})[index] = values
    return splits, probes


def near_last_face(point, split, grid):
    cx, cy, cz, half = split
    margin = 1.5 * 2.0 * half / grid
    return any(abs(p - c) > half - margin for p, c in zip(point, (cx, cy, cz)))


def jumps(points, grid, run):
    """Returns (J, step, point, kept): the largest change between step and
    step + 1, where it happened, and how many probe-steps were compared."""
    splits, probes = run
    steps = sorted(probes)
    best, where, kept = 0.0, (None, None), 0
    for s, t in zip(steps, steps[1:]):
        lasts = splits[s][max(splits[s])], splits[t][max(splits[t])]
        for p, a in probes[s].items():
            b = probes[t].get(p)
            if b is None or any(near_last_face(points[p], last, grid) for last in lasts):
                continue
            kept += 1
            d = max(abs(x - y) for x, y in zip(a, b))
            if d > best:
                best, where = d, (s, p)
    return best, where[0], where[1], kept


def report(label, result):
    j, step, point, kept = result
    print("%s J %.6f at step %s point %s (%d probe-steps)" % (label, j, step, point, kept))
    return j


def main(argv):
    if len(argv) in (5, 6) and argv[1] == "sweep":
        points, grid = read_points(argv[2]), float(argv[3])
        ref = report("REF", jumps(points, grid, read_run(argv[4])))
        if len(argv) == 5:
            ok = ref >= DETECT
            print("DETECTED" if ok else "NOT DETECTED")
            return 0 if ok else 1
        cand = report("CAND", jumps(points, grid, read_run(argv[5])))
        ok = ref >= DETECT and cand <= ref / RATIO
        print("RATIO %.3f" % (cand / ref if ref > 0 else float("inf")))
        print("PASS" if ok else "FAIL")
        return 0 if ok else 1
    if len(argv) == 9 and argv[1] == "near":
        points, radius = read_points(argv[2]), float(argv[3])
        centre = tuple(float(v) for v in argv[4:7])
        ref, cand = read_run(argv[7])[1][0], read_run(argv[8])[1][0]
        diff, count = 0.0, 0
        for p, a in ref.items():
            if sum((x - c) ** 2 for x, c in zip(points[p], centre)) > radius * radius or p not in cand:
                continue
            count += 1
            diff = max(diff, max(abs(x - y) for x, y in zip(a, cand[p])))
        print("NEAR %.6f over %d probes (limit %.6f)" % (diff, count, NEAR))
        ok = count > 0 and diff <= NEAR
        print("PASS" if ok else "FAIL")
        return 0 if ok else 1
    sys.stderr.write(__doc__)
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv))
