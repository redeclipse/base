"""Tests for tools/harness/shadercheck.py.

Run: wsl -d Ubuntu --exec python3 -m unittest discover -s "/mnt/f/Red Eclipse/tools/harness/tests" -p "test_shadercheck.py" -v
The glslang cases skip when glslangValidator is not installed.
"""
import os
import shutil
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
import shadercheck  # noqa: E402

VS = "#version 400\nin vec4 vvertex;\nvoid main(void)\n{\n    gl_Position = vvertex;\n}\n"
FS = ("#version 400\nlayout(location = 0) out vec4 fragcolor;\nuniform vec4 c;\n"
      "void main(void)\n{\n    vec4 x = c * 2.0;\n    fragcolor = x;\n}\n")
HAVE_GLSLANG = shutil.which("glslangValidator") is not None
HAVE_SPIRV = HAVE_GLSLANG and shutil.which("spirv-opt") is not None and shutil.which("spirv-remap") is not None


class Pure(unittest.TestCase):
    def test_tokens_ignore_spacing(self):
        self.assertEqual(shadercheck.tokens("a+=b;"), shadercheck.tokens("a  +=  b ;"))

    def test_tokens_keep_numbers_whole(self):
        self.assertEqual(shadercheck.tokens("x = 1.5e-3f;"), ["x", "=", "1.5e-3f", ";"])

    def test_tokens_distinguish_operators(self):
        self.assertNotEqual(shadercheck.tokens("a += b"), shadercheck.tokens("a + = b"))

    def test_strip_comments(self):
        self.assertEqual(shadercheck.tokens(shadercheck.strip_comments("a /* x */ b // y\nc")), ["a", "b", "c"])

    def test_line_directives_dropped(self):
        self.assertEqual(shadercheck.tokens('#line 3 "x"\na'), ["a"])


class Pairs(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.mkdtemp()

    def tearDown(self):
        shutil.rmtree(self.tmp)

    def blob(self, name, fs, vs=VS):
        d = os.path.join(self.tmp, name)
        os.makedirs(d)
        with open(os.path.join(d, "vs.full.glsl"), "w") as f:
            f.write(vs)
        with open(os.path.join(d, "fs.full.glsl"), "w") as f:
            f.write(fs)
        return d

    def test_comment_only_change_is_text(self):
        a = self.blob("a", FS)
        b = self.blob("b", FS.replace("fragcolor = x;", "fragcolor = x; // note"))
        self.assertEqual(shadercheck.check_pair("k", a, b, HAVE_GLSLANG)[1], "TEXT")

    @unittest.skipUnless(HAVE_GLSLANG, "glslangValidator not installed")
    def test_macro_spelling_is_text_after_preprocessing(self):
        a = self.blob("a", FS)
        b = self.blob("b", FS.replace("c * 2.0", "c * TWO").replace("uniform vec4 c;", "uniform vec4 c;\n#define TWO 2.0"))
        self.assertEqual(shadercheck.check_pair("k", a, b, True)[1], "TEXT")

    @unittest.skipUnless(HAVE_SPIRV, "glslang/spirv-tools not installed")
    def test_local_rename_is_spirv(self):
        a = self.blob("a", FS)
        b = self.blob("b", FS.replace("vec4 x", "vec4 y").replace("= x;", "= y;"))
        self.assertEqual(shadercheck.check_pair("k", a, b, True)[1], "SPIRV")

    @unittest.skipUnless(HAVE_SPIRV, "glslang/spirv-tools not installed")
    def test_constant_change_is_diff(self):
        a = self.blob("a", FS)
        b = self.blob("b", FS.replace("2.0", "3.0"))
        self.assertEqual(shadercheck.check_pair("k", a, b, True)[1], "DIFF")

    def test_without_glslang_a_real_change_is_na(self):
        a = self.blob("a", FS)
        b = self.blob("b", FS.replace("2.0", "3.0"))
        self.assertEqual(shadercheck.check_pair("k", a, b, False)[1], "NA")


if __name__ == "__main__":
    unittest.main()
