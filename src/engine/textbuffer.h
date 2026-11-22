// the text, caret, selection and undo history of a text field, without any drawing or input handling
#define TEXTBUFFER_LINELIMIT 4096 // most characters a single line field holds
#define TEXTBUFFER_AREALIMIT 65536 // most characters a text area holds
#define TEXTBUFFER_UNDOSTEPS 100
#define TEXTBUFFER_UNDOBYTES (256*1024)
#define TEXTBUFFER_UNDOGROUP 1000 // typing within this many milliseconds of the last keystroke joins its undo step
#define TEXTBUFFER_INDENT 4

struct TextBuffer
{
    enum { MOVE_CHAR = 0, MOVE_WORD, MOVE_LINE, MOVE_ALL };
    enum { EDIT_OTHER = 0, EDIT_TYPE, EDIT_BACK, EDIT_DELETE };

    struct Edit
    {
        int pos, kind, before, beforeanchor, after, afteranchor, millis;
        vector<char> removed, inserted;
    };

    vector<char> text; // always ends with a terminator
    int caret, anchor, limit;
    bool multiline;
    char *filter;
    vector<Edit *> undos, redos;
    int undobytes;
    bool grouping;

    TextBuffer();
    ~TextBuffer();

    const char *str() const { return text.getbuf(); }
    int length() const { return text.length() - 1; }
    bool empty() const { return length() <= 0; }

    void setup(bool multiline_, int limit_, const char *filter_);
    void set(const char *str); // replaces the text, keeps the caret where it can and forgets the history
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
    int insert(const char *str, int len = -1, int kind = EDIT_OTHER, int millis = 0); // returns how many characters went in
    bool erase(int kind, bool word = false, int millis = 0); // backspace (EDIT_BACK) or delete (EDIT_DELETE), or the selection if there is one
    bool indent(int millis = 0); // Tab: spaces at the caret, or every selected line indented when the selection spans lines
    bool unindent(int millis = 0); // Shift+Tab: up to one indent taken off the start of the caret's or each selected line

    bool undo();
    bool redo();
    bool canundo() const { return !undos.empty(); }
    bool canredo() const { return !redos.empty(); }
    void endgroup() { grouping = false; }
    void clearhistory();

private:
    void replace(int pos, int count, const char *str, int len, int kind, int millis);
    void apply(int pos, int count, const char *str, int len);
    void record(int pos, const char *removed, int removedlen, const char *inserted, int insertedlen, int kind, int before, int beforeanchor, int millis);
    void trimhistory();
    void clearredos();
};
