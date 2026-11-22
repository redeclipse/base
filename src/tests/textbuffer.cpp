// textbuffer.cpp: checks the text field buffer in engine/textbuffer.cpp: typing, the filter and limit, movement,
// selection and indenting. Build and run from src:
//   g++ -O1 -std=c++17 -DSTANDALONE -I. -Ishared -Iengine -Igame -Ienet/include -Isupport tests/textbuffer.cpp engine/textbuffer.cpp -o textbuffer
//   ./textbuffer
#include "cube.h"
#include "textbuffer.h"

// the character classes from shared/stream.cpp, which can't be linked on its own
#define CUBECTYPE(s, p, d, a, A, u, U) \
    0, U, U, U, U, U, U, U, U, s, s, s, s, s, U, U, \
    U, U, U, U, U, U, U, U, U, U, U, U, U, U, U, U, \
    s, p, p, p, p, p, p, p, p, p, p, p, p, p, p, p, \
    d, d, d, d, d, d, d, d, d, d, p, p, p, p, p, p, \
    p, A, A, A, A, A, A, A, A, A, A, A, A, A, A, A, \
    A, A, A, A, A, A, A, A, A, A, A, p, p, p, p, p, \
    p, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, \
    a, a, a, a, a, a, a, a, a, a, a, p, p, p, p, U, \
    U, u, u, u, u, u, u, u, u, u, u, u, u, u, u, u, \
    u, u, u, u, u, u, u, u, u, u, u, u, u, u, u, U, \
    u, U, u, U, u, U, u, U, u, U, u, U, u, U, u, U, \
    u, U, u, U, u, U, u, U, u, U, u, U, u, U, u, U, \
    u, U, u, U, u, U, u, U, U, u, U, u, U, u, U, U, \
    U, U, U, U, U, U, U, U, U, U, U, U, U, U, U, U, \
    U, U, U, U, u, u, u, u, u, u, u, u, u, u, u, u, \
    u, u, u, u, u, u, u, u, u, u, u, u, u, u, U, u

extern const uchar cubectype[256] =
{
    CUBECTYPE(CT_SPACE,
              CT_PRINT,
              CT_PRINT|CT_DIGIT,
              CT_PRINT|CT_ALPHA|CT_LOWER,
              CT_PRINT|CT_ALPHA|CT_UPPER,
              CT_PRINT|CT_UNICODE|CT_ALPHA|CT_LOWER,
              CT_PRINT|CT_UNICODE|CT_ALPHA|CT_UPPER)
};


static int failures = 0, checks = 0;

#define CHECK(cond) do { checks++; if(!(cond)) { failures++; printf("FAIL line %d: %s\n", __LINE__, #cond); } } while(0)
#define CHECKSTR(buf, want) do { checks++; if(strcmp((buf).str(), want)) { failures++; printf("FAIL line %d: got \"%s\" want \"%s\"\n", __LINE__, (buf).str(), want); } } while(0)

static void type(TextBuffer &b, const char *s)
{
    for(const char *c = s; *c; c++) b.insert(c, 1);
}

static void testtyping()
{
    TextBuffer b;
    b.setup(false, 0, NULL);
    type(b, "hello");
    CHECKSTR(b, "hello");
    CHECK(b.caret == 5 && !b.hasselection());
    b.move(TextBuffer::MOVE_ALL, -1);
    type(b, "> ");
    CHECKSTR(b, "> hello");
    b.insert("a\nb\tc\r\fd");
    CHECKSTR(b, "> a b cdhello"); // a single line takes no breaks, tabs or colour escapes

    TextBuffer m;
    m.setup(true, 0, NULL);
    m.insert("a\nb\tc");
    CHECKSTR(m, "a\nb    c");
}

static void testfilterlimit()
{
    TextBuffer b;
    b.setup(false, 0, "0123456789.-");
    b.insert("1a2b.3");
    CHECKSTR(b, "12.3"); // characters after a rejected one still go in
    CHECK(b.insert("x", 1) == 0);
    CHECKSTR(b, "12.3");

    TextBuffer l;
    l.setup(false, 5, NULL);
    CHECK(l.insert("abcdefg") == 5);
    CHECKSTR(l, "abcde");
    CHECK(l.insert("z") == 0);
    l.selectrange(1, 3);
    CHECK(l.insert("XYZ") == 2); // the selection's room counts
    CHECKSTR(l, "aXYde");

    TextBuffer cap;
    cap.setup(false, 0, NULL);
    CHECK(cap.limit == TEXTBUFFER_LINELIMIT);
    cap.setup(true, 999999, NULL);
    CHECK(cap.limit == TEXTBUFFER_AREALIMIT);
}

static void testmovement()
{
    TextBuffer b;
    b.setup(true, 0, NULL);
    b.set("one two_three, four\nsecond line");
    b.setcaret(0);
    b.move(TextBuffer::MOVE_WORD, 1);
    CHECK(b.caret == 3);
    b.move(TextBuffer::MOVE_WORD, 1);
    CHECK(b.caret == 13); // underscores are part of a word
    b.move(TextBuffer::MOVE_WORD, 1);
    CHECK(b.caret == 19);
    b.move(TextBuffer::MOVE_WORD, -1);
    CHECK(b.caret == 15);
    b.move(TextBuffer::MOVE_LINE, 1);
    CHECK(b.caret == 19);
    b.move(TextBuffer::MOVE_CHAR, 1);
    CHECK(b.caret == 20);
    b.move(TextBuffer::MOVE_LINE, 1, true);
    CHECK(b.selstart() == 20 && b.selend() == 31);
    b.move(TextBuffer::MOVE_CHAR, -1);
    CHECK(b.caret == 20 && !b.hasselection()); // left with a selection goes to its start
    b.move(TextBuffer::MOVE_ALL, 1);
    CHECK(b.caret == b.length());
    b.setcaret(-5);
    CHECK(b.caret == 0);
    b.setcaret(1000);
    CHECK(b.caret == b.length());
}

static void testselection()
{
    TextBuffer b;
    b.setup(true, 0, NULL);
    b.set("alpha  beta.gamma\nline two");
    b.selectword(2);
    CHECK(b.selstart() == 0 && b.selend() == 5);
    b.selectword(5);
    CHECK(b.selstart() == 5 && b.selend() == 7); // a run of spaces
    b.selectword(11);
    CHECK(b.selstart() == 11 && b.selend() == 12); // punctuation on its own
    b.selectline(3);
    CHECK(b.selstart() == 0 && b.selend() == 18); // with its line break
    b.selectline(20);
    CHECK(b.selstart() == 18 && b.selend() == b.length());
    b.selectall();
    vector<char> buf;
    b.getselection(buf);
    CHECK(buf.length() == b.length());
    b.selectrange(5, 12);
    b.erase(TextBuffer::EDIT_BACK);
    CHECKSTR(b, "alphagamma\nline two");
}

static void testerase()
{
    TextBuffer b;
    b.setup(false, 0, NULL);
    b.set("foo bar baz");
    b.setcaret(b.length());
    b.erase(TextBuffer::EDIT_BACK, true);
    CHECKSTR(b, "foo bar ");
    b.setcaret(0);
    b.erase(TextBuffer::EDIT_DELETE, true);
    CHECKSTR(b, " bar ");
    b.erase(TextBuffer::EDIT_DELETE);
    CHECKSTR(b, "bar ");
    CHECK(!b.erase(TextBuffer::EDIT_BACK)); // nothing before the caret
    b.setcaret(b.length());
    CHECK(!b.erase(TextBuffer::EDIT_DELETE));
}

static void testindent()
{
    TextBuffer b;
    b.setup(true, 0, NULL);
    b.set("a\n  b\nc");
    b.setcaret(1);
    b.indent();
    CHECKSTR(b, "a    \n  b\nc"); // no selection: spaces at the caret
    b.set("a\n  b\nc");
    b.selectrange(0, 4);
    b.indent();
    CHECKSTR(b, "    a\n      b\nc");
    CHECK(b.selstart() == 0 && b.selend() == 13);
    b.unindent();
    CHECKSTR(b, "a\n  b\nc");
    b.unindent();
    CHECKSTR(b, "a\nb\nc");
    CHECK(!b.unindent()); // nothing left to take off
    b.selectrange(0, 2); // ends at the start of the next line, which stays as it is
    b.indent();
    CHECKSTR(b, "    a\nb\nc");

    TextBuffer c;
    c.setup(true, 0, NULL);
    c.set("      x");
    c.setcaret(7);
    c.unindent();
    CHECKSTR(c, "  x");
    CHECK(c.caret == 3); // the caret keeps its place in the text
}

int main()
{
    testtyping();
    testfilterlimit();
    testmovement();
    testselection();
    testerase();
    testindent();
    printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
