#!/usr/bin/env python3
"""Generate the main menu's hero title pose and vanguard dance moves for the player model from its idle animation, needs numpy.

usage: src/scripts/makeposes.py data/actors/player data/actors/player [title|move...]
"""
import sys
import numpy as np
from iqmpose import load_joints, load_anim, save_anim, Pose, qaxis, qrot, qslerp, qmul, qconj

src, out, only = sys.argv[1], sys.argv[2], sys.argv[3:]
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


# the vanguard dance to the theme, We Are The Glitch: 174bpm with the kick on the even beats and the snare on the odd ones,
# each move two bars long
BPM, BEATS, BEATFRAMES = 174.0, 8, 12


def bend(P, joint, axis, angle, probe, direction):
    """Turn a joint about an axis, a positive angle moving the probe joint along a direction and a negative one against it."""
    key = ('bend', joint, probe, tuple(np.round(direction, 3)))
    if key not in CHOICES:
        before = P.pos(probe).copy()
        P.turn(joint, axis, 0.1)
        after = P.pos(probe).copy()
        P.turn(joint, axis, -0.1)
        CHOICES[key] = 1.0 if np.dot(after - before, direction) >= 0 else -1.0
    P.turn(joint, axis, angle*CHOICES[key])


# each hand's finger and palm directions in its own space, taken from the first frame
HANDAXES = {}


def orient(P, s, fingers, facing):
    """Turn a hand so its fingers point along a direction with the palm facing as close to another as it can."""
    hd = 'hand.%s' % s
    if s not in HANDAXES:
        inv = qconj(P.worldrot(hd))
        HANDAXES[s] = (qrot(inv, P.pos('finger_middle.01.%s.01' % s) - P.pos(hd)), qrot(inv, palm(P, s)))
    lf, ln = HANDAXES[s]
    P.setworldrot(hd, matquat(frame(fingers, facing) @ frame(lf, ln).T))


def smoothstep(x):
    x = np.clip(x, 0.0, 1.0)
    return x*x*(3 - 2*x)


def nodcurve(p):
    """A hit over one cycle: down on the beat, held there an instant, rising easily and then thrown down hard into the next."""
    p %= 1.0
    if p < 0.08:
        return 1.0
    if p < 0.6:
        return 1.0 - smoothstep((p - 0.08)/0.52)
    return ((p - 0.6)/0.4)**2.2


def mix(a, b, k):
    return norm(np.asarray(a)*(1 - k) + np.asarray(b)*k)


def horns(P, s, amount=1.0):
    curl(P, s, ['finger_middle', 'finger_ring'], 1.9)
    curl(P, s, ['finger_index', 'finger_pinky'], 1.9*(1 - amount) - 0.05*amount)
    curl(P, s, ['thumb'], 0.7 + 0.2*amount)


def nod(P, amount, back=0.0):
    """Pitch the neck and head forward, or back when negative."""
    bend(P, 'neck', RIGHT, 0.45*amount - back, 'tag_head', FWD)
    bend(P, 'head', RIGHT, 0.55*amount - back, 'tag_head', FWD)


def headbang(P, b):
    """Headbanging, each nod slammed down on the snare and the knees bouncing on every beat, throwing up the horns with the
    right hand for a bar and then the left, the other fist hammering down low in front."""
    hit = nodcurve((b - 1)/2)
    lead = nodcurve((b - 1)/2 + 0.04)  # the body drops just ahead of the head, which whips down after it
    bounce = nodcurve(b)
    # the right hand is up for the first bar and the left for the second, swapping over through the last beat of each
    right = 1.0 - smoothstep((b - 3.05)/0.9) + smoothstep((b - 7.05)/0.9)
    lean = 2*right - 1
    stance(P, 0.9, 0.4 + 1.3*lead + 0.5*bounce, 0.5*lean, 0.35, 0.6)
    bend(P, 'spine', RIGHT, 0.24*lead - 0.04, 'tag_head', FWD)
    bend(P, 'ribs', RIGHT, 0.32*lead - 0.08, 'tag_head', FWD)
    bend(P, 'ribs', FWD, 0.12*lean, 'tag_head', SIDE['R'])
    bend(P, 'ribs', UP, 0.2*lean, 'upper_arm.R.01', FWD)  # the raised arm's shoulder turns forward
    nod(P, hit, 0.25)
    bend(P, 'head', FWD, 0.1*lean, 'tag_head', SIDE['L'])
    for s, up in (('R', right), ('L', 1.0 - right)):
        side = SIDE[s]
        # raised: drawn back down to the temple and punched straight up into the air on the snare
        # low: elbow out and the fist hammering down in front of the hip with it
        upper = mix(-UP*0.7 + side*0.5 + FWD*0.3, side*0.6 + UP*(0.2 + 0.75*hit) + FWD*0.25, up)
        fore = mix(FWD*0.75 + UP*(0.55 - 0.9*hit) - side*0.1, UP*(0.25 + 0.75*hit) + FWD*0.55*(1 - hit) - side*0.3*(1 - hit), up)
        arm(P, s, upper, fore)
        orient(P, s, fore + FWD*0.1*up, mix(-side - UP*0.25, -FWD*0.8 - side*0.3, up))
        horns(P, s, up)
        constrain_arm(P, s)


def jump(P, b):
    """Pogoing, landing every snare and leaving the ground in between with both fists thrown up at the top, the last
    jump of the two bars the biggest."""
    u = ((b - 1)/2 + 0.05) % 1.0  # the landing is just before the snare, sinking into it
    big = 1.35 if 5.9 <= (b - 1) % 8 < 7.9 else 1.0
    crouch = 1.6*np.sin(np.pi*u/0.3) if u < 0.3 else 0.0
    air = (u - 0.3)/0.7 if u >= 0.3 else 0.0
    lift = 2.6*big*4*air*(1 - air)
    stance(P, 0.7, 0.3 + crouch, 0.0, 0.3, 0.5, lift, 0.5*lift)
    up = smoothstep(lift/(1.8*big))
    bend(P, 'spine', RIGHT, 0.2*crouch/1.6 - 0.05*up, 'tag_head', FWD)
    bend(P, 'ribs', RIGHT, 0.2*crouch/1.6 - 0.1*up, 'tag_head', FWD)
    nod(P, crouch/1.6, 0.2*up)
    for s in 'LR':
        side = SIDE[s]
        upper = mix(-UP*0.5 + side*0.7 + FWD*0.35, side*0.4 + UP*0.9 + FWD*0.1, up)
        fore = mix(UP*0.7 + FWD*0.6 - side*0.4, UP + side*0.12 + FWD*0.05, up)
        arm(P, s, upper, fore)
        orient(P, s, fore, mix(-side - FWD*0.3, -FWD - side*0.4, up))
        horns(P, s, up if big > 1 else 0.0)
        constrain_arm(P, s)


def guitar(P, b):
    """Air guitar, legs wide and leaning back, strumming down on every beat with the head nodding on the snare, and a
    windmill over the top into the last snare of the two bars."""
    hit = nodcurve((b - 1)/2)
    bounce = nodcurve(b)
    x = (b - 5.5)/1.5  # the windmill, from half way through beat five round into the snare on beat seven
    wind = smoothstep((b - 5.3)/0.3)*(1 - smoothstep((b - 7.05)/0.5))
    stance(P, 1.4, 0.9 + 0.5*bounce + 0.5*hit, -0.2, 0.45, 0.8)
    bend(P, 'spine', RIGHT, -0.1 - 0.12*wind + 0.1*hit, 'tag_head', FWD)
    bend(P, 'ribs', RIGHT, -0.08 + 0.12*hit, 'tag_head', FWD)
    bend(P, 'ribs', UP, 0.15, 'upper_arm.L.01', FWD)  # the neck end of the guitar turned out towards the crowd
    nod(P, hit, 0.2 + 0.2*wind)
    # fretting hand out along the neck to the left, sliding along it each bar
    slide = 0.15*np.sin(np.pi*b/4)
    arm(P, 'L', SIDE['L']*0.75 + FWD*0.55 - UP*0.15, SIDE['L']*(0.6 + slide) + FWD*0.55 + UP*0.35)
    orient(P, 'L', SIDE['L'] + UP*0.4 + FWD*0.3, -SIDE['R']*0.5 + UP*0.2 - FWD*0.6)
    curl(P, 'L', FINGERS, 1.1)
    curl(P, 'L', ['thumb'], 0.5)
    constrain_arm(P, 'L')
    # strumming hand down across the strings in front of the hips on every beat
    strum = nodcurve(b)
    upper = mix(-UP*0.75 + SIDE['R']*0.35 + FWD*0.35, -UP*0.9 + SIDE['R']*0.4 + FWD*0.1, strum)
    fore = mix(FWD*0.6 + UP*0.4 - SIDE['R']*0.6, FWD*0.5 - UP*0.6 - SIDE['R']*0.1, strum)
    if wind > 0:
        # the straight arm swung back, up over the top and down in front
        a = 2*np.pi*smoothstep(x)
        straight = norm(-UP*np.cos(a) - FWD*np.sin(a) + SIDE['R']*0.45)
        trailing = UP*np.sin(a) - FWD*np.cos(a)  # the hand trailing a little behind the elbow
        upper, fore = mix(upper, straight, wind), mix(fore, straight - trailing*0.35, wind)
    arm(P, 'R', upper, fore)
    orient(P, 'R', fore, -SIDE['R']*0.5 - FWD*0.5)
    curl(P, 'R', FINGERS, 1.5)
    curl(P, 'R', ['thumb'], 0.9)
    constrain_arm(P, 'R')


def clap(P, b):
    """Clapping over the head on every snare, bouncing on every beat with the hips swaying from side to side."""
    hit = nodcurve((b - 1)/2)
    bounce = nodcurve(b)
    sway = np.sin(np.pi*(b - 1)/2)  # over to one side on one snare and the other on the next
    stance(P, 1.0, 0.5 + 0.7*bounce + 0.3*hit, 0.7*sway, 0.35, 0.6)
    bend(P, 'ribs', FWD, -0.1*sway, 'tag_head', SIDE['R'])
    bend(P, 'ribs', RIGHT, -0.08 + 0.1*hit, 'tag_head', FWD)
    nod(P, 0.8*hit, 0.25)
    bend(P, 'head', FWD, 0.12*sway, 'tag_head', SIDE['R'])
    for s in 'LR':
        side = SIDE[s]
        # hands thrown apart on the kick, meeting high over the head on the snare
        upper = mix(side*0.85 + UP*0.55 + FWD*0.2, side*0.35 + UP*0.9 + FWD*0.2, hit)
        fore = mix(side*0.55 + UP*0.85 + FWD*0.1, UP*0.65 - side*0.75 + FWD*0.15, hit)
        arm(P, s, upper, fore)
        orient(P, s, mix(fore, UP, 0.5), mix(FWD*0.6 - side*0.4, -side, hit))
        curl(P, s, FINGERS, -0.1)
        curl(P, s, ['thumb'], -0.2)
        constrain_arm(P, s)


def punch(P, b):
    """Punching the air up in front, the right fist on the kick and the left on the snare, the shoulders turning into each one
    and the head nodding down on the snare."""
    hit = nodcurve((b - 1)/2)
    bounce = nodcurve(b)
    out = {'R': nodcurve(b/2), 'L': nodcurve((b - 1)/2)}
    stance(P, 1.1, 0.6 + 0.6*bounce + 0.3*hit, 0.2*(out['L'] - out['R']), 0.35, 0.6)
    bend(P, 'spine', RIGHT, 0.1 + 0.1*bounce, 'tag_head', FWD)
    bend(P, 'ribs', UP, 0.3*(out['R'] - out['L']), 'upper_arm.R.01', FWD)
    nod(P, hit, 0.2)
    for s in 'LR':
        side, e = SIDE[s], out[s]
        reach = side*0.3 + UP*0.75 + FWD*0.6
        upper = mix(-UP*0.6 + side*0.45 + FWD*0.25, reach, e)
        fore = mix(UP*0.85 + FWD*0.35 - side*0.25, reach + UP*0.15, e)
        arm(P, s, upper, fore)
        orient(P, s, fore, mix(-side - FWD*0.2, -UP*0.3 - FWD*0.6 - side*0.4, e))
        curl(P, s, FINGERS, 1.9)
        curl(P, s, ['thumb'], 0.7)
        constrain_arm(P, s)


def stomp(P, b):
    """Stomping, the knee raised high through the beat before each snare and slammed down onto it, the right foot and then
    the left, both fists hammering down with it and the body hunched over."""
    u = (b - 1)/2
    q = u - np.floor(u)
    leg = 'R' if int(np.floor(u) + 1) % 2 else 'L'  # the foot coming down on the next snare
    if q < 0.45:
        raised = 0.0
    elif q < 0.88:
        raised = smoothstep((q - 0.45)/0.35)
    else:
        raised = ((1 - q)/0.12)**1.5
    hit = nodcurve(u)
    bounce = nodcurve(b)
    stance(P, 0.9, 0.7 + 0.8*hit + 0.2*bounce, 0.5*(1 if leg == 'L' else -1)*raised, 0.35, 0.5, step={leg: 3.2*raised})
    bend(P, 'spine', RIGHT, 0.15 + 0.15*hit, 'tag_head', FWD)
    bend(P, 'ribs', RIGHT, 0.1 + 0.2*hit, 'tag_head', FWD)
    nod(P, hit, 0.15)
    for s in 'LR':
        side = SIDE[s]
        # fists drawn up to the shoulders as the knee comes up and driven down beside the hips on the stomp
        k = max(raised, 0.0)
        upper = mix(-UP*0.85 + side*0.4 + FWD*0.1, -UP*0.3 + side*0.8 + FWD*0.3, k)
        fore = mix(-UP*0.5 + FWD*0.6 + side*0.15, UP*0.9 + FWD*0.3, k)
        arm(P, s, upper, fore)
        orient(P, s, fore, -side)
        curl(P, s, FINGERS, 1.9)
        curl(P, s, ['thumb'], 0.7)
        constrain_arm(P, s)


def wave(P, b):
    """Both arms up high and swaying from side to side together, arriving at each side on the snare, the hips swinging the
    other way and the knees bouncing on every beat."""
    hit = nodcurve((b - 1)/2)
    bounce = nodcurve(b)
    sway = np.tanh(2.2*np.sin(np.pi*(b - 1)/2))/np.tanh(2.2)  # over to the right on one snare and the left on the next
    stance(P, 1.0, 0.5 + 0.6*bounce, -0.8*sway, 0.35, 0.6)
    bend(P, 'ribs', FWD, 0.18*sway, 'tag_head', SIDE['R'])
    nod(P, 0.6*hit, 0.25)
    bend(P, 'head', FWD, 0.15*sway, 'tag_head', SIDE['R'])
    lean = SIDE['R']*0.75*sway
    for s in 'LR':
        side = SIDE[s]
        # elbows bent a little so the arm has a hinge to swing on, the forearms reaching further into the sway
        upper = norm(UP*0.8 + side*0.55 + lean*0.6 + FWD*0.25)
        fore = norm(UP + lean*1.2 - side*0.1 + FWD*0.4)
        arm(P, s, upper, fore)
        # open hands, palms to the crowd, trailing the sway a little
        orient(P, s, fore - lean*0.3, FWD)
        curl(P, s, FINGERS, -0.05)
        curl(P, s, ['thumb'], -0.2)
        spread(P, s, 0.1)
        constrain_arm(P, s)


def spin(P, b):
    """Grooving through a bar with the shoulders rolling and the fists low, then crouching on the next snare and leaping into a
    full turn, landing with the arms flung wide on the last snare."""
    hit = nodcurve((b - 1)/2)
    bounce = nodcurve(b)
    # the leap: crouching into the snare on beat five, in the air from just after it until the snare on beat seven
    load = smoothstep((b - 4.2)/0.8)*(1 - smoothstep((b - 5.0)/0.3))
    air = np.clip((b - 5.15)/1.8, 0.0, 1.0)
    lift = 2.4*4*air*(1 - air)
    land = smoothstep((b - 6.6)/0.4)*(1 - smoothstep((b - 7.3)/0.7))
    groove = 1 - smoothstep((b - 4.0)/0.6) + smoothstep((b - 7.4)/0.6)
    roll = np.sin(np.pi*b)  # the shoulders rolling, one forward on each beat
    step = 0.7*np.sin(np.pi*(b - 0.5))*groove  # stepping from side to side, arriving on each beat
    stance(P, 0.8, 0.5 + groove*(0.7*bounce + 0.4*hit) + 1.4*load + 1.2*land, step, 0.3, 0.5, lift, 0.6*lift)
    bend(P, 'ribs', FWD, -0.12*step, 'tag_head', SIDE['R'])
    bend(P, 'spine', RIGHT, 0.1 + 0.15*load + 0.1*land, 'tag_head', FWD)
    bend(P, 'ribs', UP, 0.32*roll*groove, 'upper_arm.R.01', FWD)
    nod(P, groove*hit + 0.3*load, 0.2 + 0.2*land)
    tuck = smoothstep(lift/1.2)
    for s in 'LR':
        side = SIDE[s]
        swing = roll*(1 if s == 'R' else -1)*groove
        # low fists swinging with the shoulders, drawn into the chest through the turn and thrown out wide on landing
        upper = norm(-UP*0.75 + side*0.4 + FWD*(0.2 + 0.6*swing))
        fore = norm(FWD*0.8 + UP*(0.35 + 0.6*swing) - side*0.2)
        upper = mix(upper, -UP*0.6 + side*0.5 + FWD*0.3, tuck)
        fore = mix(fore, UP*0.6 + FWD*0.3 - side*0.7, tuck)
        upper = mix(upper, side + UP*0.45 + FWD*0.2, land)
        fore = mix(fore, side + UP*0.6 + FWD*0.1, land)
        arm(P, s, upper, fore)
        orient(P, s, fore, mix(-side*0.6 - FWD*0.6, FWD + UP*0.3, land))
        curl(P, s, FINGERS, 1.9*(1 - land) - 0.05*land)
        curl(P, s, ['thumb'], 0.7*(1 - land))
        constrain_arm(P, s)
    P.turn('hips', UP, 2*np.pi*smoothstep(air))


def raise_leg(P, s, up, fwd, pole, out=np.zeros(3)):
    """Lift a planted foot up and forward, the knee bending towards a pole direction and the sole kept as it was."""
    foot = P.pos('foot.%s' % s).copy()
    rot = P.worldrot('foot.%s' % s)
    P.reach('thigh.%s.01' % s, 'shin.%s.01' % s, 'foot.%s' % s, foot + UP*up + FWD*fwd + out, pole)
    P.setworldrot('foot.%s' % s, rot)


def on_hip(P, s):
    """Rest a hand on top of the hip bone at the waist, the elbow thrust out to the side and the fingers wrapping forwards."""
    side = SIDE[s]
    waist = np.array([P.pos('hips')[0], P.pos('hips')[1], P.pos('spine')[2]]) + side*1.35 + UP*0.2 + FWD*0.2
    P.reach('upper_arm.%s.01' % s, 'forearm.%s.01' % s, 'hand.%s' % s, waist, side - FWD*0.25)
    P.aim('hand.%s' % s, 'finger_middle.01.%s.01' % s, norm(FWD*0.7 - UP*0.5 - side*0.4))
    curl(P, s, FINGERS, 0.6)


def drum(P, b):
    """Air drums, the right stick on every beat and the left on the snare, crouched over the kit, the right swinging up to
    crash the cymbal on the last snare."""
    hit = nodcurve((b - 1)/2)
    crash = smoothstep((b - 6.1)/0.8)*(1 - smoothstep((b - 7.25)/0.6))
    stance(P, 1.3, 1.0 + 0.6*nodcurve(b) + 0.6*hit, 0.0, 0.4, 0.7)
    bend(P, 'spine', RIGHT, 0.15 + 0.15*hit, 'tag_head', FWD)
    bend(P, 'ribs', FWD, 0.1*np.sin(np.pi*b/2), 'tag_head', SIDE['R'])
    bend(P, 'ribs', UP, -0.2*crash, 'upper_arm.R.01', FWD)
    nod(P, hit, 0.15)
    for s, k, c in (('R', nodcurve(b), crash), ('L', hit, 0.0)):
        side = SIDE[s]
        upper = mix(-UP*0.4 + side*0.65 + FWD*0.35, side*0.7 + UP*0.55 + FWD*0.45, c)
        fore = mix(FWD*0.3 + UP*1.0 - side*0.1, FWD*0.45 - UP*0.9 - side*0.35, k)  # the stick raised high, then struck down
        fore = mix(fore, side*0.45 + UP*(0.5 - 0.6*hit) + FWD*0.6, c)
        arm(P, s, upper, fore)
        orient(P, s, fore, -side - UP*0.3)
        curl(P, s, FINGERS, 1.8)
        curl(P, s, ['thumb'], 0.8)
        constrain_arm(P, s)


def mosh(P, b):
    """Moshing, barging a shoulder out to one side on one snare and the other on the next, elbows up and head down."""
    hit = nodcurve((b - 1)/2)
    barge = np.tanh(2.5*np.sin(np.pi*(b - 1)/2))/np.tanh(2.5)
    stance(P, 1.2, 0.9 + 0.9*hit + 0.4*nodcurve(b), 1.7*barge, 0.35, 0.6)
    bend(P, 'spine', RIGHT, 0.2 + 0.15*hit, 'tag_head', FWD)
    bend(P, 'ribs', FWD, 0.38*barge, 'tag_head', SIDE['R'])
    bend(P, 'ribs', UP, 0.4*barge, 'upper_arm.L.01', FWD)
    nod(P, 0.6 + 0.4*hit, 0.0)
    bend(P, 'head', FWD, -0.15*barge, 'tag_head', SIDE['R'])
    for s, sign in (('R', 1), ('L', -1)):
        side, lead = SIDE[s], max(0.0, barge*sign)
        upper = side*0.85 + UP*(0.2 + 0.35*lead) + FWD*(0.25 - 0.3*lead)
        fore = -side*0.7 + FWD*0.6 + UP*0.1
        arm(P, s, upper, fore)
        orient(P, s, fore, -UP - FWD*0.3)
        curl(P, s, FINGERS, 1.9)
        curl(P, s, ['thumb'], 0.7)
        constrain_arm(P, s)


def kick(P, b):
    """High kicks out in front on the snares, the right leg and then the left, leaning back with the arms out wide."""
    u = (b - 1)/2
    n = int(np.round(u))
    leg = 'R' if n % 2 == 0 else 'L'
    k = smoothstep(1 - abs(u - n)/0.32)
    shift = 0.6*k*(1 if leg == 'L' else -1)  # the weight over the standing leg
    stance(P, 0.6, 0.5 + 0.4*nodcurve(b)*(1 - k), shift, 0.3, 0.4)
    bend(P, 'spine', RIGHT, -0.25*k + 0.05, 'tag_head', FWD)
    bend(P, 'ribs', RIGHT, -0.1*k, 'tag_head', FWD)
    nod(P, 0.5*nodcurve(b)*(1 - k) + 0.3*k, 0.15)
    raise_leg(P, leg, 6.5*k, 4.0*k, FWD + UP, SIDE[leg]*2.8*k)  # out on the diagonal, which reads from the front
    for s in 'LR':
        side = SIDE[s]
        upper = side*0.9 + UP*(0.05 + 0.2*k) + FWD*0.1
        fore = side*0.7 + UP*(0.4 + 0.3*k) + FWD*0.2
        arm(P, s, upper, fore)
        orient(P, s, fore, -UP + FWD*0.3)
        curl(P, s, FINGERS, 1.9)
        curl(P, s, ['thumb'], 0.7)
        constrain_arm(P, s)


def march(P, b):
    """Marching on the spot with high knees, a foot landing on every beat, the fists swinging up opposite each knee and the
    head nodding on the snare."""
    swing = np.sin(np.pi*b)
    lifts = {'R': max(0.0, swing)**1.3, 'L': max(0.0, -swing)**1.3}
    hit = nodcurve((b - 1)/2)
    stance(P, 0.6, 0.6 + 0.7*nodcurve(b), 0.45*swing, 0.25, 0.4)
    bend(P, 'ribs', UP, 0.3*swing, 'upper_arm.L.01', FWD)
    bend(P, 'spine', RIGHT, 0.1, 'tag_head', FWD)
    nod(P, hit, 0.2)
    for s in 'LR':
        raise_leg(P, s, 4.2*lifts[s], 1.6*lifts[s], FWD + SIDE[s]*0.5, SIDE[s]*0.8*lifts[s])
    for s, other in (('R', 'L'), ('L', 'R')):
        side, f = SIDE[s], lifts[other] - lifts[s]
        upper = -UP*0.7 + side*0.25 + FWD*(0.2 + 0.6*f)
        fore = FWD*0.6 + UP*(0.55 + 0.45*f) - side*0.15
        arm(P, s, upper, fore)
        orient(P, s, fore, -side)
        curl(P, s, FINGERS, 1.9)
        curl(P, s, ['thumb'], 0.7)
        constrain_arm(P, s)


def twist(P, b):
    """The twist, the hips and shoulders turning against each other on every beat, working down low over a bar and back up."""
    turn = 0.45*np.sin(np.pi*b)
    low = 0.5 - 0.5*np.cos(np.pi*b/4)
    stance(P, 0.5, 0.5 + 2.4*low + 0.3*nodcurve(b), 0.0, 0.2, 0.3, turn=turn)
    P.turn('spine', UP, -1.6*turn)
    bend(P, 'spine', RIGHT, 0.12 + 0.15*low, 'tag_head', FWD)
    nod(P, 0.6*nodcurve((b - 1)/2), 0.25)
    for s in 'LR':
        side = SIDE[s]
        upper = -UP*0.5 + side*0.75 + FWD*0.2
        fore = FWD*0.8 - side*0.35 + UP*0.15
        arm(P, s, upper, fore)
        orient(P, s, fore, -UP)
        curl(P, s, FINGERS, 1.7)
        curl(P, s, ['thumb'], 0.6)
        constrain_arm(P, s)


def roll(P, b):
    """Rolling the head round in circles, bent over with the hands braced on the knees, down at the bottom on each snare."""
    a = np.pi*(b - 1)
    stance(P, 1.3, 1.5 + 0.3*nodcurve(b), 0.0, 0.4, 0.8)
    bend(P, 'spine', RIGHT, 0.35, 'tag_head', FWD)
    bend(P, 'ribs', RIGHT, 0.2 + 0.15*np.cos(a), 'tag_head', FWD)
    bend(P, 'ribs', FWD, 0.12*np.sin(a), 'tag_head', SIDE['R'])
    bend(P, 'neck', RIGHT, 0.2 + 0.3*np.cos(a), 'tag_head', FWD)
    bend(P, 'head', RIGHT, 0.2 + 0.4*np.cos(a), 'tag_head', FWD)
    bend(P, 'neck', FWD, 0.25*np.sin(a), 'tag_head', SIDE['R'])
    bend(P, 'head', FWD, 0.3*np.sin(a), 'tag_head', SIDE['R'])
    for s in 'LR':
        side = SIDE[s]
        knee = P.pos('shin.%s.01' % s)
        P.reach('upper_arm.%s.01' % s, 'forearm.%s.01' % s, 'hand.%s' % s, knee + UP*1.3 + side*0.3 - FWD*0.1, side - FWD*0.3)
        orient(P, s, FWD*0.5 - UP*0.7 - side*0.3, -UP - side*0.2)
        curl(P, s, FINGERS, 0.5)
        constrain_arm(P, s)


def bass(P, b):
    """Bass slung low, stepping from side to side and bobbing on every beat, leaning back, plucking on the eighths."""
    step = np.tanh(2*np.sin(np.pi*(b - 0.5)/2))/np.tanh(2)
    stance(P, 1.3, 1.1 + 0.6*nodcurve(b), 0.7*step, 0.4, 0.7)
    bend(P, 'spine', RIGHT, -0.15, 'tag_head', FWD)
    bend(P, 'ribs', FWD, -0.1*step, 'tag_head', SIDE['R'])
    nod(P, 0.4*nodcurve(b) + 0.4*nodcurve((b - 1)/2), 0.25)
    slide = 0.2*np.sin(np.pi*b/4)
    arm(P, 'L', SIDE['L']*0.5 - UP*0.6 + FWD*0.35, SIDE['L']*(0.8 + slide) + FWD*0.4 - UP*0.1)
    orient(P, 'L', SIDE['L'] + FWD*0.3, -SIDE['R']*0.4 + UP*0.3 - FWD*0.6)
    curl(P, 'L', FINGERS, 1.0)
    curl(P, 'L', ['thumb'], 0.5)
    constrain_arm(P, 'L')
    pluck = nodcurve(2*b)
    fore = FWD*0.8 - SIDE['R']*0.35 - UP*(0.2 + 0.2*pluck)
    arm(P, 'R', -UP*0.85 + SIDE['R']*0.3 + FWD*0.25, fore)
    orient(P, 'R', fore - UP*0.2, -SIDE['R']*0.5 - FWD*0.5)
    curl(P, 'R', ['finger_ring', 'finger_pinky'], 1.6)
    curl(P, 'R', ['finger_index', 'finger_middle'], 0.5 + 0.4*pluck)
    curl(P, 'R', ['thumb'], 0.4)
    constrain_arm(P, 'R')


def shake(P, b):
    """Hopping on every beat with both fists up high, shaking them, thrust higher on each snare."""
    hit = nodcurve((b - 1)/2)
    x = b % 1.0
    lift = 0.7*4*x*(1 - x)
    stance(P, 0.8, 0.4 + 0.6*nodcurve(b), 0.0, 0.3, 0.5, lift, 0.4*lift)
    bend(P, 'ribs', RIGHT, -0.1, 'tag_head', FWD)
    nod(P, 0.7*hit, 0.3)
    for s, phase in (('R', 0.0), ('L', np.pi)):
        side = SIDE[s]
        wobble = np.sin(2*np.pi*2*b + phase)
        upper = side*0.45 + UP*(0.75 + 0.3*hit) + FWD*0.15
        fore = UP + side*0.1 + FWD*(0.15 + 0.15*wobble) + side*0.12*wobble
        arm(P, s, upper, fore)
        orient(P, s, fore, -FWD - side*0.3)
        curl(P, s, FINGERS, 1.9)
        curl(P, s, ['thumb'], 0.7)
        constrain_arm(P, s)


# for the quiet parts of the song: the intro, the bridge, and the outro
def calm_sway(P, b):
    """Swaying gently from side to side over each bar, the arms hanging loose and the head tipping in time."""
    sw = np.sin(np.pi*(b - 0.5)/2)
    stance(P, 0.5, 0.1 + 0.15*nodcurve((b - 1)/2), 0.6*sw, 0.25, 0.3)
    bend(P, 'ribs', FWD, -0.08*sw, 'tag_head', SIDE['R'])
    bend(P, 'head', FWD, 0.12*sw, 'tag_head', SIDE['R'])
    nod(P, 0.25*nodcurve((b - 1)/2), 0.05)
    for s in 'LR':
        side = SIDE[s]
        upper = -UP + side*0.15 + FWD*0.05 - SIDE['R']*0.12*sw
        fore = -UP*0.9 + FWD*0.35 - SIDE['R']*0.15*np.sin(np.pi*(b - 0.9)/2)  # the hands trailing the sway
        arm(P, s, upper, fore)
        orient(P, s, fore, -side)
        curl(P, s, FINGERS, 0.5)
        curl(P, s, ['thumb'], 0.3)
        constrain_arm(P, s)


def calm_tap(P, b):
    """Weight on one leg with a hand on the hip, nodding along on the snare and tapping the other hand on the thigh on every
    beat."""
    hit = nodcurve((b - 1)/2)
    stance(P, 0.5, 0.15 + 0.1*hit, 0.55, 0.25, 0.3)
    bend(P, 'ribs', FWD, -0.08, 'tag_head', SIDE['R'])
    bend(P, 'head', FWD, 0.1, 'tag_head', SIDE['L'])
    nod(P, 0.35*hit, 0.05)
    on_hip(P, 'L')
    constrain_arm(P, 'L')
    lifted = 1 - nodcurve(b)
    fore = -UP*0.8 + FWD*(0.35 + 0.35*lifted) - SIDE['R']*0.1
    arm(P, 'R', -UP + SIDE['R']*0.2 + FWD*0.05, fore)
    orient(P, 'R', fore, -SIDE['R'] - FWD*0.2)
    curl(P, 'R', FINGERS, 0.3)
    constrain_arm(P, 'R')


def calm_snap(P, b):
    """Clicking the fingers on the snare with the forearms up in front, the shoulders bopping on every beat and stepping
    gently from side to side."""
    hit = nodcurve((b - 1)/2)
    stance(P, 0.5, 0.1 + 0.2*hit, 0.35*np.sin(np.pi*(b - 1)/2), 0.25, 0.3)
    bend(P, 'ribs', UP, 0.1*np.sin(np.pi*b), 'upper_arm.R.01', FWD)
    nod(P, 0.3*hit, 0.05)
    for s in 'LR':
        side = SIDE[s]
        fore = FWD*0.7 + UP*(0.55 + 0.2*(1 - hit)) + side*0.15
        arm(P, s, -UP*0.85 + side*0.3 + FWD*0.15, fore)
        orient(P, s, fore + UP*0.3, -side - FWD*0.3)
        # the thumb and middle finger pinched together, the middle finger snapping down into the palm on the snare
        curl(P, s, ['finger_ring', 'finger_pinky'], 1.5)
        curl(P, s, ['finger_index'], 0.4)
        curl(P, s, ['finger_middle'], 0.6 + 1.0*hit)
        curl(P, s, ['thumb'], 0.6 - 0.4*hit)
        constrain_arm(P, s)


def calm_lighter(P, b):
    """Holding a lighter up high and swaying it slowly from side to side, the body swaying with it and the head tipped back
    to watch it, the other arm hanging loose."""
    sw = np.sin(2*np.pi*b/BEATS)
    hit = nodcurve((b - 1)/2)
    stance(P, 0.5, 0.1 + 0.1*hit, 0.5*sw, 0.25, 0.3)
    bend(P, 'ribs', FWD, -0.12*sw, 'tag_head', SIDE['R'])
    nod(P, 0.15*hit, 0.18)
    bend(P, 'head', FWD, 0.1*sw, 'tag_head', SIDE['R'])
    # the elbow out and bent, the forearm leaning in over the head, so the arm has a hinge to sway on
    up = norm(UP + SIDE['R']*(-0.15 + 0.45*sw) + FWD*0.2)
    arm(P, 'R', SIDE['R']*(0.65 + 0.3*sw) + UP*0.75 + FWD*0.2, up)
    orient(P, 'R', up, -FWD*0.8 - SIDE['R']*0.3)
    curl(P, 'R', FINGERS, 1.9)
    curl(P, 'R', ['thumb'], -0.1)  # thumb up on the wheel
    constrain_arm(P, 'R')
    fore = -UP*0.9 + FWD*0.3 - SIDE['R']*0.15*np.sin(2*np.pi*(b - 0.6)/BEATS)
    arm(P, 'L', -UP + SIDE['L']*0.15 + FWD*0.05, fore)
    orient(P, 'L', fore, -SIDE['L'])
    curl(P, 'L', FINGERS, 0.5)
    constrain_arm(P, 'L')


def calm_nod(P, b):
    """Hands clasped behind the back, nodding along on every beat and deeper on the snare, shifting the weight each bar."""
    hit = nodcurve((b - 1)/2)
    stance(P, 0.55, 0.1 + 0.1*nodcurve(b), 0.4*np.sin(np.pi*b/4), 0.3, 0.3)
    bend(P, 'ribs', RIGHT, -0.05, 'tag_head', FWD)
    nod(P, 0.15*nodcurve(b) + 0.25*hit, 0.05)
    for s in 'LR':
        side = SIDE[s]
        back = np.array([P.pos('hips')[0], P.pos('hips')[1], P.pos('spine')[2]]) + side*0.35 - FWD*1.6 - UP*0.6
        P.reach('upper_arm.%s.01' % s, 'forearm.%s.01' % s, 'hand.%s' % s, back, side - FWD)
        orient(P, s, -side - UP*0.3, -FWD)
        curl(P, s, FINGERS, 0.8)
        constrain_arm(P, s)


def calm_rock(P, b):
    """Rocking forwards on the snare and back on the kick, heels and toes planted, the arms swinging loose behind the body."""
    rk = np.sin(np.pi*(b - 0.5))  # forwards arriving on the snare, back on the kick
    stance(P, 0.55, 0.15 + 0.1*nodcurve(b), 0.0, 0.3, 0.3, rock=0.45*rk)
    bend(P, 'spine', RIGHT, 0.06*rk, 'tag_head', FWD)
    nod(P, 0.3*nodcurve((b - 1)/2), 0.1)
    for s in 'LR':
        side = SIDE[s]
        lag = np.sin(np.pi*(b - 0.8))
        upper = -UP + side*0.15 - FWD*0.12*lag
        fore = -UP*0.85 + FWD*(0.3 - 0.2*lag)
        arm(P, s, upper, fore)
        orient(P, s, fore, -side)
        curl(P, s, FINGERS, 0.5)
        constrain_arm(P, s)


def calm_stretch(P, b):
    """Stretching, the arms raised slowly overhead with the hands meeting, bending over to one side and then the other, and
    lowered again."""
    up = smoothstep(b/2.4)*(1 - smoothstep((b - 5.6)/2.4))
    w = smoothstep((b - 2.0)/0.6)*(1 - smoothstep((b - 5.4)/0.6))
    lean = w*np.sin(np.pi*(b - 2.4)/1.6)  # over to one side and then the other while the hands are up
    stance(P, 0.7, 0.1, -0.5*lean, 0.3, 0.3)
    bend(P, 'spine', FWD, 0.15*lean, 'tag_head', SIDE['R'])
    bend(P, 'ribs', FWD, 0.35*lean, 'tag_head', SIDE['R'])
    nod(P, 0.0, 0.15*up)
    for s in 'LR':
        side = SIDE[s]
        upper = mix(-UP + side*0.25 + FWD*0.1, UP*0.9 + side*0.4 + FWD*0.1, up)
        fore = mix(-UP*0.6 + FWD*0.6 - side*0.1, UP*0.7 - side*0.7 + FWD*0.1, up)
        arm(P, s, upper, fore)
        orient(P, s, mix(fore, UP - side*0.3, up), mix(-side, -side*0.3 + UP*0.6, up))
        curl(P, s, FINGERS, 0.5*(1 - up))
        constrain_arm(P, s)


def calm_look(P, b):
    """Hands on the hips, looking round to one side for a bar and the other for the next, nodding along on the snare."""
    look = np.tanh(2.5*np.sin(np.pi*b/4))/np.tanh(2.5)
    hit = nodcurve((b - 1)/2)
    stance(P, 0.6, 0.12 + 0.1*hit, -0.3*look, 0.3, 0.3)
    P.turn('ribs', UP, 0.12*look)
    P.turn('neck', UP, 0.3*look)
    P.turn('head', UP, 0.3*look)
    nod(P, 0.25*hit, 0.05)
    for s in 'LR':
        on_hip(P, s)
        constrain_arm(P, s)


def stance(P, width, crouch, shift, toeout, knees, lift=0.0, tuck=0.0, step=None, turn=0.0, rock=0.0):
    """Plant the feet apart with bent knees, keeping the soles flat, or lift the body off the ground with the feet tucked up
    beneath it and the toes dropping, or raise one foot by a height with its knee up in front, the hips turned over them and
    rocked forwards or back."""
    feet = {s: P.pos('foot.%s' % s).copy() for s in 'LR'}
    rots = {s: P.worldrot('foot.%s' % s) for s in 'LR'}
    if turn:
        P.turn('hips', UP, turn)
    P.move_root(UP*(lift - crouch) + SIDE['R']*shift + FWD*rock)
    for s in 'LR':
        raised = step.get(s, 0.0) if step else 0.0
        P.reach('thigh.%s.01' % s, 'shin.%s.01' % s, 'foot.%s' % s, feet[s] + SIDE[s]*width + UP*(lift - tuck + raised) + FWD*0.4*raised,
                FWD + SIDE[s]*knees)
        P.setworldrot('foot.%s' % s, rots[s])
        if lift > 0:
            bend(P, 'foot.%s' % s, RIGHT, 0.5*min(lift, 1.0), 'toe.%s' % s, -UP)
        turn_towards(P, 'foot.%s' % s, UP, toeout, 'toe.%s' % s, SIDE[s])


def hero(P, b):
    """Lead figure: wide bent legged stance with the weight on the left leg, right forearm thrown straight up wagging a finger
    to each beat, left hand on the hip and the right foot tapping on the snare."""
    stance(P, 0.45, 0.2, -0.25, 0.3, 0.4)
    # toes tapping: lifted off the planted heel over the beat between, and snapped down onto each snare, the foot put out in
    # front where it can tap, as the idle has it stepped back behind the hip, the ankle rising as it tips back on its heel and
    # the knee kept pointing out over the toes as the stance has it
    tap = 0.45*(1 - nodcurve((b - 1)/2))
    raise_leg(P, 'R', 0.55*np.sin(tap), 2.6, FWD + SIDE['R']*0.4, SIDE['R']*0.3)
    bend(P, 'foot.R', RIGHT, tap, 'toe.R', UP)
    turn_towards(P, 'ribs', FWD, 0.12, 'upper_arm.R.01', UP)
    # raised arm: elbow out at shoulder height, forearm upright and leaning in over the head, swaying a little over the two bars
    up = sway(UP - SIDE['R']*0.12 + FWD*0.08, FWD, 0.03*np.sin(2*np.pi*b/BEATS))
    arm(P, 'R', SIDE['R']*0.95 + UP*0.08 + FWD*0.1, up)
    hand(P, 'R', up + FWD*0.05, -SIDE['R'])
    curl(P, 'R', ['finger_middle', 'finger_ring', 'finger_pinky'], 1.9)
    curl(P, 'R', ['thumb'], 0.7)
    straighten(P, 'R', 'finger_index', P.pos('finger_middle.01.R.01') - P.pos('hand.R'))
    # "tsk tsk": the hand wags from side to side, reaching each end on a beat, easing into it and lingering there, the forearm
    # leading, the hand following and the finger trailing behind, a little bigger some swings than others
    def ease(x):
        return np.tanh(1.8*np.sin(x))/np.tanh(1.8)
    phase, amount = np.pi*b + np.pi/2, 0.88 + 0.12*np.sin(2*np.pi*b/BEATS + 0.7)
    wag = ease(phase)*amount
    P.turn('forearm.R.01', FWD, 0.07*np.sin(phase + 0.4)*amount)
    P.turn('hand.R', FWD, 0.24*wag)
    P.turn('hand.R', SIDE['L'], 0.05*(1.0 - np.cos(2*phase)))  # dipping slightly at each end
    lag = ease(phase - 0.7)*amount - wag
    P.turn('finger_index.01.R.01', FWD, 0.18*lag)
    P.turn('finger_index.01.R.02', FWD, 0.12*lag)
    on_hip(P, 'L')
    # face the camera with the head cocked slightly
    turn_towards(P, 'head', FWD, 0.22, 'tag_head', SIDE['L'])
    turn_towards(P, 'head', RIGHT, 0.08, 'tag_head', FWD)
    P.turn('head', FWD, 0.035*ease(phase - 0.5)*amount)  # tilting in time with the wag
    for s in ('L', 'R'):
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


# the hero's title pose and the vanguard's dance moves, each with an extra closing frame matching its first, as they're held at
# times set to the music
count = BEATS*BEATFRAMES
base = resample(idle, count)
for name, func in (('hero_title', hero), ('headbang', headbang), ('jump', jump), ('guitar', guitar), ('clap', clap), ('punch', punch), ('stomp', stomp),
                   ('wave', wave), ('spin', spin), ('drum', drum), ('mosh', mosh), ('kick', kick), ('march', march), ('twist', twist),
                   ('roll', roll), ('bass', bass), ('shake', shake), ('sway', calm_sway), ('tap', calm_tap),
                   ('snap', calm_snap), ('lighter', calm_lighter), ('nod', calm_nod), ('rock', calm_rock), ('stretch', calm_stretch),
                   ('look', calm_look)):
    name = name if name == 'hero_title' else 'vanguard_' + name
    if only and name.split('_')[1] not in only:
        continue
    frames = np.zeros((count + 1,) + base.shape[1:])
    for f in range(count):
        P = Pose(names, parents, base[f])
        func(P, BEATS*f/count)
        frames[f] = P.local
    frames[count] = frames[0]
    save_anim('%s/%s.iqm' % (out, name), name, parents, frames)
    print('wrote', name, frames.shape)
print('they dance at %gfps' % (BEATFRAMES*BPM/60))
