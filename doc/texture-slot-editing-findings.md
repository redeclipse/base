# Texture slot editing: defects, repro steps, fixes

_2026-09-24. Scope: the map editor's "Edit texture slot" panel (`config/ui/tool/tooltex.cfg`,
`config/tool/tooltex.cfg`), the texture list's slot menu (`config/ui/tool/toolview/widgets/tooltexlist.cfg`)
and the engine commands behind them (`editslot`, `cloneslot`, `changeslotshader` in `src/engine/texture.cpp`,
`src/engine/shader.cpp`)._

The reported symptom: texture slots and their shader params occasionally become corrupt after editing
materials live. This covers eight defects, each able to cause that on its own, and each needing a specific
sequence of actions, which is why the symptom looks rare. Items 1–5 were reproduced live on the unfixed build.
Items 6–7 were found by reading the code and can't be observed from script.

Regression test: `tools\harness\texslot-selftest.ps1` (starts its own game, ~1 min, 22 checks). Against the
original UI scripts it fails 17 checks. Step 8 exercises the index conversion, not the menu click itself.

All repro batches below run in a harness game with `editor.ps1 open atop`. Indices are atop's.

---

## 1. Apply silently downgrades non-bump decals to `stddecal` — **fixed**

**Symptom.** Open *Edit slot* on a decal that has no normal map and uses `glowdecal`, `pulseglowdecal`,
`dispdecal`, `glowdispdecal` or one of the `env*glow*decal` shaders. Press **Apply** without changing
anything. The decal loses its glow or displacement. atop's decal 0 is a `glowdecal`, and shipped maps use
`glowdecal` 429 times.

**Repro.**
```cubescript
tool_tex_editslot 0 1; tool_tex_editslot_apply
echo (getvshadername 0 1)        // was: stddecal   want: glowdecal
```

**Cause.** `tool_tex_editslot_apply` appended the blend hint `b` to every decal hint set without `n`. But
`glowdecal` (`g`), `dispdecal` (`v`), `envglowdecal` (`erg`) and the others are registered **without** `b`
(`config/glsl/decal.cfg:288+`). `finddecalshader "gb"` misses, and Apply falls back to `stddecal`.
`tool_tex_editslot_checkhints` also validated decal hint combinations against `worldshaders`, so the
"allowed" list shown for decals was wrong.

**Fix.** New `tool_tex_editslot_findshader`: for a non-bump decal it tries the hints with `b`, then
without. No registered decal hint set exists in both forms, so this is unambiguous. Both
`checkhints` and `apply` now use it, so decals are checked against `decalshaders`.

## 2. "Compensate scale" sets the texture scale to `inf` — **fixed**

**Symptom.** Replace a slot's diffuse with a **smaller** image while *Compensate scale* is on (the
default). Every variant of the slot gets scale `inf`, and after a save and reload that becomes 8. Other
size ratios are silently compensated wrong: 512 → 768 gets no compensation at all.

**Repro.** Slot 4 has a 512×512 diffuse; `appleflap/danger.jpg` is 64×64.
```cubescript
tool_tex_editslot 4 0; tool_texeditslot_tex0 = "appleflap/danger.jpg"; tool_tex_editslot_apply
echo (getvscale 4) (getvscale 73)    // was: inf inf   want: 8 8
```

**Cause.** `editslot` declared `int xscale, yscale` and assigned `neww/float(oldw)` to them, so 0.125
truncated to 0 and `scale *= 1.0f/0`. The same function also had these problems:
- If the unloaded-slot branch (`gettexfilesize`) couldn't read the file (a `.dds`, or a name with
  `<cmd>` prefixes), `oldw` stayed 0, which is a division by zero.
- It cached the diffuse's index into `sts` **before** running the edit script. `remtexture` shifts
  entries, so afterwards the index could point at another texture or past the end.
- The scale was not clamped, so it could leave the [1/8, 8] range that `unpackvslot` enforces on load, and
  the map then looked different after a reload.

**Fix.** Float ratios. The diffuse is looked up by type after the script runs
(`getslotdiffusesize`). Rescaling is skipped when either size is unknown or is `notexture`. The scale is
clamped to [1/8, 8].

## 3. Apply writes to whatever texture is currently picked, not the slot being edited — **fixed**

**Symptom.** Open *Edit slot* on slot A. Before pressing Apply, pick another texture B: the *Get texture*
action/hotkey, or a texture list, including the one in a texture-select popup. Apply then overwrites B
with A's textures and shader.

**Repro.**
```cubescript
tool_tex_editslot 5 0; tool_focus_tex 6; tool_tex_editslot_apply
echo (getvtexname 6 0) (getvshadername 6)
// was: jojo\windowstilelit.png bumpenvspecmapparallaxglowworld   (slot 5's)
// want: jojo\windowstile02.png bumpenvspecmapparallaxworld
```

**Cause.** The panel parsed from, and `editslot` targeted, `$tool_tex_cur`. That is the shared "current
texture" (the paint brush), not a handle bound to the panel. For decals it was worse: `tool_tex_editslot N 1`
set the world texture brush to a decal index.

**Fix.** `tool_tex_editslot_idx` is bound when the panel opens, and parse and Apply use it.
`tool_tex_cur` is still set for world slots, as before, but no longer for decals.

## 4. Edit slot on a second slot while the panel is open keeps the first slot's fields — **fixed**

**Symptom.** With the panel open on slot A, choose *Edit slot* on slot B (e.g. by right-clicking a texture
selector). The panel keeps showing A. Apply writes A's textures into B.

**Repro.**
```cubescript
tool_tex_editslot 5 0; tool_tex_editslot 7 0
echo $tool_texeditslot_tex0      // was: slot 5's diffuse   want: slot 7's
```

**Cause.** `toolpanel_open` does nothing if the panel is already open
(`config/ui/tool/toolview/toolpanel.cfg:396`), so `ui_tool_texeditslot_on_open`, the parse, never re-ran.

**Fix.** `tool_tex_editslot` opens with `p_force_reopen = 1`.

**Related guard.** Culling or removing slots from a texture list popup shifts indices under an open editor.
The panel now records a signature when it parses: slot index, shader, texture count and diffuse name.
Apply refuses, with a message, if the signature no longer matches. Map loads already close all panels
(`toolpanel.cfg:469`).
```cubescript
tool_tex_editslot 8 0; editslot 8 [texture 0 "textures/default"] 0 0
tool_texeditslot_tex0 = "appleflap/danger.jpg"; tool_tex_editslot_apply
echo (getvtexname 8 0)           // want: textures\default (apply refused)
```

## 5. `editslot` on an out-of-range index edits the default geometry slot — **fixed**

**Symptom.** A stale or invalid index silently edits slot 1 (`DEFAULT_GEOM`, the texture of all new
geometry), or, for decals, the shared `dummydecalslot`.

**Repro.**
```cubescript
editslot 99999 [texture 0 "appleflap/danger.jpg"] 0 0
echo (getvtexname 1 0)           // was: appleflap\danger.jpg   want: textures\edit\edit_1.png
```

**Cause.** `universallookup` falls back to `slots[DEFAULT_GEOM]->variants` / `dummydecalslot` for bad
indices, and `editslot` only rejected `dummyvslot`.

**Fix.** `validslotedit()` range-checks per slot type, and a bad index is refused with
`Cannot edit nonexistent slot N`.

## 6. Variants keep stale shader param locations after a shader change — **fixed (code-derived)**

**Symptom.** Change a slot's textures so Apply picks a different shader (e.g. add a normal map). Do this
with *Compensate scale* off, or on any decal. Variants of that slot with their own shader params
(`vshaderparam`) then feed their values into the **wrong uniforms** of the new shader, or lose them. This
persists until a shader or GL reload. It isn't saved, which makes it look intermittent.

**Cause.** `SlotShaderParam::loc` is an index into the shader's `defaultparams`, resolved by
`linkvslotshader` only while `VSlot::linked` is false. `editslot` cleared `linked` only inside the rescale
branch. For decal (and material) slots the `VSlot` half is the slot itself, and `defslot->cleanup()` goes
through a `Slot *`: `Slot::cleanup` is non-virtual, so `DecalSlot::cleanup` (which also unlinks) never ran.
`allchanged()` does not relink vslots.

**Fix.** `editslot` now unlinks every variant (`defslot->variants` chain, which includes a decal's own
`VSlot`) unconditionally after the edit.

## 7. `cloneslot` breaks the clone's slot-level shader params — **fixed (code-derived)**

**Symptom.** A cloned slot renders without its `setshaderparam` values (e.g. its specscale), while the UI
getters still report them. It corrects itself after a save and reload.

**Cause.** Param names are interned by `getshaderparamname` and matched **by pointer**
(`linkslotshaderparams`, `mergevslot`, `shouldreuseparams`). `cloneslot` copied them with `newstring`, so no
clone param ever matched its shader (and each clone leaked the names). `findslotparam` uses `strcmp`, which
is why the UI looked right.

**Fix.** Copy the interned pointer.

## 8. Texture list menu passes vslot indices where slot indices are needed — **fixed**

**Symptom.** *Clone slot* on a slot that was itself cloned or added after variants existed does nothing, or
clones a different slot. *Remove slot* is enabled or disabled based on the wrong slot's variants.

**Repro.**
```cubescript
local a m; a = (cloneslot 5); m = (getslottex $a)    // e.g. slot 64, list item (vslot) 162
echo (cloneslot $m)              // what the menu did: nothing (slots[162] doesn't exist)
```

**Cause.** Texture list items are vslot indices. `cloneslot` and `texhasvariants` take slot indices. They
coincide only until a slot is added while variants exist.

**Fix.** The menu calls `cloneslot (getvindex ...)` and `texhasvariants (getvindex ...)`.

## 9. Shaders whose hints the editor can't represent — **fixed**

The editor rebuilds hints from the textures on any change and drops letters it has no control for:
- **`o`**, registered only to keep names unique (`bumpenvspecglowworld` `eosrg`, `bumpenvpulseglowworld`
  `eorgG`, and the decal and disp versions; the world and decal shaders never read it). Those shaders could
  never be selected again: env + spec + glow with a normal map fell back to `stdworld`.
- **`d`** (triplanar detail). Any change downgraded `triplanardetail*` to plain `triplanar*`.

**Fix.** The finder ignores `o` in registered hints. `d` is carried over from the original shader while
triplanar stays on.

---

## Not fixed (noticed, out of scope or unverified)

- **Material slots.** `editslot <mat> [...] <r> 2` runs with `slotedit = false`, so `texture 0 x` inside it
  **creates a new world slot** and edits that. `remtexture` and `changeslotshader` are no-ops. The UI never
  edits materials, so this is only reachable from script. Not changed; making materials editable may have
  been left out on purpose.
- **`getvshadername` / `getvgrasstex*`** dereference `slot->shader` / `slot->grasstex` without a null
  check. A vslot orphaned by `texturereset` points at `dummyslot` (no shader), and a slot without loaded
  grass has no `grasstex`. Unverified; likely a crash, not corruption.
- **`tool_tex_shader_param_init`** (`config/tool/tooltexparam.cfg:335`) passes `$arg2` where
  `tool_tex_shader_param_change` expects `$arg1 $arg2`. It has no callers today.
- **Apply on a slot without grass** writes `texgrassscale 2` / `texgrassheight 4` instead of leaving 0/0
  (the parse defaults). It is invisible without a grass texture, but it does change the saved values.
