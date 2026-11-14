#!/usr/bin/env python3
"""Task 5 (task5-record.ps1): record a small corpus and check its shape."""

import os
import re
import sys

from _live import Tally, core
import shadercorpus as sc
import shaders


def main():
    t = Tally('task 5 record')
    # The m-<map> pass dumps map shaders and every generateshader-made shader;
    # conquest's texgrass slots give the map-dependent assertions something.
    code = shaders.main(['record', '--run', 't5', '--sids', 's00,s01,s07,s99', '--maps', 'conquest'])
    t.check('record exited cleanly', code == 0)
    d = os.path.join(core.HOME_DIR, 'shadercorpus', 't5')
    rows = sc.read_manifest(d)
    info = sc.read_run_info(d)
    t.check('run.txt has the sweep order', info['sweep'] == 's00 s01 s07 s99')
    t.check('run.txt has the commit', re.match(r'^[0-9a-f]{40}$', info['commit']) is not None)
    with open(os.path.join(d, 'settings', 's01.txt'), encoding='utf-8') as f:
        t.check('s01 settings recorded', f.read().strip() == 'msaa=4')
    for sid in ('s00', 's01', 's07', 's99', 'm-conquest'):
        t.check('rows for ' + sid, any(r.sid == sid for r in rows))
    t.check('msaa generated multisample light shaders', any(r.sid == 's01' and r.hash != '-' and r.name.startswith('deferredlightM') for r in rows))
    t.check('aotaps=12 reached the AO generator', any(r.sid == 's07' and r.hash != '-' and re.match(r'^ambientobscuranceshader .* 12$', r.origin) for r in rows))
    t.check('map pass dumps only map-dependent shaders', not any(r.sid == 'm-conquest' and r.name == 'stdworld' for r in rows))
    t.check('map pass includes generator shaders', any(r.sid == 'm-conquest' and r.hash != '-' and re.match(r'^(deferredlightshader|modelshader) ', r.origin) for r in rows))
    # Each model shader comes with its shimmer twin ('0' goes after the
    # a/A/u/w/d/D/n/m/e options, see loadshader()).
    models = [r for r in rows if r.sid == 'm-conquest' and r.hash != '-' and not r.name.startswith('<') and r.origin.startswith('modelshader ')]
    names = {r.name for r in models}
    no_twin = [r for r in models if '0' not in r.name and re.sub(r'^model([aAuwdDnme]*)', r'model\g<1>0', r.name) not in names]
    t.check('map pass has model shaders', len(models) > 0)
    t.check("map pass has every model shader's shimmer twin (%d missing)" % len(no_twin), not no_twin)
    with open(os.path.join(d, 'registry.txt'), encoding='utf-8', newline='') as f:
        registry = f.read().splitlines()
    t.check('registry lists stdworld', 'world stdworld ' in registry)
    t.check('registry lists stddecal', 'decal stddecal b' in registry)
    t.check('registry is the whole palette', len(registry) > 150)
    t.check('every registered shader is valid at every point', not sc.palette_coverage(rows, registry, ['s00', 's01', 's07', 's99']))
    t.check('no state leaked between s00 and s99', not [l for l in sc.find_sweep_leaks(rows, 's00', 's99') if sc.is_sweep_leak(l)])
    state = core.invoke_batch('echo (concatword "T5_MSAA=" $msaa " T5_TAPS=" $aotaps)', 1, 30)
    t.check('defaults restored afterwards', len([l for l in state if 'T5_MSAA=0 T5_TAPS=5' in l]) == 1)
    return t.finish()


if __name__ == '__main__':
    sys.exit(main())
