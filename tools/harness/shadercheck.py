#!/usr/bin/env python3
"""Offline tiers of the shader equivalence harness (tools/harness/shaders.ps1).

Tier 1 (TEXT):  both composed sources, preprocessed by glslangValidator -E
                (or comment-stripped when it is absent), have the same tokens.
Tier 2 (SPIRV): both compile to the same SPIR-V after spirv-opt -O and
                spirv-remap --map all --strip all.
Otherwise DIFF, or NA when a tier could not run (glslang missing or it
rejected the source; legacy compat-header constructs may not map to GL
SPIR-V). NA and DIFF both go on to the pixel tier.

Both tiers keep base and candidate symmetric: for a pair, either both sides
are preprocessed (tier 1) / compiled at the same GLSL version (tier 2), or
neither is -- never one side preprocessed/compiled one way and the other
side another way, which would make the comparison meaningless. See
preprocess_pair() and spirv_pair()/choose_version().

Usage:
  shadercheck.py --pairs pairs.tsv       lines: key<TAB>baseblobdir<TAB>candblobdir
  shadercheck.py --normalize FILE --stage vert|frag
"""
import argparse
import os
import re
import shutil
import subprocess
import sys
import tempfile
from concurrent.futures import ThreadPoolExecutor

STAGES = (("vs.full.glsl", "vert"), ("fs.full.glsl", "frag"))

TOKEN = re.compile(r"""
    [A-Za-z_]\w*                                    # identifier or keyword
  | (?:\d+\.\d*|\.\d+|\d+)(?:[eE][+-]?\d+)?[fFuU]?  # number
  | <<=|>>=|\+\+|--|&&|\|\||\^\^|[<>=!+\-*/%&|^]=|<<|>>
  | \S                                              # any other single character
""", re.X)
COMMENT = re.compile(r"//[^\n]*|/\*.*?\*/", re.S)
VERSION = re.compile(r"^\s*#\s*version\s+\d+.*$", re.M)


def run(cmd):
    return subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)


def strip_comments(text):
    return COMMENT.sub(" ", text)


def tokens(text):
    out = []
    for line in text.splitlines():
        if line.lstrip().startswith("#line"):
            continue
        out.extend(TOKEN.findall(line))
    return out


def read(path):
    with open(path, encoding="utf-8", errors="replace") as f:
        return f.read()


def normal_lines_from_text(text):
    lines = []
    for line in text.splitlines():
        t = tokens(line)
        if t:
            lines.append(" ".join(t))
    return lines


def flatten(lines):
    """Flatten already-tokenized lines (each already ' '.join(tokens)) into
    one token stream. A plain whitespace split reproduces the same tokens
    the TOKEN regex would, since each line is already single-space-joined
    tokens with none containing whitespace -- no need to re-tokenize."""
    return " ".join(lines).split()


def try_preprocess(path, stage, tmp, tag):
    """Attempt glslangValidator -E on one file. Returns the preprocessed
    text, or None if -E rejected the source (e.g. an unterminated #if)."""
    src = os.path.join(tmp, tag + "." + stage)
    shutil.copyfile(path, src)
    r = run(["glslangValidator", "-E", src])
    if r.returncode == 0:
        return r.stdout
    return None


def preprocessed(path, stage, tmp, have_glslang):
    """The source as the compiler sees it, or comment-stripped raw text.
    Single-file version used only by --normalize, which has no "other side"
    to stay symmetric with; check_pair uses preprocess_pair() instead so a
    pair is never compared preprocessed-vs-raw."""
    if have_glslang:
        text = try_preprocess(path, stage, tmp, "pp")
        if text is not None:
            return text
    return strip_comments(read(path))


def normal_lines(path, stage, tmp, have_glslang):
    """Tier-1 normal form for a single file (the --normalize CLI path)."""
    return normal_lines_from_text(preprocessed(path, stage, tmp, have_glslang))


def preprocess_pair(base, cand, stage, tmp, have_glslang):
    """Tier-1 normal-form lines for both sides of one stage, kept symmetric:
    if glslangValidator -E fails on either side, BOTH sides fall back to
    comment-stripped raw text (never one preprocessed and one raw -- that
    would compare apples to oranges). Returns (lines_a, lines_b, note),
    where note names the fallback (e.g. for a caller-visible detail string)
    or is "" when no fallback happened."""
    if have_glslang:
        ta = try_preprocess(base, stage, tmp, "a")
        tb = try_preprocess(cand, stage, tmp, "b")
        if ta is not None and tb is not None:
            return normal_lines_from_text(ta), normal_lines_from_text(tb), ""
        failed = [name for name, t in (("base", ta), ("cand", tb)) if t is None]
        note = "tier1 raw fallback: %s -E failed on %s" % (stage, "|".join(failed))
    else:
        note = ""
    return (normal_lines_from_text(strip_comments(read(base))),
            normal_lines_from_text(strip_comments(read(cand))),
            note)


def spirv(path, stage, tmp, tag, version):
    """Canonical SPIR-V bytes for one file at a fixed GLSL version
    ("native" keeps the file's own #version; otherwise it is rewritten), or
    (None, reason). The version to use is decided by the caller (see
    spirv_pair/choose_version) so both sides of a pair are always compiled
    at the same version. "native" (not None) is used for "keep as-is" so it
    can never collide with choose_version()'s None "no version worked"
    sentinel."""
    text = read(path)
    src = os.path.join(tmp, "%s.%s" % (tag, stage))
    with open(src, "w") as f:
        f.write(text if version == "native" else VERSION.sub("#version " + version, text, count=1))
    spv = src + ".spv"
    r = run(["glslangValidator", "-G", "--auto-map-locations", "--auto-map-bindings", "-o", spv, src])
    if r.returncode != 0:
        err = (r.stdout.strip().splitlines() or ["glslang failed"])[-1]
        return None, "glslang: " + err
    opt = spv + ".opt"
    r = run(["spirv-opt", "-O", spv, "-o", opt])
    if r.returncode != 0:
        return None, "spirv-opt: " + (r.stdout.strip().splitlines() or ["failed"])[-1]
    outdir = os.path.join(tmp, tag + "-remap")
    os.makedirs(outdir, exist_ok=True)
    r = run(["spirv-remap", "--map", "all", "--strip", "all", "-i", opt, "-o", outdir])
    if r.returncode != 0:
        return None, "spirv-remap: " + (r.stdout.strip().splitlines() or ["failed"])[-1]
    with open(os.path.join(outdir, os.path.basename(opt)), "rb") as f:
        return f.read(), ""


def choose_version(versions, ok):
    """Pick the first version in `versions` for which ok(version) reports
    success on BOTH sides (an (ok_a, ok_b) pair). Returns None if no version
    works for both -- the caller's cue to report NA. `versions` must not
    contain None (reserved for "not found"); use a string like "native" for
    "keep the file's own #version". Deliberately has no compiler calls of
    its own -- `ok` is the only place real work happens -- so the retry
    order (native first, then both sides together at the next version,
    NEVER a mix of versions between sides) is testable without glslang
    installed."""
    for v in versions:
        ok_a, ok_b = ok(v)
        if ok_a and ok_b:
            return v
    return None


def spirv_pair(base, cand, stage, tmp):
    """Compile one stage of both sides to canonical SPIR-V, keeping the GLSL
    version symmetric: try both natively; if either fails, retry BOTH sides
    forced to #version 450; if either still fails there, report NA. Returns
    (bytes_a, bytes_b, detail) -- bytes_a is None on NA, with detail set."""
    results = {}

    def ok(version):
        sa, ea = spirv(base, stage, tmp, "a", version)
        sb, eb = spirv(cand, stage, tmp, "b", version)
        results[version] = (sa, ea, sb, eb)
        return sa is not None, sb is not None

    versions = ("native", "450")
    version = choose_version(versions, ok)
    if version is None:
        sa, ea, sb, eb = results[versions[-1]]
        return None, None, "%s %s" % (stage, ea or eb)
    sa, ea, sb, eb = results[version]
    return sa, sb, ""


def combine(notes, detail):
    parts = [n for n in notes if n]
    if detail:
        parts.append(detail)
    return "; ".join(parts)


def check_pair(key, base, cand, have_glslang):
    with tempfile.TemporaryDirectory() as tmp:
        same = True
        notes = []
        for fname, stage in STAGES:
            a, b, note = preprocess_pair(os.path.join(base, fname), os.path.join(cand, fname), stage, tmp, have_glslang)
            if note:
                notes.append(note)
            if flatten(a) != flatten(b):
                same = False
                break
        if same:
            return key, "TEXT", combine(notes, "")
        if not have_glslang or not shutil.which("spirv-opt") or not shutil.which("spirv-remap"):
            return key, "NA", combine(notes, "glslang/spirv-tools not installed")
        for fname, stage in STAGES:
            sa, sb, err = spirv_pair(os.path.join(base, fname), os.path.join(cand, fname), stage, tmp)
            if sa is None:
                return key, "NA", combine(notes, err)
            if sa != sb:
                return key, "DIFF", combine(notes, "%s SPIR-V differs" % stage)
        return key, "SPIRV", combine(notes, "")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--pairs")
    ap.add_argument("--normalize")
    ap.add_argument("--stage", choices=("vert", "frag"), default="frag")
    args = ap.parse_args()
    have_glslang = shutil.which("glslangValidator") is not None

    if args.normalize:
        with tempfile.TemporaryDirectory() as tmp:
            print("\n".join(normal_lines(args.normalize, args.stage, tmp, have_glslang)))
        return 0

    if not args.pairs:
        ap.error("--pairs or --normalize is required")
    jobs = []
    with open(args.pairs, encoding="utf-8") as f:
        for n, line in enumerate(f, 1):
            line = line.rstrip("\r\n")
            if not line:
                continue
            parts = line.split("\t")
            if len(parts) != 3:
                sys.stderr.write("%s:%d: expected key<TAB>base<TAB>cand\n" % (args.pairs, n))
                return 2
            jobs.append(parts)
    with ThreadPoolExecutor(max_workers=os.cpu_count() or 4) as pool:
        for key, tier, detail in pool.map(lambda j: check_pair(j[0], j[1], j[2], have_glslang), jobs):
            print("%s\t%s\t%s" % (key, tier, detail))
    return 0


if __name__ == "__main__":
    sys.exit(main())
