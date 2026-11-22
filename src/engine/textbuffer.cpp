// textbuffer.cpp: the text, caret, selection and undo history behind a text field
#include "cube.h"
#include "textbuffer.h"

static inline bool iswordchar(uchar c) { return iscubealnum(c) || c == '_'; }

TextBuffer::TextBuffer() : caret(0), anchor(0), limit(0), multiline(false), filter(NULL), undobytes(0), grouping(false)
{
    text.add('\0');
}

TextBuffer::~TextBuffer()
{
    clearhistory();
    DELETEA(filter);
}

void TextBuffer::setup(bool multiline_, int limit_, const char *filter_)
{
    multiline = multiline_;
    int cap = multiline ? TEXTBUFFER_AREALIMIT : TEXTBUFFER_LINELIMIT;
    limit = limit_ > 0 ? min(limit_, cap) : cap;
    if(filter_ && *filter_)
    {
        if(!filter || strcmp(filter, filter_)) { DELETEA(filter); filter = newstring(filter_); }
    }
    else DELETEA(filter);
}

void TextBuffer::set(const char *str)
{
    int len = min(int(strlen(str)), limit > 0 ? limit : TEXTBUFFER_AREALIMIT);
    text.setsize(0);
    text.put(str, len);
    text.add('\0');
    caret = clamp(caret, 0, length());
    anchor = caret;
    clearhistory();
}

void TextBuffer::getselection(vector<char> &buf) const
{
    buf.put(&text[selstart()], selend() - selstart());
}

void TextBuffer::setcaret(int pos, bool extend)
{
    caret = clamp(pos, 0, length());
    if(!extend) anchor = caret;
    grouping = false;
}

int TextBuffer::linestart(int pos) const
{
    pos = clamp(pos, 0, length());
    while(pos > 0 && text[pos-1] != '\n') pos--;
    return pos;
}

int TextBuffer::lineend(int pos) const
{
    pos = clamp(pos, 0, length());
    while(pos < length() && text[pos] != '\n') pos++;
    return pos;
}

int TextBuffer::wordstart(int pos) const
{
    while(pos > 0 && !iswordchar(text[pos-1])) pos--;
    while(pos > 0 && iswordchar(text[pos-1])) pos--;
    return pos;
}

int TextBuffer::wordend(int pos) const
{
    while(pos < length() && !iswordchar(text[pos])) pos++;
    while(pos < length() && iswordchar(text[pos])) pos++;
    return pos;
}

void TextBuffer::move(int kind, int dir, bool extend)
{
    int pos = caret;
    switch(kind)
    {
        case MOVE_CHAR:
            if(hasselection() && !extend) pos = dir < 0 ? selstart() : selend();
            else pos += dir < 0 ? -1 : 1;
            break;
        case MOVE_WORD: pos = dir < 0 ? wordstart(caret) : wordend(caret); break;
        case MOVE_LINE: pos = dir < 0 ? linestart(caret) : lineend(caret); break;
        case MOVE_ALL: pos = dir < 0 ? 0 : length(); break;
    }
    setcaret(pos, extend);
}

void TextBuffer::selectrange(int start, int end)
{
    anchor = clamp(start, 0, length());
    caret = clamp(end, 0, length());
    grouping = false;
}

void TextBuffer::selectall() { selectrange(0, length()); }

void TextBuffer::selectword(int pos)
{
    pos = clamp(pos, 0, length());
    if(pos >= length() && pos > 0) pos--;
    if(pos >= length()) { selectrange(pos, pos); return; }
    int start = pos, end = pos;
    uchar c = text[pos];
    if(iswordchar(c))
    {
        while(start > 0 && iswordchar(text[start-1])) start--;
        while(end < length() && iswordchar(text[end])) end++;
    }
    else if(iscubespace(c) && c != '\n')
    {
        while(start > 0 && text[start-1] != '\n' && iscubespace(text[start-1])) start--;
        while(end < length() && text[end] != '\n' && iscubespace(text[end])) end++;
    }
    else end++;
    selectrange(start, end);
}

void TextBuffer::selectline(int pos)
{
    int end = lineend(pos);
    if(end < length()) end++; // take the line break too, as other editors do
    selectrange(linestart(pos), end);
}

bool TextBuffer::accepts(uchar c) const
{
    if(!c || c == '\r' || c == '\f') return false;
    if(c == '\n' && !multiline) return false;
    if(filter && !strchr(filter, c)) return false;
    return true;
}

int TextBuffer::insert(const char *str, int len, int kind, int millis)
{
    if(len < 0) len = strlen(str);
    vector<char> buf;
    loopi(len)
    {
        uchar c = str[i];
        if(c == '\t') { loopj(multiline ? TEXTBUFFER_INDENT : 1) if(accepts(' ')) buf.add(' '); continue; }
        if(c == '\n' && !multiline) c = ' ';
        if(accepts(c)) buf.add(c);
    }
    int room = limit - (length() - (selend() - selstart()));
    if(buf.length() > room) buf.setsize(max(room, 0));
    if(buf.empty()) return 0;
    replace(selstart(), selend() - selstart(), buf.getbuf(), buf.length(), kind, millis);
    return buf.length();
}

bool TextBuffer::erase(int kind, bool word, int millis)
{
    if(hasselection())
    {
        replace(selstart(), selend() - selstart(), "", 0, EDIT_OTHER, millis);
        return true;
    }
    if(kind == EDIT_BACK)
    {
        if(caret <= 0) return false;
        int start = word ? wordstart(caret) : caret - 1;
        replace(start, caret - start, "", 0, word ? EDIT_OTHER : EDIT_BACK, millis);
    }
    else
    {
        if(caret >= length()) return false;
        int end = word ? wordend(caret) : caret + 1;
        replace(caret, end - caret, "", 0, word ? EDIT_OTHER : EDIT_DELETE, millis);
    }
    return true;
}

bool TextBuffer::indent(int millis)
{
    int start = selstart(), end = selend();
    if(!hasselection() || !memchr(&text[start], '\n', end - start))
    {
        static const char spaces[TEXTBUFFER_INDENT+1] = "    ";
        return insert(spaces, TEXTBUFFER_INDENT, EDIT_OTHER, millis) > 0;
    }
    if(text[end-1] == '\n') end--;
    int ls = linestart(start), le = lineend(end), lines = 1;
    for(int i = ls; i < le; i++) if(text[i] == '\n') lines++;
    if(length() + lines*TEXTBUFFER_INDENT > limit) return false;
    vector<char> buf;
    for(int i = ls; i <= le; i++)
    {
        if(i == ls || text[i-1] == '\n') loopj(TEXTBUFFER_INDENT) buf.add(' ');
        if(i < le) buf.add(text[i]);
    }
    replace(ls, le - ls, buf.getbuf(), buf.length(), EDIT_OTHER, millis);
    selectrange(ls, ls + buf.length());
    return true;
}

bool TextBuffer::unindent(int millis)
{
    bool sel = hasselection();
    int start = selstart(), end = selend();
    if(sel && text[end-1] == '\n' && end > start) end--;
    int ls = linestart(start), le = lineend(end), removed = 0, caretremoved = 0;
    vector<char> buf;
    for(int i = ls; i <= le;)
    {
        int n = 0;
        while(n < TEXTBUFFER_INDENT && i + n < le && text[i+n] == ' ') n++;
        if(!sel && i == linestart(caret)) caretremoved = min(n, caret - i);
        removed += n;
        i += n;
        while(i < le && text[i] != '\n') buf.add(text[i++]);
        if(i < le) buf.add(text[i]);
        i++;
    }
    if(!removed) return false;
    int oldcaret = caret;
    replace(ls, le - ls, buf.getbuf(), buf.length(), EDIT_OTHER, millis);
    if(sel) selectrange(ls, ls + buf.length());
    else setcaret(oldcaret - caretremoved);
    if(!undos.empty()) { undos.last()->after = caret; undos.last()->afteranchor = anchor; }
    return true;
}

void TextBuffer::apply(int pos, int count, const char *str, int len)
{
    text.remove(pos, count);
    if(len > 0) text.insert(pos, str, len);
}

void TextBuffer::replace(int pos, int count, const char *str, int len, int kind, int millis)
{
    int before = caret, beforeanchor = anchor;
    record(pos, &text[pos], count, str, len, kind, before, beforeanchor, millis);
    apply(pos, count, str, len);
    caret = anchor = pos + len;
    Edit *e = undos.last();
    e->after = caret;
    e->afteranchor = anchor;
}

void TextBuffer::record(int pos, const char *removed, int removedlen, const char *inserted, int insertedlen, int kind, int before, int beforeanchor, int millis)
{
    clearredos();
    Edit *last = grouping && !undos.empty() ? undos.last() : NULL;
    if(last && last->kind == kind && millis - last->millis < TEXTBUFFER_UNDOGROUP && last->after == before && last->afteranchor == beforeanchor)
    {
        bool join = false;
        switch(kind)
        {
            case EDIT_TYPE: // a new step starts with each word
                join = !removedlen && last->pos + last->inserted.length() == pos && !(insertedlen && iswordchar(inserted[0]) && !last->inserted.empty() && !iswordchar(last->inserted.last()));
                if(join) last->inserted.put(inserted, insertedlen);
                break;
            case EDIT_BACK:
                join = !insertedlen && pos + removedlen == last->pos && last->inserted.empty();
                if(join) { last->removed.insert(0, removed, removedlen); last->pos = pos; }
                break;
            case EDIT_DELETE:
                join = !insertedlen && pos == last->pos && last->inserted.empty();
                if(join) last->removed.put(removed, removedlen);
                break;
        }
        if(join)
        {
            last->millis = millis;
            undobytes += removedlen + insertedlen;
            trimhistory();
            return;
        }
    }
    Edit *e = new Edit;
    e->pos = pos;
    e->kind = kind;
    e->before = before;
    e->beforeanchor = beforeanchor;
    e->after = e->afteranchor = pos + insertedlen;
    e->millis = millis;
    e->removed.put(removed, removedlen);
    e->inserted.put(inserted, insertedlen);
    undos.add(e);
    undobytes += removedlen + insertedlen;
    grouping = kind != EDIT_OTHER;
    trimhistory();
}

void TextBuffer::trimhistory()
{
    while(undos.length() > 1 && (undos.length() > TEXTBUFFER_UNDOSTEPS || undobytes > TEXTBUFFER_UNDOBYTES))
    {
        Edit *e = undos.remove(0);
        undobytes -= e->removed.length() + e->inserted.length();
        delete e;
    }
}

void TextBuffer::clearredos()
{
    loopv(redos) undobytes -= redos[i]->removed.length() + redos[i]->inserted.length();
    redos.deletecontents();
}

void TextBuffer::clearhistory()
{
    undos.deletecontents();
    redos.deletecontents();
    undobytes = 0;
    grouping = false;
}

bool TextBuffer::undo()
{
    if(undos.empty()) return false;
    Edit *e = undos.pop();
    apply(e->pos, e->inserted.length(), e->removed.getbuf(), e->removed.length());
    caret = clamp(e->before, 0, length());
    anchor = clamp(e->beforeanchor, 0, length());
    redos.add(e);
    grouping = false;
    return true;
}

bool TextBuffer::redo()
{
    if(redos.empty()) return false;
    Edit *e = redos.pop();
    apply(e->pos, e->removed.length(), e->inserted.getbuf(), e->inserted.length());
    caret = clamp(e->after, 0, length());
    anchor = clamp(e->afteranchor, 0, length());
    undos.add(e);
    grouping = false;
    return true;
}
