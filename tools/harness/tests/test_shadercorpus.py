"""Unit tests for shadercorpus.py. No game required.

The Python counterpart of shadercorpus.tests.ps1 (less ConvertTo-WslPath,
which only the Windows driver needs):
  python3 -m unittest discover -s tools/harness/tests -p "test_*.py" -v
"""

import os
import shutil
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
import shadercorpus as sc  # noqa: E402


def write(path, text, newline='\n'):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, 'w', encoding='utf-8', newline=newline) as f:
        f.write(text)


class Base(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.mkdtemp(prefix='shadercorpus-tests-')

    def tearDown(self):
        shutil.rmtree(self.tmp, ignore_errors=True)

    def corpus(self, name, rows, blobs):
        d = os.path.join(self.tmp, name)
        write(os.path.join(d, 'manifest.tsv'), '\n'.join(rows) + '\n')
        for h, files in blobs.items():
            for f, text in files.items():
                write(os.path.join(d, 'blobs', h, f), text)
        return d

    def registry(self, name, lines, newline='\n'):
        d = os.path.join(self.tmp, name)
        write(os.path.join(d, 'registry.txt'), newline.join(lines) + newline if lines else '')
        return d


def blob(meta, reflect):
    return {'meta.txt': meta, 'reflect.txt': reflect}


class ReadSweep(Base):
    def test_sweep(self):
        p = os.path.join(self.tmp, 'sweep.txt')
        write(p, '# comment\ns00\n\ns01 msaa=4   # trailing\ns02 aotaps=12 hdrgamma=0.5\n')
        sweep = sc.read_sweep(p)
        self.assertEqual(len(sweep), 3)
        self.assertTrue(sweep[0].id == 's00' and not sweep[0].settings)
        self.assertTrue(sweep[1].settings == {'msaa': '4'})
        self.assertEqual(sweep[2].settings['hdrgamma'], '0.5')
        self.assertEqual(sc.sweep_vars(sweep), ['msaa', 'aotaps', 'hdrgamma'])

    def test_rejects(self):
        p = os.path.join(self.tmp, 'sweep.txt')
        for text in ('s00\ns00\n', 's01 msaa\n', 'm-atop\n'):
            write(p, text)
            with self.assertRaises(sc.CorpusError):
                sc.read_sweep(p)


class ReadRunPoints(Base):
    def test_points(self):
        run = os.path.join(self.tmp, 'run')
        write(os.path.join(run, 'run.txt'), 'commit abc\nsweep s00 s01\nmaps atop\n')
        write(os.path.join(run, 'settings', 's00.txt'), '')
        write(os.path.join(run, 'settings', 's01.txt'), 'msaa=4\n')
        pts = sc.read_run_points(run)
        self.assertEqual([p.id for p in pts], ['s00', 's01'])
        self.assertEqual(pts[1].settings['msaa'], '4')
        self.assertEqual(sc.read_run_info(run)['maps'], 'atop')


class CompareCorpus(Base):
    def setUp(self):
        super().setUp()
        m1 = 'type 1\nparam gloss 1 1 1 0 0 0 0\nparam specscale 2 2 2 0 0 0 0\n'
        m2 = 'type 1\nparam specscale 2 2 2 0 0 0 0\nparam gloss 1 1 1 0 0 0 0\n'
        r1 = 'attrib vvertex vec4 1 0\n'
        r2 = 'attrib vvertex vec4 1 1\n'
        self.base = self.corpus('base', [
            'same\ts00\taaaa\tx', 'contract\ts00\tbbbb\tx', 'order\ts00\tcccc\tx', 'pending\ts00\tdddd\tx',
            'missing\ts00\teeee\tx', 'gone\ts00\teeee\tx', 'stub\ts00\t-\tx', 'Case\ts00\taaaa\tx', 'other\ts01\taaaa\tx',
            'reflonly\ts00\tdddd\tx', 'bumpworld\ts00\taaaa\tx', '<variant:0,1>bumpworld\ts00\taaaa\tx', 'bumpworldx\ts00\taaaa\tx',
        ], {k: blob(m1, r1) for k in ('aaaa', 'bbbb', 'cccc', 'dddd', 'eeee')})
        self.cand = self.corpus('cand', [
            'same\ts00\taaaa\ty', 'contract\ts00\tffff\tx', 'order\ts00\tgggg\tx', 'pending\ts00\thhhh\tx',
            'gone\ts00\t-\tx', 'extra\ts00\taaaa\tx', 'case\ts00\taaaa\tx', 'other\ts01\taaaa\tx',
            'reflonly\ts00\tiiii\tx', 'bumpworld\ts00\taaaa\tx', '<variant:0,1>bumpworld\ts00\taaaa\tx', 'bumpworldx\ts00\taaaa\tx',
        ], {'aaaa': blob(m1, r1), 'ffff': blob('type 1\nparam specscale 2 2 2 0 0 0 0\n', r1), 'gggg': blob(m2, r1),
            'hhhh': blob(m1, r1), 'iiii': blob(m1, r2)})
        self.res = sc.compare_corpus(self.base, self.cand)

    def status(self, name, res=None, sid='s00'):
        for r in (res if res is not None else self.res):
            if r.name == name and r.sid == sid:
                return r.status
        return '(none)'

    def detail(self, name):
        return [r for r in self.res if r.name == name][0].detail

    def test_tiers(self):
        self.assertEqual(self.status('same'), 'PASS-TEXT')
        self.assertEqual(self.status('contract'), 'FAIL')
        self.assertIn('meta: -param gloss', self.detail('contract'))
        self.assertEqual(self.status('order'), 'FAIL')
        self.assertIn('order changed', self.detail('order'))
        self.assertEqual(self.status('pending'), 'PENDING')
        self.assertEqual(self.status('missing'), 'MISSING')
        self.assertEqual(self.status('gone'), 'MISSING')
        self.assertEqual(self.status('stub'), '(none)')
        self.assertEqual(self.status('extra'), 'EXTRA')
        # names are case-sensitive
        self.assertEqual((self.status('Case'), self.status('case')), ('MISSING', 'EXTRA'))
        self.assertEqual(self.status('reflonly'), 'FAIL')
        self.assertIn('reflect: -attrib', self.detail('reflonly'))

    def test_narrowing(self):
        self.assertEqual(len(sc.compare_corpus(self.base, self.cand, filter='pend*')), 1)
        self.assertEqual(len(sc.compare_corpus(self.base, self.cand, sids=['s01'])), 1)

    def test_skip_reflection(self):
        skip = sc.compare_corpus(self.base, self.cand, skip_reflection=True)
        self.assertEqual((self.status('contract', skip), self.status('order', skip)), ('FAIL', 'FAIL'))
        self.assertEqual(self.status('reflonly', skip), 'PENDING')
        self.assertEqual(sc.contract_diff(os.path.join(self.base, 'blobs', 'dddd'), os.path.join(self.cand, 'blobs', 'iiii'), True), [])

    def test_filter_includes_variants(self):
        self.assertEqual([r.name for r in sc.compare_corpus(self.base, self.cand, filter='bumpworld')], ['<variant:0,1>bumpworld', 'bumpworld'])
        self.assertEqual([r.name for r in sc.compare_corpus(self.base, self.cand, filter='bump*')],
                         ['<variant:0,1>bumpworld', 'bumpworld', 'bumpworldx'])

    def test_exit_code_and_summary(self):
        self.assertEqual(sc.check_exit_code(self.res), 1)
        self.assertEqual(sc.check_exit_code([r for r in self.res if r.status == 'PASS-TEXT']), 0)
        summary = sc.format_summary(self.res)
        self.assertRegex(summary, r'^== \d+ configs: \d+ text, 0 spirv, 0 pixel, 0 weak, 3 fail, \d+ missing, \d+ extra$')


class CompareRegistry(Base):
    REG = ['world stdworld ', 'world specworld s', 'world bumpworld B', 'decal stddecal b']

    def test_registry(self):
        base = self.registry('reg-base', self.REG)
        self.assertIsNone(sc.compare_registry(base, self.registry('reg-same', self.REG)))
        changed = sc.compare_registry(base, self.registry('reg-changed', ['world stdworld ', 'world specworld sS', 'world bumpworld B', 'decal stddecal b']))
        self.assertTrue(changed.status == 'FAIL' and changed.name == 'registry' and changed.sid == '-')
        self.assertEqual(changed.detail, 'line 2: -world specworld s; +world specworld sS')
        reordered = sc.compare_registry(base, self.registry('reg-order', ['world stdworld ', 'world bumpworld B', 'world specworld s', 'decal stddecal b']))
        self.assertEqual(reordered.detail, 'line 2: -world specworld s; +world bumpworld B')
        dropped = sc.compare_registry(base, self.registry('reg-missing', ['world stdworld ', 'world bumpworld B', 'decal stddecal b']))
        self.assertEqual(dropped.detail, 'line 2: -world specworld s; +world bumpworld B (4 vs 3 lines)')

    def test_crlf(self):
        # writetofile writes in text mode: CRLF on Windows. Shown without the
        # CR, but a line-end-only difference still fails (byte-exact).
        base = self.registry('reg-base', self.REG)
        crlf_base = self.registry('reg-crlf-base', self.REG, '\r\n')
        crlf_cand = self.registry('reg-crlf-cand', ['world stdworld ', 'world specworld sS', 'world bumpworld B', 'decal stddecal b'], '\r\n')
        self.assertEqual(sc.compare_registry(crlf_base, crlf_cand).detail, 'line 2: -world specworld s; +world specworld sS')
        self.assertEqual(sc.compare_registry(base, crlf_base).detail, 'same lines, different bytes')

    def test_missing_file(self):
        base = self.registry('reg-base', self.REG)
        r = sc.compare_registry(base, os.path.join(self.tmp, 'reg-none'))
        self.assertTrue(r.detail.startswith('no registry.txt in ') and r.detail.endswith('reg-none'))


class SameGpu(unittest.TestCase):
    A = 'vendor A\nrenderer R1\nversion 4.6\nglslversion 460\n'
    B = 'vendor A\nrenderer R2\nversion 4.6\nglslversion 460\n'

    def test_same_gpu(self):
        self.assertIs(sc.same_gpu(self.A, self.A, 'abc123'), True)
        self.assertIs(sc.same_gpu(self.A, self.B, 'abc123', allow_cross_gpu=True), False)
        with self.assertRaises(sc.CorpusError) as cm:
            sc.same_gpu(self.A, self.B, 'abc123')
        msg = str(cm.exception)
        for needle in ('renderer R1', 'renderer R2', 'commit abc123', '--allow-cross-gpu'):
            self.assertIn(needle, msg)

    def test_note(self):
        noted = [sc.Result('PASS-TEXT', 'a', 's00', detail='identical'), sc.Result('PASS-TEXT', 'b', 's00', detail='')]
        sc.add_cross_gpu_note(noted)
        self.assertEqual(noted[0].detail, 'identical (cross-gpu: reflection and pixels skipped)')
        self.assertEqual(noted[1].detail, '(cross-gpu: reflection and pixels skipped)')


class SweepLeaks(Base):
    def test_leaks(self):
        rows = sc.read_manifest(self.corpus('leak', ['a\ts00\t1111\tx', 'a\ts99\t1111\tx', 'b\ts00\t2222\tx', 'b\ts99\t3333\ty', 'c\ts99\t-\tx',
                                                     'd\ts00\t4444\tx', 'e\ts99\t5555\tx', 'f\ts00\t6666\tx', 'f\ts99\t-\tx'], {}))
        diffs = sc.find_sweep_leaks(rows, 's00', 's99')
        leaks = {d.name for d in diffs if sc.is_sweep_leak(d)}
        self.assertIn('b', leaks)           # changed hash
        self.assertNotIn('c', leaks)        # a stub after the sweep
        self.assertIn('d', leaks)           # vanishing after the sweep
        self.assertIn('f', leaks)           # valid at s00, invalid at s99
        self.assertNotIn('e', leaks)        # appearing only at s99 ...
        self.assertIn('e', {d.name for d in diffs})  # ... is still reported as a difference
        self.assertNotIn('a', leaks)        # identical
        self.assertEqual(sc.palette_coverage(rows, ['world a ', 'world b sS', 'decal d b'], ['s00', 's99']), ['d s99'])


class BenchLines(unittest.TestCase):
    def test_bench_lines(self):
        p = sc.parse_bench_line('SHADERBENCH hud PASS maxerr=0.0001 cov=87 seeds=4')
        self.assertEqual((p.status, p.name, p.detail), ('PASS-PIXEL', 'hud', 'maxerr=0.0001 cov=87 seeds=4'))
        self.assertEqual(sc.parse_bench_line('SHADERBENCH bumpworld FAIL maxerr=0.5 cov=91 seeds=4').status, 'FAIL')
        u = sc.parse_bench_line('SHADERBENCH watervortex FAIL maxerr=0.9 cov=40 seeds=4 reason=unsupported uniform 0x8B5E')
        self.assertEqual(u.status, 'WEAK')
        self.assertEqual(u.detail, 'maxerr=0.9 cov=40 seeds=4 reason=unsupported uniform 0x8B5E')
        self.assertEqual(sc.parse_bench_line('SHADERBENCH watervortex FAIL maxerr=0.9 cov=40 seeds=4 reason=unsupported sampler sampler2DMS').status, 'FAIL')
        self.assertEqual(sc.parse_bench_line('SHADERBENCH skin FAIL maxerr=0.9 cov=40 seeds=4 reason=unsupported attribute ivec4').status, 'FAIL')
        self.assertEqual(sc.parse_bench_line('SHADERBENCH hudtext WEAK maxerr=0.0002 cov=30 seeds=4 reason=coverage').status, 'WEAK')
        self.assertIsNone(sc.parse_bench_line('some other log line'))
        ts = sc.parse_bench_line('2026-09-25 15:04.12 SHADERBENCH hud PASS maxerr=0 cov=100 seeds=4')
        self.assertEqual(ts.status, 'PASS-PIXEL')


if __name__ == '__main__':
    unittest.main()
