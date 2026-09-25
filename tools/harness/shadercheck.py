#!/usr/bin/env python3
"""Offline tiers of the shader equivalence harness (tools/harness/shaders.ps1).

Tier 1 (TEXT):  both composed sources, preprocessed by glslangValidator -E
                (or comment-stripped when it is absent), have the same tokens.
Tier 2 (SPIRV): both compile to the same SPIR-V after spirv-opt -O and
                spirv-remap --map all --strip all.
Otherwise DIFF, or NA when a tier could not run (glslang missing or it
rejected the source; legacy compat-header constructs may not map to GL
SPIR-V). NA and DIFF both go on to the pixel tier.

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


def preprocessed(path, stage, tmp, have_glslang):
    """The source as the compiler sees it, or comment-stripped raw text."""
    if have_glslang:
        src = os.path.join(tmp, "pp." + stage)
        shutil.copyfile(path, src)
        r = run(["glslangValidator", "-E", src])
        if r.returncode == 0:
            return r.stdout
    return strip_comments(read(path))


def normal_lines(path, stage, tmp, have_glslang):
    lines = []
    for line in preprocessed(path, stage, tmp, have_glslang).splitlines():
        t = tokens(line)
        if t:
            lines.append(" ".join(t))
    return lines


def spirv(path, stage, tmp, tag):
    """Canonical SPIR-V bytes, or (None, reason)."""
    text = read(path)
    err = ""
    for version in (None, "450"):
        src = os.path.join(tmp, "%s.%s" % (tag, stage))
        with open(src, "w") as f:
            f.write(text if version is None else VERSION.sub("#version " + version, text, count=1))
        spv = src + ".spv"
        r = run(["glslangValidator", "-G", "--auto-map-locations", "--auto-map-bindings", "-o", spv, src])
        if r.returncode == 0:
            break
        err = (r.stdout.strip().splitlines() or ["glslang failed"])[-1]
    else:
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


def check_pair(key, base, cand, have_glslang):
    with tempfile.TemporaryDirectory() as tmp:
        same = True
        for fname, stage in STAGES:
            a = normal_lines(os.path.join(base, fname), stage, tmp, have_glslang)
            b = normal_lines(os.path.join(cand, fname), stage, tmp, have_glslang)
            if tokens(" ".join(a)) != tokens(" ".join(b)):
                same = False
                break
        if same:
            return key, "TEXT", ""
        if not have_glslang or not shutil.which("spirv-opt") or not shutil.which("spirv-remap"):
            return key, "NA", "glslang/spirv-tools not installed"
        for fname, stage in STAGES:
            sa, ea = spirv(os.path.join(base, fname), stage, tmp, "a")
            sb, eb = spirv(os.path.join(cand, fname), stage, tmp, "b")
            if sa is None or sb is None:
                return key, "NA", "%s %s" % (stage, ea or eb)
            if sa != sb:
                return key, "DIFF", "%s SPIR-V differs" % stage
        return key, "SPIRV", ""


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
