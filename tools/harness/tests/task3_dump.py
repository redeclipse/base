#!/usr/bin/env python3
"""Task 3 (task3-dump.ps1): shaderdumpall writes a complete, deterministic
corpus. Needs a running harness; no map required."""

import os
import re
import shutil
import sys

from _live import Tally, core


def fnv1a64(data):
    """64-bit FNV-1a, as shaderharness::fnv1a."""
    h = 0xcbf29ce484222325
    for b in data:
        h = ((h ^ b) * 0x100000001b3) & 0xFFFFFFFFFFFFFFFF
    return '%016x' % h


def lines(path):
    with open(path, encoding='utf-8', newline='') as f:
        return f.read().splitlines()


def read(path):
    with open(path, encoding='utf-8', newline='') as f:
        return f.read()


def main():
    t = Tally('task 3')
    root = os.path.join(core.HOME_DIR, 'shadercorpus')
    for r in ('t3a', 't3b'):
        shutil.rmtree(os.path.join(root, r), ignore_errors=True)
    out = core.invoke_batch('shaderdumpall t3a s00 0', 1, 300) + core.invoke_batch('shaderdumpall t3b s00 0', 1, 300)
    t.check('both dumps reported', len([l for l in out if re.search(r'SHADERDUMP t3[ab] s00 \d+ \d+', l)]) == 2)
    bad = core.invoke_batch('echo (concatword "T3_BAD=" (shaderdumpall "../x" s00 0))', 1, 60)
    t.check('a path-like run name is refused', len([l for l in bad if 'T3_BAD=-1' in l]) == 1)

    a = lines(os.path.join(root, 't3a', 'manifest.tsv'))
    b = lines(os.path.join(root, 't3b', 'manifest.tsv'))
    t.check('manifest has hundreds of rows', len(a) > 300)
    t.check('every row has four tab-separated fields', all(len(l.split('\t')) == 4 for l in a))
    t.check('the same state gives the same manifest', a == b)
    std = [l for l in a if l.startswith('stdworld\t')]
    t.check('stdworld has exactly one row', len(std) == 1)
    f = std[0].split('\t')
    t.check('stdworld origin is its defershader', f[3] == 'defer:stdworld')
    t.check('stdworld hash is 16 hex digits', re.match(r'^[0-9a-f]{16}$', f[2]) is not None)
    t.check('the stdworld variant is dumped', len([l for l in a if l.startswith('<variant:0,0>stdworld\t')]) == 1)
    hud = [l for l in a if l.startswith('hud\t')]
    t.check('the hud shader came from init.cfg', len(hud) == 1 and hud[0].split('\t')[3] == 'config/glsl/init.cfg')

    blob = os.path.join(root, 't3a', 'blobs', f[2])
    for n in ('vs.glsl', 'fs.glsl', 'vs.full.glsl', 'fs.full.glsl', 'meta.txt', 'reflect.txt'):
        t.check('blob has ' + n, os.path.exists(os.path.join(blob, n)))
    meta = lines(os.path.join(blob, 'meta.txt'))
    t.check('meta starts with the type', re.match(r'^type \d+$', meta[0]) is not None)
    t.check('stdworld lists the gloss default', len([l for l in meta if l.startswith('param gloss ')]) == 1)
    t.check('stdworld has variant rows', len([l for l in meta if l.startswith('variants ')]) >= 1)
    reflect = lines(os.path.join(blob, 'reflect.txt'))
    t.check('reflection lists vvertex', len([l for l in reflect if l.startswith('attrib vvertex ')]) == 1)
    t.check('reflection lists the diffusemap sampler with its unit', len([l for l in reflect if re.match(r'^sampler diffusemap sampler2D \d+$', l)]) == 1)
    t.check('reflection lists a fragment output', len([l for l in reflect if l.startswith('fragdata ')]) >= 1)
    t.check('reflection is sorted', reflect == sorted(reflect))  # strcmp order
    full = read(os.path.join(blob, 'fs.full.glsl'))
    t.check('composed source starts with the version header', re.match(r'^#version \d+', full) is not None)
    t.check('composed source ends with the body', full.endswith(read(os.path.join(blob, 'fs.glsl')).lstrip()))
    t.check('gl.txt names the renderer', len([l for l in lines(os.path.join(root, 't3a', 'gl.txt')) if l.startswith('renderer ')]) == 1)

    # Content addressing: the directory name is the FNV-1a of exactly the
    # bytes written -- vs.full.glsl, NUL, fs.full.glsl, NUL, then meta.txt and
    # reflect.txt back to back.
    data = b''
    for n, sep in (('vs.full.glsl', b'\0'), ('fs.full.glsl', b'\0'), ('meta.txt', b''), ('reflect.txt', b'')):
        with open(os.path.join(blob, n), 'rb') as fh:
            data += fh.read() + sep
    t.check('the blob hash reproduces the blob directory name', fnv1a64(data) == f[2])
    return t.finish()


if __name__ == '__main__':
    sys.exit(main())
