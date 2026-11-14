#!/usr/bin/env python3
"""shader_new loader (shadersource.ps1): shader_new, shader_define,
shader_include_vs/fs, shader_source, variantshader_new. Needs a running
harness on a build with the loader; no map required.

Fixtures go to home/uitest/config/glsl/harness/ (findfile searches the home
dir first), plus one fixture at home/uitest/data/harness/ to prove the
"outside config/glsl" refusal is a real refusal and not just a missing file.
All fixture dirs are removed at the end, even on failure. Shader names carry a
per-run tag, so the script can be rerun in the same game session.
"""

import os
import random
import shutil
import sys

from _live import Tally, core


def unstamped(lines):
    return [core.strip_stamp(l).strip() for l in lines]


def main():
    t = Tally('shader source loader')
    tag = 'src%d' % random.randrange(1000000)
    fix_dir = os.path.join(core.HOME_DIR, 'config', 'glsl', 'harness')
    outside_dir = os.path.join(core.HOME_DIR, 'data', 'harness')
    run_dir = os.path.join(core.HOME_DIR, 'shadercorpus', 'srcloader')
    os.makedirs(fix_dir, exist_ok=True)
    os.makedirs(outside_dir, exist_ok=True)
    shutil.rmtree(run_dir, ignore_errors=True)

    def echo(lines, expected):
        return unstamped(lines).count(expected) == 1

    def logged(lines, text):
        return any(text in l for l in lines)

    try:
        vert = 'attribute vec4 vvertex;\nvoid main(void)\n{\n    gl_Position = vvertex;\n}\n'
        frag = ('fragdata(0) vec4 fragcolor;\nvoid main(void)\n{\n#ifdef SRC_FLAG\n    #if SRC_MODE == 2\n    fragcolor = SRC_COLOR;\n'
                '    #endif\n#else\n    fragcolor = vec4(1.0);\n#endif\n}\n')
        frag2 = 'fragdata(0) vec4 fragcolor;\nvoid main(void)\n{\n    fragcolor = vec4(0.0);\n}\n'
        fogfrag = 'fragdata(0) vec4 fragcolor;\nvoid main(void)\n{\n    fragcolor = vec4(1.0);\n    //:fog\n}\n'
        variantfrag = 'fragdata(0) vec4 fragcolor;\nvoid main(void)\n{\n    fragcolor = vec4(1.0);\n    //:variant\n}\n'
        # CRLF and no final newline: the loader must drop the CRs and add the newline.
        common = '// shared\r\n#define SRC_COLOR vec4(0.25, 0.5, 0.75, 1.0)'
        for name, text in (('t.vert', vert), ('t.frag', frag), ('t2.frag', frag2), ('fog.frag', fogfrag), ('variant.frag', variantfrag),
                           ('common.glsl', common),
                           # "Empty" is the assembled TEXT being empty: a lone '\r' is 1 byte
                           # on disk, but the loader strips it and adds no line of its own.
                           ('empty.frag', '\r')):
            with open(os.path.join(fix_dir, name), 'w', encoding='utf-8', newline='') as f:
                f.write(text)
        # A real file outside config/glsl, so the SRC_OUTSIDE refusal is proven
        # to come from the path rule, not from the target simply not existing.
        with open(os.path.join(outside_dir, 't.frag'), 'w', encoding='utf-8', newline='') as f:
            f.write(frag)

        defs = '#define SRC_MODE 2\n#define SRC_FLAG\n'
        expect_vs = defs + vert
        expect_fs = defs + '// shared\n#define SRC_COLOR vec4(0.25, 0.5, 0.75, 1.0)\n' + frag

        # 'setshader null' clears leftover texture-slot params, which shader()
        # would otherwise turn into extra uniform declarations.
        made = core.invoke_batch('''setshader null
shader_new 0 {t}ok [
    shader_define SRC_MODE 2
    shader_define SRC_FLAG ""
    shader_include_fs "config/glsl/harness/common.glsl"
    shader_source "config/glsl/harness/t.vert" "config/glsl/harness/t.frag"
]
echo (concatword "SRC_OK=" (hasshader {t}ok))
shader_new 0 {t}fog [ shader_source "config/glsl/harness/t.vert" "config/glsl/harness/fog.frag" ]
echo (concatword "SRC_FOG=" (hasshader {t}fog))
setshaderparam srcparam 1 2 3 4
shader_new 0 {t}param [ shader_source "config/glsl/harness/t.vert" "config/glsl/harness/t.frag" ]
echo (concatword "SRC_PARAM=" (hasshader {t}param))
shader_new 0 {t}outer [
    shader_define SRC_MODE 2
    shader_new 0 {t}inner [ shader_source "config/glsl/harness/t.vert" "config/glsl/harness/t.frag" ]
    shader_define SRC_FLAG ""
    shader_include_fs "config/glsl/harness/common.glsl"
    shader_source "config/glsl/harness/t.vert" "config/glsl/harness/t.frag"
]
echo (concatword "SRC_OUTER=" (hasshader {t}outer) " SRC_INNER=" (hasshader {t}inner))
srcran = 0
shader_new 0 {t}ok [ srcran = 1 ]
echo (concatword "SRC_RAN=" $srcran)
shader_new 0 {t}twice [
    shader_source "config/glsl/harness/t.vert" "config/glsl/harness/t.frag"
    shader_source "config/glsl/harness/t.vert" "config/glsl/harness/t2.frag"
]
echo (concatword "SRC_TWICE=" (hasshader {t}twice))
shader_new 0 {t}genvariant [ shader_source "config/glsl/harness/t.vert" "config/glsl/harness/variant.frag" ]
echo (concatword "SRC_VARIANTGEN=" (hasshader "<variant:0,0>{t}genvariant"))'''.format(t=tag), 1, 120)
        t.check('a shader from defines, an include and two files is created', echo(made, 'SRC_OK=1'))
        t.check('a //:fog pragma inside a file still creates the shader', echo(made, 'SRC_FOG=1'))
        t.check('a shader with a texture-slot param is created', echo(made, 'SRC_PARAM=1'))
        t.check('nested shader_new bodies both create their shaders', echo(made, 'SRC_OUTER=1 SRC_INNER=1'))
        t.check('the body of an existing shader is not run', echo(made, 'SRC_RAN=0'))
        t.check('shader_source called twice creates the shader', echo(made, 'SRC_TWICE=1'))
        t.check('a //:variant pragma inside a file creates the generic variant', echo(made, 'SRC_VARIANTGEN=1'))

        failed = core.invoke_batch('''shader_new 0 {t}missing [ shader_source "config/glsl/harness/t.vert" "config/glsl/harness/nope.frag" ]
echo (concatword "SRC_MISSING=" (hasshader {t}missing))
shader_new 0 {t}dotdot [ shader_source "config/glsl/../config/glsl/harness/t.vert" "config/glsl/harness/t.frag" ]
echo (concatword "SRC_DOTDOT=" (hasshader {t}dotdot))
shader_new 0 {t}outside [ shader_source "config/glsl/harness/t.vert" "data/harness/t.frag" ]
echo (concatword "SRC_OUTSIDE=" (hasshader {t}outside))
shader_new 0 {t}bothbad [ shader_source "data/harness/t.frag" "config/glsl/../config/glsl/harness/t.frag" ]
echo (concatword "SRC_BOTHBAD=" (hasshader {t}bothbad))
shader_new 0 {t}badname [ shader_define "1BAD" 1; shader_source "config/glsl/harness/t.vert" "config/glsl/harness/t.frag" ]
echo (concatword "SRC_BADNAME=" (hasshader {t}badname))
shader_new 0 {t}badvalue [ shader_define SRC_X "1^n2"; shader_source "config/glsl/harness/t.vert" "config/glsl/harness/t.frag" ]
echo (concatword "SRC_BADVALUE=" (hasshader {t}badvalue))
shader_new 0 {t}onestage [ shader_source "config/glsl/harness/t.vert" "" ]
echo (concatword "SRC_ONESTAGE=" (hasshader {t}onestage))
shader_new 0 {t}orphan [ shader_include_vs "config/glsl/harness/common.glsl"; shader_source "" "config/glsl/harness/t.frag" ]
echo (concatword "SRC_ORPHAN=" (hasshader {t}orphan))
shader_new 0 {t}badinclude [ shader_include_fs "config/glsl/harness/noinclude.glsl"; shader_source "config/glsl/harness/t.vert" "config/glsl/harness/t.frag" ]
echo (concatword "SRC_BADINCLUDE=" (hasshader {t}badinclude))
shader_new 0 {t}emptyfrag [ shader_source "config/glsl/harness/t.vert" "config/glsl/harness/empty.frag" ]
echo (concatword "SRC_EMPTYFRAG=" (hasshader {t}emptyfrag))
shader_define SRC_LOOSE 1'''.format(t=tag), 1, 120)
        t.check('a missing file creates no shader', echo(failed, 'SRC_MISSING=0'))
        t.check('a missing file is named in the log', logged(failed, 'cannot read config/glsl/harness/nope.frag'))
        t.check('a .. path is refused', echo(failed, 'SRC_DOTDOT=0'))
        t.check('a path outside config/glsl is refused', echo(failed, 'SRC_OUTSIDE=0'))
        t.check('both bad paths in one shader_source call are refused', echo(failed, 'SRC_BOTHBAD=0'))
        t.check('refused paths are logged', len([l for l in failed if 'refusing' in l]) == 4)
        t.check('an invalid define name is refused', echo(failed, 'SRC_BADNAME=0'))
        t.check('a define value with a newline is refused', echo(failed, 'SRC_BADVALUE=0'))
        t.check('invalid defines are logged', len([l for l in failed if 'invalid define' in l]) == 2)
        t.check('shader_new needs both stages', echo(failed, 'SRC_ONESTAGE=0') and logged(failed, 'needs both a vertex and a fragment source'))
        t.check('includes without a source for that stage are refused', echo(failed, 'SRC_ORPHAN=0') and logged(failed, 'includes given but no vertex source'))
        t.check('an unreadable include creates no shader', echo(failed, 'SRC_BADINCLUDE=0'))
        t.check('an unreadable include is named in the log', logged(failed, 'cannot read config/glsl/harness/noinclude.glsl'))
        t.check('an empty source file creates no shader', echo(failed, 'SRC_EMPTYFRAG=0'))
        t.check('an empty source file is logged', logged(failed, 'config/glsl/harness/empty.frag is empty'))
        t.check('shader_define outside a body is reported', logged(failed, 'only valid inside a shader_new or variantshader_new body'))

        dump = core.invoke_batch('shaderdumpall srcloader s00 0', 1, 300)
        t.check('the dump ran', any('SHADERDUMP srcloader s00 ' in l for l in dump))
        rows = []

        def load_rows():
            with open(os.path.join(run_dir, 'manifest.tsv'), encoding='utf-8', newline='') as f:
                rows[:] = f.read().splitlines()

        def stage(name, filename):
            # None for a missing manifest row (a shader that was never created).
            match = [r for r in rows if r.split('\t')[0] == name]
            if len(match) != 1:
                return None
            with open(os.path.join(run_dir, 'blobs', match[0].split('\t')[2], filename), encoding='utf-8', newline='') as f:
                return f.read()

        load_rows()
        t.check('vertex text is the defines then the file', stage(tag + 'ok', 'vs.glsl') == expect_vs)
        t.check('fragment text is the defines, the CR-stripped include, then the file', stage(tag + 'ok', 'fs.glsl') == expect_fs)
        t.check('genfogshader still runs on assembled text (fs)', 'uniform vec3 fogcolor;' in (stage(tag + 'fog', 'fs.glsl') or ''))
        t.check('genfogshader still runs on assembled text (vs)', 'lineardepth = dot(lineardepthscale, gl_Position.zw);' in (stage(tag + 'fog', 'vs.glsl') or ''))
        t.check('texture-slot params still become uniforms (vs)', 'uniform vec4 srcparam;' in (stage(tag + 'param', 'vs.glsl') or ''))
        t.check('texture-slot params still become uniforms (fs)', 'uniform vec4 srcparam;' in (stage(tag + 'param', 'fs.glsl') or ''))
        t.check('the outer body keeps its own defines', stage(tag + 'outer', 'fs.glsl') == expect_fs)
        t.check('the inner body gets none of the outer defines', stage(tag + 'inner', 'fs.glsl') == frag)
        t.check('shader_source called twice: the last call wins (vs)', stage(tag + 'twice', 'vs.glsl') == vert)
        t.check('shader_source called twice: the last call wins (fs)', stage(tag + 'twice', 'fs.glsl') == frag2)

        # variantshader_new. A variant with only a fragment file reuses the
        # parent's vertex stage. The dump above ran before these existed.
        v = core.invoke_batch('''setshader null
shader_new 0 {t}parent [ shader_source "config/glsl/harness/t.vert" "config/glsl/harness/t.frag" ]
variantshader_new 0 {t}parent 1 1 [
    shader_define SRC_MODE 2
    shader_define SRC_FLAG ""
    shader_include_fs "config/glsl/harness/common.glsl"
    shader_source "" "config/glsl/harness/t.frag"
]
echo (concatword "SRC_VARIANT=" (hasshader "<variant:0,1>{t}parent"))
srcvarran = 0
variantshader_new 0 {t}noparent 1 1 [ srcvarran = 1 ]
echo (concatword "SRC_VARRAN=" $srcvarran)
variantshader_new 0 {t}parent 1 1 [ shader_include_vs "config/glsl/harness/common.glsl"; shader_source "" "config/glsl/harness/t.frag" ]
echo (concatword "SRC_VARORPHAN=" (hasshader "<variant:1,1>{t}parent"))
variantshader_new 0 {t}rowless -1 0 [ shader_source "config/glsl/harness/t.vert" "config/glsl/harness/t.frag" ]
echo (concatword "SRC_ROWLESS=" (hasshader {t}rowless))
variantshader_new 0 {t}parent 2 1 [ shader_source "config/glsl/harness/t.vert" "config/glsl/harness/nope.frag" ]
echo (concatword "SRC_VARBADPATH=" (hasshader "<variant:0,2>{t}parent"))
variantshader_new 0 {t}parent 3 1 [ shader_source "" "config/glsl/harness/empty.frag" ]
echo (concatword "SRC_VAREMPTY=" (hasshader "<variant:0,3>{t}parent"))
srcvarrow99ran = 0
variantshader_new 0 {t}parent 99 1 [ srcvarrow99ran = 1 ]
echo (concatword "SRC_ROW99RAN=" $srcvarrow99ran)'''.format(t=tag), 1, 120)
        t.check('a variant with only a fragment file is created', echo(v, 'SRC_VARIANT=1'))
        t.check('no body runs when the parent is missing', echo(v, 'SRC_VARRAN=0'))
        t.check('variant includes without a source are refused', echo(v, 'SRC_VARORPHAN=0') and logged(v, 'includes given but no vertex source'))
        t.check('row -1 behaves like shader_new', echo(v, 'SRC_ROWLESS=1'))
        t.check('a failed variant body creates no variant', echo(v, 'SRC_VARBADPATH=0'))
        t.check('a failed variant body names the variant, not the parent', logged(v, '<variant row 2>%sparent: cannot read config/glsl/harness/nope.frag' % tag))
        t.check('a failed variant build is reported against the variant label', logged(v, 'shader <variant row 2>%sparent: not created' % tag))
        t.check('an empty variant source file creates no variant', echo(v, 'SRC_VAREMPTY=0'))
        t.check('an empty variant source file is logged against the variant label',
                logged(v, 'shader <variant row 3>%sparent: config/glsl/harness/empty.frag is empty' % tag))
        t.check('row >= MAXVARIANTROWS does not run the body', echo(v, 'SRC_ROW99RAN=0'))

        shutil.rmtree(run_dir, ignore_errors=True)
        core.invoke_batch('shaderdumpall srcloader s00 0', 1, 300)
        load_rows()
        t.check('the variant fragment stage is assembled', stage('<variant:0,1>%sparent' % tag, 'fs.glsl') == expect_fs)
        t.check('the variant reuses the parent vertex stage', stage('<variant:0,1>%sparent' % tag, 'vs.glsl') == vert)
    finally:
        # Cleanup -- always, even if an assertion or a batch threw.
        for d in (fix_dir, outside_dir, run_dir):
            shutil.rmtree(d, ignore_errors=True)
    return t.finish()


if __name__ == '__main__':
    sys.exit(main())
