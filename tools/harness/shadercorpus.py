"""Pure helpers for the shader equivalence harness (shaders.py).

The Python counterpart of shadercorpus.ps1: sweep file and corpus parsing, the
tier-0 contract comparison, self-checks and report formatting. Nothing here
talks to the game, so tests/test_shadercorpus.py runs without one. Every
comparison is case-sensitive and ordinal (shader names can differ only by
case), which Python strings and dicts already are.
"""

import fnmatch
import os
import re
from dataclasses import dataclass, field


class CorpusError(Exception):
    pass


@dataclass
class Point:
    id: str
    settings: dict = field(default_factory=dict)  # var -> value text, in order


@dataclass
class Row:
    name: str
    sid: str
    hash: str
    origin: str

    @property
    def key(self):
        return '%s\t%s' % (self.sid, self.name)


@dataclass
class Result:
    status: str
    name: str
    sid: str
    base_hash: str = ''
    cand_hash: str = ''
    detail: str = ''


@dataclass
class Leak:
    name: str
    first: str
    last: str
    origin: str


def _lines(path):
    with open(path, 'r', encoding='utf-8', newline='') as f:
        return f.read().splitlines()


def read_sweep(path):
    points, seen = [], set()
    for n, raw in enumerate(_lines(path), 1):
        line = re.sub(r'#.*$', '', raw).strip()
        if not line:
            continue
        parts = line.split()
        pid = parts[0]
        if not re.match(r'^[A-Za-z0-9_-]+$', pid):
            raise CorpusError("%s:%d: bad settings id '%s'" % (path, n, pid))
        if pid.startswith('m-'):
            raise CorpusError("%s:%d: ids starting 'm-' are reserved for the per-map pass" % (path, n))
        if pid in seen:
            raise CorpusError("%s:%d: duplicate settings id '%s'" % (path, n, pid))
        seen.add(pid)
        settings = {}
        for p in parts[1:]:
            m = re.match(r'^([A-Za-z_][A-Za-z0-9_]*)=(-?[0-9]+(\.[0-9]+)?)$', p)
            if not m:
                raise CorpusError("%s:%d: expected var=number, got '%s'" % (path, n, p))
            settings[m.group(1)] = m.group(2)
        points.append(Point(pid, settings))
    return points


def sweep_vars(points):
    out = []
    for p in points:
        for k in p.settings:
            if k not in out:
                out.append(k)
    return out


def format_settings(settings):
    return ' '.join('%s=%s' % (k, v) for k, v in settings.items())


def read_run_info(run_dir):
    path = os.path.join(run_dir, 'run.txt')
    if not os.path.exists(path):
        raise CorpusError('No run.txt in %s -- not a recorded corpus?' % run_dir)
    info = {}
    for line in _lines(path):
        i = line.find(' ')
        if i > 0:
            info[line[:i]] = line[i + 1:]
        elif line:
            info[line] = ''
    return info


def read_run_points(run_dir):
    info = read_run_info(run_dir)
    points = []
    for pid in [s for s in info.get('sweep', '').split(' ') if s]:
        settings = {}
        for line in _lines(os.path.join(run_dir, 'settings', pid + '.txt')):
            m = re.match(r'^([A-Za-z_][A-Za-z0-9_]*)=(\S+)$', line)
            if m:
                settings[m.group(1)] = m.group(2)
        points.append(Point(pid, settings))
    return points


def read_manifest(directory):
    path = os.path.join(directory, 'manifest.tsv')
    if not os.path.exists(path):
        raise CorpusError('No manifest: %s' % path)
    rows = []
    for line in _lines(path):
        if not line:
            continue
        f = line.split('\t')
        if len(f) != 4:
            raise CorpusError('Malformed manifest row in %s: %s' % (path, line))
        rows.append(Row(f[0], f[1], f[2], f[3]))
    return rows


def valid_row_map(rows):
    """Invalid rows ('-') are treated as absent: resetshaders leaves stubs of
    shaders generated at earlier sweep points, so they are not comparable."""
    return {r.key: r for r in rows if r.hash != '-'}


def blob_lines(path):
    if not os.path.exists(path):
        return []
    return _lines(path)


def contract_diff(base_blob, cand_blob, skip_reflection=False):
    """skip_reflection drops reflect.txt only: it comes from the GL driver,
    while meta.txt (params, flags, variant rows, reuse links, uniformlocs)
    does not."""
    out = []
    files = ['meta.txt'] + ([] if skip_reflection else ['reflect.txt'])
    for name in files:
        section = os.path.splitext(name)[0]
        a = blob_lines(os.path.join(base_blob, name))
        b = blob_lines(os.path.join(cand_blob, name))
        if a == b:
            continue
        set_a, set_b = set(a), set(b)
        before = len(out)
        out += ['%s: -%s' % (section, l) for l in a if l not in set_b]
        out += ['%s: +%s' % (section, l) for l in b if l not in set_a]
        if len(out) == before:
            out.append('%s: order changed' % section)
    return out


def _like(name, pattern):
    """PowerShell -like: wildcards * ? [..], case-insensitive."""
    return fnmatch.fnmatchcase(name.lower(), pattern.lower())


def compare_corpus(base_dir, cand_dir, filter='*', sids=None, skip_reflection=False):
    base = valid_row_map(read_manifest(base_dir))
    cand = valid_row_map(read_manifest(cand_dir))
    results = []
    for k in sorted(set(base) | set(cand)):
        b, c = base.get(k), cand.get(k)
        row = b or c
        if sids and row.sid not in sids:
            continue
        # A variant belongs to its parent's family: '--filter bump*' must
        # include '<variant:0,1>bumpworld', not only 'bumpworld'.
        parent = re.sub(r'^<variant:[^>]*>', '', row.name)
        if not _like(row.name, filter) and not _like(parent, filter):
            continue
        if not c:
            results.append(Result('MISSING', row.name, row.sid, b.hash, '', 'no valid shader in the candidate'))
            continue
        if not b:
            results.append(Result('EXTRA', row.name, row.sid, '', c.hash, 'not in the baseline'))
            continue
        if b.hash == c.hash:
            results.append(Result('PASS-TEXT', row.name, row.sid, b.hash, c.hash, 'identical'))
            continue
        diff = contract_diff(os.path.join(base_dir, 'blobs', b.hash), os.path.join(cand_dir, 'blobs', c.hash), skip_reflection)
        if diff:
            shown = '; '.join(diff[:3])
            if len(diff) > 3:
                shown += '; (+%d more)' % (len(diff) - 3)
            results.append(Result('FAIL', row.name, row.sid, b.hash, c.hash, 'contract: ' + shown))
            continue
        results.append(Result('PENDING', row.name, row.sid, b.hash, c.hash, ''))
    return results


def _registry_bytes(directory):
    path = os.path.join(directory, 'registry.txt')
    if not os.path.exists(path):
        return None
    with open(path, 'rb') as f:
        return f.read()


def compare_registry(base_dir, cand_dir):
    """The editor palette is part of the contract: tooltex.cfg picks world and
    decal shaders out of $worldshaders/$decalshaders by position, so an
    entry's text and its place both count. Byte-exact, line by line, in
    order. Returns one FAIL result naming the first differing line, or None."""
    a, b = _registry_bytes(base_dir), _registry_bytes(cand_dir)
    detail = None
    if a is None or b is None:
        gone = [d for d, data in ((base_dir, a), (cand_dir, b)) if data is None]
        detail = 'no registry.txt in %s' % ' or '.join(gone)
    elif a != b:
        # The bytes decided; the lines only locate it. writetofile writes in
        # text mode, so CRLF is the normal line end on Windows: shown without it.
        def split(data):
            return data.decode('utf-8', 'replace').replace('\r\n', '\n').rstrip('\n').split('\n')
        la, lb = split(a), split(b)
        n = max(len(la), len(lb))
        i, x, y = 0, None, None
        while i < n:
            x = la[i] if i < len(la) else None
            y = lb[i] if i < len(lb) else None
            if x != y:
                break
            i += 1
        if i >= n:
            detail = 'same lines, different bytes'
        else:
            parts = []
            if x is not None:
                parts.append('-' + x)
            if y is not None:
                parts.append('+' + y)
            detail = 'line %d: %s' % (i + 1, '; '.join(parts))
        if len(la) != len(lb):
            detail += ' (%d vs %d lines)' % (len(la), len(lb))
    if detail is not None:
        return Result('FAIL', 'registry', '-', '', '', detail)
    return None


CROSS_GPU_NOTE = '(cross-gpu: reflection and pixels skipped)'


def same_gpu(base_gl, cand_gl, commit, allow_cross_gpu=False):
    """Reflection and pixels come from the GL driver, so check only compares
    them against a baseline recorded on the same GPU/driver (gl.txt). True
    when the two match, False when they differ and allow_cross_gpu accepts
    the downgrade; otherwise refuses, naming both and the commit to
    re-record from."""
    if base_gl == cand_gl:
        return True
    if allow_cross_gpu:
        return False
    raise CorpusError('The baseline was recorded on a different GPU/driver, so its reflection and pixel tiers do not apply here.\n'
                      'Baseline gl.txt:\n%s\nThis machine:\n%s\n'
                      "Re-record the baseline on this machine from commit %s (its run.txt): check that commit out, build it, and run "
                      "'tools/harness/shaders.py record'. Or pass --allow-cross-gpu to compare only the metadata contract, text and SPIR-V."
                      % (base_gl.rstrip(), cand_gl.rstrip(), commit))


def add_cross_gpu_note(results):
    for r in results:
        r.detail = '%s %s' % (r.detail, CROSS_GPU_NOTE) if r.detail else CROSS_GPU_NOTE


def find_sweep_leaks(rows, first, last):
    a, b = {}, {}
    for r in rows:
        if r.hash == '-':
            continue
        if r.sid == first:
            a[r.name] = r
        elif r.sid == last:
            b[r.name] = r
    leaks = []
    for name in sorted(set(a) | set(b)):
        x, y = a.get(name), b.get(name)
        hx = x.hash if x else '(none)'
        hy = y.hash if y else '(none)'
        if hx != hy:
            leaks.append(Leak(name, hx, hy, (x or y).origin))
    return leaks


def is_sweep_leak(diff):
    """A shader valid at the first point must be valid at the last with the
    same hash; one valid only at the last is a variant row an earlier sweep
    point registered for good (see record in shaders.py), not a leak."""
    return diff.first != '(none)'


def palette_coverage(rows, registry_lines, sids):
    valid = {'%s\t%s' % (r.sid, r.name) for r in rows if r.hash != '-'}
    gaps = []
    for line in registry_lines:
        f = line.strip().split()
        if len(f) < 2:
            continue
        for sid in sids:
            if '%s\t%s' % (sid, f[1]) not in valid:
                gaps.append('%s %s' % (f[1], sid))
    return gaps


@dataclass
class Bench:
    name: str
    status: str
    detail: str


def parse_bench_line(line):
    """One "SHADERBENCH <name> <PASS|FAIL|WEAK> maxerr=... cov=... seeds=...[
    reason=...]" line (a log timestamp in front is fine), or None."""
    m = re.search(r'SHADERBENCH (\S+) (PASS|FAIL|WEAK) (.*)$', line)
    if not m:
        return None
    name, status, detail = m.group(1), m.group(2), m.group(3)
    if status == 'PASS':
        status = 'PASS-PIXEL'
    # 'reason=unsupported uniform': the bench left a uniform of a type it
    # can't seed unset, so the two programs may have read different values
    # from it and a FAIL is inconclusive. An unsupported sampler or attribute
    # binds nothing on either side, so a pixel difference there is real.
    if status == 'FAIL' and 'reason=unsupported uniform' in detail:
        status = 'WEAK'
    return Bench(name, status, detail)


def format_result(r):
    return ('%-11s %-44s %-8s %s' % (r.status, r.name, r.sid, r.detail)).rstrip()


STATUSES = ('PASS-TEXT', 'PASS-SPIRV', 'PASS-PIXEL', 'WEAK', 'FAIL', 'MISSING', 'EXTRA')


def format_summary(results):
    n = {s: sum(1 for r in results if r.status == s) for s in STATUSES}
    return '== %d configs: %d text, %d spirv, %d pixel, %d weak, %d fail, %d missing, %d extra' % (
        len(results), n['PASS-TEXT'], n['PASS-SPIRV'], n['PASS-PIXEL'], n['WEAK'], n['FAIL'], n['MISSING'], n['EXTRA'])


def check_exit_code(results):
    return 1 if any(r.status in ('FAIL', 'MISSING') for r in results) else 0
