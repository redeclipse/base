#!/usr/bin/env python3
"""Bake the player mixers, RGBA textures fitted to the player model's UVs, needs numpy, scipy and PIL.

Each texel of a mixer is found on the bind pose model, so a design is drawn on the body rather than the texture.
The first person hands are a separate mesh, their texels are moved onto the body through the matching bones, so
both views show the same design. Hand made mixers, painted onto the body's UVs, are kept in mixers/ beside this.

It also gives the player's pattern textures the team colour over the whole of each light, so whatever a mixer puts
around them the lights show the team. Name "pattern" to only do that.

usage: src/scripts/makemixers.py data/actors/player data/mixers [pattern] [mixer...]
"""
import os
import struct
import sys
import numpy as np
from PIL import Image
from scipy import ndimage, spatial
from iqmpose import header, text, load_joints, qmul, qrot

SUPER = 2  # supersampling per axis, edges come out antialiased
SOURCES = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'mixers')  # hand made mixers, drawn on the body's UVs


def norm(v):
    v = np.asarray(v, float)
    return v / np.linalg.norm(v, axis=-1, keepdims=True)


class Mesh:
    """Bind pose vertices and joints of a model."""

    def __init__(self, path):
        buf = open(path, 'rb').read()
        h = header(buf)
        nv = h['num_vertexes']
        arrays = {}
        for i in range(h['num_vertexarrays']):
            kind, flags, fmt, size, ofs = struct.unpack_from('<5I', buf, h['ofs_vertexarrays'] + i*20)
            dtype = np.float32 if fmt == 7 else np.uint8
            arrays[kind] = np.frombuffer(buf, dtype, nv*size, ofs).reshape(nv, size).astype(float)
        self.pos, self.uv, self.nrm = arrays[0], arrays[1], arrays[2]
        self.tris = np.frombuffer(buf, np.uint32, h['num_triangles']*3, h['ofs_triangles']).reshape(-1, 3)
        idx, wgt = arrays[4].astype(int), arrays[5]
        self.bone = idx[np.arange(nv), wgt.argmax(axis=1)]
        self.joints = load_joints(path)
        self.names = [j[0] for j in self.joints]
        self.jpos, self.jrot = [], []
        for name, parent, vals in self.joints:
            t, q = vals[0:3], vals[3:7] / np.linalg.norm(vals[3:7])
            if parent >= 0:
                t = self.jpos[parent] + qrot(self.jrot[parent], t)
                q = qmul(self.jrot[parent], q)
            self.jpos.append(t)
            self.jrot.append(q)
        self.jpos = np.array(self.jpos)

    def joint(self, name):
        return self.jpos[self.names.index(name)]


def rasterise(mesh, size):
    """The triangle and barycentric weights covering each texel, -1 where nothing does."""
    tri = np.full((size, size), -1, np.int32)
    bary = np.zeros((size, size, 3), np.float32)
    uv = mesh.uv * size - 0.5
    for t, (a, b, c) in enumerate(mesh.tris):
        pa, pb, pc = uv[a], uv[b], uv[c]
        x0, y0 = np.floor(np.minimum(np.minimum(pa, pb), pc)).astype(int)
        x1, y1 = np.ceil(np.maximum(np.maximum(pa, pb), pc)).astype(int)
        x0, y0, x1, y1 = max(x0, 0), max(y0, 0), min(x1, size - 1), min(y1, size - 1)
        if x1 < x0 or y1 < y0:
            continue
        den = (pb[1] - pc[1])*(pa[0] - pc[0]) + (pc[0] - pb[0])*(pa[1] - pc[1])
        if abs(den) < 1e-12:
            continue
        ys, xs = np.mgrid[y0:y1 + 1, x0:x1 + 1]
        w0 = ((pb[1] - pc[1])*(xs - pc[0]) + (pc[0] - pb[0])*(ys - pc[1])) / den
        w1 = ((pc[1] - pa[1])*(xs - pc[0]) + (pa[0] - pc[0])*(ys - pc[1])) / den
        w2 = 1 - w0 - w1
        inside = (w0 >= -1e-4) & (w1 >= -1e-4) & (w2 >= -1e-4)
        tri[ys[inside], xs[inside]] = t
        bary[ys[inside], xs[inside]] = np.stack([w0, w1, w2], -1)[inside]
    return tri, bary


class Bake:
    """Per texel attributes of a mesh: where it is on the body, its normal and its bone."""

    def __init__(self, mesh, size, place=None):
        self.size = size
        tri, bary = rasterise(mesh, size)
        self.mask = tri >= 0
        corners = mesh.tris[tri[self.mask]]
        b = bary[self.mask][..., None]
        self.pos = (mesh.pos[corners] * b).sum(1)
        self.nrm = norm((mesh.nrm[corners] * b).sum(1))
        nearest = corners[np.arange(len(corners)), bary[self.mask].argmax(1)]
        self.bone = np.array(mesh.names)[mesh.bone[nearest]]
        if place:
            self.pos, self.nrm, self.bone = place(self.pos, self.nrm, self.bone)
        # limbs are mirrored in the UVs, either side can land on a texel, so they're all kept on the left
        self.region = np.array([region(n) for n in self.bone])
        limb = ~np.isin(self.region, ('head', 'torso', 'pelvis'))
        flip = limb & (self.pos[:, 0] < 0)
        self.pos[flip, 0] *= -1
        self.nrm[flip, 0] *= -1
        self.bone = np.where(limb, np.char.replace(self.bone, '.R', '.L'), self.bone)

    def sample(self, img):
        """Nearest texel of an image for each texel of the bake."""
        ys, xs = np.nonzero(self.mask)
        h, w = img.shape[:2]
        return img[(ys * h) // self.size, (xs * w) // self.size]

    def image(self, values, fill=0.0):
        out = np.full((self.size, self.size) + values.shape[1:], fill, np.float32)
        out[self.mask] = values
        return out


REGIONS = [('head', ('head', 'neck', 'tag_head')), ('pelvis', ('hips',)), ('torso', ('spine', 'ribs', 'shoulder')),
           ('upperarm', ('upper_arm',)), ('forearm', ('forearm',)), ('hand', ('hand', 'palm', 'finger', 'thumb')),
           ('thigh', ('thigh',)), ('shin', ('shin',)), ('foot', ('foot', 'toe'))]


def region(bone):
    for name, prefixes in REGIONS:
        if bone.startswith(prefixes):
            return name
    return 'torso'


def side(bone):
    return 1 if '.L' in bone else -1 if '.R' in bone else 0


# hands bone: (body bone, hands segment start, hands segment end, body segment start, body segment end)
def handsegments(s, b):
    segs = {
        f'Upperarm.{s}': (f'upper_arm.{b}.01', f'Upperarm.{s}', f'Forearm.{s}', f'upper_arm.{b}.01', f'forearm.{b}.01'),
        f'Forearm.{s}': (f'forearm.{b}.01', f'Forearm.{s}', f'Hand.{s}', f'forearm.{b}.01', f'hand.{b}'),
        f'Hand.{s}': (f'hand.{b}', f'Hand.{s}', f'FingersB1.{s}', f'hand.{b}', f'finger_middle.01.{b}.01'),
    }
    fingers = {'FingerA': 'finger_index', 'FingersA': 'finger_index', 'FingersB': 'finger_middle', 'FingersC': 'finger_ring',
               'Thumb': 'thumb'}
    for hand, body in fingers.items():
        root = 'thumb.01' if body == 'thumb' else f'{body}.01'
        for k in (1, 2, 3):
            segs[f'{hand}{k}.{s}'] = (f'{body}.03.{b}', f'{hand}1.{s}', f'{hand}3.{s}', f'{root}.{b}.01', f'{body}.03.{b}')
    return segs


def frames(mesh, segs, side, armbones):
    """Segment axis, a perpendicular reference and the mean radius of its vertices, alike in both skeletons."""
    upper, fore, hand = [mesh.joint(n) for n in armbones]
    bend = norm(np.cross(fore - upper, hand - fore))
    out = {}
    for key, (start, end) in segs.items():
        a, e = mesh.joint(start), mesh.joint(end)
        length = np.linalg.norm(e - a)
        d = (e - a) / length
        e1 = norm(bend - d*np.dot(bend, d))
        e2 = np.cross(d, e1)
        out[key] = (a, d, length, e1, e2)
    return out


def placehands(hands, body):
    """Moves texels of the first person hands onto the body through the matching bones."""
    table = {}
    for s, b in (('Left', 'L'), ('Right', 'R')):
        segs = {k: v for k, v in handsegments(s, b).items() if v[1] in hands.names and v[2] in hands.names}
        hf = frames(hands, {k: v[1:3] for k, v in segs.items()}, s, (f'Upperarm.{s}', f'Forearm.{s}', f'Hand.{s}'))
        bf = frames(body, {k: v[3:5] for k, v in segs.items()}, b, (f'upper_arm.{b}.01', f'forearm.{b}.01', f'hand.{b}'))
        # the elbow's bend sets the roll of the arms, the thumb then turns the forearm and hand to match
        def angle(mesh, frame, joint):
            a, d, length, e1, e2 = frame
            r = mesh.joint(joint) - a
            return np.arctan2(r @ e2, r @ e1)
        roll = angle(hands, hf[f'Hand.{s}'], f'Thumb1.{s}') - angle(body, bf[f'Hand.{s}'], f'thumb.01.{b}.01')
        c, n = np.cos(roll), np.sin(roll)
        for k, v in segs.items():
            a, d, length, e1, e2 = hf[k]
            if not k.startswith('Upperarm'):
                e1, e2 = c*e1 + n*e2, c*e2 - n*e1
            table[k] = (v[0], (a, d, length, e1, e2), bf[k])
    # mean radius of each segment's vertices, so a thicker arm in one model still lines up
    def radius(mesh, frame, bones):
        a, d, length, e1, e2 = frame
        sel = np.isin(np.array(mesh.names)[mesh.bone], bones)
        if not sel.any():
            return 1.0
        r = mesh.pos[sel] - a
        r = r - np.outer(r @ d, d)
        return np.linalg.norm(r, axis=1).mean() / length

    hnames = np.array(hands.names)
    scale = {}
    for k, (bbone, hframe, bframe) in table.items():
        scale[k] = radius(body, bframe, [bbone]) / max(radius(hands, hframe, [k]), 1e-6)

    def place(pos, nrm, bone):
        opos, onrm, obone = pos.copy(), nrm.copy(), bone.astype(object)
        for k, (bbone, (ha, hd, hl, he1, he2), (ba, bd, bl, be1, be2)) in table.items():
            sel = bone == k
            if not sel.any():
                continue
            r = pos[sel] - ha
            t, x, y = r @ hd / hl, r @ he1 / hl * scale[k], r @ he2 / hl * scale[k]
            opos[sel] = ba + bl*(np.outer(t, bd) + np.outer(x, be1) + np.outer(y, be2))
            n = nrm[sel]
            onrm[sel] = norm(np.outer(n @ hd, bd) + np.outer(n @ he1, be1) + np.outer(n @ he2, be2))
            obone[sel] = bbone
        return opos, onrm, obone.astype(str)

    return place


def encode(w):
    """Weights of body, accent, team and weapon colours into mixer channels.

    The model shader lays each channel over the last in that order, a channel of 0 covers what's under it, so each
    layer takes its share of what the layers above it left uncovered.
    """
    out = np.ones(w.shape, np.float32)
    left = np.ones(w.shape[:-1], np.float32)
    for k in (3, 2, 1):
        out[..., k] = 1 - np.clip(w[..., k] / np.maximum(left, 1e-6), 0, 1)
        left = np.maximum(left - w[..., k], 0)
    out[..., 0] = 0
    return out


def decode(c):
    """Mixer channels back into weights, undoing encode."""
    w = np.zeros(c.shape, np.float32)
    left = np.ones(c.shape[:-1], np.float32)
    for k in (3, 2, 1, 0):
        w[..., k] = left * (1 - c[..., k])
        left = left * c[..., k]
    w[..., 0] += left  # nothing painted is bare armour, closest to the body colour
    return w


def resolve(bake, weights, size):
    """Supersampled weights down to the texture size, with the gaps filled from the nearest island for mipmaps."""
    img = bake.image(weights)
    cover = bake.image(np.ones(len(weights), np.float32))
    img = img.reshape(size, SUPER, size, SUPER, 4).sum((1, 3))
    cover = cover.reshape(size, SUPER, size, SUPER).sum((1, 3))
    inside = cover > 0
    img[inside] /= cover[inside][:, None]
    _, (iy, ix) = ndimage.distance_transform_edt(~inside, return_indices=True)
    return img[iy, ix]


def save(path, weights):
    Image.fromarray((encode(weights) * 255 + 0.5).astype(np.uint8), 'RGBA').save(path, optimize=True)


# designs return weights per texel of the mixer's channels, the player's body, accent, team and weapon colours
BODY, ACCENT, TEAM, WEAPON = np.eye(4, dtype=np.float32)


def pick(*layers):
    """Weights from stacked masks, each (mask, colour) covering the ones before it, a colour of None takes weights."""
    w = None
    for mask, colour in layers:
        if colour is None:  # weights from another design to build on
            w = np.asarray(mask, np.float32)
            continue
        m = np.asarray(mask, np.float32)[:, None]
        w = colour * m if w is None else w * (1 - m) + colour * m
    return w


def smooth(d, width):
    """A mask from a signed distance, 1 inside, softened over width so edges stay clean once supersampled."""
    return np.clip(0.5 - d / max(width, 1e-6), 0, 1)


# limbs as chains of joints on the left side, with the direction their angle is measured from
LIMBS = {
    'arm': (('upper_arm.L.01', 'forearm.L.01', 'hand.L', 'finger_middle.03.L'), (0, 0, 1.0)),
    'leg': (('thigh.L.01', 'shin.L.01', 'foot.L', 'toe.L'), (1.0, 0, 0)),
}
ARMS, LEGS = ('upperarm', 'forearm', 'hand'), ('thigh', 'shin', 'foot')
TORSO = (0, 0.4)  # the vertical axis the torso and head wrap around


class Shape:
    """Places on the body a design is drawn with, worked out from a bake.

    t runs along a limb from 0 at its root, in body units, phi is the angle around it, 0 at the top of an arm or the
    outside of a leg. theta is the angle around the torso, 0 at the front, and around runs the same way in units.
    """

    def __init__(self, bake, mesh):
        self.bake, self.pos, self.nrm, self.region = bake, bake.pos, bake.nrm, bake.region
        self.n = len(bake.pos)
        self.x, self.y, self.z = bake.pos.T
        self.ax = np.abs(self.x)
        self.arm, self.leg = np.isin(self.region, ARMS), np.isin(self.region, LEGS)
        self.limb = self.arm | self.leg
        self.core = ~self.limb
        self.t = np.zeros(self.n)
        self.phi = np.zeros(self.n)
        self.radius = np.zeros(self.n)
        for name, sel in (('arm', self.arm), ('leg', self.leg)):
            chain, ref = LIMBS[name]
            pts = np.array([mesh.joint(j) for j in chain])
            p = self.pos[sel]
            best = np.full(len(p), np.inf)
            t, phi, rad = np.zeros(len(p)), np.zeros(len(p)), np.zeros(len(p))
            start = 0.0
            for i in range(len(pts) - 1):
                a, e = pts[i], pts[i + 1]
                length = np.linalg.norm(e - a)
                d = (e - a) / length
                s = np.clip((p - a) @ d, 0 if i else -np.inf, length if i < len(pts) - 2 else np.inf)
                r = p - a - np.outer(s, d)
                dist = np.linalg.norm(r, axis=1)
                e1 = norm(np.asarray(ref) - d*np.dot(ref, d))
                e2 = np.cross(d, e1)
                closer = dist < best
                best[closer] = dist[closer]
                t[closer] = start + s[closer]
                phi[closer] = np.arctan2(r[closer] @ e2, r[closer] @ e1)
                rad[closer] = dist[closer]
                start += length
            self.t[sel], self.phi[sel], self.radius[sel] = t, phi, rad
        self.theta = np.arctan2(self.x, -(self.y - TORSO[1]))
        self.around = self.theta * np.hypot(self.x, self.y - TORSO[1])
        # -1 at the back to 1 at the front, limbs measure their angle from a quarter turn away from forward
        self.front = np.where(self.limb, np.sin(self.phi), np.cos(self.theta))


# noise, hashed from integer lattice points so a design comes out the same on every run
def hash3(ix, iy, iz, seed=0):
    h = (ix.astype(np.int64)*73856093 ^ iy.astype(np.int64)*19349663 ^ iz.astype(np.int64)*83492791 ^ seed*2654435761) & 0xffffffff
    h = (h ^ (h >> 13)) * 1274126177 & 0xffffffff
    h = h ^ (h >> 16)
    return (h & 0xffffff) / float(0x1000000)


def noise(p, seed=0):
    """Smooth value noise in 0-1 at points p."""
    f = np.floor(p)
    i = f.astype(np.int64)
    u = p - f
    u = u*u*(3 - 2*u)
    out = 0
    for dx in (0, 1):
        for dy in (0, 1):
            for dz in (0, 1):
                w = (u[:, 0] if dx else 1 - u[:, 0]) * (u[:, 1] if dy else 1 - u[:, 1]) * (u[:, 2] if dz else 1 - u[:, 2])
                out = out + w*hash3(i[:, 0] + dx, i[:, 1] + dy, i[:, 2] + dz, seed)
    return out


def fbm(p, octaves=4, seed=0):
    out, amp, total = 0, 1.0, 0
    for k in range(octaves):
        out = out + amp*noise(p * 2**k, seed + k)
        total += amp
        amp *= 0.5
    return out / total


def cells(p, seed=0):
    """Distances to the nearest three of a jittered point per unit cell, and the nearest cell's id."""
    f = np.floor(p).astype(np.int64)
    d1, d2, d3 = (np.full(len(p), np.inf) for _ in range(3))
    ident = np.zeros(len(p))
    for dx in (-1, 0, 1):
        for dy in (-1, 0, 1):
            for dz in (-1, 0, 1):
                c = f + np.array([dx, dy, dz])
                jit = np.stack([hash3(c[:, 0], c[:, 1], c[:, 2], seed + k) for k in (1, 2, 3)], 1)
                d = np.linalg.norm(c + jit - p, axis=1)
                d3 = np.where(d < d2, d2, np.minimum(d3, d))
                d2 = np.where(d < d1, d1, np.where(d < d2, d, d2))
                ident = np.where(d < d1, hash3(c[:, 0], c[:, 1], c[:, 2], seed + 7), ident)
                d1 = np.minimum(d1, d)
    return d1, d2, d3, ident


def stripe(v, period, width, offset=0.0):
    """1 on stripes of a width repeating along v."""
    d = np.abs(((v - offset) / period + 0.5) % 1 - 0.5) * period
    return smooth(d - width/2, 0.02)


def default(sh):
    """The armour itself, plates in the body colour over an accent colour undersuit."""
    return pick((np.ones(sh.n), ACCENT), (sh.plate, BODY))


def superhero(sh):
    """A bold emblem on the chest and back with gauntlets and boots, team at the heart of the emblem."""
    chest = sh.core & (sh.z > 10.6) & (sh.z < 14.4)
    v = (14.0 - sh.z) - 1.2*np.maximum(sh.ax, 0) ** 1.0
    emblem = chest & (v > -0.1) & (sh.ax < 2.4 - 0.4*(14.0 - sh.z))
    heart = chest & (np.abs(sh.z - 12.6) + sh.ax*1.2 < 0.55)
    gauntlet = sh.arm & (sh.t > 3.4)
    boot = sh.leg & (sh.t > 7.0)
    belt = sh.core & (np.abs(sh.z - 9.0) < 0.3)
    return pick((np.ones(sh.n), BODY), (emblem | gauntlet | boot | belt, ACCENT), (heart, TEAM))


def traced(sh, name):
    """A hand made mixer painted onto the body's UVs, taken as it is."""
    return decode(sh.trace(np.asarray(Image.open(f'{SOURCES}/{name}.png').convert('RGBA'), np.float32) / 255))


def softhero(sh):
    """Ludwig "SniperGoth" Boscolo's harness, bands and boots."""
    return traced(sh, 'softhero')


def lines(sh):
    """Ludwig "SniperGoth" Boscolo's line work."""
    return traced(sh, 'lines')


def mutant(sh):
    """A vein network grown over the body, team where the veins meet."""
    p = sh.pos * 0.9
    p = p + 0.35*np.stack([fbm(p*0.8, 3, s) - 0.5 for s in (11, 12, 13)], 1)
    d1, d2, d3, _ = cells(p, 5)
    vein = smooth((d2 - d1) - 0.07, 0.03)
    knot = smooth((d3 - d1) - 0.09, 0.02)
    return pick((np.ones(sh.n), BODY), (vein, ACCENT), (knot, TEAM))


def racer(sh):
    """A stripe down the spine and along each limb, edged in team pinstripes."""
    core = sh.core & (sh.region != 'head')
    centre = np.where(sh.limb, np.abs(sh.phi) * sh.radius, np.where(core | (sh.region == 'head'), sh.ax, 99))
    stripe_ = smooth(centre - 0.42, 0.02)
    pin = smooth(np.abs(centre - 0.56) - 0.06, 0.02)
    return pick((np.ones(sh.n), BODY), (pin, TEAM), (stripe_, ACCENT))


def tiger(sh):
    """Stripes wrapping the limbs and torso, tapering towards the front, with team marks on the brow and chest."""
    warp = fbm(sh.pos * 0.6, 3, 31) - 0.5
    along = np.where(sh.limb, sh.t, sh.z)
    wave = np.abs(((along + 1.2*warp) / 1.1) % 1 - 0.5) * 2
    side = np.where(sh.limb, 0.5 + 0.5*np.cos(sh.phi), 1 - np.abs(sh.front))
    width = 0.15 + 0.3*side + 0.15*fbm(sh.pos*1.3, 2, 33)
    tiger_ = smooth(wave - width, 0.03)
    brow = (sh.region == 'head') & (sh.front > 0.8) & (np.abs(sh.z - 16.3) < 0.18) & (sh.ax < 0.5)
    chest = sh.core & (sh.front > 0.9) & (np.abs(sh.z - 13.2) + sh.ax < 0.45)
    return pick((np.ones(sh.n), BODY), (tiger_, ACCENT), (brow | chest, TEAM))


def camo(sh):
    """Blotches in the body and accent colours with the odd small team blotch."""
    a = fbm(sh.pos * 0.45, 4, 41)
    b = fbm(sh.pos * 0.9, 3, 43)
    return pick((np.ones(sh.n), BODY), (smooth(0.5 - a, 0.01), ACCENT), (smooth(0.72 - b, 0.01), TEAM))


def split(sh):
    """Front in the body colour and back in the accent colour, with a wide team band between them, seen from the front and back."""
    band = smooth(np.abs(sh.front) - 0.55, 0.02)
    return pick((np.ones(sh.n), BODY), (smooth(sh.front, 0.02), ACCENT), (band, TEAM))


def fade(sh):
    """The body colour at the head through to the accent colour at the feet, team banding the waist."""
    t = np.clip((14.5 - sh.z) / 11.0, 0, 1)
    t = t*t*(3 - 2*t)
    waist = smooth(np.abs(sh.z - 9.0) - 0.22, 0.02) * sh.core
    return pick((np.ones(sh.n), BODY), (t, ACCENT), (waist, TEAM))


def lattice(u, v, size, key):
    """Nearest and second nearest centres of a hexagon lattice, as distances, and a hash of the nearest."""
    row = size * np.sqrt(3) / 2
    j0 = np.floor(v / row).astype(np.int64)
    d1, d2 = np.full(len(u), np.inf), np.full(len(u), np.inf)
    ident = np.zeros(len(u))
    for dj in (-1, 0, 1, 2):
        j = j0 + dj
        shift = (j % 2) * 0.5
        i0 = np.floor(u / size - shift).astype(np.int64)
        for di in (-1, 0, 1, 2):
            i = i0 + di
            d = np.hypot(u - (i + shift) * size, v - j * row)
            nearer = d < d1
            d2 = np.where(nearer, d1, np.minimum(d2, d))
            ident = np.where(nearer, hash3(i, j, key, 51), ident)
            d1 = np.where(nearer, d, d1)
    return d1, d2, ident


def wrap(sh):
    """Coordinates across the surface, around and along the limbs, around and up the torso."""
    u = np.where(sh.limb, sh.phi * sh.radius.clip(0.3), sh.around)
    v = np.where(sh.limb, sh.t, sh.z)
    return u, v, sh.arm.astype(np.int64) + 2*sh.leg


def hexes(sh):
    """Hexagon cells wrapped around the torso and limbs, the odd one in the accent colour, separated in team."""
    u, v, key = wrap(sh)
    d1, d2, ident = lattice(u, v, 0.6, key)
    gap = smooth(-(0.035 - (d2 - d1)), 0.01)
    cell = ident < 0.4
    return pick((np.ones(sh.n), BODY), (cell, ACCENT), (gap, TEAM))


def chevron(sh):
    """V shapes stacked down the chest, back and limbs, team on the leading one."""
    v = np.where(sh.limb, sh.t + 0.9*np.abs(np.sin(sh.phi)) * sh.radius, -sh.z + 0.6*sh.ax)
    stack = stripe(v, 1.3, 0.55)
    lead = sh.core & (np.abs(v + 14.2) < 0.28) & (sh.z < 14.8)
    return pick((np.ones(sh.n), BODY), (stack * (v > -14.6), ACCENT), (lead, TEAM))


def circuit(sh):
    """Traces in the weapon colour over the body and accent colours, team on the pads they run to."""
    u = np.where(sh.limb, sh.phi * sh.radius.clip(0.3), sh.around)
    v = np.where(sh.limb, sh.t + 20*sh.leg, sh.z)
    g = 0.45
    iu, iv = np.floor(u / g), np.floor(v / g)
    fu, fv = u / g - iu, v / g - iv
    key = sh.arm.astype(np.int64)*3 + sh.leg*5
    h = hash3(iu.astype(np.int64), iv.astype(np.int64), key, 61)
    hv = hash3(iu.astype(np.int64), iv.astype(np.int64), key, 62)
    horiz = (h < 0.45) & (np.abs(fv - 0.5) < 0.09)
    vert = (hv < 0.45) & (np.abs(fu - 0.5) < 0.09)
    pad = (hash3(iu.astype(np.int64), iv.astype(np.int64), key, 63) < 0.08) & (np.hypot(fu - 0.5, fv - 0.5) < 0.24)
    base = fbm(sh.pos * 0.35, 2, 64) < 0.5
    return pick((np.ones(sh.n), BODY), (base, ACCENT), (horiz | vert, WEAPON), (pad, TEAM))


def skeleton(sh):
    """Bones over the skeleton, ribs, spine, pelvis and the limb bones, team on the skull."""
    torso = sh.region == 'torso'
    ribs = torso & (sh.front > 0.3) & (sh.z > 10.6) & (sh.z < 13.8) & (sh.ax > 0.3) & (sh.ax < 1.9)
    ribs &= stripe(sh.z + 0.35*sh.ax, 0.7, 0.28) > 0.5
    sternum = torso & (sh.front > 0.5) & (sh.ax < 0.18) & (sh.z > 11.0) & (sh.z < 14.0)
    spine = sh.core & (sh.front < -0.5) & (sh.ax < 0.28) & (sh.z > 8.4) & (sh.z < 14.6) & (stripe(sh.z, 0.55, 0.36) > 0.5)
    pelvis = (sh.region == 'pelvis') & (sh.front > 0.3) & (np.abs(np.hypot(sh.ax * 0.8, sh.z - 9.8) - 1.1) < 0.18)
    # a long bone down the front of each limb section, swelling at the joints
    joints = np.where(sh.arm, np.minimum(np.abs(sh.t - 2.2), np.abs(sh.t - 0.2)), np.minimum(np.abs(sh.t - 4.6), np.abs(sh.t - 0.4)))
    width = 0.16 + 0.22*np.exp(-(joints / 0.3)**2)
    across = np.abs(sh.phi - np.pi/2) * sh.radius
    bone = sh.limb & (sh.region != 'foot') & (sh.region != 'hand') & (across < width)
    fingers = (sh.region == 'hand') & (np.sin(sh.phi) < -0.3) & (stripe(sh.phi * sh.radius, 0.3, 0.12) > 0.5)
    face = (sh.region == 'head') & (sh.front > 0.45) & (sh.z > 15.1)
    return pick((np.ones(sh.n), BODY), (ribs | sternum | spine | pelvis | bone | fingers, ACCENT), (face, TEAM))


def flames(sh):
    """Flames rising from the feet and hands, team at their tips."""
    reach = np.where(sh.leg, 11.0 - sh.t, np.where(sh.arm, 6.8 - sh.t, 99))
    lick = 2.2 + 1.3*(0.5 + 0.5*np.sin(sh.phi*3 + 1.7*fbm(sh.pos*0.8, 2, 71))) + 0.8*fbm(sh.pos*1.5, 2, 72)
    lick = lick * np.where(sh.arm, 0.75, 1.6)
    inside = lick - reach
    flame = smooth(-inside, 0.03)
    tip = flame * (inside < 0.7)
    return pick((np.ones(sh.n), BODY), (flame, ACCENT), (tip, TEAM))


def harlequin(sh):
    """A diamond lattice wrapping the torso and limbs, one team diamond on the chest and back."""
    u = np.where(sh.limb, sh.phi * sh.radius.clip(0.3), sh.around)
    v = np.where(sh.limb, sh.t, sh.z - 12.6)
    a, b = (u / 0.7 + v / 1.0), (u / 0.7 - v / 1.0)
    check = (np.floor(a) + np.floor(b)) % 2 == 0
    heart = sh.core & (np.abs(u) / 0.7 + np.abs(v) / 1.0 < 1.0) & (np.abs(sh.front) > 0.6)
    return pick((np.ones(sh.n), BODY), (check, ACCENT), (heart, TEAM))


def digital(sh):
    """Blocky pixel camo built from cubes, with a scattering of team pixels."""
    q = np.floor(sh.pos / 0.32) + 0.5
    a = fbm(q * 0.32 * 0.5, 3, 81)
    b = hash3(q[:, 0].astype(np.int64), q[:, 1].astype(np.int64), q[:, 2].astype(np.int64), 82)
    return pick((np.ones(sh.n), BODY), (a < 0.48, ACCENT), (b < 0.03, TEAM))


def bands(sh):
    """Rings around the body at fixed heights, team on the one around the chest."""
    ring = stripe(sh.z, 1.5, 0.5)
    chest = sh.core & (np.abs(sh.z - 12.0) < 0.25)
    return pick((np.ones(sh.n), BODY), (ring, ACCENT), (chest, TEAM))


def hazard(sh):
    """Diagonal warning stripes on the forearms and shins, trimmed in team."""
    fore = sh.arm & (sh.t > 2.6) & (sh.t < 5.0)
    shin = sh.leg & (sh.t > 5.0) & (sh.t < 8.6)
    zone = fore | shin
    diag = stripe(sh.t + sh.phi * sh.radius.clip(0.3), 0.8, 0.4)
    trim = (sh.arm & ((np.abs(sh.t - 2.5) < 0.12) | (np.abs(sh.t - 5.1) < 0.12))) | (sh.leg & ((np.abs(sh.t - 4.9) < 0.14) | (np.abs(sh.t - 8.7) < 0.14)))
    return pick((np.ones(sh.n), BODY), (zone * diag, ACCENT), (trim, TEAM))


def suit(sh):
    """A jacket and trousers in the accent colour over a shirt, gloves and head in the body colour, with a team tie."""
    jacket = (sh.region != 'head') & ~((sh.region == 'hand') | (sh.arm & (sh.t > 4.8)))
    shirt = sh.core & (sh.front > 0) & (sh.z > 11.0) & (sh.ax < (sh.z - 11.0) * 0.45)
    tie = sh.core & (sh.front > 0) & (sh.ax < 0.18 + 0.06*np.clip(14.0 - sh.z, 0, 3)) & (sh.z > 10.6) & (sh.z < 14.4)
    shoes = sh.leg & (sh.t > 9.0)
    return pick((np.ones(sh.n), BODY), (jacket, ACCENT), (shirt, BODY), (tie, TEAM), (shoes, BODY))


def magma(sh):
    """Cracked rock plates with the weapon colour glowing in the cracks, the odd team ember."""
    d1, d2, _, ident = cells(sh.pos * 0.8, 91)
    crack = smooth((d2 - d1) - 0.06, 0.03)
    cooled = ident < 0.45
    ember = ident > 0.96
    return pick((np.ones(sh.n), BODY), (cooled, ACCENT), (ember, TEAM), (crack, WEAPON))


def upright(sh):
    """Coordinates across the surface like wrap, with v always pointing up the body as the player stands."""
    u, v, key = wrap(sh)
    return u, np.where(sh.limb, -sh.t, sh.z), key


def tiles(u, v, size, key, seed):
    """A motif's cell, rows offset by half, as local coordinates from -0.5 to 0.5, and a hash of it."""
    j = np.floor(v / size)
    shift = 0.5 * (j % 2)
    i = np.floor(u / size - shift)
    x = u / size - shift - i - 0.5
    y = v / size - j - 0.5
    ident = hash3(i.astype(np.int64), j.astype(np.int64), key, seed)
    return x, y, ident


def polar(x, y):
    return np.hypot(x, y), np.arctan2(x, y)


def motifs(sh, shape, size, seed):
    """A motif stamped across the body in the accent colour."""
    u, v, key = upright(sh)
    x, y, ident = tiles(u, v, size, key, seed)
    inside = shape(x, y, ident)
    return pick((np.ones(sh.n), BODY), (inside, ACCENT))


def heart(sh):
    """Rows of hearts in the accent colour."""
    def shape(x, y, ident):
        x, y = np.abs(x) * 3.2, y * 3.2 + 0.25
        return (x*x + y*y - 1)**3 - x*x * y**3 <= 0
    return motifs(sh, shape, 0.75, 101)


def crown(sh):
    """Rows of small crowns in the accent colour."""
    def shape(x, y, ident):
        band = (np.abs(x) < 0.34) & (y > -0.3) & (y < -0.12)
        tip = np.minimum(np.minimum(np.abs(x + 0.3), np.abs(x)), np.abs(x - 0.3))
        points = (np.abs(x) < 0.34) & (y >= -0.12) & (y < 0.3 - 2.4*tip)
        return band | points
    return motifs(sh, shape, 0.6, 102)


def star(sh):
    """Scattered stars of different sizes in the accent colour."""
    u, v, key = upright(sh)
    out = np.zeros(sh.n, bool), np.zeros(sh.n, bool)
    for size, seed, layer in ((0.9, 103, 0), (0.45, 104, 1)):
        x, y, ident = tiles(u, v, size, key, seed)
        jx = hash3((ident * 1e6).astype(np.int64), key, np.zeros_like(key), seed + 1) - 0.5
        jy = hash3((ident * 1e6).astype(np.int64), key, np.ones_like(key), seed + 2) - 0.5
        r, a = polar(x - 0.3*jx, y - 0.3*jy)
        spin = ident * 2*np.pi
        radius = (0.18 + 0.12*ident) * (0.55 + 0.45*np.abs(np.cos(2.5*(a + spin))) ** 6)
        out[layer][:] |= r < radius
    return pick((np.ones(sh.n), BODY), (out[1] | out[0], ACCENT))


def flower(sh):
    """Five petalled flowers in the accent colour with a team centre, among small team dots."""
    u, v, key = upright(sh)
    x, y, ident = tiles(u, v, 0.8, key, 105)
    r, a = polar(x, y)
    petals = r < 0.34 * (0.55 + 0.45*np.abs(np.cos(2.5*a)))
    centre = r < 0.08
    dot = np.hypot(np.abs(x) - 0.5, np.abs(y) - 0.5) < 0.06
    return pick((np.ones(sh.n), BODY), (petals, ACCENT), (centre | dot, TEAM))


def circle(sh):
    """Polka dots in the accent colour."""
    return motifs(sh, lambda x, y, ident: np.hypot(x, y) < 0.3, 0.55, 106)


def ninja(sh):
    """Throwing stars in the accent colour."""
    def shape(x, y, ident):
        r, a = polar(x, y)
        a = a + 2.5*r
        return r < 0.42 * (0.22 + 0.78*np.abs(np.cos(2*a)) ** 8)
    return motifs(sh, shape, 0.7, 107)


def checker(sh):
    """A checkerboard in the accent colour over the body colour."""
    u, v, key = upright(sh)
    i, j = np.floor(u / 0.45), np.floor(v / 0.45)
    dark = (i + j) % 2 == 0
    return pick((np.ones(sh.n), BODY), (dark, ACCENT))


def pixel(sh):
    """Fine square pixels in clusters of the accent colour."""
    u, v, key = upright(sh)
    i, j = np.floor(u / 0.2), np.floor(v / 0.2)
    gap = (np.abs(u / 0.2 - i - 0.5) < 0.4) & (np.abs(v / 0.2 - j - 0.5) < 0.4)
    h = hash3(i.astype(np.int64), j.astype(np.int64), key, 108)
    cluster = fbm(sh.pos * 0.8, 2, 109)
    return pick((np.ones(sh.n), BODY), (gap & ((h < cluster - 0.15) | (h > 1.2 - cluster)), ACCENT))


def zigzag(sh):
    """Zigzag bands around the body in the accent colour, edged in team."""
    u, v, key = upright(sh)
    zig = v + 0.25 * np.abs(((u / 0.5) % 2) - 1)
    return pick((np.ones(sh.n), BODY), (stripe(zig, 1.0, 0.36), ACCENT), (stripe(zig + 0.3, 1.0, 0.1), TEAM))


def zebra(sh):
    """Wavering zebra stripes in the accent colour, each with a thin team stripe beside it."""
    warp = fbm(sh.pos * 0.5, 3, 110) - 0.5
    along = np.where(sh.limb, sh.t, sh.z) + 1.6*warp + 0.3*np.sin(sh.ax * 2.0)
    return pick((np.ones(sh.n), BODY), (stripe(along, 0.8, 0.3), ACCENT), (stripe(along + 0.33, 0.8, 0.08), TEAM))


def leopard(sh):
    """Rosettes, broken rings in the accent colour around team spots."""
    p = sh.pos * 1.6 + 0.25*np.stack([fbm(sh.pos, 2, s) - 0.5 for s in (111, 112, 113)], 1)
    d1, d2, _, ident = cells(p, 114)
    ring = (d1 > 0.2) & (d1 < 0.34) & (fbm(p * 3, 2, 115) > 0.38)
    spot = d1 < 0.16
    return pick((np.ones(sh.n), BODY), (ring, ACCENT), (spot, TEAM))


DESIGNS = {
    'default': default, 'bands': bands, 'camo': camo, 'checker': checker, 'chevron': chevron, 'circle': circle,
    'circuit': circuit, 'crown': crown, 'digital': digital, 'fade': fade, 'flames': flames, 'flower': flower,
    'harlequin': harlequin, 'hazard': hazard, 'heart': heart, 'hex': hexes, 'leopard': leopard, 'lines': lines,
    'magma': magma, 'mutant': mutant, 'ninja': ninja, 'pixel': pixel, 'racer': racer, 'skeleton': skeleton,
    'softhero': softhero, 'split': split, 'superhero': superhero, 'star': star, 'suit': suit, 'tiger': tiger, 'zebra': zebra, 'zigzag': zigzag,
}

BODYSIZE, HANDSIZE = 1024, 2048


def undersuit(diffuse):
    """The undersuit of a diffuse texture, found from the fine detail of its mesh, the plates are smooth."""
    detail = ndimage.uniform_filter(np.abs(diffuse - ndimage.uniform_filter(diffuse, 5)), 21) > 3.0
    mesh = ndimage.binary_closing(ndimage.binary_opening(detail, iterations=6), iterations=4)
    labels, count = ndimage.label(mesh)
    sizes = np.bincount(labels.ravel(), minlength=count + 1)
    keep = sizes > diffuse.size // 2000
    keep[0] = False
    return keep[labels]


def pattern(path, mask, size):
    """The player's pattern, which the model lays over any mixer, given the team colour over the whole of each light."""
    team = lights(np.asarray(Image.open(mask).convert('RGBA'))[..., 1]).astype(np.float32)
    if team.shape[0] != size:
        team = np.asarray(Image.fromarray(team).resize((size, size), Image.BILINEAR))
    old = np.asarray(Image.open(path).convert('RGBA'), np.float32) / 255
    old[..., 2] = np.minimum(old[..., 2], 1 - team)
    Image.fromarray((old * 255 + 0.5).astype(np.uint8), 'RGBA').save(path, optimize=True)


def lights(glow):
    """Where the glow mask lights the armour, widened a little so filtering leaves no rim of another colour."""
    return ndimage.binary_dilation(glow > 32, iterations=max(glow.shape[0] // 512, 1))


def main():
    src, out, only = sys.argv[1], sys.argv[2], sys.argv[3:]
    if not only or 'pattern' in only:
        pattern(src + '/pattern.png', src + '/mask.png', BODYSIZE)
        pattern(src + '/hudhandspattern.png', src + '/hudhandsmask.png', HANDSIZE)
        print('pattern')
        if only == ['pattern']:
            return
    body = Mesh(src + '/male/RE_Player_male.iqm')
    hands = Mesh(src + '/hud_hands.iqm')
    tp = Bake(body, BODYSIZE * SUPER)
    fp = Bake(hands, HANDSIZE * SUPER, placehands(hands, body))
    tpshape, fpshape = Shape(tp, body), Shape(fp, body)
    # hand made mixers are drawn on the body, the hands take them from the nearest point on the arms
    arms = np.isin(tp.region, ARMS)
    near = np.flatnonzero(arms)[spatial.cKDTree(tp.pos[arms]).query(fp.pos)[1]]
    tpshape.trace = lambda img: tp.sample(img)
    fpshape.trace = lambda img: tp.sample(img)[near]
    # armour plates, the body's mask has them in alpha, the hands only have their diffuse, where the undersuit is a mesh
    tpshape.plate = tp.sample(np.asarray(Image.open(src + '/mask.png'))[..., 3] > 127)
    fpshape.plate = fp.sample(~undersuit(np.asarray(Image.open(src + '/hudhandsdiffuse.png').convert('L'), float)))
    for name, design in DESIGNS.items():
        if only and name not in only:
            continue
        save(f'{out}/{name}.png', resolve(tp, design(tpshape), BODYSIZE))
        save(f'{out}/{name}fp.png', resolve(fp, design(fpshape), HANDSIZE))
        print(name)


if __name__ == '__main__':
    main()
