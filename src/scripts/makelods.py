#!/usr/bin/env python3
# makelods.py: gives models simpler detail levels, as lod1 and lod2 folders beside their config and mdllod lines in it
#
# Triangles are only removed, using meshoptimizer's simplifier, so skeletons, weights, tags and texture coordinates stay as they
# were. It needs the meshoptimizer package, whose library is usually /usr/lib/libmeshoptimizer.so:
#
#   src/scripts/makelods.py --lib /usr/lib/libmeshoptimizer.so [--dry-run] [--error 0.01,0.04] [--no-lock] [--permissive] actors/drone misc/barbed_wire/2 ...
#
# Each level stops at the error limit, relative to the model's size, so a mesh which can't lose triangles cleanly gets fewer levels.
# Models that already have mdllod lines are left alone.

import argparse
import ctypes
import os
import re
import struct
import subprocess
import sys

import numpy as np

LEVELS = (  # folder, share of triangles to aim for, error limit, width on screen in pixels to switch at
    ('lod1', 0.5, 0.01, 192),
    ('lod2', 0.2, 0.04, 64),
)
MINKEEP = 0.8  # a level has to have at most this share of the previous one's triangles, or isn't worth having
NORMALWEIGHT = 0.5
UVWEIGHT = 1.0
LOCKBORDER = 1  # meshopt_SimplifyLockBorder, so separate shells and pipe ends keep their edges
PERMISSIVE = 1 << 5  # meshopt_SimplifyPermissive, collapsing across seams where normals or texture coordinates split
MESHEXT = ('.iqm', '.obj')
TEXEXT = ('', '.png', '.jpg', '.dds', '.tga')


class Simplifier:
    def __init__(self, path):
        self.lib = ctypes.CDLL(path)
        self.fn = self.lib.meshopt_simplifyWithAttributes
        self.fn.restype = ctypes.c_size_t
        self.options = LOCKBORDER

    def run(self, indices, pos, attrs, weights, target, maxerror):
        idx = indices.astype(np.uint32).ravel()
        out = np.zeros(len(idx), dtype=np.uint32)
        err = ctypes.c_float(0)
        pos = pos.astype(np.float32)
        attrs = attrs.astype(np.float32)
        wts = np.array(weights, dtype=np.float32)
        ptr = lambda a, t: a.ctypes.data_as(ctypes.POINTER(t))
        count = self.fn(ptr(out, ctypes.c_uint), ptr(idx, ctypes.c_uint), ctypes.c_size_t(len(idx)),
                        ptr(pos, ctypes.c_float), ctypes.c_size_t(len(pos)), ctypes.c_size_t(12),
                        ptr(attrs, ctypes.c_float), ctypes.c_size_t(attrs.shape[1]*4), ptr(wts, ctypes.c_float), ctypes.c_size_t(attrs.shape[1]),
                        None, ctypes.c_size_t(max(int(target)*3, 3)), ctypes.c_float(maxerror), ctypes.c_uint(self.options), ctypes.byref(err))
        return out[:count].reshape(-1, 3)


def simplifylevels(simp, submeshes, level):
    # submeshes: list of (triangles, pos, attrs), returns the triangles for each at the wanted share and limit
    _, share, limit, _ = level
    weights = [NORMALWEIGHT]*3 + [UVWEIGHT]*2
    return [simp.run(t, p, a, weights, len(t)*share, limit) if len(t) else t for t, p, a in submeshes]


class ObjMesh:
    def __init__(self, path):
        self.lines = []  # the file's other lines, with a marker where each run of faces goes
        self.v, self.vt, self.vn = [], [], []
        self.vextra = []
        self.groups = []
        self.names = []  # the name each run of faces is under, which configs refer to
        current = None
        name = ''
        for line in open(path, errors='surrogateescape'):
            tok = line.split()
            if not tok:
                continue
            if tok[0] == 'v':
                self.v.append([float(x) for x in tok[1:4]])
                self.vextra.append(' '.join(tok[4:]))  # vertex colours, which hold baked wind
            elif tok[0] == 'vt':
                self.vt.append([float(x) for x in tok[1:3]])
            elif tok[0] == 'vn':
                self.vn.append([float(x) for x in tok[1:4]])
            elif tok[0] == 'f':
                if current is None:
                    current = []
                    self.groups.append(current)
                    self.names.append(name)
                    self.lines.append(None)
                refs = [self.ref(x) for x in tok[1:]]
                for i in range(1, len(refs) - 1):  # the engine fans polygons the same way
                    current.append((refs[0], refs[i], refs[i + 1]))
            else:
                if tok[0] in ('o', 'g', 'usemtl'):
                    current = None
                if tok[0] in ('o', 'g'):
                    name = tok[1] if len(tok) > 1 else ''
                self.lines.append(line)

    def ref(self, text):
        out = []
        parts = text.split('/')
        for i, table in enumerate((self.v, self.vt, self.vn)):
            n = int(parts[i]) if i < len(parts) and parts[i] else 0
            out.append(n + len(table) if n < 0 else n - 1 if n > 0 else -1)
        return tuple(out)

    def submeshes(self):
        # a vertex for each distinct position, texture coordinate and normal, as the engine makes them
        self.refs = []
        out = []
        for group in self.groups:
            table = {}
            tris = []
            for tri in group:
                tris.append([table.setdefault(r, len(table)) for r in tri])
            refs = list(table)
            self.refs.append(refs)
            pos = np.array([self.v[r[0]] for r in refs], dtype=np.float32)
            attrs = np.array([(self.vn[r[2]] if r[2] >= 0 else [0, 0, 0]) + (self.vt[r[1]] if r[1] >= 0 else [0, 0]) for r in refs], dtype=np.float32)
            out.append((np.array(tris, dtype=np.uint32), pos, attrs))
        return out

    def write(self, path, tris):
        used = [{}, {}, {}]
        faces = []
        for refs, group in zip(self.refs, tris):
            rows = []
            for tri in group:
                row = []
                for k in tri:
                    r = refs[int(k)]
                    row.append('/'.join(str(used[i].setdefault(r[i], len(used[i])) + 1) if r[i] >= 0 else '' for i in range(3)).rstrip('/'))
                rows.append('f ' + ' '.join(row) + '\n')
            faces.append(rows)
        with open(path, 'w', errors='surrogateescape') as f:
            for r in used[0]:
                f.write('v %s\n' % ' '.join([repr(x) for x in self.v[r]] + ([self.vextra[r]] if self.vextra[r] else [])))
            for r in used[1]:
                f.write('vt %s %s\n' % tuple(repr(x) for x in self.vt[r]))
            for r in used[2]:
                f.write('vn %s %s %s\n' % tuple(repr(x) for x in self.vn[r]))
            run = iter(faces)
            pending = []  # a mesh left with no faces, such as a collision one, loses the lines naming it
            for line in self.lines:
                if line is None:
                    rows = next(run)
                    if rows:
                        f.writelines(pending)
                        f.writelines(rows)
                    pending = []
                elif not line.startswith('mtllib'):
                    pending.append(line)


class IqmMesh:
    HEADER = '<16s27I'
    FIELDS = ('version filesize flags num_text ofs_text num_meshes ofs_meshes num_vertexarrays num_vertexes ofs_vertexarrays '
              'num_triangles ofs_triangles ofs_adjacency num_joints ofs_joints num_poses ofs_poses num_anims ofs_anims '
              'num_frames num_framechannels ofs_frames ofs_bounds num_comment ofs_comment num_extensions ofs_extensions').split()
    FORMATSIZE = {0: 1, 1: 1, 2: 2, 3: 2, 4: 4, 5: 4, 6: 2, 7: 4, 8: 8}
    POSITION, TEXCOORD, NORMAL = 0, 1, 2

    def __init__(self, path):
        self.buf = open(path, 'rb').read()
        vals = struct.unpack_from(self.HEADER, self.buf)
        if vals[0] != b'INTERQUAKEMODEL\0' or vals[1] != 2:
            raise ValueError('not an IQM version 2 file')
        self.h = dict(zip(self.FIELDS, vals[1:]))
        h = self.h
        self.meshes = [list(struct.unpack_from('<6I', self.buf, h['ofs_meshes'] + i*24)) for i in range(h['num_meshes'])]
        self.arrays = [list(struct.unpack_from('<5I', self.buf, h['ofs_vertexarrays'] + i*20)) for i in range(h['num_vertexarrays'])]
        self.tris = np.frombuffer(self.buf, dtype='<u4', count=h['num_triangles']*3, offset=h['ofs_triangles']).reshape(-1, 3)
        text = self.buf[h['ofs_text']:h['ofs_text'] + h['num_text']]
        self.names = [text[m[0]:text.index(b'\0', m[0])].decode(errors='replace') for m in self.meshes]
        byattr = {a[0]: a for a in self.arrays}
        self.pos = self.array(byattr[self.POSITION])
        n = h['num_vertexes']
        norm = self.array(byattr[self.NORMAL]) if self.NORMAL in byattr else np.zeros((n, 3), dtype=np.float32)
        uv = self.array(byattr[self.TEXCOORD]) if self.TEXCOORD in byattr else np.zeros((n, 2), dtype=np.float32)
        self.attrs = np.hstack((norm, uv))

    def array(self, a):
        return np.frombuffer(self.buf, dtype='<f4', count=self.h['num_vertexes']*a[3], offset=a[4]).reshape(-1, a[3])

    def submeshes(self):
        return [(self.tris[m[4]:m[4] + m[5]], self.pos, self.attrs) for m in self.meshes]

    def write(self, path, tris):
        h = dict(self.h)
        keep, remap, meshes = [], {}, []
        newtris, first = [], 0
        for m, group in zip(self.meshes, tris):
            firstvert = len(keep)
            for v in sorted(set(int(x) for x in np.asarray(group).ravel())):
                remap[v] = len(keep)
                keep.append(v)
            mapped = [[remap[int(x)] for x in t] for t in group]
            newtris.extend(mapped)
            meshes.append([m[0], m[1], firstvert, len(keep) - firstvert, first, len(mapped)])
            first += len(mapped)
        keep = np.array(keep, dtype=np.int64)

        out = bytearray(struct.calcsize(self.HEADER))

        def section(data):
            ofs = len(out)
            out.extend(data)
            return ofs

        def copy(ofs, size):
            return section(self.buf[ofs:ofs + size]) if size else 0

        h['ofs_text'] = copy(self.h['ofs_text'], h['num_text'])
        h['ofs_meshes'] = section(b''.join(struct.pack('<6I', *m) for m in meshes))
        arrays, datas = [], []
        for a in self.arrays:
            size = self.FORMATSIZE[a[2]]*a[3]
            raw = np.frombuffer(self.buf, dtype=np.uint8, count=self.h['num_vertexes']*size, offset=a[4]).reshape(-1, size)
            datas.append(raw[keep].tobytes())
            arrays.append(list(a))
        h['ofs_vertexarrays'] = section(bytes(sum(20 for a in arrays)*[0]))
        for a, d in zip(arrays, datas):
            while len(out) % 4:
                out.append(0)
            a[4] = section(d)
        for i, a in enumerate(arrays):
            struct.pack_into('<5I', out, h['ofs_vertexarrays'] + i*20, *a)
        h['num_vertexes'] = len(keep)
        h['num_triangles'] = len(newtris)
        while len(out) % 4:
            out.append(0)
        h['ofs_triangles'] = section(np.array(newtris, dtype='<u4').tobytes())
        h['ofs_adjacency'] = 0
        h['ofs_joints'] = copy(self.h['ofs_joints'], h['num_joints']*48)
        h['ofs_poses'] = copy(self.h['ofs_poses'], h['num_poses']*88)
        h['ofs_anims'] = copy(self.h['ofs_anims'], h['num_anims']*32)
        while len(out) % 4:
            out.append(0)
        h['ofs_frames'] = copy(self.h['ofs_frames'], h['num_frames']*h['num_framechannels']*2)
        while len(out) % 4:
            out.append(0)
        h['ofs_bounds'] = copy(self.h['ofs_bounds'], h['num_frames']*32) if self.h['ofs_bounds'] else 0
        h['ofs_comment'] = copy(self.h['ofs_comment'], h['num_comment'])
        h['num_extensions'] = h['ofs_extensions'] = 0
        h['filesize'] = len(out)
        struct.pack_into(self.HEADER, out, 0, b'INTERQUAKEMODEL\0', *[h[k] for k in self.FIELDS])
        open(path, 'wb').write(out)


def findconfig(model):
    for name in ('iqm.cfg', 'obj.cfg'):
        path = os.path.join(model, name)
        if os.path.isfile(path):
            return path
    return None


def findmesh(model, cfgtext):
    # the mesh the config loads, without which the engine falls back to one in the folder
    found = re.findall(r'^\s*(?:iqm|obj)load\s+"?([^\s"]+)"?', cfgtext, re.M)
    for name in found:
        for ext in ('',) + MESHEXT:
            path = os.path.normpath(os.path.join(model, name + ext))
            if os.path.isfile(path) and path.endswith(MESHEXT):
                return name + ext if ext else name, path
    return None, None


def repathconfig(text, model):
    # a config is read from a folder deeper down, so paths to files beside the model go up one, and paths from the game's root stay
    def fix(token):
        quote = '"' if token.startswith('"') else ''
        body = token.strip('"')
        match = re.match(r'((?:<[^>]*>)*)(.*)', body)
        prefix, path = match.group(1), match.group(2)
        if not path or path == '*':
            return token
        if any(os.path.isfile(os.path.join(model, path + ext)) for ext in TEXEXT) or path.startswith('..'):
            return quote + prefix + '../' + path + quote
        return token

    out = []
    for line in text.splitlines():
        command = line.split()[0] if line.split() else ''
        if command == 'mdllod' or re.match(r'^(iqm|obj)(tricollide|load)$', command) or command in ('mdlcollide', 'mdlellipsecollide'):
            continue
        out.append(' '.join(fix(t) for t in re.findall(r'"[^"]*"|\S+', line)) if re.match(r'^(iqm|obj)', command) else line)
    return '\n'.join(out) + '\n'


def generated(path):
    # whether a mesh is an untracked one from an earlier run of this script, which may be made again
    folder, name = os.path.split(path)
    return subprocess.run(['git', '-C', folder, 'ls-files', '--error-unmatch', name], capture_output=True).returncode != 0


def makelods(simp, model, dryrun):
    cfgpath = findconfig(model)
    if not cfgpath:
        return 'no config'
    text = open(cfgpath, errors='surrogateescape').read()
    if re.search(r'^\s*mdllod\s', text, re.M):
        return 'already has levels'
    if re.search(r'^\s*(exec|if|alias)\b', text, re.M):
        return 'scripted config, skipped'
    name, path = findmesh(model, text)
    if not path:
        return 'no mesh found'
    mesh = IqmMesh(path) if path.endswith('.iqm') else ObjMesh(path)
    # meshes named by tricollide are only for physics, which reads the model itself, so levels go without them
    hidden = set(re.findall(r'^\s*(?:iqm|obj)tricollide\s+"?([^\s"]+)', text, re.M))
    submeshes = [(t[:0] if n in hidden else t, p, a) for n, (t, p, a) in zip(mesh.names, mesh.submeshes())]
    base = sum(len(t) for t, _, _ in submeshes)
    loader = 'iqmload' if path.endswith('.iqm') else 'objload'
    shared = '/' in name  # used by other models too, so its levels go beside it
    done, last, results = [], base, []
    for level in LEVELS:
        folder = level[0]
        stem, ext = os.path.splitext(name)
        authored = None
        if shared:  # a hand-made level beside a shared mesh is better than one made here, and is never replaced
            authored = os.path.normpath(os.path.join(model, '%s_%s%s' % (stem, folder, ext)))
            authored = authored if os.path.isfile(authored) and not generated(authored) else None
        if authored:
            tris = None
            count = sum(m[5] for m in IqmMesh(authored).meshes) if ext == '.iqm' else sum(len(g) for g in ObjMesh(authored).groups)
        else:
            tris = simplifylevels(simp, submeshes, level)
            count = sum(len(t) for t in tris)
            if count > last*MINKEEP or any(len(s[0]) and not len(t) for s, t in zip(submeshes, tris)):
                continue
        results.append((folder, count))
        last = count
        if dryrun:
            continue
        target = os.path.join(model, folder)
        os.makedirs(target, exist_ok=True)
        if shared:
            meshfile = '%s_%s%s' % (stem, folder, ext)
            outpath = os.path.normpath(os.path.join(model, meshfile))
            relname = '../' + meshfile
        else:
            relname = os.path.basename(name)
            outpath = os.path.join(target, relname)
        if tris is not None:
            mesh.write(outpath, tris)
        cfg = repathconfig(text, model)
        with open(os.path.join(target, os.path.basename(cfgpath)), 'w', errors='surrogateescape') as f:
            f.write('%s %s\n%s' % (loader, relname, cfg))
        done.append('mdllod %s %d\n' % (folder, level[3]))
    if done:
        with open(cfgpath, 'a', errors='surrogateescape') as f:
            f.write(('' if text.endswith('\n') else '\n') + ''.join(done))
    return '%d %s' % (base, ' '.join('%s=%d' % r for r in results) if results else 'no level worth having')


def main():
    parser = argparse.ArgumentParser(description='Give models simpler detail levels')
    parser.add_argument('--lib', required=True, help='path to the built meshoptimizer library')
    parser.add_argument('--root', default='data', help='folder models are named from')
    parser.add_argument('--dry-run', action='store_true', help='report the triangle counts without writing')
    parser.add_argument('--error', help='error limits for each level, for models that can\'t simplify within the usual ones')
    parser.add_argument('--no-lock', action='store_true', help='let open edges move, for models made of many open shells')
    parser.add_argument('--permissive', action='store_true', help='collapse across seams, for flat shaded models where every edge is one')
    parser.add_argument('models', nargs='+')
    args = parser.parse_args()
    simp = Simplifier(os.path.abspath(args.lib))
    if args.no_lock:
        simp.options = 0
    if args.permissive:
        simp.options |= PERMISSIVE
    if args.error:
        global LEVELS
        LEVELS = tuple((l[0], l[1], float(e), l[3]) for l, e in zip(LEVELS, args.error.split(',')))
    failed = 0
    for model in args.models:
        try:
            print(model, makelods(simp, os.path.join(args.root, model), args.dry_run))
        except (OSError, ValueError, IndexError, KeyError) as err:
            print(model, 'FAILED:', err)
            failed = 1
    sys.exit(failed)


if __name__ == '__main__':
    main()
