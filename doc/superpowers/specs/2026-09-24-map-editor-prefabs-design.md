# Map editor prefabs — design

Date: 2026-09-24
Status: approved for planning

## Goal

Give the map editor a prefab workflow: save a geometry selection as a named, reusable
prefab, browse the library with 3D previews, load a prefab onto the clipboard and paste it
with the existing paste flow, and manage the library (folders, overwrite, rename, delete).

## Decisions

These were settled with the user during design and are not open for re-litigation in the plan.

| Topic | Decision |
|---|---|
| Contents | **Geometry only.** Keep the existing `.obr` format (`"OEBR"`, version 0). No texture-name table, no entities. Textures stay raw slot indices (see *Known limitations*). |
| Placement | **Clipboard-based.** Loading a prefab runs `copyprefab`; the user pastes with the existing paste (`ta_paste`, Ctrl+V). No stamp mode. |
| Browser location | **Docked right panel** "Prefabs", like Textures, with a right-toolbar button. |
| Browser widget | **Extend `ui_tool_filelist`** (the asset/file browser widget) with a prefab file type, rather than a new widget. The tile look follows the mapmodel browser (`ui_tool_modellist`: spinning previews). |
| Library features in v1 | Folders (as subdirectories), overwrite, delete, rename/move. |
| Architecture | CubeScript library and UI, thin engine layer (approach A). |

## Background: what already exists

### Engine (`src/engine/octaedit.cpp:1511`–`1841`)

| Surface | Behaviour |
|---|---|
| `saveprefab <name>` (`:1553`) | Copies `sel` into a cached `prefab` and writes `prefab/<name>.obr` (gzip) via `findfile(..., "w")` → **home dir**, creating directories. Refused to `IDF_MAP`, requires a selection (`noedit(true)`), refused under `nompedit` in multiplayer. |
| `loadprefab` (`:1591`) | Cache-first (`hashnameset<prefab> prefabs`), else reads the file (home, then cwd, then package dirs). |
| `copyprefab <name>` (`:1628`) | Loads onto `localedit` (the clipboard). In multiplayer it calls `edittrigger(sel, EDIT_COPY, 1)` → `needclipboard = 1`, so the next paste uploads the clipboard first (`src/game/client.cpp:1682`). **Multiplayer paste already works.** |
| `delprefab <name>` (`:1541`) | Drops the cache entry only; does not touch the file. |
| `uiprefabpreview name colour blend minw minh [children]` (`src/engine/ui.cpp:6523`) | `Preview` subclass, sibling of `uimodelpreview`; renders the prefab mesh flat-shaded with an outline. Honours `uipreviewyaw`. |
| **Naming quirk** (`:1565`, `:1596`) | If the name contains `/` or `\`, the `prefab/` prefix is **not** added: `saveprefab trees/oak` writes `trees/oak.obr`. The UI therefore always passes the full relative path (`prefab/trees/oak`), which also makes top-level and foldered prefabs uniform. |

Nothing under `config/` uses any of this; only `setup.cfg:21` (tab-completion) and
`usage.cfg:620` (descriptions) mention it.

### Bugs found in the existing backend (fixed by this work)

1. **NULL dereference.** `blockcopy` returns `NULL` when the selection exceeds 100 MB
   (`:931`); `saveprefab` then does `packblock(*b->copy, s)`.
2. **Stale preview mesh.** `saveprefab` replaces `b->copy` on an existing cache entry
   without `cleanup()`, so `numtris` stays non-zero and `renderprefab` keeps drawing the
   old VBO. Overwrite would show the old shape until restart.
3. **Cache lies after a failed write.** The cache entry is created and filled before the
   file is opened; if the write fails, the cache still claims the prefab.

### Editor UI patterns reused

| Pattern | Where |
|---|---|
| `tool_action <id> [p_short_desc … p_category … p_code …]` — one declaration makes a command searchable (F3), bindable (bindings panel, `toolbind`), and usable as a toolbar button | `config/tool/actions.cfg` |
| `toolpanel_open <name> left\|right\|center\|popup [props]`, `toolpanel_toggle`, `ui_<name>_on_open/_on_close`, `toolpanel_open_menu` (context menus with `p_item_names`, `p_tips`, `p_disabled`, `p_on_select`) | `config/ui/tool/toolview/toolpanel.cfg` |
| `tool_register_control <desc> <tags> <category>` + `tool_goto_control_<category>` — makes panel widgets findable in search | `config/ui/tool/toolview/toolwidgets.cfg:18` |
| `tool_confirm_prompt <text> <code> [p_noundo_warn = 1]` for irreversible operations | used throughout `config/ui/tool/tooltex.cfg` |
| `tool_info_show` / `tool_info_show_action` toolinfo notices | `config/ui/tool/toolview/toolinfo.cfg` |
| `ui_tool_filelist` / `ui_tool_fileselect`: type-filtered file grid, folder navigation, search, refresh | `config/ui/tool/toolview/widgets/toolfilelist.cfg`, `toolfileselect.cfg` |
| `ui_tool_preview_spin` (`uipreviewyaw` from `$totalmillis`) | `toolwidgets.cfg:196` |
| Toolbar slots `toolbar_actions_right` | `config/ui/tool/toolview/toolbar.cfg` `toolbar_init` |
| `backup()` — home-dir `remove` + `rename` via `findfile(…, "w")` into `backups/` | `src/shared/stream.cpp:331` |

`listfiles <dir> <ext> <filter>` (`src/shared/stream.cpp:647`) merges the working directory,
the home dir and every package dir, so the browser sees user and shipped prefabs together.
It does not deduplicate.

## Architecture

| Layer | File | Holds |
|---|---|---|
| Engine | `src/engine/octaedit.cpp` (beside `saveprefab`) | `validprefabpath`, `prefabinfo`, `removeprefab`, `renameprefab`; the three `saveprefab` fixes. `validprefabpath` declared in `src/engine/engine.h` for the unit test. |
| Engine test | **new** `src/tests/prefab.cpp` | `testprefab()`; called next to `testedharness()` in `src/engine/main.cpp:1333`, object added next to `tests/edharness.o` in `src/Makefile:352` (debug builds only). |
| Widget | `config/ui/tool/toolview/widgets/toolfilelist.cfg`, `config/tool/toolcommon.cfg` | Extensions listed under *File browser widget changes*. |
| Logic | **new** `config/tool/toolprefab.cfg`, exec'd from `config/tool.cfg` | State, name validation, save/overwrite/rename/delete/load wrappers, actions. |
| Panel | **new** `config/ui/tool/toolprefab.cfg`, exec'd from `config/ui/tool.cfg` | `ui_tool_prefabs` right panel, `ui_tool_prefab_save` and `ui_tool_prefab_rename` popups, control registration. |
| Toolbar | `config/ui/tool/toolview/toolbar.cfg` | `append toolbar_actions_right ta_prefabs` (after `ta_ents`). |
| Binds | `config/tool/binds/default.cfg` | `toolbind F6 ta_prefabs`. |
| Docs | `config/usage.cfg`, `tools/harness/README.md`, `doc/agent-handoff.md` | `setdesc` for the new commands; selftest docs; handoff state. |

### Data flow

```
open panel (ta_prefabs) ─► ui_tool_prefabs_on_open ─► filelist fetchdir (root "prefab", type PREFAB, instance "prefab")
click tile            ─► tool_prefab_cur = "prefab/<dir>/<name>" ─► header shows prefabinfo
double-click / Load   ─► copyprefab $tool_prefab_cur ─► toolinfo "Prefab loaded — paste with <ta_paste bind>"
Ctrl+V (existing)     ─► editpaste ─► pastehilight + paste on release (undo + MP sync unchanged)
Save…                 ─► popup ─► saveprefab prefab/<folder>/<name> ─► rescan, select new
Rename / Delete       ─► renameprefab / removeprefab ─► rescan, reselect / clear
```

## Engine

All commands are `ICOMMAND(0, …)`, refuse when `identflags&IDF_MAP`, and are **not**
behind `DEBUG_UTILS` (they are shipped editor features). None sends network messages.

### `bool validprefabpath(const char *name)`

True only if all hold:

- starts with `prefab/` and has at least one character after it;
- every character is in `[A-Za-z0-9_.-/]` (ASCII only);
- no empty segment (`//`), no trailing `/`, no segment ending in `.` (covers `.` and `..`; Windows strips a trailing dot, so `prefab/st./x` would name `prefab/st/x`);
- `strlen(name) + strlen(".obr") < MAXSTRLEN`.

(`\` and `:` are excluded by the character class.)

### `prefabinfo <path>` → string

`"sx sy sz grid user"` or `""` when the prefab cannot be loaded.

- Loads through `loadprefab(name, false)` (cached, silent).
- `sx sy sz` = `copy->s`, `grid` = `copy->grid`.
- `user` = 1 if `<homedir><path>.obr` exists (`fileexists(..., "r")`), else 0.
- Does not require `validprefabpath` (read-only, same reach as `copyprefab`).

### `removeprefab <path>` → int

1. `validprefabpath` else red `conoutf`, return 0.
2. `<homedir><path>.obr` must exist, else "Prefab %s is not a user prefab", return 0.
3. Move it to `backups/<path>.obr` in the home dir: resolve both with `findfile(..., "w")`
   (creates directories), `remove(dst)`, then `rename(src, dst)`; on rename failure,
   red `conoutf`, return 0.
4. Drop the cache entry (`cleanup()` + `prefabs.remove`), white `conoutf` "Removed prefab %s
   (backup in backups/)", return 1.

### `renameprefab <from> <to>` → int

1. Both pass `validprefabpath`, else 0.
2. Source exists in home, else "not a user prefab", 0.
3. Unless `strcasecmp(from, to) == 0` (case-only rename), the target must not exist in the
   home dir **or** be findable anywhere (`findfile(target, "r")` + `fileexists`), so a user
   prefab can never shadow a shipped one. Else "Prefab %s already exists", 0.
4. Resolve target with `findfile(..., "w")` (creates directories), `rename`; failure → 0.
5. Drop cache entries for both names; return 1.

The emptied source directory is left in place (no directory removal primitive exists);
the browser shows it empty.

### `saveprefab` fixes

1. After `blockcopy`, if `b->copy == NULL`: red "Selection too large for a prefab", remove
   the cache entry if it was just created, return.
2. Call `b->cleanup()` before replacing an existing `b->copy`, so the preview mesh rebuilds.
3. If `opengzfile` fails or `packblock` fails: remove the cache entry (`cleanup()` +
   `prefabs.remove(name)`), keep the existing red message.

Out of scope: `saveprefab` still accepts arbitrary paths (`../foo`). It only writes `.obr`
under the home dir; the UI never passes such paths; tightening it could break console use.

## File browser widget changes (`ui_tool_filelist`)

Every addition defaults to today's behaviour, so the existing callers (`toolenv.cfg`,
`toolimgedit.cfg`, `toolmap.cfg`, `toolmat.cfg`, `tooltex.cfg`) are unaffected.

| Change | Detail |
|---|---|
| `TOOL_FILE_PREFAB = 5` | `toolcommon.cfg`; `tool_file_type_exts` returns `"obr"` for it. |
| `p_root` (default `"data"`) | `tool_filelist_fetchdir` lists `(concatword $p_root "/" $tool_filelist_curdir)` (no trailing `/` when `curdir` is empty). "Go up" is disabled when `curdir` is `""`. The current check `(=s $tool_filelist_curdir "data")` never matches, because `curdir` is relative to the root; this fixes it for all callers. |
| Prefab tile | In `ui_tool_filelist_item`, when `p_file_type` is `TOOL_FILE_PREFAB` and the item is a file: `uiprefabpreview (concatword $p_root "/" <curdir/> <name without .obr>) 0xFFFFFF 1 $_icon_size $_icon_size [ @@(ui_tool_preview_spin) ]`. Directories keep the folder icon. |
| `p_footer` (default 1) | 0 hides the Selected/Current/OK/Cancel footer. |
| `p_on_select` | Runs on single click of a **file**, after the selection state updates; `$arg1` = selected relative path (extension stripped when `p_strip_ext`). |
| `p_on_activate` | If set, double-clicking a file runs it (with the same `$arg1`) **instead of** `tool_filelist_select`. |
| `p_on_item_menu` | If set, right-clicking (`uialtrelease`) a file runs it with the same `$arg1`. |
| `p_instance` (default `""`) | If set, on entry the widget saves the current `tool_filelist_*` globals to temporaries and loads `tool_filelist_<instance>_*` into them; on exit it writes the globals back to `tool_filelist_<instance>_*` and restores the temporaries. Covers `curdir dirs files numdirs numfiles sel_type sel_index sel_path active_path`. Instance variables that were never set start empty (`-1` for `sel_index`). The search query is not swapped: the search field binds `tool_filelist_<instance>_filter_query` directly (`tool_filelist_query_var`), because the engine keeps one text-editor buffer and focus per variable name. Double-click ids include the instance too. |
| `p_refetch` (default 0) | The widget refetches when `$toolpanel_this_isinit` **or** `p_refetch` is true. Today it only fetches when its panel opens, so a docked panel needs this to rescan after a save, rename or delete. |
| Dedup | `tool_filelist_fetchdir` removes duplicate names from `dirs` and `files` (`listfiles` merges several roots). |

The swap in `p_instance` is safe because the UI is immediate-mode: every handler inside the
widget (`uirelease`, `uidoublepress`, `uialtrelease`, the `p_on_*` callbacks) runs between
entry and exit. **Constraint, documented in a comment:** a handler in an instanced widget
must not open another file picker, because the picker's setup (`tool_filelist_curdir = …`)
would be overwritten by the restore. The prefab handlers never do.

## CubeScript logic (`config/tool/toolprefab.cfg`)

### State

| Variable | Meaning |
|---|---|
| `tool_prefab_cur` | Selected prefab as full relative path without extension (`prefab/trees/oak`), or `""`. |
| `tool_prefab_cur_info` | Cached `prefabinfo` result for `tool_prefab_cur`; refreshed on selection change and after save/rename. |
| `tool_filelist_prefab_*` | The browser's widget instance state (see `p_instance`). |

### Helpers

| Helper | Behaviour |
|---|---|
| `tool_prefab_name_valid <name>` | Mirrors `validprefabpath` for a single segment: non-empty, `[A-Za-z0-9_.-]` only, no leading `.` (the browser hides such names) and no trailing `.`. Used for both name and folder fields. |
| `tool_prefab_path <folder> <name>` | `prefab/<name>` or `prefab/<folder>/<name>`. |
| `tool_prefab_exists <path>` | `!=s (prefabinfo $path) ""`. |
| `tool_prefab_is_user <path>` | Last field of `prefabinfo`. |
| `tool_prefab_select <path>` | Sets `tool_prefab_cur`, refreshes `tool_prefab_cur_info`. |
| `tool_prefab_rescan` | Sets `tool_prefab_needs_rescan = 1`. The panel passes it as `p_refetch` and clears it after the widget call, so the fetch happens on the next build with the widget's props in scope. |
| `tool_prefab_load <path>` | `copyprefab $path`; toolinfo "Prefab loaded" with subtext "Paste with <bind>" from `tool_action_pretty_bind_info ta_paste` (just "Paste" if unbound). |
| `tool_prefab_save <path>` | `saveprefab $path`; if `tool_prefab_exists $path` afterwards → select, rescan, toolinfo "Saved prefab"; else toolinfo "Could not save prefab". |
| `tool_prefab_remove <path>` | Confirm prompt (`p_noundo_warn = 1`, text notes the backup location) → `removeprefab`; on 1: clear selection if it was current, rescan, toolinfo; on 0: toolinfo "Could not delete prefab". |
| `tool_prefab_rename <from> <to>` | `renameprefab`; on 1: select `<to>`, rescan, toolinfo; on 0: toolinfo "Could not rename prefab". |

### Actions (category `"Prefabs"`)

| Action | Icon | Code |
|---|---|---|
| `ta_prefabs` "Prefabs panel" | `<grey>textures/icons/edit/cube` | `toolpanel_toggle tool_prefabs right [p_title = "Prefabs"; p_clear_stack = 1]` |
| `ta_prefab_save` "Save selection as prefab" | `<grey>textures/icons/edit/new` | Opens `tool_prefab_save` popup at `(uicursorpos)`; if nothing is selected (`! $havesel`), toolinfo "Select geometry first" instead. |
| `ta_prefab_load` "Load prefab to clipboard" | `<grey>textures/icons/edit/copy` | `tool_prefab_load $tool_prefab_cur`, or toolinfo "No prefab selected". |
| `ta_prefab_paste` "Paste prefab" | `<grey>textures/icons/edit/paste` | `ta_prefab_load`, then `editpaste` (so `onrelease` placement works when bound to a key). |

No action carries `TA_FLAG_NOONLINE`: file operations are local and loading uses the
existing clipboard sync.

## UI (`config/ui/tool/toolprefab.cfg`)

### Panel `ui_tool_prefabs`

```
┌ Prefabs ─────────────────────────── [x] ┐
│ ┌────────┐  prefab/trees/oak             │
│ │ spin   │  12×12×20 @ grid 8   user     │
│ │preview │  [Load to clipboard] [Save…]  │
│ └────────┘                               │
│ ─────────────────────────────────────────│
│ ui_tool_filelist (root "prefab",         │
│   type PREFAB, instance "prefab",        │
│   p_footer 0, 3 columns): search,        │
│   refresh, up, breadcrumb, folders then  │
│   spinning prefab tiles                  │
└──────────────────────────────────────────┘
```

- **Header:** a large `uiprefabpreview` of `tool_prefab_cur` (spinning), the path, the size
  line from `tool_prefab_cur_info`, and a `user` / `shipped` tag. Buttons from
  `ui_tool_get_action_button ta_prefab_load` (disabled when nothing is selected) and
  `ta_prefab_save` (disabled when `! $havesel`). When nothing is selected, the header shows
  "No prefab selected".
- **List:** `ui_tool_filelist tool_prefab_browse [p_root = prefab; p_file_type =
  $TOOL_FILE_PREFAB; p_instance = prefab; p_footer = 0; p_columns = 3; p_sel_size = 0.1;
  p_refetch = $tool_prefab_needs_rescan; p_on_select = [tool_prefab_select (concatword
  "prefab/" $arg1)]; p_on_activate = […load…]; p_on_item_menu = […menu…]]`. `p_sel_size`
  0.1 mirrors the mapmodel browser's 0.11 tiles at 3 columns and fits the 0.2-wide right
  panel. It's tuned by eye from screenshots.
- **Context menu** (`toolpanel_open_menu`): "Load to clipboard", "Overwrite with selection",
  "Rename / move…", "Delete". Items 2–4 disabled with tip "Shipped prefab (read-only)" when
  `user` is 0; item 2 also disabled with tip "Nothing selected" when `! $havesel`.
- **Empty state:** when the root has no files or folders: "No prefabs yet. Select geometry
  and press Save… to create one."
- `ui_tool_prefabs_on_open`: set the rescan flag. The panel registers its header controls
  with `tool_register_control` in category `"Prefabs"`; `tool_goto_control_Prefabs` opens the
  panel.

### Popup `ui_tool_prefab_save`

- Fields: **Name** (`ui_tool_textinput`), **Folder** (`ui_tool_dropdown` of `(root)`, the
  existing top-level folders and "New folder…"; choosing "New folder…" reveals a text field).
  The folder defaults to the browser's current directory (top level only).
- Live line: "Size: sx×sy×sz @ grid g" from the engine selection stats (the toolbar's
  selection box reads the same values, `getenginestat 30..32` and `41`).
- The button reads **Save**, or **Overwrite** when the target path exists. Overwrite goes
  through `tool_confirm_prompt`. Overwriting a shipped (non-user) prefab is not possible:
  saving always writes to home, so it creates a user copy that shadows the shipped file. The
  button tip says so.
- Disabled with an inline warning while the name or folder fails `tool_prefab_name_valid`,
  or while there is no selection.

### Popup `ui_tool_prefab_rename`

- The same Name and Folder fields, prefilled from the source path.
- An inline warning "A prefab with that name already exists" and a disabled button when the
  target exists and is not a case-only rename of the source.

### Opened from the menu

"Overwrite with selection" on a user prefab goes straight to `tool_confirm_prompt` →
`tool_prefab_save <that path>`.

## Error handling

| Situation | Handling |
|---|---|
| No selection when saving | Buttons disabled; the action shows toolinfo "Select geometry first". |
| Invalid name or folder | Inline warning in the popup; button disabled. The engine validator is a backstop. |
| Target exists (save) | Button becomes Overwrite, with a confirm prompt. |
| Target exists (rename) | Inline warning; button disabled. Engine refuses too. |
| Shipped prefab: delete, rename, overwrite | Menu items disabled with a tip. Engine refuses too. |
| Engine command returns 0 or `""` | toolinfo notice with the reason; the engine also logs a red `conoutf`. |
| Selection too large | Engine message "Selection too large for a prefab"; UI reports "Could not save prefab". |
| Multiplayer | New commands are local-only. Paste goes through the existing `needclipboard` upload. |

## Testing

Suites run cheapest first. For any problem with the harness itself: `git stash`, stop, and
report, per the user's standing instruction. Harness problems are investigated in a separate
session.

### 1. C++ unit test (`src/tests/prefab.cpp`)

`testprefab()`, a table of `validprefabpath` cases.

- Accept: `prefab/oak`, `prefab/trees/oak`, `prefab/a.b-c_d`.
- Reject: `oak`, `prefab/`, `prefab//x`, `prefab/../x`, `prefab/x/..`, `prefab/./x`,
  `prefab\x`, `C:/x`, `prefab/x/`, `prefab/sp ace`, a name reaching `MAXSTRLEN` with `.obr`.

Prints `testprefab: ok`.

### 2. End-to-end: `tools/harness/prefab-selftest.ps1`

Uses `editor-selftest.ps1`'s `Step` / `Expect` helpers. It starts its own game in
`home/uitest/` and first clears `home/uitest/prefab/` and `home/uitest/backups/prefab/`.

| Step | Check |
|---|---|
| Setup | `editor.ps1 newmap`; select a box over solid geometry with `edselbox`. **Verify during implementation** that a fresh `newmap` has a solid floor; if it does not, create cubes first (`editface`). |
| Save | `saveprefab prefab/st/box` → the file exists under `home/uitest/prefab/st/`; `prefabinfo` matches the selection size and grid, `user` = 1. |
| Round trip | `copyprefab prefab/st/box`, `pastehilight` + `paste` at a different selection, re-save that region as `prefab/st/rt`. The **decompressed** bytes of both files are equal (PowerShell `GZipStream`). |
| Clipboard size | After a UI Load (double-click), `pastehilight` → `EDSTATE` selection size equals the `prefabinfo` size. |
| Overwrite | Re-save `prefab/st/box` from a different-sized selection → `prefabinfo` reports the new size (stale-cache fix). |
| Rename | To `prefab/st2/box2` → moved and folder created; onto an existing name → 0; an invalid target → 0; case-only (`box2` → `BOX2`) → 1. |
| Remove | The file moves to `home/uitest/backups/prefab/…`; `prefabinfo` → `""`. |
| Read-only | Copy a prefab to `<repo>/prefab/zz_selftest_shipped.obr` (working dir, outside home) → `user` = 0; `removeprefab` and `renameprefab` → 0. Deleted in a `finally` block; the `<repo>/prefab/` directory is removed only if the test created it. |
| Not tested | Selection over 100 MB and `IDF_MAP` refusal: not reachable from the harness; covered by code review. The report says so. |

### 3. UI checks (same script, via `harness.ps1` `send` / `tree` / `click` / `shot`)

- `tool_do_action ta_prefabs` → the panel is open (`toolpanel_isopen tool_prefabs`); the tree
  shows folder `st`. After entering it: tile `box` is present, search `zz` hides it.
- The context menu on a user prefab enables Delete and Rename; on the read-only prefab they
  are disabled (tip text appears in the tree dump).
- The Save popup's button reads Save for a new name and Overwrite for an existing one, and
  is disabled with no selection.
- **Instance isolation:** with the Prefabs panel inside `st/`, open the Map panel's mapmusic
  picker and navigate it → `tool_filelist_prefab_curdir` is still `st`, and the picker lists
  `data/…`.
- **Existing pickers:** mapmusic and the haze texture picker still list `data/`, OK still
  selects, and Up is disabled at the root.
- **Screenshots, reviewed by eye:** the panel with tiles, the empty state, both popups, the
  context menu, the read-only tip.

### 4. Regression

Rerun on a debug build: EDSTATE parser units, `task5-smoke.ps1`, `task8-editor.ps1`,
`editor-selftest.ps1`.

## Known limitations (accepted)

- **Textures are raw slot indices.** A prefab pasted into a different map takes that map's
  slot N; an out-of-range index falls back to the default geometry texture
  (`src/engine/texture.cpp:3291`). The panel header shows the hint "Textures follow this
  map's slot order".
- **No entities.** Lights, mapmodels and other entities are not stored.
- **Empty folders remain** after renaming or deleting their last prefab.
- **Previews are untextured** (flat-shaded mesh with an outline), as `uiprefabpreview`
  already renders them.
- **The first open loads every prefab in the current folder** to build previews
  (`loadprefab` caches them for the session). Acceptable for folder-sized libraries.

## Deviations during implementation

| Spec said | Built | Why |
|---|---|---|
| `copyprefab` unchanged | `copyprefab` uses `noedit(true)` and empties the engine entity clipboard (`entcopyclear`); `tool_prefab_load` also clears the script-side `entcopybuf` | `noedit()` refused whenever the (possibly stale) selection was out of view, so Load failed silently. `editpaste` pasted stale copied entities along with the prefab, or replaced entities instead of pasting geometry when `entcopybuf` was set. |
| Delete confirm with `p_noundo_warn = 1` | Plain confirm; the text says a copy is kept in `backups/` | The warning reads "This operation cannot be undone!", which is false here. |
| Panel registers controls with `tool_register_control` | Not registered | Every panel function is a `tool_action`, and actions are already searchable (F3). |
| Popups focus the name field | No auto-focus | A focused immediate field writes its text back into the variable every frame, which overwrites prefilled values (Rename) and scripted input. Matches the entity-template popups. |
| Folder field: dropdown of top-level folders + "New folder…" | As specified; the new-folder text accepts nested paths (`trees/oak`) | Rename must be able to show a nested source folder. |
| Default bind F6 → `ta_prefabs` | `I` → `ta_prefabs` | F6 is bound to `ta_ents` (`config/setup.cfg:329`); F1–F9 are all taken in edit mode and F10/F11 take screenshots. |
| Action `ta_prefab_paste` (load + paste) | Dropped | Loading puts the prefab on the single clipboard, so the existing paste already places it; a second paste key only makes sense with a separate prefab buffer, which was deferred (user decision). |
| Load leaves the clipboard implicit | The Load notice says "Replaced your copied selection…" | Loading a prefab replaces the whole copy buffer (geometry and copied entities); the user should not lose a copy silently. |
| `saveprefab` returns nothing | `saveprefab` returns 1/0 (`intret`), and `tool_prefab_save` branches on it | The script could not tell a failed overwrite from success: the old file still existed, so the UI said "Saved". |
| `ui_tool_filelist` changes limited to the listed props | Also skips the scroll widgets when no item is visible | An empty or fully filtered list made the engine's `ScrollBar::vscale` compute 0/0 (NaN geometry), which also broke the harness's UI-tree parsing. |
| Round-trip check compares prefab files byte for byte | Compares them with the stored block origin (decompressed bytes 8–19) masked | `.obr` records the selection's absolute origin, which paste never reads, so two locations can never match byte for byte. |
