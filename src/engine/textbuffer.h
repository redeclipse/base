// the text, caret and selection of a text field, without any drawing or input handling
#define TEXTBUFFER_LINELIMIT 4096 // most characters a single line field holds
#define TEXTBUFFER_AREALIMIT 65536 // most characters a text area holds
#define TEXTBUFFER_INDENT 4 // spaces a tab becomes

struct TextBuffer
{
    enum { MOVE_CHAR = 0, MOVE_WORD, MOVE_LINE, MOVE_ALL };
    enum { EDIT_BACK = 0, EDIT_DELETE };

    vector<char> text; // always ends with a terminator
    int caret, anchor, limit;
    bool multiline;
    char *filter;

    TextBuffer();
    ~TextBuffer();

    const char *str() const { return text.getbuf(); }
    int length() const { return text.length() - 1; }
    bool empty() const { return length() <= 0; }

    void setup(bool multiline_, int limit_, const char *filter_);
    void set(const char *str); // replaces the text, keeping the caret where it can
    bool equals(const char *str) const { return !strcmp(text.getbuf(), str); }

    bool hasselection() const { return caret != anchor; }
    int selstart() const { return min(caret, anchor); }
    int selend() const { return max(caret, anchor); }
    void getselection(vector<char> &buf) const;

    void setcaret(int pos, bool extend = false);
    void move(int kind, int dir, bool extend = false);
    void selectall();
    void selectword(int pos);
    void selectline(int pos);
    void selectrange(int start, int end);
    int linestart(int pos) const;
    int lineend(int pos) const;
    int wordstart(int pos) const;
    int wordend(int pos) const;

    bool accepts(uchar c) const;
    int insert(const char *str, int len = -1); // returns how many characters went in
    bool erase(int kind, bool word = false); // backspace (EDIT_BACK) or delete (EDIT_DELETE), or the selection if there is one

private:
    void replace(int pos, int count, const char *str, int len);
};
