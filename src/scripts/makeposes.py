#!/usr/bin/env python3
"""Generate the OST cover poses for the player model from its idle animation, needs numpy.

usage: src/scripts/makeposes.py data/actors/player data/actors/player
"""
import sys
import numpy as np
from iqmpose import load_joints, load_anim, save_anim, Pose, qaxis, qrot, qslerp, qmul, qconj

src, out = sys.argv[1], sys.argv[2]
joints = load_joints(src + '/male/RE_Player_male.iqm')
names = [j[0] for j in joints]
parents, idle, _ = load_anim(src + '/idle.iqm')

BIND = {j[0]: j[2][3:7] / np.linalg.norm(j[2][3:7]) for j in joints}

UP, RIGHT, FWD = np.array([0, 0, 1.0]), np.array([-1.0, 0, 0]), np.array([0, -1.0, 0])
SIDE = {'L': -RIGHT, 'R': RIGHT}
FINGERS = ['finger_index', 'finger_middle', 'finger_ring', 'finger_pinky']


def norm(v):
    v = np.asarray(v, float)
    return v / np.linalg.norm(v)


def sway(v, axis, angle):
    return qrot(qaxis(axis, angle), norm(v))


def arm(P, s, upper, fore):
    P.aim('upper_arm.%s.01' % s, 'forearm.%s.01' % s, norm(upper))
    P.aim('forearm.%s.01' % s, 'hand.%s' % s, norm(fore))


# choices made on the first frame and kept for the rest, so the loop can't flicker between them
CHOICES = {}


def turn_towards(P, joint, axis, angle, probe, direction):
    """Turn a joint about an axis by an angle, in whichever sense moves the probe joint along a direction."""
    key = ('turn', joint, probe, tuple(np.round(direction, 3)), round(angle, 3))
    if key not in CHOICES:
        before = P.pos(probe).copy()
        P.turn(joint, axis, angle)
        after = P.pos(probe).copy()
        P.turn(joint, axis, -angle)
        CHOICES[key] = 1.0 if np.dot(after - before, direction) >= 0 else -1.0
    P.turn(joint, axis, angle*CHOICES[key])


def best_twist(P, joint, child, probe, direction, steps=72, base=None):
    """Roll a joint about its own segment so the probe joint sits furthest along a direction from the base joint."""
    base = base or child
    key = ('twist', joint, probe)
    if key in CHOICES:
        P.twist(joint, child, CHOICES[key])
        return
    origin = P.local.copy()
    best, bestangle = -1e9, 0.0
    for k in range(steps):
        angle = 2*np.pi*k/steps
        P.local = origin.copy()
        P.update()
        P.twist(joint, child, angle)
        score = np.dot(norm(P.pos(probe) - P.pos(base)), norm(direction))
        if score > best:
            best, bestangle = score, angle
    P.local = origin
    P.update()
    P.twist(joint, child, bestangle)
    CHOICES[key] = bestangle


def curl(P, s, fingers, angle):
    """Bend finger segments about the hand's knuckle line."""
    for f in fingers:
        for seg in ('.01.%s.01', '.01.%s.02', '.02.%s', '.03.%s'):
            j = f + seg % s
            if j not in P.index:
                continue
            axis = P.pos('finger_pinky.01.%s.01' % s) - P.pos('finger_index.01.%s.01' % s)
            P.turn(j, axis if s == 'L' else -axis, angle)


def straighten(P, s, finger, direction):
    """Lay a finger's segments out straight along a direction."""
    segs = [finger + seg % s for seg in ('.01.%s.01', '.01.%s.02', '.02.%s', '.03.%s')]
    for a, b in zip(segs, segs[1:]):
        P.aim(a, b, norm(direction))


def spread(P, s, amount):
    """Fan the fingers apart in the plane of the palm."""
    lateral = P.pos('finger_pinky.01.%s.01' % s) - P.pos('finger_index.01.%s.01' % s)
    for k, f in enumerate(FINGERS):
        j = f + '.01.%s.01' % s
        tip = f + '.02.%s' % s
        normal = np.cross(lateral, P.pos(tip) - P.pos(j))
        turn_towards(P, j, normal, abs(amount*(k - 1.2)), tip, -lateral if k < 1.2 else lateral)


def palm(P, s):
    """Direction the palm faces: the way the fingertips move as the fingers start to close."""
    origin = P.local.copy()
    tip = 'finger_middle.03.%s' % s
    before = P.pos(tip).copy()
    along = norm(before - P.pos('finger_middle.01.%s.01' % s))
    curl(P, s, ['finger_middle'], 0.3)
    moved = P.pos(tip) - before
    facing = norm(moved - along*np.dot(moved, along))  # ignore the tip drawing back towards the wrist
    P.local = origin
    P.update()
    return facing


def face_palm(P, s, fingers, direction, steps=72):
    """Point the hand along a direction, rolled so the palm faces as close to another direction as it can."""
    P.aim('hand.%s' % s, 'finger_middle.01.%s.01' % s, norm(fingers))
    key = ('palm', s)
    if key not in CHOICES:
        origin = P.local.copy()
        best, bestangle = -1e9, 0.0
        for k in range(steps):
            angle = 2*np.pi*k/steps
            P.local = origin.copy()
            P.update()
            P.twist('hand.%s' % s, 'finger_middle.01.%s.01' % s, angle)
            score = np.dot(palm(P, s), norm(direction))
            if score > best:
                best, bestangle = score, angle
        P.local = origin
        P.update()
        CHOICES[key] = bestangle
    P.twist('hand.%s' % s, 'finger_middle.01.%s.01' % s, CHOICES[key])


def hand(P, s, fingers, index):
    """Point the hand along a direction, rolled so its index knuckle lies towards another from the pinky knuckle."""
    P.aim('hand.%s' % s, 'finger_middle.01.%s.01' % s, norm(fingers))
    best_twist(P, 'hand.%s' % s, 'finger_middle.01.%s.01' % s, 'finger_index.01.%s.01' % s, index, base='finger_pinky.01.%s.01' % s)


def twistof(q, axis):
    """How far a rotation turns about an axis, in radians."""
    a = 2*np.arctan2(np.dot(q[:3], axis), q[3])
    return (a + np.pi) % (2*np.pi) - np.pi  # either sign of the quaternion is the same turn


def setlocal(P, joint, q):
    P.local[P.index[joint], 3:7] = q
    P.update()


def frame(a, b):
    """Rotation matrix whose columns are a, b made perpendicular to it, and their cross."""
    a = norm(a)
    b = norm(b - a*np.dot(a, b))
    return np.column_stack((a, b, np.cross(a, b)))


def matquat(m):
    """Quaternion of a rotation matrix, from its largest component so turns near half way round stay exact."""
    t = np.trace(m)
    if t > 0:
        w = np.sqrt(1.0 + t)*2
        return norm([(m[2, 1] - m[1, 2])/w, (m[0, 2] - m[2, 0])/w, (m[1, 0] - m[0, 1])/w, 0.25*w])
    i = int(np.argmax(np.diag(m)))
    j, k = (i + 1) % 3, (i + 2) % 3
    r = np.sqrt(1.0 + m[i, i] - m[j, j] - m[k, k])*2
    q = np.zeros(4)
    q[i], q[j], q[k], q[3] = 0.25*r, (m[j, i] + m[i, j])/r, (m[k, i] + m[i, k])/r, (m[k, j] - m[j, k])/r
    return norm(q)


def constrain_arm(P, s):
    """Rebuild an arm within what the rig allows, keeping its shoulder, elbow and wrist where they are and the hand facing the same way.

    Every bone runs along its local Y, and the hand made animations only ever bend the elbow about its local X, turn the
    forearm over by twisting forearm.02 about Y, and tilt the wrist without twisting it. Aiming bones straight at targets
    bends the elbow sideways and wrings the wrist instead, which tears the skin around them."""
    ua, fa, fa2, hd = 'upper_arm.%s.01' % s, 'forearm.%s.01' % s, 'forearm.%s.02' % s, 'hand.%s' % s
    shoulder, elbow, wrist, hand = P.pos(ua).copy(), P.pos(fa).copy(), P.pos(hd).copy(), P.worldrot(hd)
    upper, fore = norm(elbow - shoulder), norm(wrist - elbow)
    # the elbow bends about its hinge until the forearm makes the same angle with the upper arm
    Y, X = np.array([0, 1.0, 0]), np.array([1.0, 0, 0])
    bends = np.radians(np.arange(-20.0, 165.0, 0.05))
    dirs = np.array([qrot(qmul(BIND[fa], qaxis(X, b)), Y) for b in bends])
    bend = bends[np.argmin(np.abs(dirs @ Y - np.dot(upper, fore)))]
    setlocal(P, fa, qmul(BIND[fa], qaxis(X, bend)))
    # then the whole arm turns from the shoulder to lie along both directions
    now = frame(P.pos(fa) - P.pos(ua), P.pos(hd) - P.pos(fa))
    P.rotate_world(ua, matquat(frame(upper, fore) @ now.T))
    # the forearm twists to turn the hand over, leaving the wrist to tilt
    turn = 0.0
    for _ in range(8):
        setlocal(P, fa2, qmul(BIND[fa2], qaxis(Y, turn)))
        local = qmul(qconj(P.worldrot(fa2)), hand)
        turn = np.clip(turn + twistof(qmul(qconj(BIND[hd]), local), Y), -np.radians(155), np.radians(155))
    setlocal(P, fa2, qmul(BIND[fa2], qaxis(Y, turn)))
    setlocal(P, hd, qmul(qconj(P.worldrot(fa2)), hand))
    assert np.linalg.norm(P.pos(hd) - wrist) < 1e-2 and np.linalg.norm(P.pos(fa) - elbow) < 1e-2, 'arm moved %s' % np.linalg.norm(P.pos(hd) - wrist)


def stance(P, width, crouch, shift, toeout, knees):
    """Plant the feet apart with bent knees, keeping the soles flat."""
    feet = {s: P.pos('foot.%s' % s).copy() for s in 'LR'}
    rots = {s: P.worldrot('foot.%s' % s) for s in 'LR'}
    P.move_root(-UP*crouch + SIDE['R']*shift)
    for s in 'LR':
        P.reach('thigh.%s.01' % s, 'shin.%s.01' % s, 'foot.%s' % s, feet[s] + SIDE[s]*width, FWD + SIDE[s]*knees)
        P.setworldrot('foot.%s' % s, rots[s])
        turn_towards(P, 'foot.%s' % s, UP, toeout, 'toe.%s' % s, SIDE[s])


def hero(P, t):
    """Lead figure: wide bent legged stance, left forearm thrown straight up wagging a finger, right hand on the hip."""
    stance(P, 0.45, 0.2, 0.25, 0.3, 0.4)
    turn_towards(P, 'ribs', FWD, 0.12, 'upper_arm.L.01', UP)
    # raised arm: elbow out at shoulder height, forearm upright and leaning in over the head
    up = sway(UP - SIDE['L']*0.12 + FWD*0.08, FWD, 0.03*np.sin(t))
    arm(P, 'L', SIDE['L']*0.95 + UP*0.08 + FWD*0.1, up)
    hand(P, 'L', up + FWD*0.05, -SIDE['L'])
    curl(P, 'L', ['finger_middle', 'finger_ring', 'finger_pinky'], 1.9)
    curl(P, 'L', ['thumb'], 0.7)
    straighten(P, 'L', 'finger_index', P.pos('finger_middle.01.L.01') - P.pos('hand.L'))
    # "tsk tsk": the hand wags side to side three times over the two second loop, easing into each end and lingering there,
    # the forearm leading, the hand following and the finger trailing behind, a little bigger some swings than others
    def ease(x):
        return np.tanh(1.8*np.sin(x))/np.tanh(1.8)
    phase, amount = 3*t, 0.88 + 0.12*np.sin(t + 0.7)
    wag = ease(phase)*amount
    P.turn('forearm.L.01', FWD, 0.07*np.sin(phase + 0.4)*amount)
    P.turn('hand.L', FWD, 0.24*wag)
    P.turn('hand.L', SIDE['L'], 0.05*(1.0 - np.cos(2*phase)))  # dipping slightly at each end
    lag = ease(phase - 0.7)*amount - wag
    P.turn('finger_index.01.L.01', FWD, 0.18*lag)
    P.turn('finger_index.01.L.02', FWD, 0.12*lag)
    # hand on the hip: resting on top of the hip bone at the waist, elbow bent and thrust out to the side, fingers wrapping forwards
    waist = P.pos('spine')
    waist = np.array([P.pos('hips')[0], P.pos('hips')[1], waist[2]]) + SIDE['R']*1.35 + UP*0.2 + FWD*0.2
    P.reach('upper_arm.R.01', 'forearm.R.01', 'hand.R', waist, SIDE['R'] - FWD*0.25)
    P.aim('hand.R', 'finger_middle.01.R.01', norm(FWD*0.7 - UP*0.5 - SIDE['R']*0.4))
    curl(P, 'R', FINGERS, 0.6)
    # face the camera with the head cocked slightly
    turn_towards(P, 'head', FWD, 0.22, 'tag_head', SIDE['R'])
    turn_towards(P, 'head', RIGHT, 0.08, 'tag_head', FWD)
    P.turn('head', FWD, 0.035*ease(phase - 0.5)*amount)  # tilting in time with the wag
    for s in ('L', 'R'):
        constrain_arm(P, s)


def vanguard(P, t):
    """Flanking figures: arms held out wide with the forearms raised, palms open to the sky and tipped towards the viewer."""
    stance(P, 0.15, 0.0, 0.0, 0.1, 0.0)
    for s in ('L', 'R'):
        lift = 0.04*np.sin(t + (0 if s == 'L' else 1.3))
        arm(P, s, SIDE[s]*0.95 + UP*0.22 + FWD*0.25, SIDE[s]*0.35 + UP*(0.9 + lift) + FWD*0.2)
        # wrist bent back so the open palm faces the sky, fingers reaching outwards
        face_palm(P, s, SIDE[s]*0.6 + UP*0.6 + FWD*0.1, UP + SIDE[s]*0.15 + FWD*0.5)
        along = P.pos('finger_middle.01.%s.01' % s) - P.pos('hand.%s' % s)
        for finger in FINGERS:
            straighten(P, s, finger, along)
        curl(P, s, FINGERS, -0.15)
        curl(P, s, ['thumb'], -0.3)
        spread(P, s, 0.14)
        constrain_arm(P, s)


def resample(frames, count):
    """Stretch a looping animation to a different number of frames, interpolating between the originals."""
    out = np.zeros((count,) + frames.shape[1:])
    for f in range(count):
        x = f*len(frames)/count
        a, b, k = int(x) % len(frames), (int(x) + 1) % len(frames), x - int(x)
        out[f] = frames[a]*(1 - k) + frames[b]*k
        for p in range(frames.shape[1]):
            out[f, p, 3:7] = qslerp(frames[a, p, 3:7], frames[b, p, 3:7], k)
    return out


# name, pose, frames: the hero's pose plays at 30fps for its quick finger wag, everything else at the idle's 10fps
for name, func, count in (('pose_hero', hero, 60), ('pose_vanguard', vanguard, len(idle))):
    base = resample(idle, count)
    frames = np.zeros_like(base)
    for f in range(count):
        P = Pose(names, parents, base[f])
        func(P, 2*np.pi*f/count)
        frames[f] = P.local
    save_anim('%s/%s.iqm' % (out, name), name, parents, frames)
    print('wrote', name, frames.shape)
