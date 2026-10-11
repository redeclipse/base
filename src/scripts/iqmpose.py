"""Read, edit and write IQM skeletal animations for the Red Eclipse player model.

Poses are edited as local joint transforms (translate xyz, quaternion xyzw, scale xyz) in IQM space.
"""
import struct
import numpy as np

HDR = '<16s27I'
HNAMES = ['version', 'filesize', 'flags', 'num_text', 'ofs_text', 'num_meshes', 'ofs_meshes', 'num_vertexarrays',
          'num_vertexes', 'ofs_vertexarrays', 'num_triangles', 'ofs_triangles', 'ofs_adjacency', 'num_joints',
          'ofs_joints', 'num_poses', 'ofs_poses', 'num_anims', 'ofs_anims', 'num_frames', 'num_framechannels',
          'ofs_frames', 'ofs_bounds', 'num_comment', 'ofs_comment', 'num_extensions', 'ofs_extensions']


def header(buf):
    h = struct.unpack(HDR, buf[:124])
    assert h[0].startswith(b'INTERQUAKEMODEL') and h[1] == 2
    return dict(zip(HNAMES, h[1:]))


def text(buf, h, ofs):
    start = h['ofs_text'] + ofs
    return buf[start:buf.index(b'\0', start)].decode()


def load_joints(path):
    """Joint names, parents and rest transforms from a model file."""
    buf = open(path, 'rb').read()
    h = header(buf)
    joints = []
    for i in range(h['num_joints']):
        o = h['ofs_joints'] + i*48
        name, parent = struct.unpack('<Ii', buf[o:o+8])
        vals = struct.unpack('<10f', buf[o+8:o+48])
        joints.append((text(buf, h, name), parent, np.array(vals)))
    return joints


def load_anim(path):
    """Decoded frames [frame][pose][10 channels] plus the pose parents and anim info."""
    buf = open(path, 'rb').read()
    h = header(buf)
    poses = []
    for i in range(h['num_poses']):
        o = h['ofs_poses'] + i*88
        parent, mask = struct.unpack('<iI', buf[o:o+8])
        offset = np.array(struct.unpack('<10f', buf[o+8:o+48]))
        scale = np.array(struct.unpack('<10f', buf[o+48:o+88]))
        poses.append((parent, mask, offset, scale))
    nf, nc = h['num_frames'], h['num_framechannels']
    raw = np.frombuffer(buf, dtype='<u2', count=nf*nc, offset=h['ofs_frames']) if nf and nc else np.zeros(0)
    frames = np.zeros((nf, len(poses), 10))
    k = 0
    for f in range(nf):
        for p, (parent, mask, offset, scale) in enumerate(poses):
            for c in range(10):
                v = offset[c]
                if mask & (1 << c):
                    v += raw[k] * scale[c]
                    k += 1
                frames[f, p, c] = v
    anims = []
    for i in range(h['num_anims']):
        o = h['ofs_anims'] + i*20
        name, first, num, rate, flags = struct.unpack('<3IfI', buf[o:o+20])
        anims.append((text(buf, h, name), first, num, rate, flags))
    return [p[0] for p in poses], frames, anims


def save_anim(path, name, parents, frames, framerate=0.0, flags=0):
    """Write frames [frame][pose][10] as a single animation, quantised to 16 bits per channel."""
    nf, npose, _ = frames.shape
    lo, hi = frames.min(axis=0), frames.max(axis=0)
    posedata, channels = b'', []
    for p in range(npose):
        mask = 0
        scale = np.zeros(10)
        for c in range(10):
            if hi[p, c] - lo[p, c] > 1e-6:
                mask |= 1 << c
                scale[c] = (hi[p, c] - lo[p, c]) / 65535.0
                channels.append((p, c))
        posedata += struct.pack('<iI', parents[p], mask) + struct.pack('<10f', *lo[p]) + struct.pack('<10f', *scale)
    framedata = bytearray()
    for f in range(nf):
        for p, c in channels:
            s = (hi[p, c] - lo[p, c]) / 65535.0
            framedata += struct.pack('<H', int(round((frames[f, p, c] - lo[p, c]) / s)))
    textdata = b'\0' + name.encode() + b'\0'
    while len(textdata) % 4:
        textdata += b'\0'
    ofs_text = 124
    ofs_poses = ofs_text + len(textdata)
    ofs_anims = ofs_poses + len(posedata)
    ofs_frames = ofs_anims + 20
    filesize = ofs_frames + len(framedata)
    h = dict.fromkeys(HNAMES, 0)
    h.update(version=2, filesize=filesize, num_text=len(textdata), ofs_text=ofs_text, num_poses=npose, ofs_poses=ofs_poses,
             num_anims=1, ofs_anims=ofs_anims, num_frames=nf, num_framechannels=len(channels), ofs_frames=ofs_frames)
    out = struct.pack(HDR, b'INTERQUAKEMODEL\0', *[h[k] for k in HNAMES])
    out += textdata + posedata + struct.pack('<3IfI', 1, 0, nf, framerate, flags) + bytes(framedata)
    open(path, 'wb').write(out)


# quaternions are (x, y, z, w)
def qmul(a, b):
    ax, ay, az, aw = a
    bx, by, bz, bw = b
    return np.array([aw*bx + ax*bw + ay*bz - az*by, aw*by - ax*bz + ay*bw + az*bx,
                     aw*bz + ax*by - ay*bx + az*bw, aw*bw - ax*bx - ay*by - az*bz])


def qconj(q):
    return np.array([-q[0], -q[1], -q[2], q[3]])


def qrot(q, v):
    return qmul(qmul(q, np.array([v[0], v[1], v[2], 0.0])), qconj(q))[:3]


def qaxis(axis, angle):
    axis = np.asarray(axis, float)
    axis = axis / np.linalg.norm(axis)
    s = np.sin(angle/2)
    return np.array([axis[0]*s, axis[1]*s, axis[2]*s, np.cos(angle/2)])


def qbetween(a, b):
    a = a / np.linalg.norm(a)
    b = b / np.linalg.norm(b)
    d = np.dot(a, b)
    if d < -0.9999:
        perp = np.cross(a, [1, 0, 0])
        if np.linalg.norm(perp) < 1e-3:
            perp = np.cross(a, [0, 1, 0])
        return qaxis(perp, np.pi)
    c = np.cross(a, b)
    q = np.array([c[0], c[1], c[2], 1 + d])
    return q / np.linalg.norm(q)


def qslerp(a, b, t):
    if np.dot(a, b) < 0:
        b = -b
    q = a*(1 - t) + b*t
    return q / np.linalg.norm(q)


class Pose:
    """One frame of local joint transforms with forward kinematics."""

    def __init__(self, names, parents, frame):
        self.names, self.parents = names, parents
        self.index = {n: i for i, n in enumerate(names)}
        self.local = frame.copy()
        self.update()

    def update(self):
        n = len(self.names)
        self.wrot = np.zeros((n, 4))
        self.wpos = np.zeros((n, 3))
        for i in range(n):
            t, q = self.local[i, 0:3], self.local[i, 3:7] / np.linalg.norm(self.local[i, 3:7])
            p = self.parents[i]
            if p < 0:
                self.wrot[i], self.wpos[i] = q, t
            else:
                self.wrot[i] = qmul(self.wrot[p], q)
                self.wpos[i] = self.wpos[p] + qrot(self.wrot[p], t)

    def pos(self, name):
        return self.wpos[self.index[name]]

    def rotate_world(self, name, q):
        """Apply a world space rotation to a joint, keeping its children attached."""
        i = self.index[name]
        p = self.parents[i]
        neww = qmul(q, self.wrot[i])
        local = neww if p < 0 else qmul(qconj(self.wrot[p]), neww)
        self.local[i, 3:7] = local / np.linalg.norm(local)
        self.update()

    def aim(self, name, child, direction, amount=1.0):
        """Rotate a joint so the segment to its child points along a world direction."""
        cur = self.pos(child) - self.pos(name)
        q = qbetween(cur, np.asarray(direction, float))
        if amount < 1:
            q = qslerp(np.array([0, 0, 0, 1.0]), q, amount)
        self.rotate_world(name, q)

    def twist(self, name, child, angle):
        """Roll a joint about its own segment."""
        axis = self.pos(child) - self.pos(name)
        self.rotate_world(name, qaxis(axis, angle))

    def turn(self, name, axis, angle):
        self.rotate_world(name, qaxis(axis, angle))

    def worldrot(self, name):
        return self.wrot[self.index[name]].copy()

    def setworldrot(self, name, q):
        """Give a joint an exact world space orientation."""
        self.rotate_world(name, qmul(q, qconj(self.wrot[self.index[name]])))

    def move_root(self, offset):
        """Shift the root joint in world space, carrying the whole skeleton."""
        i = self.parents.index(-1)
        self.local[i, 0:3] += np.asarray(offset, float)
        self.update()

    def reach(self, upper, lower, end, target, pole):
        """Two bone IK: put the end joint on a target with the middle joint bending towards a pole direction."""
        a = np.linalg.norm(self.pos(lower) - self.pos(upper))
        b = np.linalg.norm(self.pos(end) - self.pos(lower))
        start = self.pos(upper)
        d = np.asarray(target, float) - start
        dist = min(np.linalg.norm(d), (a + b)*0.999)
        d = d / np.linalg.norm(d)
        x = (a*a - b*b + dist*dist) / (2*dist)
        h = np.sqrt(max(a*a - x*x, 0.0))
        pole = np.asarray(pole, float)
        perp = pole - d*np.dot(pole, d)
        perp = perp / np.linalg.norm(perp)
        self.aim(upper, lower, start + d*x + perp*h - start)
        self.aim(lower, end, start + d*dist - self.pos(lower))
