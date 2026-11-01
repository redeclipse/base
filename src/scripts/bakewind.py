#!/usr/bin/env python3
"""Bake the vertex colours the model wind shader reads into vegetation and cloth models.

Red drives the fast flutter, green is the phase and blue the slower sway, as in the q009 vegetation.
MD3 and MD5 can't hold colours, so those become OBJ and IQM, and the model's cfg is rewritten to match.

Usage: bakewind.py <plant|palm|hanging|ivy|banner|flag|painted> <strength> <model dir>...
A palm's fronds sway whole, with no fast flutter, as it jitters across them. Ivy is a curved sheet placed at any rotation, so where it touches its wall stays pinned, its bulge moves most, and it can't move past the side it curves towards. A painted model already has its colours, so only its cfg changes. A cloth has no height bend, so it stays pinned, and little lean, so it doesn't stretch.
"""
import math
import os
import re
import struct
import sys
import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import iqmpose

RMAX, BMAX = 0.8, 0.5
STILLNAMES = ('bark', 'trunk', 'cylinder')
CLOTHRIPPLE = 9.0 # the phase across a cloth's height, in radians
CLOTHLEAN = 0.15
FLUTTEREDGE = 6.0 # flutter fades on triangle edges longer than this in world units, as coarse leaves jitter
PHASESCALE = 123.0 # WIND_PHASE_SHIFT_SCALE


def phasehash(p):
    v = math.sin(p[0]*12.9898 + p[1]*78.233 + p[2]*37.719)*43758.5453
    return v - math.floor(v)


def islands(pos, tris):
    """A component index per vertex, joining vertices split only by their texture seams."""
    keys = {}
    parent = list(range(len(pos)))

    def find(a):
        while parent[a] != a:
            parent[a] = parent[parent[a]]
            a = parent[a]
        return a

    for i, p in enumerate(np.round(pos, 3)):
        k = tuple(p)
        if k in keys:
            parent[find(i)] = find(keys[k])
        else:
            keys[k] = i
    for t in tris:
        parent[find(int(t[1]))] = find(int(t[0]))
        parent[find(int(t[2]))] = find(int(t[0]))
    return np.array([find(i) for i in range(len(pos))])


def sheetback(pos):
    """The side a curved sheet wraps towards: its thinnest axis, signed towards its edges."""
    centered = pos - pos.mean(axis=0)
    axes = np.linalg.svd(centered, full_matrices=False)[2]
    back, depth = axes[2], centered @ axes[2]
    spread = np.linalg.norm(centered @ axes[:2].T, axis=1)
    edge = spread >= np.median(spread)
    return back if depth[edge].mean() > depth[~edge].mean() else -back


def stillmeshes(names):
    """The meshes of a flag that stay still, its pole, of which it needs one apart from its cloth."""
    still = [n for n in names if n.lower().startswith(STILLNAMES)]
    if not still or len(still) == len(names):
        raise SystemExit('a flag needs a pole mesh named %s and a cloth mesh, but has: %s' % ('/'.join(STILLNAMES), ', '.join(names)))
    return still


def paint(meshes, mode, gain=1.0, pinned=None):
    """Colours for [(name, positions, triangles)], in engine space where z is up. pinned masks vertices per mesh that a cloth hangs from.

    The shader's sway grows with mdlscale, so gain evens it out across plants of different scales.
    """
    if mode == 'flag':
        stillmeshes([m[0] for m in meshes])
    moving = [m[1] for m in meshes if len(meshes) == 1 or not m[0].lower().startswith(STILLNAMES)]
    every = np.concatenate(moving)
    zmin, zmax = every[:, 2].min(), every[:, 2].max()
    height = max(zmax - zmin, 1e-3)
    if mode == 'flag':
        stillpos = np.concatenate([m[1] for m in meshes if m[0].lower().startswith(STILLNAMES)])
        axis = stillpos[:, :2].mean(axis=0)
    else:
        allpos = np.concatenate([m[1] for m in meshes])
        low = allpos[allpos[:, 2] <= allpos[:, 2].min() + (allpos[:, 2].max() - allpos[:, 2].min())*0.05]
        axis = low[:, :2].mean(axis=0)
    reach = max(np.hypot(*(every[:, :2] - axis).T).max(), 1e-3)
    out = []
    for i, (name, pos, tris) in enumerate(meshes):
        col = np.zeros((len(pos), 3))
        out.append(col)
        if len(meshes) > 1 and name.lower().startswith(STILLNAMES):
            continue
        radial = np.hypot(*(pos[:, :2] - axis).T)
        if mode in ('banner', 'flag'):
            free = ~pinned[i] if pinned else np.ones(len(pos), bool)
            if mode == 'banner':
                top = pos[free, 2].max()
                along = np.clip((top - pos[:, 2])/max(np.ptp(pos[free, 2]), 1e-3), 0, 1)
            else:
                along = np.linalg.norm(pos[:, None] - stillpos[None], axis=2).min(axis=1)
                along /= max(along.max(), 1e-3)
            along[~free] = 0
            extent = (pos[free].max(axis=0) - pos[free].min(axis=0)).max()
            col[:, 0] = along*along*RMAX
            col[free, 1] = CLOTHRIPPLE/(PHASESCALE*max(extent, 1e-3))
            col[:, 2] = along
            continue
        if mode == 'ivy':
            back = sheetback(pos)
            depth = pos @ back
            along = (depth.max() - depth)/max(np.ptp(depth), 1e-3)
            extent = (pos.max(axis=0) - pos.min(axis=0)).max()
            col[:, 0] = along*along*RMAX
            col[:, 1] = CLOTHRIPPLE/(PHASESCALE*max(extent, 1e-3))
            col[:, 2] = along
            continue
        edges = np.linalg.norm(pos[tris] - pos[np.roll(tris, 1, axis=1)], axis=2).mean()/gain
        flutter = 0.0 if mode == 'palm' else min(FLUTTEREDGE/max(edges, 1e-3), 1.0)
        comp = islands(pos, tris)
        for c in np.unique(comp):
            idx = np.nonzero(comp == c)[0]
            if mode in ('plant', 'palm') and np.ptp(pos[idx, 2]) >= height*0.5 and np.ptp(radial[idx]) < reach*0.35:
                continue # a trunk sharing its mesh with the leaves
            anchor = idx[np.argmax(pos[idx, 2])] if mode == 'hanging' else idx[np.argmin(radial[idx])]
            dist = np.linalg.norm(pos[idx] - pos[anchor], axis=1)
            col[idx, 0] = dist/max(dist.max(), 1e-3)*RMAX*flutter
            col[idx, 1] = phasehash(pos[anchor])
            col[idx, 2] = np.minimum(((zmax - pos[idx, 2])/height if mode == 'hanging' else radial[idx]/reach)*BMAX*gain, 1)
    return out


def read_md3(path):
    buf = open(path, 'rb').read()
    assert buf[:4] == b'IDP3'
    assert struct.unpack('<i', buf[76:80])[0] == 1, path + ' is animated'
    nummeshes = struct.unpack('<i', buf[84:88])[0]
    o = struct.unpack('<i', buf[100:104])[0]
    meshes = []
    for _ in range(nummeshes):
        name = buf[o+4:o+68].split(b'\0')[0].decode()
        nv, nt = struct.unpack('<ii', buf[o+80:o+88])
        otri, _, ouv, overt, size = struct.unpack('<5i', buf[o+88:o+108])
        tris = np.frombuffer(buf, '<i4', nt*3, o+otri).reshape(-1, 3)
        uv = np.frombuffer(buf, '<f4', nv*2, o+ouv).reshape(-1, 2)
        raw = np.frombuffer(buf, '<i2', nv*4, o+overt).reshape(-1, 4)
        pos = raw[:, :3]/64.0*np.array([1, -1, 1])
        lng = (raw[:, 3] & 0xFF)*2*np.pi/255
        lat = ((raw[:, 3] >> 8) & 0xFF)*2*np.pi/255
        norm = np.stack([np.cos(lat)*np.sin(lng), -np.sin(lat)*np.sin(lng), np.cos(lng)], axis=1)
        meshes.append((name, pos, tris, uv, norm))
        o += size
    return meshes


def md3toobj(src, dst, mode, gain):
    """The OBJ loader maps (x, y, z) to (z, -x, y) and reverses each face."""
    meshes = read_md3(src)
    cols = paint([m[:3] for m in meshes], mode, gain)
    lines, base = [], 1
    for (name, pos, tris, uv, norm), col in zip(meshes, cols):
        lines.append('g %s' % name)
        for p, c in zip(pos, col):
            lines.append('v %.6f %.6f %.6f %.6f %.6f %.6f' % (-p[1], p[2], p[0], *c))
        lines.extend('vt %.6f %.6f' % (t[0], 1 - t[1]) for t in uv)
        lines.extend('vn %.6f %.6f %.6f' % (-n[1], n[2], n[0]) for n in norm)
        for t in tris:
            lines.append('f ' + ' '.join('%d/%d/%d' % ((int(t[k]) + base,)*3) for k in (2, 1, 0)))
        base += len(pos)
    open(dst, 'w').write('\n'.join(lines) + '\n')


def paintobj(path, mode, gain):
    lines = open(path).read().splitlines()
    verts = [i for i, l in enumerate(lines) if l.startswith('v ') or l.startswith('v\t')]
    allpos = np.array([list(map(float, lines[i].split()[1:4])) for i in verts])
    allpos = np.stack([allpos[:, 2], -allpos[:, 0], allpos[:, 1]], axis=1)
    groups, name = {}, ''
    for l in lines:
        if l.startswith('g '):
            name = l[2:].strip()
        elif l.startswith('f '):
            face = []
            for tok in l.split()[1:]:
                n = int(tok.split('/')[0])
                face.append(n - 1 if n > 0 else len(verts) + n)
            for k in range(1, len(face) - 1):
                groups.setdefault(name, []).append((face[0], face[k], face[k+1]))
    colours = np.zeros((len(verts), 3))
    meshes = []
    remaps = []
    for name, tris in groups.items():
        tris = np.array(tris)
        used = np.unique(tris)
        remap = {int(v): i for i, v in enumerate(used)}
        meshes.append((name, allpos[used], np.vectorize(remap.get)(tris)))
        remaps.append(used)
    for used, col in zip(remaps, paint(meshes, mode, gain)):
        colours[used] = col
    for i, c in zip(verts, colours):
        lines[i] = 'v %s %.6f %.6f %.6f' % (' '.join(lines[i].split()[1:4]), *c)
    open(path, 'w').write('\n'.join(lines) + '\n')


def restorew(x, y, z):
    return (x, y, z, -math.sqrt(max(0.0, 1 - x*x - y*y - z*z)))


def read_md5mesh(path):
    text = open(path).read()
    joints = []
    block = re.search(r'joints \{(.*?)\n\}', text, re.S).group(1)
    pat = r'"?([^"\s]+)"?\s+(-?\d+) \( (\S+) (\S+) (\S+) \) \( (\S+) (\S+) (\S+) \)'
    for m in re.finditer(pat, block):
        v = list(map(float, m.groups()[2:]))
        joints.append((m.group(1), int(m.group(2)), np.array(v[:3]), np.array(restorew(*v[3:]))))
    meshes = []
    for chunk in re.findall(r'mesh \{(.*?)\n\}', text, re.S):
        name = re.search(r'// meshes: *(\S+)', chunk).group(1)
        verts = re.findall(r'vert \d+ \( (\S+) (\S+) \) (\d+) (\d+)', chunk)
        tris = np.array(re.findall(r'tri \d+ (\d+) (\d+) (\d+)', chunk), dtype=int)
        weights = [(int(j), float(b), np.array([float(x), float(y), float(z)])) for j, b, x, y, z in
                   re.findall(r'weight \d+ (\d+) (\S+) \( (\S+) (\S+) (\S+) \)', chunk)]
        meshes.append((name, verts, tris, weights))
    return joints, meshes


def writeiqm(path, joints, meshes):
    """[(name, parent, pos, quat)] and [(name, pos, uv, norm, index, weight, colour, tris)], all in file space."""
    text = b'\0'

    def intern(s):
        nonlocal text
        o = len(text)
        text += s.encode() + b'\0'
        return o

    jointdata, meshdata = b'', b''
    for name, parent, pos, q in joints:
        jointdata += struct.pack('<Ii10f', intern(name), parent, *pos, *q, 1, 1, 1)
    fv = ft = 0
    arrays = [[] for _ in range(6)]
    tridata = b''
    for name, pos, uv, norm, index, weight, colour, tris in meshes:
        meshdata += struct.pack('<6I', intern(name), 0, fv, len(pos), ft, len(tris))
        for a, v in zip(arrays, (pos, uv, norm, index, weight, colour)):
            a.append(v)
        tridata += (tris + fv).astype('<u4').tobytes()
        fv += len(pos)
        ft += len(tris)
    while len(text) % 4:
        text += b'\0'
    types = [(0, 7, 3, '<f4'), (1, 7, 2, '<f4'), (2, 7, 3, '<f4'), (4, 1, 4, 'u1'), (5, 1, 4, 'u1'), (6, 1, 4, 'u1')]
    body = text + meshdata
    ofs_text, ofs_meshes = 124, 124 + len(text)
    ofs_arrays = 124 + len(body)
    cursor = ofs_arrays + 20*len(types)
    table, blobs = b'', b''
    for (kind, fmt, size, dtype), a in zip(types, arrays):
        blob = np.concatenate(a).astype(dtype).tobytes()
        table += struct.pack('<5I', kind, 0, fmt, size, cursor)
        blobs += blob
        cursor += len(blob)
    pad = -cursor % 4
    blobs += b'\0'*pad
    cursor += pad
    ofs_tris = cursor
    ofs_joints = ofs_tris + len(tridata)
    h = dict.fromkeys(iqmpose.HNAMES, 0)
    h.update(version=2, filesize=ofs_joints + len(jointdata), num_text=len(text), ofs_text=ofs_text, num_meshes=len(meshes),
             ofs_meshes=ofs_meshes, num_vertexarrays=len(types), num_vertexes=fv, ofs_vertexarrays=ofs_arrays,
             num_triangles=ft, ofs_triangles=ofs_tris, num_joints=len(joints), ofs_joints=ofs_joints)
    out = struct.pack(iqmpose.HDR, b'INTERQUAKEMODEL\0', *[h[k] for k in iqmpose.HNAMES])
    open(path, 'wb').write(out + body + table + blobs + tridata + jointdata)


def md5meshtoiqm(src, dst, mode, gain):
    joints, meshes = read_md5mesh(src)
    local = []
    for name, parent, pos, q in joints:
        if parent < 0:
            local.append((name, parent, pos, q))
        else:
            pq = iqmpose.qconj(joints[parent][3])
            local.append((name, parent, iqmpose.qrot(pq, pos - joints[parent][2]), iqmpose.qmul(pq, q)))
    built, pins = [], []
    for name, verts, tris, weights in meshes:
        n = len(verts)
        pos, uv = np.zeros((n, 3)), np.zeros((n, 2))
        index, weight = np.zeros((n, 4), int), np.zeros((n, 4))
        for i, (u, v, start, count) in enumerate(verts):
            uv[i] = float(u), float(v)
            used = sorted(weights[int(start):int(start)+int(count)], key=lambda w: -w[1])
            for j, b, p in used:
                pos[i] += (iqmpose.qrot(joints[j][3], p) + joints[j][2])*b
            for k, (j, b, p) in enumerate(used[:4]):
                index[i, k], weight[i, k] = j, b
        total = np.round(weight/weight.sum(axis=1, keepdims=True)*255).astype(int)
        total[:, 0] += 255 - total.sum(axis=1)
        engine = pos*np.array([1, -1, 1])
        norm = np.zeros((n, 3))
        for t in tris:
            face = np.cross(engine[t[1]] - engine[t[0]], engine[t[2]] - engine[t[0]])
            norm[t] += face
        norm /= np.maximum(np.linalg.norm(norm, axis=1, keepdims=True), 1e-9)
        built.append([name, pos, uv, norm*np.array([1, -1, 1]), index, total, None, tris])
        rigid = [i for i, j in enumerate(joints) if j[1] < 0 and j[0] == 'base']
        pins.append(np.isin(index[:, 0], rigid) & (total[:, 0] == 255))
    painted = paint([(m[0], m[1]*np.array([1, -1, 1]), m[7]) for m in built], mode, gain, pins)
    for m, c in zip(built, painted):
        m[6] = np.round(np.concatenate([c, np.ones((len(c), 1))], axis=1)*255).astype(int)
    writeiqm(dst, local, built)


def md5animtoiqm(src, dst):
    text = open(src).read()
    hier = re.findall(r'"?([^"\s]+)"?\s+(-?\d+) (\d+) (\d+)', re.search(r'hierarchy \{(.*?)\n\}', text, re.S).group(1))
    base = re.findall(r'\( (\S+) (\S+) (\S+) \) \( (\S+) (\S+) (\S+) \)', re.search(r'baseframe \{(.*?)\n\}', text, re.S).group(1))
    parents = [int(h[1]) for h in hier]
    frames = []
    for chunk in re.findall(r'frame \d+ \{(.*?)\n\}', text, re.S):
        data = list(map(float, chunk.split()))
        frame = []
        for (name, parent, flags, start), b in zip(hier, base):
            v, flags, start = list(map(float, b)), int(flags), int(start)
            for bit in range(6):
                if flags & (1 << bit):
                    v[bit] = data[start]
                    start += 1
            frame.append(np.array(v[:3] + list(restorew(*v[3:])) + [1, 1, 1]))
        frames.append(frame)
    iqmpose.save_anim(dst, os.path.basename(dst), parents, np.array(frames))


def paintiqm(path, mode, gain):
    """Replace an IQM's colour array, or append one if it has none."""
    buf = open(path, 'rb').read()
    h = iqmpose.header(buf)
    table = buf[h['ofs_vertexarrays']:h['ofs_vertexarrays'] + 20*h['num_vertexarrays']]
    posofs = colofs = None
    for i in range(h['num_vertexarrays']):
        kind, _, _, _, ofs = struct.unpack('<5I', table[i*20:i*20+20])
        if kind == 0:
            posofs = ofs
        elif kind == 6:
            colofs = ofs
    pos = np.frombuffer(buf, '<f4', h['num_vertexes']*3, posofs).reshape(-1, 3)*np.array([1, -1, 1])
    tris = np.frombuffer(buf, '<u4', h['num_triangles']*3, h['ofs_triangles']).reshape(-1, 3)
    meshes = []
    for i in range(h['num_meshes']):
        name, _, fv, nv, ft, nt = struct.unpack('<6I', buf[h['ofs_meshes'] + i*24:h['ofs_meshes'] + i*24 + 24])
        meshes.append((iqmpose.text(buf, h, name), pos[fv:fv+nv], tris[ft:ft+nt].astype(int) - fv))
    colours = np.round(np.concatenate(paint(meshes, mode, gain))*255).astype(int)
    blob = np.concatenate([colours, np.full((len(colours), 1), 255)], axis=1).astype('u1').tobytes()
    out = bytearray(buf)
    if colofs is not None:
        out[colofs:colofs + len(blob)] = blob
        open(path, 'wb').write(out)
        return
    out += b'\0'*(-len(out) % 4)
    dataofs = len(out)
    out += blob
    out += b'\0'*(-len(out) % 4)
    newtable = len(out)
    out += table + struct.pack('<5I', 6, 0, 1, 4, dataofs)
    h['num_vertexarrays'] += 1
    h['ofs_vertexarrays'] = newtable
    h['filesize'] = len(out)
    out[:124] = struct.pack(iqmpose.HDR, b'INTERQUAKEMODEL\0', *[h[k] for k in iqmpose.HNAMES])
    open(path, 'wb').write(out)


def readmodel(path):
    """The mesh names and every position, in engine space, of an OBJ or IQM."""
    if path.endswith('.iqm'):
        buf = open(path, 'rb').read()
        h = iqmpose.header(buf)
        posofs = None
        for i in range(h['num_vertexarrays']):
            kind, _, _, _, ofs = struct.unpack('<5I', buf[h['ofs_vertexarrays'] + i*20:h['ofs_vertexarrays'] + i*20 + 20])
            if kind == 0:
                posofs = ofs
        names = [iqmpose.text(buf, h, struct.unpack('<I', buf[h['ofs_meshes'] + i*24:h['ofs_meshes'] + i*24 + 4])[0]) for i in range(h['num_meshes'])]
        return names, np.frombuffer(buf, '<f4', h['num_vertexes']*3, posofs).reshape(-1, 3)*np.array([1, -1, 1])
    lines = open(path).read().splitlines()
    names = [l[2:].strip() for l in lines if l.startswith('g ')]
    verts = np.array([list(map(float, l.split()[1:4])) for l in lines if l.startswith('v ') or l.startswith('v\t')])
    return names, np.stack([verts[:, 2], -verts[:, 0], verts[:, 1]], axis=1)


def addwind(text, fmt, strength, mode, path):
    extra = ''
    if mode == 'ivy':
        text = re.sub(r'^(mdlwind|mdlwindback|%swind) .*\n?' % fmt, '', text, flags=re.M)
        back = sheetback(readmodel(path)[1])
        return text.rstrip('\n') + '\nmdlwind %s\n%swind * %s 0 %s\nmdlwindback %.3f %.3f %.3f\n' % (strength, fmt, strength, CLOTHLEAN, *back)
    if 'mdlwind ' not in text:
        extra += 'mdlwind %s\n' % strength
    if mode in ('banner', 'flag') and fmt + 'wind ' not in text:
        extra += '%swind * %s 0 %s\n' % (fmt, strength, CLOTHLEAN)
        if mode == 'flag':
            extra += ''.join('%swind %s 0\n' % (fmt, name) for name in stillmeshes(readmodel(path)[0]))
        else:
            extra += 'mdlwindback -1 0 0\n'
    return text.rstrip('\n') + '\n' + extra


def bake(mode, strength, mdl):
    for fmt in ('md3', 'md5', 'obj', 'iqm'):
        cfg = os.path.join(mdl, fmt + '.cfg')
        if os.path.exists(cfg):
            break
    else:
        raise SystemExit('no cfg in ' + mdl)
    text = open(cfg).read()
    load = re.search(r'%sload\s+"?([^"\s]+)"?' % fmt, text).group(1)
    src = os.path.normpath(os.path.join(mdl, load))
    scale = re.search(r'mdlscale\s+([\d.]+)', text)
    gain = 100/float(scale.group(1)) if scale else 1.0
    path, replaced = src, []
    if mode == 'painted':
        pass
    elif fmt == 'md3':
        rel = load.replace('tris.md3', 'mesh.obj').replace('.md3', '.obj')
        dst = path = os.path.normpath(os.path.join(mdl, rel))
        if not os.path.exists(dst):
            md3toobj(src, dst, mode, gain)
        replaced.append(src)
        text = re.sub(r'\bmd3(\w+)', r'obj\1', text.replace(load, rel))
    elif fmt == 'md5':
        rel = load.replace('.md5mesh', '.iqm')
        dst = path = os.path.normpath(os.path.join(mdl, rel))
        if not os.path.exists(dst):
            md5meshtoiqm(src, dst, mode, gain)
        replaced.append(src)
        for anim in re.findall(r'"?([^"\s]+\.md5anim)"?', text):
            arel = anim.replace('.md5anim', '.iqm')
            if os.path.normpath(os.path.join(mdl, arel)) == dst:
                arel = anim.replace('.md5anim', 'anim.iqm')
            asrc, adst = os.path.normpath(os.path.join(mdl, anim)), os.path.normpath(os.path.join(mdl, arel))
            if not os.path.exists(adst):
                md5animtoiqm(asrc, adst)
            replaced.append(asrc)
            text = text.replace(anim, arel)
        text = re.sub(r'\bmd5(\w+)', r'iqm\1', text.replace(load, rel))
    elif fmt == 'obj':
        paintobj(src, mode, gain)
    else:
        paintiqm(src, mode, gain)
    newfmt = {'md3': 'obj', 'md5': 'iqm'}.get(fmt, fmt)
    newcfg = os.path.join(mdl, newfmt + '.cfg')
    text = addwind(text, newfmt, strength, mode, path)
    open(newcfg, 'w').write(text)
    if newcfg != cfg:
        os.remove(cfg)
    for old in replaced:
        if os.path.exists(old):
            os.remove(old)


if __name__ == '__main__':
    if len(sys.argv) < 4:
        raise SystemExit(__doc__)
    for model in sys.argv[3:]:
        bake(sys.argv[1], sys.argv[2], model)
        print('baked', model)
