#!/usr/bin/env python3
"""End-to-end self-test for the map editor prefab workflow (prefab-selftest.ps1).

Boots a client in the harness home, builds the deterministic 'newmap 12'
scratch world, and drives the prefab engine commands, the file browser widget,
the CubeScript prefab library and the Prefabs panel.

User prefabs live under home/uitest/prefab/ and are wiped first. One "shipped"
(read-only) prefab is simulated by a file under the repo's own prefab/ folder:
the game runs with the repo root as its working directory, which listfiles and
findfile search after the home dir. It is removed on exit, along with the
folder if this script created it.

  tools/harness/prefab_selftest.py [--keep-running]
"""

import argparse
import base64
import gzip
import os
import re
import shutil
import stat
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import core  # noqa: E402
import editor as ed  # noqa: E402
import harness  # noqa: E402
from selftest import Suite, eval_cs, map_load, send  # noqa: E402

# Same scratch world as editor_selftest.py: 'newmap 12' is 4096 units, solid
# below z = 2048, with an 8-unit grid at the default gridpower.
FLOOR, MID, GRID = 2048, 2048, 8

USER_DIR = os.path.join(core.HOME_DIR, 'prefab')
BACKUP_DIR = os.path.join(core.HOME_DIR, 'backups', 'prefab')
SHIPPED_DIR = os.path.join(core.REPO_ROOT, 'prefab')
SHIPPED_FILE = os.path.join(SHIPPED_DIR, 'zz_selftest_shipped.obr')


def user_file(rel):
    return os.path.join(USER_DIR, *rel.split('/'))


def backup_file(rel):
    return os.path.join(BACKUP_DIR, *rel.split('/'))


def _getint(buf, i):
    """One of cube's putint values at buf[i]: (value, next index)."""
    c = buf[i] - 256 if buf[i] > 127 else buf[i]
    if c == -128:
        return int.from_bytes(buf[i + 1:i + 3], 'little', signed=True), i + 3
    if c == -127:
        return int.from_bytes(buf[i + 1:i + 5], 'little', signed=True), i + 5
    return c, i + 1


def prefab_payload(path):
    """A prefab file's decompressed bytes, with what records where it was
    saved from masked out. Decompressed layout: 8-byte prefabheader (magic +
    version), then packblock's block3 header -- ivec o (3 little-endian
    int32s, bytes 8..19) -- then s/grid/orient and the cubes; from version 1
    an int32 length and the paint around the block (copyblendregion), which
    starts with the region's absolute bounds as six putints. pasteblock()
    reads neither o nor those bounds back, so two prefabs saved from
    different selections legitimately differ in them even when everything
    paste reproduces is identical."""
    with gzip.open(path, 'rb') as f:
        data = bytearray(f.read())
    for i in range(8, min(20, len(data))):
        data[i] = 0
    if int.from_bytes(data[4:8], 'little') >= 1:
        # The paint is last, after its length: find where it starts, as the
        # place whose length fits and whose six bounds read as a box.
        for start in range(len(data) - 1, 20, -1):
            if int.from_bytes(data[start - 4:start], 'little') != len(data) - start:
                continue
            try:
                vals, i = [], start
                for _ in range(6):
                    v, i = _getint(data, i)
                    vals.append(v)
            except IndexError:
                continue
            if i <= len(data) and all(0 <= v <= 1 << 24 for v in vals) and all(vals[k] <= vals[k + 3] for k in range(3)):
                data = data[:start] + data[i:]
                break
    return base64.b64encode(bytes(data)).decode('ascii')


def find_click(label):
    """Click point of the first drawn widget whose text matches label (exact
    first, then substring), as harness.py find reports it; None if none."""
    tree = harness.get_ui_tree()
    hits = harness.add_click_point(tree, harness.find_widget(tree, label))
    return (core.format_coord(hits[0].cursor_x), core.format_coord(hits[0].cursor_y)) if hits else None


def click_at(point, double=False, right=False):
    code = -3 if right else -1
    second = 'sleep 40 [ uikeypress %d 1; sleep 40 [ uikeypress %d 0 ] ]' % (code, code) if double else ''
    send('uisetcursor %s %s\nsleep 100 [ uikeypress %d 1; sleep 40 [ uikeypress %d 0; %s ] ]' % (point[0], point[1], code, code, second), 600)


def click(label, double=False, right=False):
    """Clicks a widget by label the way harness.py click does (move, let a
    frame see the hover, press, release), optionally twice or with the alt
    button."""
    pt = find_click(label)
    if not pt:
        raise core.HarnessError("no drawn widget matching '%s'" % label)
    click_at(pt, double, right)


def go_up():
    """Clicks a file browser's "Go up" button. It is an icon without a label,
    so it is found in the tree instead: the nearest button (#BorderedImage)
    before the "Directory:" label, at the same depth. Assumes one file
    browser is drawn."""
    tree = harness.get_ui_tree()
    nodes = harness.add_click_point(tree, [n for n in tree.nodes if n.drawn])
    d = next((i for i, n in enumerate(nodes) if n.type == '#TextString' and re.match(r'^Directory:\s*$', n.text)), -1)
    if d < 0:
        raise core.HarnessError('no drawn "Directory:" label')
    depth = nodes[d].depth
    for i in range(d - 1, -1, -1):
        if nodes[i].depth < depth:
            break
        if nodes[i].depth == depth and nodes[i].type == '#BorderedImage':
            click_at((core.format_coord(nodes[i].cursor_x), core.format_coord(nodes[i].cursor_y)))
            return
    raise core.HarnessError('no "Go up" button before the "Directory:" label')


def drawn_editors():
    """The drawn text editors (uifield and friends), left to right, with the
    text each one shows. uidumptree leaves that text out; uidumpeditors
    reports it."""
    eds = []
    for line in send('uidumpeditors', 1):
        m = re.search(r'UIEDITOR ([01]) ([-0-9.]+) ([-0-9.]+) ([-0-9.]+) ([-0-9.]+) (\S+) ?(.*)$', line)
        if m and m.group(1) == '1':
            eds.append({'x': float(m.group(2)), 'y': float(m.group(3)), 'name': m.group(6), 'text': m.group(7).strip()})
    return sorted(eds, key=lambda e: e['x'])


def open_prefab_panel():
    send('if (toolpanel_isopen tool_prefabs) [] [tool_do_action ta_prefabs]', 500)


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('--keep-running', action='store_true')
    a = p.parse_args(argv)
    t = Suite('prefab self-test', state_on_failure=True)
    ev, exists = eval_cs, os.path.exists
    state = ed.get_state
    created_shipped_dir = [False]

    for d in (USER_DIR, BACKUP_DIR):
        if exists(d):
            shutil.rmtree(d)

    core.stop()
    core.start()
    try:
        def setup():
            with map_load():
                ed.newmap(12)
            ed.frame(MID, MID, FLOOR, dist=256, yaw=0, pitch=-45)
            t.expect('edit mode', state().mode.edit_mode, 1)
        t.step('setup: scratch world, camera over the test area', setup)

        # ==== panel: empty library ========================================

        def empty_panel():
            ed.cursor('on')
            open_prefab_panel()
            t.expect('panel is open', ev('(toolpanel_isopen tool_prefabs)'), '1')
            t.expect_true('empty-state text is shown', find_click('No prefabs yet.') is not None)
            core.invoke_shot('prefab-empty', quiet=True)
            send('toolpanel_close tool_prefabs', 300)
            ed.cursor('off')
        t.step('panel: empty library', empty_panel)

        # ==== engine commands =============================================

        def save():
            # Two cubes deep, straddling the floor surface: solid below, air above.
            ed.sel(MID, MID, FLOOR - GRID, size=[2, 2, 2])
            send('saveprefab prefab/st/box')
            t.expect_true('file written under the home prefab dir', exists(user_file('st/box.obr')))
            t.expect('prefabinfo', ev('(prefabinfo prefab/st/box)'), '2 2 2 8 1')
            t.expect('prefabinfo of a missing prefab', ev('(prefabinfo prefab/st/nothing)'), '')
        t.step('saveprefab writes a user prefab; prefabinfo reports it', save)

        def copy_paste():
            send('copyprefab prefab/st/box')
            # Up in the air, clear of the floor, so the paste is all that is there.
            ed.sel(MID + 64, MID, FLOOR + 64, size=[2, 2, 2])
            send('pastehilight; paste')
            send('saveprefab prefab/st/rt')
            x, y = prefab_payload(user_file('st/box.obr')), prefab_payload(user_file('st/rt.obr'))
            t.expect_true('decompressed files are identical apart from the stored origin', x == y, 'base64 lengths %d vs %d' % (len(x), len(y)))
        t.step('copyprefab + paste reproduces the geometry exactly', copy_paste)

        def copy_out_of_view():
            # Put an entity on the engine's entity clipboard first.
            send('cancelsel')
            ed.sel(MID, MID, FLOOR)
            send('newent playerstart')
            t.expect_true('an entity is selected to copy', len(state().ent_sel) == 1)
            send('entcopy; entcancel; pasteclear; cancelsel')
            # Look at the sky: the stale selection box is out of view.
            ed.aim(180, 80)
            send('copyprefab prefab/st/box')
            send('pastehilight')
            s = state()
            t.expect('pastehilight took the prefab size (x)', s.sel.sx, 2)
            t.expect('pastehilight took the prefab size (y)', s.sel.sy, 2)
            t.expect('pastehilight took the prefab size (z)', s.sel.sz, 2)
            send('entpaste')
            t.expect('no stale entities pasted with the prefab', len(state().ent_sel), 0)
            ed.frame(MID, MID, FLOOR, dist=256, yaw=0, pitch=-45)
        t.step('copyprefab: works with the selection out of view, replaces copied entities', copy_out_of_view)

        def overwrite():
            ed.sel(MID, MID, FLOOR - GRID, size=[3, 1, 1])
            send('saveprefab prefab/st/box')
            t.expect('prefabinfo reflects the new selection', ev('(prefabinfo prefab/st/box)'), '3 1 1 8 1')
        t.step('saveprefab over an existing prefab replaces it', overwrite)

        def rename():
            t.expect('rename into a new folder', ev('(renameprefab prefab/st/rt prefab/st2/rt2)'), '1')
            t.expect_true('source file is gone', not exists(user_file('st/rt.obr')))
            t.expect_true('target file exists', exists(user_file('st2/rt2.obr')))
            t.expect('rename onto an existing prefab is refused', ev('(renameprefab prefab/st2/rt2 prefab/st/box)'), '0')
            t.expect_true('both files survive the refused rename', exists(user_file('st2/rt2.obr')) and exists(user_file('st/box.obr')))
            t.expect('a target outside prefab/ is refused', ev('(renameprefab prefab/st2/rt2 st2/rt2)'), '0')
            t.expect('a target with .. is refused', ev('(renameprefab prefab/st2/rt2 prefab/../rt2)'), '0')
            t.expect('a target segment ending in . is refused', ev('(renameprefab prefab/st2/rt2 prefab/st2./rt2)'), '0')
            # Valid on its own (500 chars: it fits a MAXSTRLEN string with
            # ".obr"), but not once the home dir is prefixed: refused rather
            # than truncated, and the console message shows it was the guard.
            long_name = 'prefab/' + 'a' * 493
            out = send('echo (concatword "SELFTEST_EVAL=" (renameprefab prefab/st2/rt2 %s))' % long_name, 1)
            res = None
            for line in out:
                m = re.search(r'SELFTEST_EVAL=(.*)$', line)
                if m:
                    res = m.group(1).strip()
            t.expect('a target too long for the home dir is refused', res, '0')
            t.expect_true('by the length guard', any('Prefab path too long' in l for l in out), ' | '.join(out))
            t.expect_true('the source survives the refused long rename', exists(user_file('st2/rt2.obr')))
            t.expect('a case-only rename is allowed', ev('(renameprefab prefab/st2/rt2 prefab/st2/RT2)'), '1')
            names = os.listdir(user_file('st2'))
            t.expect_true('the file took the new case', 'RT2.obr' in names, 'files: ' + ', '.join(names))
            t.expect('prefabinfo under the new name', ev('(prefabinfo prefab/st2/RT2)'), '2 2 2 8 1')
        t.step('renameprefab', rename)

        def remove():
            t.expect('remove', ev('(removeprefab prefab/st2/RT2)'), '1')
            t.expect_true('file left the prefab dir', not exists(user_file('st2/RT2.obr')))
            t.expect_true('file is in backups', exists(backup_file('st2/RT2.obr')))
            t.expect('prefabinfo no longer finds it', ev('(prefabinfo prefab/st2/RT2)'), '')
            t.expect('removing it again fails', ev('(removeprefab prefab/st2/RT2)'), '0')
            t.expect('a bare name is refused', ev('(removeprefab box)'), '0')
        t.step('removeprefab moves the file to backups/', remove)

        def shipped():
            if not exists(SHIPPED_DIR):
                os.mkdir(SHIPPED_DIR)
                created_shipped_dir[0] = True
            shutil.copyfile(user_file('st/box.obr'), SHIPPED_FILE)
            t.expect('prefabinfo marks it shipped', ev('(prefabinfo prefab/zz_selftest_shipped)'), '3 1 1 8 0')
            t.expect('remove is refused', ev('(removeprefab prefab/zz_selftest_shipped)'), '0')
            t.expect('rename is refused', ev('(renameprefab prefab/zz_selftest_shipped prefab/zz2)'), '0')
            t.expect_true('the file is untouched', exists(SHIPPED_FILE))
            t.expect('a user prefab cannot be renamed over it', ev('(renameprefab prefab/st/box prefab/zz_selftest_shipped)'), '0')
        t.step('a prefab outside the home dir is read-only', shipped)

        # Not covered here, by design: a selection over 100 MB (saveprefab's
        # NULL guard) and IDF_MAP refusal are unreachable from the harness.

        # ==== file browser widget =========================================

        def browser_helpers():
            t.expect('prefab type extension', ev('(tool_file_type_exts $TOOL_FILE_PREFAB)'), 'obr')
            t.expect('dedup keeps one of each', ev('(listlen (tool_filelist_dedup "a b a c b"))'), '3')
            t.expect('dedup keeps the first order', ev('(at (tool_filelist_dedup "a b a c b") 2)'), 'c')
            send('tool_filelist_curdir = ""')
            t.expect('relpath at the root', ev('(tool_filelist_relpath oak)'), 'oak')
            t.expect('dirpath at the root', ev('(tool_filelist_dirpath prefab)'), 'prefab')
            send('tool_filelist_curdir = trees')
            t.expect('relpath in a folder', ev('(tool_filelist_relpath oak)'), 'trees/oak')
            t.expect('dirpath in a folder', ev('(tool_filelist_dirpath prefab)'), 'prefab/trees')
            send('tool_filelist_curdir = ""')
        t.step('file browser: prefab type and helpers', browser_helpers)

        def browser_instances():
            send('tool_filelist_curdir = sounds; tool_filelist_filter_query = x; tool_filelist_instance_enter pfltest')
            t.expect('a fresh instance starts at the root', ev('$tool_filelist_curdir'), '')
            t.expect('a fresh instance has no selection', ev('$tool_filelist_sel_index'), '-1')
            # The search query is not swapped: the engine keeps one text editor
            # (and its focus) per variable name, so an instance searches
            # through a variable of its own instead.
            t.expect('the plain query is left alone', ev('$tool_filelist_filter_query'), 'x')
            send('tool_filelist_curdir = st; tool_filelist_instance_leave pfltest')
            t.expect("leave restores the caller's directory", ev('$tool_filelist_curdir'), 'sounds')
            t.expect("the caller's query is untouched", ev('$tool_filelist_filter_query'), 'x')
            t.expect('the query is not stashed per instance', ev('(identexists tool_filelist_pfltest_filter_query)'), '0')
            t.expect('leave keeps the instance directory', ev('(getalias tool_filelist_pfltest_curdir)'), 'st')
            send('tool_filelist_instance_enter pfltest')
            t.expect('re-entering restores the instance directory', ev('$tool_filelist_curdir'), 'st')
            send('tool_filelist_instance_leave pfltest; tool_filelist_curdir = ""; tool_filelist_filter_query = ""')
            t.expect('search variable without an instance', ev('(tool_filelist_query_var "")'), 'tool_filelist_filter_query')
            t.expect('search variable of an instance', ev('(tool_filelist_query_var prefab)'), 'tool_filelist_prefab_filter_query')
            t.expect('filter: no query shows everything', ev('(tool_filelist_filter sounds "")'), '1')
            t.expect('filter: a match, any case', ev('(tool_filelist_filter Sounds oun)'), '1')
            t.expect('filter: no match', ev('(tool_filelist_filter sounds zz)'), '0')
        t.step('file browser: per-instance state', browser_instances)

        picker = ('tool_filelist_curdir = ""; toolpanel_open tool_fileselect_picker center [p_title = "File browser"; p_width = (uiwidth 0.25); '
                  'p_user_data = [p_var = mapmusic; p_file_type = 2; p_width = (uiwidth 0.25)]]')

        def picker_browses():
            ed.cursor('on')
            send(picker, 600)
            # data/ has ~40 top-level folders and "sounds" sorts past what fits
            # in the unscrolled root view; narrow the picker's own search to
            # bring it into view, then clear the search again before the shot.
            send('tool_filelist_filter_query = sounds', 400)
            t.expect_true('the data/sounds folder is listed', find_click('sounds') is not None)
            send('tool_filelist_filter_query = ""', 400)
            t.expect_true('the OK/Cancel footer is shown', find_click('Cancel') is not None)
            # Look at this one: "Go up" (arrow icon, top left) must be greyed out at the root.
            core.invoke_shot('prefab-picker-root', quiet=True)
            send('toolpanel_close tool_fileselect_picker', 300)
            ed.cursor('off')
        t.step('file browser: an existing picker still browses data/', picker_browses)

        # The "existing pickers" checks, through the real ui_tool_fileselect
        # controls: their Browse sets tool_filelist_curdir from the current
        # value (or p_dir) and the picker lists it through tool_filelist_dirpath.
        def mapmusic():
            prev = ev('$mapmusic')
            ed.cursor('on')
            try:
                send('tool_do_action ta_mapsettings', 600)
                # The control shows the value, or "Random" when there is none
                click(prev if prev else 'Random')
                t.expect('Browse opened the picker', ev('(toolpanel_isopen tool_fileselect_picker)'), '1')
                t.expect("the picker starts in the control's folder", ev('$tool_filelist_curdir'), 'sounds/music')
                t.expect_true('it lists data/sounds/music', find_click('track_01.ogg') is not None)
                go_up()
                go_up()
                t.expect('Go up reaches the root', ev('$tool_filelist_curdir'), '')
                send('tool_filelist_filter_query = sounds', 400)
                t.expect_true('the root lists the data/ folders', find_click('sounds') is not None)
                click('sounds', double=True)
                t.expect('double-click entered sounds', ev('$tool_filelist_curdir'), 'sounds')
                send('tool_filelist_filter_query = music', 400)
                click('music', double=True)
                send('tool_filelist_filter_query = ""', 400)
                t.expect('double-click entered sounds/music', ev('$tool_filelist_curdir'), 'sounds/music')
                click('track_02.ogg')
                click('Ok')
                t.expect('OK set mapmusic', ev('$mapmusic'), 'sounds/music/track_02.ogg')
                t.expect('OK closed the picker', ev('(toolpanel_isopen tool_fileselect_picker)'), '0')
            finally:
                send('toolpanel_close tool_fileselect_picker; toolpanel_close tool_map_settings; tool_filelist_filter_query = ""; mapmusic "%s"' % prev, 400)
                ed.cursor('off')
            t.expect('mapmusic restored', ev('$mapmusic'), prev)
        t.step('file browser: the mapmusic control browses data/ and OK selects', mapmusic)

        def hazetex():
            prev = ev('$hazetex')
            prev_tab = ev('(getalias tool_env_tabs)') or '0'
            ed.cursor('on')
            try:
                # Environment panel, Haze tab (index 8 in ui_tool_env_view)
                send('tool_env_tabs = 8; tool_do_action ta_env', 700)
                # The Default column's control comes first (the Alternate one
                # shows the same value further right)
                click(prev)
                t.expect('Browse opened the picker', ev('(toolpanel_isopen tool_fileselect_picker)'), '1')
                t.expect("the picker starts in the value's folder", ev('$tool_filelist_curdir'), 'textures')
                go_up()
                t.expect('Go up reaches the root', ev('$tool_filelist_curdir'), '')
                send('tool_filelist_filter_query = textures', 400)
                t.expect_true('the root lists the data/ folders', find_click('textures') is not None)
                click('textures', double=True)
                t.expect('double-click entered textures', ev('$tool_filelist_curdir'), 'textures')
                send('tool_filelist_filter_query = waterfalln', 400)
                click('waterfalln')
                click('Ok')
                t.expect('OK set hazetex', ev('$hazetex'), 'textures/waterfalln')
                t.expect('OK closed the picker', ev('(toolpanel_isopen tool_fileselect_picker)'), '0')
            finally:
                send('toolpanel_close tool_fileselect_picker; toolpanel_close tool_env; tool_filelist_filter_query = ""; tool_env_tabs = "%s"; '
                     'hazetex "%s"' % (prev_tab, prev), 400)
                ed.cursor('off')
            t.expect('hazetex restored', ev('$hazetex'), prev)
        t.step('file browser: the hazetex control browses data/ and OK selects', hazetex)

        # ==== prefab library ==============================================

        def validation():
            cases = [('plain name', '(tool_prefab_name_valid "oak_2-b.c")', '1'), ('empty name', '(tool_prefab_name_valid "")', '0'),
                     ('dot-dot', '(tool_prefab_name_valid "..")', '0'), ('space', '(tool_prefab_name_valid "o k")', '0'),
                     ('slash', '(tool_prefab_name_valid "a/b")', '0'), ('trailing dot (Windows strips it)', '(tool_prefab_name_valid "oak.")', '0'),
                     ('leading dot (the browser hides it)', '(tool_prefab_name_valid ".oak")', '0'),
                     ('dot in the middle', '(tool_prefab_name_valid "o.ak")', '1'), ('root folder', '(tool_prefab_folder_valid "")', '1'),
                     ('nested folder', '(tool_prefab_folder_valid "a/b")', '1'), ('double slash', '(tool_prefab_folder_valid "a//b")', '0'),
                     ('leading slash', '(tool_prefab_folder_valid "/a")', '0'), ('trailing slash', '(tool_prefab_folder_valid "a/")', '0'),
                     ('space in folder', '(tool_prefab_folder_valid "a b")', '0'), ('hidden folder', '(tool_prefab_folder_valid "a/.b")', '0')]
            for what, expr, want in cases:
                t.expect(what, ev(expr), want)
        t.step('library: name and folder validation', validation)

        def paths():
            t.expect('root path', ev('(tool_prefab_path "" oak)'), 'prefab/oak')
            t.expect('folder path', ev('(tool_prefab_path trees oak)'), 'prefab/trees/oak')
            t.expect('folder of a nested path', ev('(tool_prefab_path_folder prefab/trees/big/oak)'), 'trees/big')
            t.expect('folder of a root path', ev('(tool_prefab_path_folder prefab/oak)'), '')
            t.expect('name of a path', ev('(tool_prefab_path_name prefab/trees/oak)'), 'oak')
            # listfiles returns the folders in the filesystem's order: sorted on
            # NTFS, not on APFS or ext4, so compare them as a set.
            folders = sorted((ev('(tool_prefab_list_folders)') or '').split())
            t.expect('top-level folders', ' '.join(folders), '"st" "st2"')
        t.step('library: paths', paths)

        t.step('library: actions are registered', lambda: t.expect('Prefabs category', ev('$tool_actions_Prefabs'), 'ta_prefabs ta_prefab_save ta_prefab_load'))

        def wrappers():
            ed.sel(MID, MID, FLOOR - GRID, size=[1, 1, 3])
            send('tool_prefab_save prefab/lib/one')
            t.expect_true('saved', exists(user_file('lib/one.obr')))
            t.expect('the new prefab is selected', ev('$tool_prefab_cur'), 'prefab/lib/one')
            t.expect('its info is cached', ev('$tool_prefab_cur_info'), '1 1 3 8 1')
            t.expect('a rescan is requested', ev('$tool_prefab_needs_rescan'), '1')
            ed.sel(MID, MID, FLOOR - GRID, size=[2, 2, 2])
            send('entcopybuf = "playerstart 0"; pasteclear; tool_prefab_load prefab/lib/one')
            t.expect('loading clears the script entity clipboard', ev('$entcopybuf'), '')
            send('cancelsel; pastehilight')
            t.expect('the clipboard holds the prefab', state().sel.sz, 3)
            send('tool_prefab_rename prefab/lib/one prefab/lib/two')
            t.expect_true('renamed on disk', exists(user_file('lib/two.obr')))
            t.expect('the selection follows the rename', ev('$tool_prefab_cur'), 'prefab/lib/two')
            # tool_prefab_remove asks first; its confirmed half does the work.
            send('tool_prefab_pending = prefab/lib/two; tool_prefab_remove_confirmed')
            t.expect_true('moved to backups', exists(backup_file('lib/two.obr')))
            t.expect('the selection is cleared', ev('$tool_prefab_cur'), '')
        t.step('library: save, load, rename, delete through the wrappers', wrappers)

        def write_failure():
            ro = user_file('lib/ro.obr')
            try:
                ed.sel(MID, MID, FLOOR - GRID, size=[2, 1, 1])
                send('tool_prefab_save prefab/lib/ro')
                t.expect_true('saved', exists(ro))
                t.expect('prefabinfo before the failed overwrite', ev('(prefabinfo prefab/lib/ro)'), '2 1 1 8 1')
                # Read-only: fopen(..., "wb") fails for it (unless run as root).
                os.chmod(ro, stat.S_IRUSR | stat.S_IRGRP | stat.S_IROTH)
                ed.sel(MID, MID, FLOOR - GRID, size=[3, 3, 1])
                send('tool_prefab_save prefab/lib/ro')
                t.expect('the failure is reported', ev('$tool_info_text'), 'Could not save prefab')
                t.expect('prefabinfo still reports the old size', ev('(prefabinfo prefab/lib/ro)'), '2 1 1 8 1')
            finally:
                if exists(ro):
                    os.chmod(ro, stat.S_IRUSR | stat.S_IWUSR)
                    os.remove(ro)
        t.step('library: save reports a write failure without touching the old file', write_failure)

        # ==== prefabs panel ===============================================

        def binds():
            t.expect('toolbar has the Prefabs button', ev('(listhas $toolbar_actions_right ta_prefabs)'), '1')
            # Checked in the file, not by exec'ing the preset: that would rebind
            # the harness home's keys for every later suite.
            preset = os.path.join(core.REPO_ROOT, 'config', 'tool', 'binds', 'default.cfg')
            with open(preset, encoding='utf-8') as f:
                preset_lines = f.read().splitlines()
            with open(os.path.join(core.REPO_ROOT, 'config', 'setup.cfg'), encoding='utf-8') as f:
                setup_lines = f.read().splitlines()
            t.expect_true('default preset binds I to the panel', any(re.match(r'^toolbind I ta_prefabs$', l) for l in preset_lines))
            taken = [l for l in setup_lines + preset_lines if re.match(r'^\s*(tool|edit)bind I\b', l)]
            t.expect('nothing else binds I in edit mode', len(taken), 1)
        t.step('panel: toolbar button and default binds', binds)

        def browse_select():
            ed.cursor('on')
            send('tool_filelist_prefab_curdir = ""; tool_prefab_select ""')
            open_prefab_panel()
            t.expect_true('folder st is listed', find_click('st') is not None)
            click('st', double=True)
            t.expect('double-click entered the folder', ev('(getalias tool_filelist_prefab_curdir)'), 'st')
            # Search before selecting: once box is selected the header prints
            # its full path ("st/box"), which itself contains the needle.
            t.expect_true('box tile is listed', find_click('box') is not None)
            send('tool_filelist_prefab_filter_query = zz', 400)
            t.expect_true('search hides non-matching tiles', find_click('box') is None)
            send('tool_filelist_prefab_filter_query = ""', 400)
            t.expect_true('clearing the search shows it again', find_click('box') is not None)
            click('box')
            t.expect('click selects the prefab', ev('$tool_prefab_cur'), 'prefab/st/box')
            # Double-click ids are global: the instance keeps a click here and
            # one on the same index in a picker from counting as a double-click
            t.expect("the tile's double-click id names the instance", ev('$ui_last_press_id'), 'tool_filelist_item_prefab_f_0')
            t.expect('the header has its info', ev('$tool_prefab_cur_info'), '3 1 1 8 1')
            # Look at this one: the header preview must be a 3x1x1 bar.
            core.invoke_shot('prefab-panel', quiet=True)
        t.step('panel: browse into a folder, select, search', browse_select)

        def double_click_load():
            send('pasteclear; cancelsel')
            click('box', double=True)
            send('pastehilight')
            s = state()
            t.expect('clipboard size x', s.sel.sx, 3)
            t.expect('clipboard size y', s.sel.sy, 1)
            t.expect('clipboard size z', s.sel.sz, 1)
        t.step('panel: double-click loads the prefab to the clipboard', double_click_load)

        def context_menu():
            click('box', right=True)
            t.expect('menu opened', ev('(toolpanel_isopen toolpanel_menu)'), '1')
            t.expect('Rename enabled for a user prefab', ev('(tool_prefab_menu_disabled 2)'), '0')
            t.expect('Delete enabled for a user prefab', ev('(tool_prefab_menu_disabled 3)'), '0')
            send('toolpanel_close toolpanel_menu', 300)
            send('tool_filelist_prefab_curdir = ""; tool_prefab_rescan', 400)
            click('zz_selftest_shipped', right=True)
            t.expect('Overwrite disabled for a shipped prefab', ev('(tool_prefab_menu_disabled 1)'), '1')
            t.expect('Rename disabled for a shipped prefab', ev('(tool_prefab_menu_disabled 2)'), '1')
            t.expect('Delete disabled for a shipped prefab', ev('(tool_prefab_menu_disabled 3)'), '1')
            # Look at this one: three greyed-out items.
            core.invoke_shot('prefab-menu-shipped', quiet=True)
            send('toolpanel_close toolpanel_menu', 300)
        t.step('panel: the context menu respects read-only prefabs', context_menu)

        def save_popup():
            send('tool_filelist_prefab_curdir = st; tool_prefab_rescan', 400)
            ed.sel(MID, MID, FLOOR - GRID, size=[1, 2, 1])
            click('Save...')
            t.expect('popup opened', ev('(toolpanel_isopen tool_prefab_save)'), '1')
            t.expect('folder defaults to the browser folder', ev('(tool_prefab_form_get_folder)'), 'st')
            send('tool_prefab_form_name = box', 300)
            t.expect_true('an existing name offers Overwrite', find_click('Overwrite') is not None)
            core.invoke_shot('prefab-save-overwrite', quiet=True)
            send('tool_prefab_form_name = newone', 300)
            click('Save')
            t.expect_true('saved', exists(user_file('st/newone.obr')))
            t.expect('the new prefab is selected', ev('$tool_prefab_cur'), 'prefab/st/newone')
            t.expect('popup closed', ev('(toolpanel_isopen tool_prefab_save)'), '0')
        t.step('panel: the save popup saves, and offers Overwrite for an existing name', save_popup)

        def rename_popup():
            send('toolpanel_open tool_prefab_rename popup [p_position = [0.4 0.3]; p_width = 0.3]', 400)
            t.expect('name prefilled', ev('$tool_prefab_form_name'), 'newone')
            t.expect('folder prefilled', ev('(tool_prefab_form_get_folder)'), 'st')
            send('tool_prefab_form_name = box', 300)
            t.expect_true('the clash is reported', find_click('A prefab with that name already exists') is not None)
            send('tool_prefab_form_name = renamed', 300)
            click('Rename')
            t.expect_true('renamed on disk', exists(user_file('st/renamed.obr')))
            t.expect('the selection follows', ev('$tool_prefab_cur'), 'prefab/st/renamed')
        t.step('panel: the rename popup renames, and refuses clashes', rename_popup)

        def delete_popup():
            send('tool_prefab_remove prefab/st/renamed', 400)
            t.expect_true('nothing happens before confirming', exists(user_file('st/renamed.obr')))
            click('Confirm')
            t.expect_true('moved to backups', exists(backup_file('st/renamed.obr')))
            t.expect_true('gone from the library', not exists(user_file('st/renamed.obr')))
        t.step('panel: delete asks, then moves the file to backups/', delete_popup)

        def beside_picker():
            send('tool_filelist_prefab_curdir = st; tool_prefab_rescan', 400)
            send(picker, 600)
            # Narrow the picker's own search to bring "sounds" into view
            # instead of teaching the harness to scroll.
            send('tool_filelist_filter_query = sounds', 400)
            # Each search field shows its own query. The picker is centred and
            # the Prefabs panel docked right, so left to right: picker, Prefabs.
            eds = drawn_editors()
            t.expect('two search fields are drawn', len(eds), 2)
            if len(eds) == 2:
                t.expect("the picker's search field", eds[0]['text'], 'sounds')
                t.expect("the Prefabs panel's search field is still empty", eds[1]['text'], '')
                t.expect("the Prefabs panel's field has its own editor", eds[1]['name'], 'tool_filelist_prefab_filter_query')
            send('tool_filelist_prefab_filter_query = zz', 400)
            eds = drawn_editors()
            if len(eds) == 2:
                t.expect("the Prefabs panel's search field", eds[1]['text'], 'zz')
                t.expect("the picker's search field is unchanged", eds[0]['text'], 'sounds')
            t.expect_true('the Prefabs query does not filter the picker', find_click('sounds') is not None)
            send('tool_filelist_prefab_filter_query = ""', 400)
            click('sounds', double=True)
            t.expect('the picker navigated', ev('$tool_filelist_curdir'), 'sounds')
            # Nothing in data/sounds matches "sounds": the empty list must keep
            # its height, not push the footer out of the panel
            t.expect_true('an empty list keeps the OK/Cancel footer', find_click('Cancel') is not None)
            t.expect("a picker tile's double-click id has no instance", ev('(strstr $ui_last_press_id "tool_filelist_item_d_")'), '0')
            t.expect('the prefab browser did not move', ev('(getalias tool_filelist_prefab_curdir)'), 'st')
            t.expect_true('the prefab browser still shows its tiles', find_click('box') is not None)
            # Look at this one: the picker's search reads "sounds", the Prefabs
            # panel's is empty (its "[Enter text here]" prompt only).
            core.invoke_shot('prefab-with-picker', quiet=True)
            send('toolpanel_close tool_fileselect_picker', 300)
            ed.cursor('off')
        t.step('panel: the browser keeps its own state beside a file picker', beside_picker)
    finally:
        if exists(SHIPPED_FILE):
            os.remove(SHIPPED_FILE)
        if created_shipped_dir[0] and exists(SHIPPED_DIR) and not os.listdir(SHIPPED_DIR):
            os.rmdir(SHIPPED_DIR)
        if not a.keep_running:
            core.stop()
    return t.finish()


if __name__ == '__main__':
    sys.exit(main())
