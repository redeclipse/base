#!/usr/bin/env python3
"""Task 8 (task8-editor.ps1): editor.py subcommand behaviour, against a running harness."""

import os
import sys

from _live import Tally, core
import editor as ed


def main():
    t = Tally('task 8 editor')
    ed.newmap(12)
    t.run('state reports edit mode', lambda: ed.get_state().mode.edit_mode == 1)

    def goto():
        ed.goto(512, 512, 512)
        return abs(ed.get_state().cam.x - 512) < 1
    t.run('goto moves the view', goto)

    def aim():
        ed.aim(90, 0)
        return abs(ed.get_state().cam.yaw - 90) < 1
    t.run('aim sets yaw', aim)

    def frame():
        ed.frame(512, 512, 512, dist=100, yaw=0, pitch=0)
        s = ed.get_state()
        return abs(s.cam.y - 412) < 1 and abs(s.cam.yaw) < 1
    t.run('frame positions and aims', frame)

    def cursor():
        ed.cursor('on')
        on = ed.get_state().ui.free_cursor
        ed.cursor('off')
        return on != 0 and ed.get_state().ui.free_cursor == 0
    t.run('cursor on then off', cursor)

    def sel():
        ed.sel(512, 512, 512)
        return ed.get_state().sel.have_sel == 1
    t.run('sel selects a cube', sel)

    def unknown_key():
        try:
            ed.key('NOTAKEY')
            return False
        except core.HarnessError as e:
            return "Unknown key name 'NOTAKEY'" in str(e)
    t.run('key rejects unknown keys', unknown_key)
    t.run('shot writes a png', lambda: os.path.exists(ed.shot('task8')))
    return t.finish()


if __name__ == '__main__':
    sys.exit(main())
