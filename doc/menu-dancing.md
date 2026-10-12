# Main Menu Dancing

The main menu scene shows five figures standing in front of the eclipse: the hero in the middle, wearing the player's own
profile, and four vanguard figures either side of them. All five move in time with the menu theme. The vanguard dance a
rotating set of moves, switching to calmer ones through the quiet parts of the song, and the hero holds a title pose that wags
a finger and taps a foot to the beat.

This document covers how that works, how to make new dance moves, and how to write the timing file that keeps a music track
in step with them.

| Part | Where |
|---|---|
| Menu scene script: figures, move rotation, timing | `config/ui/game/menubg.cfg` |
| Pose generator: every dance move and the hero's pose | `src/scripts/makeposes.py` |
| IQM reading, writing and posing library | `src/scripts/iqmpose.py` |
| Generated animations | `data/actors/player/hero_title.iqm`, `data/actors/player/vanguard_*.iqm` |
| Animation registration | `data/actors/player/player.cfg`, `src/game/game.h` (`ANIM_*` enum and `animnames`) |
| Music playback position | `musicmillis` in `src/engine/sound.cpp` |
| Theme timing | `data/sounds/music/wearetheglitch.cfg`, beside `wearetheglitch.ogg` |

## How it works

### Keeping in time with the music

`musicmillis [track]` returns how far into the playing music track playback is, in milliseconds, or -1 when nothing is
playing. Given a track name, it only answers for that track. The music is streamed through a ring of OpenAL buffers, so the
position is worked out as what has been read from the file, less what is queued but not yet played, plus the source's
`AL_SAMPLE_OFFSET`. It wraps when a looping track starts over.

Each frame, `menubg.cfg` reads `musicmillis $musictheme` and works out where in the music it is:

- **beat length** = `60000 / musictempo` milliseconds
- **loops** = `(millis - musicfirstbeat) / (beat length * 8)`, the number of two bar moves danced so far
- **phrase** = `floor(loops / 2)`, as each move is danced twice over before the next
- the pose is held at `frac(loops)` of the way through the move

The animations are never left to loop on their own. Every frame the menu passes the exact time into the move to
`uiplayerpreviewanim <name> <millis> 1 0`, which holds the animation at that time (`ANIM_SETTIME`), so the figures can't
drift away from the music however long the menu is open. Because the time is held rather than looped, every generated move
has an extra closing frame identical to its first, so it blends cleanly from its last frame back round.

If the theme isn't playing, or there's no timing file beside it, the figures dance to the UI clock at the tempo the moves
were made at (174bpm) with no quiet parts.

### The move rotation

`menubg_dances` lists the sixteen energetic moves and `menubg_calmdances` the eight calm ones. Each vanguard figure has its
own order through `menubg_dances` in column 12 of `menubg_figures`, as indexes into the list:

```
[-0.47 0.12 0.31 215 0xffffff 0 1 ""           $W_MELEE 300 0    "0 11 3 8 1 12 6 10 4 13 2 15 5 9 7 14"]
[ 0.47 0.12 0.31 145 0xffffff 0 0 ""           $W_MELEE 350 0    "1 12 6 10 4 13 2 15 5 9 7 14 0 11 3 8"]
...
```

All four figures follow the same sequence, each starting four places further along it, so no two figures ever dance the same
move at once. The sequence alternates kinds of move (headbanging, jumping, instruments, arm moves) so similar moves don't
follow one another. A figure changes move every `menubg_dancerepeat` loops, two by default, so four bars.

While the music is in one of the track's quiet parts (`musicquiet`), every figure takes a calm move instead, each figure two
places apart in `menubg_calmdances`, still changing every four bars.

The hero has no move list. Its column 8 names the anim it holds, `"hero title"`, kept in time with the music the same way.

### Drawing

The dancers are drawn at `menubg_dancescale` (0.75) in a box `1/0.75` times taller, so they're the same size on screen
but have room above their heads to jump and throw their arms up into without being cut off by the edge of their preview. The
box is lowered a little to make up for the feet sitting further above its bottom edge.

`menubgglitch` (off by default) puts the background glitch's prisms over the vanguard figures. Turn it off to see the dancing
clearly.

## Making dance moves

There are two ways to make a move, and they can be mixed freely in the rotation:

- **By hand**, keyframed in Blender (or any tool that can write IQM animations for the player's skeleton), for moves an
  animator wants full control over.
- **Generated** by `src/scripts/makeposes.py`, where each move is a Python function that poses the skeleton for any moment of
  it. This is how all the current moves were made, and it keeps them exactly on the beat and quick to tweak.

Either way the result is an animation file, `data/actors/player/vanguard_<name>.iqm`, registered and added to the rotation
the same way.

### What every move must be

The menu doesn't play moves as ordinary looping animations. It works out how far through the current two bars the music is
and shows the move at exactly that point, so a move has to fit the music's grid precisely:

- **Length:** two bars, **eight beats**, at **174bpm**, the tempo of the theme: `8*60000/174 = 2758.6` milliseconds.
- **Frames:** the eight beats as `N` frames, plus **a closing frame that is a copy of the first**, so `N + 1` frames in the
  file. The menu holds the animation at a time rather than looping it, so the closing frame is what lets the last frame
  blend back round into the first. The generated moves use 12 frames a beat, `N = 96`, so 97 frames.
- **Rate:** the frames per second given in `player.cfg` must make the `N` frames last exactly the eight beats:
  `N*174/480`. For 96 frames that's **34.8**. Other counts work too, such as 48 frames at 17.4 or 120 at 43.5.
- **Start:** frame 0 is the first downbeat of the two bars, with a kick on it.
- **Whole body:** the move drives both halves of the model, legs and upper body, so every bone must be animated; nothing
  is layered over the idle animation.
- **Skeleton:** the player's 78 joints, in the order listed in `data/actors/player/bones.txt`. The male and female models
  share this skeleton, so one animation works for both. The engine matches an animation's poses to the model's bones by
  position, not by name, so the count and order must be exactly right.

Where the beats fall, at 12 frames a beat:

| Beat | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | (8) |
|---|---|---|---|---|---|---|---|---|---|
| Frame | 0 | 12 | 24 | 36 | 48 | 60 | 72 | 84 | 96 |
| Drum | kick | snare | kick | snare | kick | snare | kick | snare | the closing frame, a copy of 0 |

In the theme the **kick lands on the even beats and the snare on the odd ones**. Put the big hits, like the head going
down, fists punching and feet landing, on the **snare**, with a lighter bounce on every beat.

A move made at 174bpm plays at any other tempo given in a track's timing file, as the menu stretches it to fit, but it was
designed around this one's feel.

### What looks good, and what goes wrong

These apply to every move, however it's made.

- **Go bigger than feels right.** At menu size the figures are small. The player model is about **15 units tall** (hips at
  about 7, head at about 14). Crouches of 1 to 2 units, jumps of 2 to 3 units and torso bends of 10 to 20 degrees read
  clearly. Anything under half a unit is invisible.
- **Move across the screen, not towards it.** The figures face the camera, so a kick or knee raised straight forwards is
  foreshortened to almost nothing. Send legs and arms out on the diagonal.
- **Keep to the rig's joints.** The player's own animations only ever bend the elbow on its hinge (`forearm.01` about its
  local X), turn the hand over by twisting `forearm.02` along its length, and tilt the wrist (`hand`) without twisting it.
  Bending an elbow sideways or wringing a wrist tears the skin around it.
- **Keep a bend in the elbow.** An arm held dead straight has no plane to hinge in, and its twist can flip from one frame to
  the next. Keep the forearm at least 20 to 30 degrees off the upper arm, especially overhead.
- **Point knees over the toes.** A knee pointing straight ahead over a foot that's turned out looks like it's bending the
  wrong way.
- **Mind where the feet start.** The idle animation has the right foot stepped back behind the hip. A foot tapping its toes
  back there reads as the shin bending backwards, so bring a tapping foot forward first.
- **Watch the swing speed.** Twelve frames a beat is not many. An arm swinging 180 degrees in under half a beat looks like a
  pop; give big swings most of a beat.
- **Leave room overhead.** Arms up plus a jump of around 2.6 units fills the dancers' taller box. Much more and they'll be cut
  off at the top.
- **Calm moves stay small.** The calm moves for the quiet parts sway, nod and shift weight within a few tenths of a unit and
  keep the hands near the body. They should read as listening, not dancing.

### Making a move by hand

The player's source scenes are kept with the model, in `data/actors/player/male/` (`RE_Player_male.blend`) and
`data/actors/player/female/` (`RE_Player_female.blend`). Animate on the armature in one of these, checking it has the 78
bones of `bones.txt`. Other hand-made player animations to compare against are in `data/actors/player/`, such as
`emo_wave.iqm`, `emo_neko.iqm` and `claw_idle.iqm`.

1. **Set up the timing.** Set the scene's frame rate to 34.8 so playback runs at the real speed: Blender takes it as a custom
   rate of 174 frames a second over a base of 5. Set the frame range to 0 to 96. To animate against the music, add the track
   as a sound strip lined up so that the start of a move falls on frame 0, just as the menu does: moves start over every
   two bars, 2.7586 seconds, from the track's first downbeat, so line frame 0 up with any multiple of that. Markers every 12
   frames help, with the snares at 12, 36, 60 and 84.
2. **Animate the whole body** across frames 0 to 96, keeping to the rig's joints as above. Moving the root (`hips`) up and
   down is how crouches and jumps are done.
3. **Make frame 96 a copy of frame 0**, by copying every bone's keys from frame 0 to frame 96, so the move comes back round
   exactly.
4. **Export the animation** as an IQM with the Inter-Quake Model exporter for Blender from the IQM project. Export only the
   skeleton and the action, without meshes, over the full range 0 to 96 inclusive, to
   `data/actors/player/vanguard_<name>.iqm`. Use a name none of the generated moves use, and don't add it to
   `makeposes.py`, which would write over it.
5. **Check the file** has 78 poses and 97 frames, with its last frame matching its first. From the repository root:

   ```python
   import sys
   import numpy as np
   sys.path.insert(0, 'src/scripts')
   from iqmpose import load_anim
   parents, frames, anims = load_anim('data/actors/player/vanguard_<name>.iqm')
   print(len(parents), 'poses,', len(frames), 'frames,', anims)
   print('closing frame matches the first:', np.allclose(frames[0], frames[-1], atol=1e-3))
   ```

   If it has a different number of frames `N + 1`, either fix the frame range or give it the rate `N*174/480` when
   registering it.

Then register it and add it to the rotation, as below.

To hand-tweak one of the generated moves rather than start from nothing, rename its file, take it out of the generator's
list so it isn't overwritten, and work from that, with an IQM importer for Blender if you have one.

### Generating a move

Each generated move is a Python function in `src/scripts/makeposes.py`, called as `func(P, b)`, where `P` is a `Pose` of
the skeleton to modify and `b` is the time in **beats**, from 0 up to 8. The script calls it for each of the 96 frames,
starting each frame from the idle animation, and writes them out with the closing frame:

```python
BPM, BEATS, BEATFRAMES = 174.0, 8, 12
```

#### The timing curves

`nodcurve(p)` is the basic shape of a hit over one cycle `p` in [0, 1). It's 1 on the hit, holds there for an instant, eases
back to 0, and is then thrown down hard into the next hit. Use it with the cycle you want:

```python
hit    = nodcurve((b - 1)/2)   # 1 on each snare (beats 1, 3, 5, 7)
bounce = nodcurve(b)           # 1 on every beat
kick   = nodcurve(b/2)         # 1 on each kick (beats 0, 2, 4, 6)
eighth = nodcurve(2*b)         # 1 on every half beat
```

Other helpers for timing:

- `smoothstep(x)` eases 0 to 1 over x in [0, 1], for blending into and out of parts of a move, like
  `smoothstep((b - 3.05)/0.9)` to swap arms over the last beat of a bar.
- `np.sin(np.pi*(b - 1)/2)` swings from side to side, arriving at one side on one snare and the other on the next. Wrap it
  in `np.tanh(k*...)/np.tanh(k)` to make it snap into each side and linger there.

Every curve must come back to where it started at `b = 8`, or the move will jump as it loops. Periods that divide 8 beats
(1, 2, 4 or 8) do.

#### Directions

Directions are world space vectors in the model's frame. The model faces `FWD`, which is towards the viewer on the menu:

| Name | Meaning |
|---|---|
| `UP` | up |
| `FWD` | forwards, towards the camera |
| `RIGHT` | the figure's right, the viewer's left |
| `SIDE['L']`, `SIDE['R']` | out to the figure's left or right |

Build directions as sums, `side*0.5 + UP*0.8 + FWD*0.2`, and blend between two with `mix(a, b, k)`, which normalises the
result. The player model is about **15 units tall** (hips at about 7, head at about 14), which sets the scale of every
distance: a crouch of 0.4 is barely visible, 1.5 is deep.

#### Posing helpers

Body:

- `stance(P, width, crouch, shift, toeout, knees, lift=0, tuck=0, step=None, turn=0, rock=0)` plants the feet apart and
  bends the knees, keeping the soles flat. `crouch` lowers the hips, `shift` moves them towards `SIDE['R']`, `toeout` turns
  the feet out and `knees` sends the knees out over them. `lift` raises the whole body off the ground for jumps, with `tuck`
  drawing the feet up under it. `step={'R': h}` raises one foot with its knee up in front. `turn` turns the hips over the
  planted feet and `rock` shifts them forwards or back. Call it first, as everything else builds on where it puts the hips.
- `raise_leg(P, s, up, fwd, pole, out)` lifts a planted foot up, forwards and out, the knee bending towards `pole`, for
  kicks, high knees and foot taps.
- `bend(P, joint, axis, angle, probe, direction)` turns a joint about an axis, positive angles moving the `probe` joint
  along `direction`. This is how the spine, ribs, neck and head are posed:
  `bend(P, 'ribs', RIGHT, 0.3, 'tag_head', FWD)` pitches the chest forwards.
- `nod(P, amount, back=0)` pitches the neck and head forwards by `amount`, tipped back by `back`, as in `nod(P, hit, 0.25)`.
- `P.turn(joint, axis, angle)` turns a joint directly when the sense doesn't matter, like a symmetric twist.

Arms and hands:

- `arm(P, s, upper, fore)` points the upper arm and forearm of side `s` along two directions.
- `P.reach(upper, lower, end, target, pole)` puts a hand (or foot) on a point instead, the elbow (or knee) bending towards
  `pole`, for hands on knees or behind the back.
- `on_hip(P, s)` rests a hand on the hip with the elbow out.
- `orient(P, s, fingers, facing)` turns the hand so the fingers point along `fingers` with the palm facing `facing` as
  near as it can.
- `curl(P, s, fingers, angle)` bends the listed fingers, from `FINGERS` and `'thumb'`: about 1.9 makes a fist, 0 is
  straight, negative opens the hand. `horns(P, s, amount)` makes the devil horns. `spread(P, s, amount)` fans the fingers.
- **`constrain_arm(P, s)` must be called last for each arm.** It rebuilds the arm the way the rig's own animations move it:
  the elbow only hinges, the forearm turns the hand over by twisting and the wrist only tilts. Aiming bones freely bends the
  elbow sideways and wrings the wrist, which tears the skin. It keeps the shoulder, elbow, wrist and hand where they were and
  asserts if it can't.

Once a move has posed the body, it poses each arm in this order:

```python
for s in 'LR':
    side = SIDE[s]
    arm(P, s, upper, fore)
    orient(P, s, fore, facing)
    curl(P, s, FINGERS, 1.9)
    constrain_arm(P, s)
```

#### A worked example

The headbang move, abridged:

```python
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
    nod(P, hit, 0.25)
    for s, up in (('R', right), ('L', 1.0 - right)):
        side = SIDE[s]
        upper = mix(-UP*0.7 + side*0.5 + FWD*0.3, side*0.6 + UP*(0.2 + 0.75*hit) + FWD*0.25, up)
        fore = mix(FWD*0.75 + UP*(0.55 - 0.9*hit) - side*0.1, UP*(0.25 + 0.75*hit) + FWD*0.55*(1 - hit) - side*0.3*(1 - hit), up)
        arm(P, s, upper, fore)
        orient(P, s, fore + FWD*0.1*up, mix(-side - UP*0.25, -FWD*0.8 - side*0.3, up))
        horns(P, s, up)
        constrain_arm(P, s)
```

Things to notice:

- The timing values (`hit`, `bounce`, `right`) are worked out first, then drive everything else.
- Each arm blends between a raised pose and a low one with `up`, rather than switching, so the swap is a smooth movement.
- The body leads the head slightly (`lead` is a little ahead of `hit`), so the head whips down after it.

#### Generator pitfalls

- `constrain_arm` keeps arms within the rig's joints, but it can only work from where the arm was aimed. Aim the arms
  roughly where the rig can put them and give them a bend.
- When re-solving a leg with `P.reach` or `raise_leg`, give it the same pole the stance uses, `FWD + SIDE[s]*knees`, so the
  knee stays over the toes.
- Moves start from the idle animation, so anything the move doesn't set (like the right foot stepped back) comes from there.
- `bend`, `turn_towards` and the hand helpers work out which way to turn on the first frame they're used and remember it,
  so a move can't flicker between the two senses. Use `bend`'s sign, not a different probe, to turn the other way.

#### Generating it

1. **Write the function** in `makeposes.py`, alongside the others. Name calm moves `calm_<name>`. Keep the docstring style
   of the existing moves, saying what the move does and where its hits land.
2. **Add it to the generator loop** at the bottom of the script, as `('<name>', <function>)`. It's written to
   `data/actors/player/vanguard_<name>.iqm`.
3. **Generate it**, from the repository root, passing just the moves you want rebuilt. Each takes around 15 seconds; the
   whole set takes about six minutes.

   ```
   src/scripts/makeposes.py data/actors/player data/actors/player <name> [<name>...]
   ```

   Pass `title` to rebuild the hero's pose. With no names it rebuilds everything.

### Registering a move

However it was made, a move needs registering in three places:

1. **The model**, in `data/actors/player/player.cfg`. Each move is listed **twice**, once for each animation part, before
   and after `iqmanimpart ribs`, with its rate:

   ```
   iqmanim "vanguard <name>" "vanguard_<name>.iqm" 34.8
   ```

   This file uses Windows (CRLF) line endings; keep them, or the whole file shows as changed.
2. **The game**, in `src/game/game.h`: add `ANIM_VANGUARD_<NAME>` to the animation enum before `ANIM_MAX`, and
   `"vanguard <name>"` at the same place in `animnames`. Animation names are looked up exactly, so they must match
   `player.cfg`. Rebuild the client.
3. **The rotation**, in `menubg.cfg`: add it to `menubg_dances` (or `menubg_calmdances`), and for an energetic move, add its
   index to every vanguard figure's order in column 12. Keep the orders as the same sequence shifted along by a quarter of
   its length per figure, so no two figures share a move at once.

### Changing the hero

The hero's pose is `hero(P, b)` in the same script, on the same eight beat timing, and is written to `hero_title.iqm`. It
raises the right hand wagging a finger, reaching each end of the wag on a beat, with the left hand on the hip and the right
foot tapping on the snare. The menu holds it in time just like the dancers. Its column 11 nudge in `menubg_figures`
(-0.03) centres the pose under the eclipse; if the hero is changed to lean the other way, flip its sign.

### Previewing and checking

Check every new move for **pops**, joints jumping between frames, which usually mean an arm or leg flipped. From the
repository root:

```python
import sys
import numpy as np
sys.path.insert(0, 'src/scripts')
from iqmpose import load_anim, load_joints
names = [j[0] for j in load_joints('data/actors/player/male/RE_Player_male.iqm')]
par, fr, _ = load_anim('data/actors/player/vanguard_<name>.iqm')
q = fr[:, :, 3:7]/np.linalg.norm(fr[:, :, 3:7], axis=2, keepdims=True)
step = np.degrees(2*np.arccos(np.clip(np.abs((q[1:]*q[:-1]).sum(2)), 0, 1)))
for j in np.argsort(step.max(0))[::-1][:4]:
    print(names[j], 'jumps %.0f degrees at frame %d' % (step[:, j].max(), step[:, j].argmax()))
```

A few degrees a frame is smooth; tens of degrees in a single frame is a flip unless the move is meant to be that fast there
(a drum stroke, a landing).

Moves are much easier to judge outside the menu, held at chosen times in a test UI. Run the client with a scratch home and
a script (see `src/engine/main.cpp` for the startup flags):

```
LD_LIBRARY_PATH=$PWD/bin/amd64 ./bin/amd64/redeclipse_linux -h<scratch>/home -dw1900 -dh1000 -df0 "-xexec <scratch>/sheet.cfg"
```

A contact sheet of a move, every half beat across its two bars, in the same 0.75 scale the menu uses:

```
newui "dancesheet" $SURFACE_FOREGROUND [
    uizindex 100
    uicolour 0x202020 $uiaspect 1 [
        uihlist 0.004 [
            loop c 16 [
                uiplayerpreview 0.75 1 (divf $uiaspect 16.6) 0.5 (format [
                    uiplayerpreviewmodel 0
                    uiplayerpreviewweapselect %1
                    uiplayerpreviewanim "vanguard headbang" %2 1 0
                ] $W_MELEE (*f $c (divf 30000 174))) [
                    uipreviewinteract 0
                    uipreviewyaw 195
                    uipreviewpitch -4
                ]
            ]
        ]
    ]
]
sleep 6000 [showui dancesheet; sleep 1500 [screenshot; sleep 300 [quit]]]
```

To see a move against the music, step the held time a twelfth of a beat at a time, take a `screenshot` each step, then
put the frames together at 34.8fps with the track starting on a loop boundary (any multiple of `480/174` seconds):

```
ffmpeg -framerate 34.8 -i seq/%03d.png -ss 60.6897 -i data/sounds/music/wearetheglitch.ogg \
    -map 0:v -map 1:a -c:v libx264 -pix_fmt yuv420p -c:a aac -shortest dance.mp4
```

On the live menu itself, set `menubgglitch 0` to see the figures clearly.

## Timing files for music

A music track can have a timing file kept beside it, named the same with a `.cfg` extension, like
`data/sounds/music/wearetheglitch.cfg` beside `wearetheglitch.ogg`. It tells the menu the track's tempo, where its beats
fall and which parts of it are quiet. Without one, the menu's figures dance to the clock instead of the track.

### The file

```
// timing of We Are The Glitch, for anything kept in time with it like the main menu's dancers
musictempo 174 // beats a minute
musicmeter 4 // beats a bar
musicfirstbeat 0 // millis into the track of the first downbeat
// the quiet parts, from and to the bar of each, -1 to the end: the intro, the bridge up to the last chorus, and the outro
musicquiet "0 4 128 146 196 -1"
```

| Variable | Type | Default | Meaning |
|---|---|---|---|
| `musictempo` | float, 1 to 1000 | 174 | Beats a minute. |
| `musicmeter` | int, 1 to 16 | 4 | Beats a bar. |
| `musicfirstbeat` | int | 0 | Milliseconds into the track of the first downbeat, beat 0 of bar 0. |
| `musicquiet` | string | `""` | Pairs of bars, from and to, of the quiet parts. The end bar is not included; -1 runs to the end. |

The variables are defined in `menubg.cfg` (`deffvar`, `defvar`, `defsvar`). They aren't persistent, so a track's timing is
never saved into the player's config. Whenever `musictheme` changes, the menu resets all four to their defaults and runs
the new track's file, so a value a file leaves out falls back to the default rather than to the last track's.

The menu finds the file the same way the track is found, trying the name as given and then under `sounds/`,
`sounds/music/` and `sounds/egmusic/`. A missing file is not an error.

Bars count from `musicfirstbeat`: bar `n` starts at `musicfirstbeat + n*musicmeter*60000/musictempo` milliseconds. For the
theme, one bar is `4*60000/174 = 1379.3` milliseconds, so bar 128 starts at 176.6 seconds.

### Finding the tempo and first beat

The tempo should be exact, as any error adds up over the length of the track: at 174bpm, being out by 0.1bpm puts the
figures nearly half a beat off by the end of a four and a half minute song. Most tracks are produced to a whole number tempo
with a fixed grid, so measure it, then round to the nearest whole number if it lands close to one.

Decode the track to raw mono samples and measure how strongly its onsets repeat at each candidate tempo:

```
ffmpeg -i track.ogg -ac 1 -ar 11025 -f f32le track.f32
```

```python
import numpy as np
x = np.fromfile('track.f32', dtype='<f4'); sr = 11025; hop = 64; n = 1024
frames = np.lib.stride_tricks.sliding_window_view(x, n)[::hop]*np.hanning(n)
S = np.log1p(100*np.abs(np.fft.rfft(frames, axis=1)))
onset = np.maximum(np.diff(S, axis=0), 0).sum(1)
onset = np.maximum(onset - np.convolve(onset, np.ones(100)/100, 'same'), 0)
t = np.arange(len(onset))*hop/sr
# the tempo whose beat grid the onsets line up with most strongly
tempos = np.arange(80, 200, 0.02)
power = [abs((onset*np.exp(2j*np.pi*t*bpm/60)).sum()) for bpm in tempos]
bpm = tempos[np.argmax(power)]
# and where on that grid the beats fall
phase = np.angle((onset*np.exp(2j*np.pi*t*bpm/60)).sum())
print('tempo %.2f, first beat %.0f millis' % (bpm, (-phase/(2*np.pi)) % 1*60000/bpm))
```

It will often find double or half the real tempo too; pick whichever the drums actually play. For the theme, it found
174bpm, with beats landing on the grid from 0 milliseconds.

`musicfirstbeat` must be a **downbeat**, and for the moves made for the theme, a beat with the **kick** on it rather than
the snare, as the moves put their heavy hits on the beats between (beats 1, 3, 5, 7 of each move). To tell which beats have
the kick and which the snare, add up onsets in a low band (under about 120Hz for the kick) and a mid band (roughly 180 to
400Hz for the snare's body) at each position of the bar across the whole track, and see which beats each lands on. If the
moves' hits come out on the kick instead, move `musicfirstbeat` along by one beat.

### Finding the quiet parts

Quiet parts are where the figures should stop dancing hard: an intro before the band comes in, a breakdown or bridge, an
outro. Loudness alone is misleading, as a section can build up again while still being quiet in feel. What matters most is
whether the **drums** are playing, and the kick most of all.

A useful way to see the song's sections is to print, for every bar, its overall loudness and how much of its energy is
percussive (sharp onsets) against sustained (held notes), with the kick band separately. A median filter across time keeps
the sustained parts of a spectrogram, and one across frequency keeps the percussive parts:

```python
import numpy as np
from numpy.lib.stride_tricks import sliding_window_view as sw
# full quality, so the cymbals are in it: ffmpeg -i track.ogg -ac 1 -ar 44100 -f f32le full.f32
x = np.fromfile('full.f32', dtype='<f4'); sr = 44100; hop = 512; n = 2048
bar = musicmeter*60/musictempo  # seconds
S = np.abs(np.fft.rfft(sw(x, n)[::hop]*np.hanning(n), axis=1)); f = np.fft.rfftfreq(n, 1/sr)
t = np.arange(len(S))*hop/sr + n/2/sr - musicfirstbeat/1000
def medfilt(a, k, axis):
    pad = [(0, 0)]*2; pad[axis] = (k//2, k//2)
    return np.median(sw(np.pad(a, pad, mode='edge'), k, axis=axis), axis=-1)
P = S*(medfilt(S, 17, 1) > medfilt(S, 17, 0))  # the percussive part
for i in range(int(t[-1]/bar)):
    k = (t >= i*bar) & (t < (i + 1)*bar)
    print('bar %3d %6.1fs loud %8.0f drums %8.0f kick %7.0f' % (i, i*bar + musicfirstbeat/1000,
          (S[k]**2).sum(1).mean(), (P[k]**2).sum(1).mean(), (P[k][:, f < 120]**2).sum(1).mean()))
```

For the theme, this shows the kick falling away at bar 126, the band nearly silent from bar 128, the bass coming back and
building from bar 138 with still no kick, the kick returning with a fill at bar 145 and the full kit on the last chorus at
bar 146. The whole stretch from 128 to 146 is the bridge, so the figures stay calm through all of it.

Give every quiet part a whole number of bars. The calm moves change over every four bars just as the energetic ones do,
but the switch between calm and energetic happens on the exact bar, wherever it falls in a move.

### Checking the timing

To check a file has loaded, look at the variables on the menu: `echo $musictempo $musicfirstbeat $musicquiet`. The menu
also keeps `menubg_dancetimed` (1 when the file was found), `menubg_dancesynced` (1 when it's dancing to the track rather
than the clock) and `menubg_dancequiet` (1 in a quiet part).

The quiet parts are easiest to check by setting `musicquiet` live on the menu to a range a few bars ahead of where the
track is, `musicquiet "6 10"` for example, and watching the figures change over and back. The menu only reruns the file
when the theme changes, so the value stays set until then.

Listen as well as look: the figures' big hits should land on the snare, and if they all seem a little early or late,
`musicfirstbeat` is out. Nudge it in steps of 10 to 20 milliseconds.

### A different theme

To make another track the menu theme with dancing, set `musictheme` to it and put a timing file beside it. The moves are
stretched to the track's tempo, so they suit tracks roughly in the region of 140 to 200bpm, or half that, played as
half time. For a very different feel, the moves themselves may need remaking at its tempo with `BPM` changed in
`makeposes.py`, along with the 34.8fps rate in `player.cfg` (`12*BPM/60`) and `menubg_dancemove` in `menubg.cfg`, which
tells the menu the tempo the moves were made at.
