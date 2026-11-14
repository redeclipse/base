#!/usr/bin/env python3
"""Shader equivalence harness: record shader corpora and compare them.

The Linux/macOS counterpart of shaders.ps1.

record  Sweeps render settings in the running game and writes a corpus (every
        shader configuration: composed source, metadata, GL reflection) to
        home/uitest/shadercorpus/<run>/.
check   Records a candidate corpus from the current build and compares it with
        a baseline through the tier ladder: contract -> text -> SPIR-V ->
        pixel. Exits 1 on any FAIL or MISSING. Refuses a baseline recorded on
        another GPU/driver unless --allow-cross-gpu, which skips reflection and
        pixels.
diff    Shows why one configuration differs.

Tiers 1-2 run shadercheck.py with this Python; it uses glslangValidator,
spirv-opt and spirv-remap from PATH when they are there (glslang-tools and
spirv-tools packages) and falls back to the raw text comparison otherwise.
See doc/superpowers/specs/2026-09-25-shader-equivalence-harness-design.md.

  tools/harness/shaders.py record
  tools/harness/shaders.py check --filter 'bump*'
  tools/harness/shaders.py check --sids s00 --no-maps
  tools/harness/shaders.py diff bumpworld --sid s00
"""

import argparse
import datetime
import os
import re
import shutil
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import core  # noqa: E402
import editor  # noqa: E402
import shadercorpus as sc  # noqa: E402
from core import HarnessError, invoke_batch, say  # noqa: E402

CORPUS_ROOT = os.path.join(core.HOME_DIR, 'shadercorpus')
SHADERCHECK = os.path.join(core.HARNESS_DIR, 'shadercheck.py')
SWEEP_FILE = os.path.join(core.HARNESS_DIR, 'shader-sweep.txt')

ORDER = {'FAIL': 0, 'MISSING': 1, 'WEAK': 2, 'EXTRA': 3, 'PASS-PIXEL': 4, 'PASS-SPIRV': 5, 'PASS-TEXT': 6}


def _lines_of(text):
    return text.replace('\r\n', '\n').split('\n')


class Recorder:
    """The game side: one instance per command, holding the session's state
    (the sweep defaults, and which map and point are loaded)."""

    def __init__(self, map_name='atop', seeds=4):
        self.map = map_name
        self.seeds = seeds
        self.defaults = None   # var -> default value text, for every swept var
        self.current_map = None
        self.current_point = None

    # ------------------------------------------------------------ game ----

    def checked(self, script, settle_ms, timeout):
        lines = invoke_batch(script, settle_ms, timeout)
        bad = [l for l in lines if core.ERROR_PATTERN.search(l) or 'valid range for' in l]
        if bad:
            raise HarnessError('The game reported errors:\n' + '\n'.join(bad))
        return lines

    def var_defaults(self, names):
        defaults = {}
        if not names:
            return defaults
        script = '\n'.join('echo (concatword "SWEEPVAR %s " (getvartype %s) " " (? (= (getvartype %s) 1) (getfvardef %s 0) (getvardef %s 0)))'
                           % (v, v, v, v, v) for v in names)
        for line in self.checked(script, 1, 60):
            m = re.search(r'SWEEPVAR (\S+) (-?\d+) (\S+)', line)
            if m:
                if m.group(2) not in ('0', '1'):
                    raise HarnessError("Sweep var '%s' is not an int or float var (type %s)." % (m.group(1), m.group(2)))
                defaults[m.group(1)] = m.group(3)
        for v in names:
            if v not in defaults:
                raise HarnessError("The game did not report sweep var '%s'." % v)
        return defaults

    def set_point(self, point):
        """Applies one settings vector on top of the defaults and regenerates
        shaders. The settle lets frames render, which is when the C++ setup
        paths call their generateshaders (AO, bilateral, deferred lights,
        volumetrics, AA)."""
        lines = ['%s %s' % (v, point.settings.get(v, self.defaults[v])) for v in self.defaults] + ['resetshaders']
        out = self.checked('\n'.join(lines), 2000, 300)
        # Vars carrying initwarning(..., INIT_LOAD, CHANGE_SHADERS) log "Pending
        # shader change:" and leave the change queued; detect the message and
        # force the real reload with a follow-up resetgl (needs applydialog 1).
        if any('Pending shader change:' in l for l in out):
            out += self.checked('resetgl', 3000, 300)
        errors = sum(1 for l in out if 'GLSL ERROR' in l)
        if errors:
            core.warn('%s: %d GLSL compile error(s); those shaders are recorded as invalid.' % (point.id, errors))
        self.current_point = point.id

    def open_map(self, name):
        # Loading a map while an entity is hovered trips an unguarded enthover
        # read; entity editing off empties it. See editor_selftest.py.
        self.checked('entediting 0', 300, 60)
        editor.open_map(name)
        self.current_map = name
        self.current_point = None

    def enter_point(self, pid, points):
        """Puts the game into the state a settings id was recorded in."""
        if pid.startswith('m-'):
            name = pid[2:]
            # Reset after every map load: generated shaders (models, deferred
            # lights) outlive a map change.
            if self.current_map != name:
                self.open_map(name)
            if self.current_point != pid:
                self.set_point(sc.Point(pid, {}))
            return
        if self.current_map != self.map:
            self.open_map(self.map)
        if self.current_point != pid:
            match = [p for p in points if p.id == pid]
            if not match:
                raise HarnessError("Unknown settings id '%s'." % pid)
            self.set_point(match[0])

    def dump(self, run, pid, maps_only):
        out = self.checked('shaderdumpall %s %s %d' % (run, pid, 1 if maps_only else 0), 1, 600)
        pattern = re.compile(r'SHADERDUMP %s %s (\d+) (\d+)' % (re.escape(run), re.escape(pid)))
        hit = [l for l in out if pattern.search(l)]
        if not hit:
            raise HarnessError('shaderdumpall did not report for %s:\n%s' % (pid, '\n'.join(out)))
        errors = sum(1 for l in out if 'GLSL ERROR' in l)
        if errors:
            core.warn('%s: %d GLSL compile error(s) while forcing; those shaders are recorded as invalid.' % (pid, errors))
        stuck = [l for l in out if re.search(r'forceallshaders: .*still deferred', l)]
        if stuck:
            core.warn('%s: %s' % (pid, re.sub(r'^.*forceallshaders: ', '', stuck[0])))
        say('  %-14s %s' % (pid, re.sub(r'^.*SHADERDUMP \S+ \S+ ', 'rows/new blobs: ', hit[0])))

    def write_registry(self, run):
        script = ('writetofile "shadercorpus/RUN/registry.txt" (concatword (looplistconcatword e $worldshaders [concatword "world " (at $e 0) " " (at $e 1) "^n"]) '
                  '(looplistconcatword e $decalshaders [concatword "decal " (at $e 0) " " (at $e 1) "^n"]))').replace('RUN', run)
        self.checked(script, 1, 60)
        path = os.path.join(CORPUS_ROOT, run, 'registry.txt')
        if not os.path.exists(path):
            raise HarnessError('registry.txt was not written: %s' % path)

    def game_gl_text(self):
        """The game's GL identity, in gl.txt's format, so a cross-GPU check is
        refused before the candidate sweep rather than after it."""
        probe = os.path.join(CORPUS_ROOT, 'glprobe.txt')
        if os.path.exists(probe):
            os.remove(probe)
        self.checked('writetofile "shadercorpus/glprobe.txt" (concatword "vendor " $gfxvendor "^n" "renderer " $gfxrenderer "^n" "version " '
                     '$gfxversion "^n" "glslversion " $glslversion "^n")', 1, 60)
        if not os.path.exists(probe):
            raise HarnessError('The GL probe was not written: %s' % probe)
        with open(probe, encoding='utf-8', newline='') as f:
            return f.read().replace('\r\n', '\n')

    # ---------------------------------------------------------- record ----

    def record(self, run, points, maps, var_points=None):
        """var_points: the points whose vars every sweep point resets to their
        defaults. Pass the whole sweep even when recording a subset, or a var
        only an unselected point sets keeps whatever value the session last
        left. Returns the number of self-check problems."""
        var_points = var_points or points
        if not core.is_running():
            core.start()
        run_dir = os.path.join(CORPUS_ROOT, run)
        if os.path.exists(run_dir):
            shutil.rmtree(run_dir)
        os.makedirs(os.path.join(run_dir, 'settings'))

        # addchange() returns early when $applydialog is 0, so the resetgl
        # trigger in set_point would never fire: pin it on for the sweep.
        self.checked('applydialog 1', 300, 60)
        self.defaults = self.var_defaults(sc.sweep_vars(var_points))
        say("Recording '%s': %d settings point(s), %d map(s)" % (run, len(points), len(maps)), 'cyan')

        for p in points:
            self.enter_point(p.id, points)
            self.dump(run, p.id, False)
            core.write_text(os.path.join(run_dir, 'settings', p.id + '.txt'), ''.join('%s=%s\n' % kv for kv in p.settings.items()))
        self.write_registry(run)
        for m in maps:
            self.enter_point('m-' + m, points)
            self.dump(run, 'm-' + m, True)

        # Leave the persisted settings at their defaults.
        self.set_point(sc.Point('defaults', {}))

        commit = subprocess.run(['git', '-C', core.REPO_ROOT, 'rev-parse', 'HEAD'], capture_output=True, text=True).stdout.strip()
        dirty = 1 if subprocess.run(['git', '-C', core.REPO_ROOT, 'status', '--porcelain', '--', 'config/glsl'],
                                    capture_output=True, text=True).stdout.strip() else 0
        info = ['commit %s' % commit, 'glsldirty %d' % dirty, 'recorded %s' % datetime.datetime.now().strftime('%Y-%m-%dT%H:%M:%S'),
                'map %s' % self.map, 'sweep %s' % ' '.join(p.id for p in points), 'maps %s' % ' '.join(maps)]
        core.write_text(os.path.join(run_dir, 'run.txt'), '\n'.join(info) + '\n')

        # Self-checks.
        rows = sc.read_manifest(run_dir)
        ids = [p.id for p in points]
        problems = 0
        if 's00' in ids and 's99' in ids:
            # A name '(none)' at s00 but present at s99 is not a reset
            # failure (resetshaders never frees variants an earlier point
            # registered); valid at s00 and gone or different at s99 is.
            diffs = sc.find_sweep_leaks(rows, 's00', 's99')
            leaks = [d for d in diffs if sc.is_sweep_leak(d)]
            grown = [d for d in diffs if not sc.is_sweep_leak(d)]
            for l in leaks:
                say('  LEAK  %s: s00 %s vs s99 %s (origin %s)' % (l.name, l.first, l.last, l.origin), 'red')
            if leaks:
                say('  State leaked between sweep points: a var these shaders read is missing from the reset list in shader-sweep.txt.', 'red')
                problems += 1
            else:
                say('  no state leaked between s00 and s99', 'green')
            if grown:
                say('  (%d shader row(s) exist only at s99: permanent registrations left behind by an earlier sweep point, not a leak)' % len(grown), 'yellow')
        with open(os.path.join(run_dir, 'registry.txt'), encoding='utf-8', newline='') as f:
            registry = f.read().splitlines()
        gaps = sc.palette_coverage(rows, registry, ids)
        for g in gaps:
            say('  PALETTE  %s has no valid shader' % g, 'red')
        if gaps:
            problems += 1
        else:
            say('  all %d registered world/decal shaders valid at every point' % len(registry), 'green')
        if dirty and run == 'baseline':
            core.warn('config/glsl has uncommitted changes: this baseline is not a pinned commit.')
        return problems

    # ----------------------------------------------------------- tiers ----

    def offline_tiers(self, pending, base_dir, cand_dir):
        """Tiers 1-2 for every PENDING result; index -> (tier, detail)."""
        out = {}
        if not pending:
            return out
        pairs = os.path.join(CORPUS_ROOT, 'pairs.tsv')
        core.write_text(pairs, ''.join('%d\t%s\t%s\n' % (i, os.path.join(base_dir, 'blobs', p.base_hash), os.path.join(cand_dir, 'blobs', p.cand_hash))
                                       for i, p in enumerate(pending)))
        proc = subprocess.run([sys.executable, SHADERCHECK, '--pairs', pairs], capture_output=True, text=True)
        if proc.returncode != 0:
            raise HarnessError('shadercheck.py --pairs failed:\n%s%s' % (proc.stdout, proc.stderr))
        for line in proc.stdout.splitlines():
            f = line.split('\t', 2)
            if len(f) >= 2 and re.match(r'^\d+$', f[0]):
                out[int(f[0])] = (f[1], f[2] if len(f) > 2 else '')
        return out

    def benches(self, queue, base_run, points):
        """Tier 3 for the queued results, grouped by settings point;
        '<sid>\\t<name>' -> (status, detail)."""
        results = {}
        groups = {}
        for r in queue:
            groups.setdefault(r.sid, []).append(r)
        for sid, items in groups.items():
            self.enter_point(sid, points)
            self.checked('shaderforceall', 1, 300)
            for i in range(0, len(items), 20):
                script = '\n'.join('shaderbench %s %s "%s" %d' % (base_run, r.base_hash, r.name, self.seeds) for r in items[i:i + 20])
                for line in invoke_batch(script, 1, 600):
                    b = sc.parse_bench_line(line)
                    if b:
                        results['%s\t%s' % (sid, b.name)] = (b.status, b.detail)
        return results

    # ----------------------------------------------------------- check ----

    def check(self, run='baseline', sids=None, no_maps=False, filter='*', max_tier=3, allow_cross_gpu=False):
        # 'candidate' is the scratch corpus check records into: checking
        # against it would delete the baseline and compare it with itself.
        if run.lower() == 'candidate':
            raise HarnessError("check --run candidate is refused: 'candidate' is the scratch corpus check records into, so it would overwrite "
                               "the baseline and compare it with itself. Record the baseline under another name.")
        base_dir = os.path.join(CORPUS_ROOT, run)
        cand_dir = os.path.join(CORPUS_ROOT, 'candidate')
        if not os.path.exists(os.path.join(base_dir, 'manifest.tsv')):
            raise HarnessError('No baseline corpus at %s. Record one first: tools/harness/shaders.py record' % base_dir)

        # Replay exactly what the baseline recorded, not the current sweep file.
        info = sc.read_run_info(base_dir)
        with open(os.path.join(base_dir, 'gl.txt'), encoding='utf-8', newline='') as f:
            base_gl = f.read()
        if not core.is_running():
            core.start()
        try:
            sc.same_gpu(base_gl, self.game_gl_text(), info.get('commit', ''), allow_cross_gpu)
        except sc.CorpusError as e:
            raise HarnessError(str(e))

        all_points = sc.read_run_points(base_dir)
        points = [p for p in all_points if not sids or p.id in sids]
        maps = []
        if not no_maps:
            maps = [m for m in info.get('maps', '').split(' ') if m]
            if sids:
                maps = [m for m in maps if 'm-' + m in sids]
        self.map = info.get('map', self.map)
        # Reset the vars of every recorded point, not only the selected ones.
        if self.record('candidate', points, maps, all_points):
            core.warn('The candidate recording reported leaks or palette gaps (above).')

        wanted = [p.id for p in points] + ['m-' + m for m in maps]
        # gl.txt is the authority; the probe above only saves a wasted sweep.
        with open(os.path.join(cand_dir, 'gl.txt'), encoding='utf-8', newline='') as f:
            cand_gl = f.read()
        try:
            same = sc.same_gpu(base_gl, cand_gl, info.get('commit', ''), allow_cross_gpu)
        except sc.CorpusError as e:
            raise HarnessError(str(e))
        if not same:
            core.warn('The baseline was recorded on a different GPU/driver (--allow-cross-gpu): reflection and pixel tiers are skipped; '
                      'the metadata contract, text and SPIR-V still run.')
            max_tier = min(max_tier, 2)

        # The registry is compared whatever --filter and --sids say.
        results = [r for r in [sc.compare_registry(base_dir, cand_dir)] if r]
        results += sc.compare_corpus(base_dir, cand_dir, filter, wanted, not same)
        pending = [r for r in results if r.status == 'PENDING']
        offline = self.offline_tiers(pending, base_dir, cand_dir) if max_tier >= 1 else {}
        queue = []
        for i, r in enumerate(pending):
            o = offline.get(i)
            # Keep the checker's detail when it has one: it names a raw fallback.
            if o and o[0] == 'TEXT':
                r.status, r.detail = 'PASS-TEXT', o[1] or 'same tokens after preprocessing'
                continue
            if o and o[0] == 'SPIRV' and max_tier >= 2:
                r.status, r.detail = 'PASS-SPIRV', o[1]
                continue
            if max_tier >= 3:
                queue.append(r)
                continue
            r.status = 'FAIL'
            if o:
                r.detail = 'tier %s: %s' % o
            elif max_tier >= 1:
                r.detail = 'no offline tier result reported for this pair'
            else:
                r.detail = 'content differs (text tiers disabled)'
        if queue:
            bench = self.benches(queue, run, points)
            for r in queue:
                b = bench.get('%s\t%s' % (r.sid, r.name))
                r.status, r.detail = b if b else ('FAIL', 'bench did not report')
        if not same:
            sc.add_cross_gpu_note(results)
        return sorted(results, key=lambda r: (ORDER[r.status], r.sid, r.name))


def diff(name, sid='s00', run='baseline'):
    """Lines saying why one configuration differs."""
    if not name:
        raise HarnessError('Usage: shaders.py diff <name> [--sid s00]')
    base_dir = os.path.join(CORPUS_ROOT, run)
    cand_dir = os.path.join(CORPUS_ROOT, 'candidate')
    b = [r for r in sc.read_manifest(base_dir) if r.name == name and r.sid == sid and r.hash != '-']
    c = [r for r in sc.read_manifest(cand_dir) if r.name == name and r.sid == sid and r.hash != '-']
    if not b:
        raise HarnessError("No valid baseline row for '%s' at %s in %s." % (name, sid, base_dir))
    if not c:
        raise HarnessError("No valid candidate row for '%s' at %s. Run 'shaders.py check' first." % (name, sid))
    out = ['baseline   %s  origin %s' % (b[0].hash, b[0].origin), 'candidate  %s  origin %s' % (c[0].hash, c[0].origin)]
    bb = os.path.join(base_dir, 'blobs', b[0].hash)
    cb = os.path.join(cand_dir, 'blobs', c[0].hash)
    contract = sc.contract_diff(bb, cb)
    if contract:
        out.append('--- contract')
        out += ['    ' + l for l in contract]
    else:
        out.append('--- contract: identical')
    for short, stage in (('vs', 'vert'), ('fs', 'frag')):
        files = []
        for side, blob in (('baseline', bb), ('candidate', cb)):
            proc = subprocess.run([sys.executable, SHADERCHECK, '--normalize', os.path.join(blob, short + '.full.glsl'), '--stage', stage],
                                  capture_output=True, text=True)
            if proc.returncode != 0:
                raise HarnessError('shadercheck.py --normalize failed:\n%s%s' % (proc.stdout, proc.stderr))
            path = os.path.join(CORPUS_ROOT, 'diff-%s.%s.txt' % (side, short))
            core.write_text(path, proc.stdout.rstrip('\n') + '\n')
            files.append(path)
        out.append('--- %s (normalized)' % short)
        proc = subprocess.run(['git', '--no-pager', 'diff', '--no-index', '--no-color', '-U3', '--'] + files, capture_output=True, text=True)
        out += proc.stdout.splitlines()
    return out


def shipped_maps():
    d = os.path.join(core.REPO_ROOT, 'data', 'maps')
    return sorted(os.path.splitext(n)[0] for n in os.listdir(d) if n.endswith('.mpz'))


def main(argv=None):
    p = argparse.ArgumentParser(prog='shaders.py', description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter, allow_abbrev=False)
    p.add_argument('command', choices=('record', 'check', 'diff'))
    p.add_argument('name', nargs='?', help='diff: the shader name')
    p.add_argument('--run', default='baseline')
    p.add_argument('--sids', type=lambda s: [x for x in re.split(r'[,\s]+', s) if x], help='settings ids, comma-separated')
    p.add_argument('--maps', type=lambda s: [x for x in re.split(r'[,\s]+', s) if x], help='record: maps for the per-map pass, comma-separated')
    p.add_argument('--no-maps', action='store_true')
    p.add_argument('--map', default='atop', help='record: the map the settings sweep runs on')
    p.add_argument('--sweep-file', default=SWEEP_FILE)
    p.add_argument('--filter', default='*')
    p.add_argument('--max-tier', type=int, choices=range(0, 4), default=3)
    p.add_argument('--seeds', type=int, default=4)
    p.add_argument('--sid', default='s00', help='diff: the settings id')
    p.add_argument('--allow-cross-gpu', action='store_true')
    a = p.parse_args(argv)
    try:
        if a.command == 'record':
            sweep = sc.read_sweep(a.sweep_file)
            points = [x for x in sweep if not a.sids or x.id in a.sids]
            maps = [] if a.no_maps else (a.maps or shipped_maps())
            # Reset every var in the sweep file, even for a --sids subset.
            return 1 if Recorder(a.map, a.seeds).record(a.run, points, maps, sweep) else 0
        if a.command == 'check':
            results = Recorder(seeds=a.seeds).check(a.run, a.sids, a.no_maps, a.filter, a.max_tier, a.allow_cross_gpu)
            for r in results:
                print(sc.format_result(r))
            print(sc.format_summary(results))
            return sc.check_exit_code(results)
        for line in diff(a.name, a.sid, a.run):
            print(line)
        return 0
    except (HarnessError, sc.CorpusError) as e:
        say(str(e), 'red')
        return 1


if __name__ == '__main__':
    sys.exit(main())
