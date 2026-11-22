// textbuffer.cpp: the text, caret and selection behind a text field
#include "cube.h"
#include "textbuffer.h"

static inline bool iswordchar(uchar c) { return iscubealnum(c) || c == '_'; }

TextBuffer::TextBuffer() : caret(0), anchor(0), limit(0), multiline(false), filter(NULL)
{
    text.add('\0');
}

TextBuffer::~TextBuffer()
{
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
}

void TextBuffer::getselection(vector<char> &buf) const
{
    buf.put(&text[selstart()], selend() - selstart());
}

void TextBuffer::setcaret(int pos, bool extend)
{
    caret = clamp(pos, 0, length());
    if(!extend) anchor = caret;
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

int TextBuffer::insert(const char *str, int len)
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
    replace(selstart(), selend() - selstart(), buf.getbuf(), buf.length());
    return buf.length();
}

bool TextBuffer::erase(int kind, bool word)
{
    if(hasselection())
    {
        replace(selstart(), selend() - selstart(), "", 0);
        return true;
    }
    if(kind == EDIT_BACK)
    {
        if(caret <= 0) return false;
        int start = word ? wordstart(caret) : caret - 1;
        replace(start, caret - start, "", 0);
    }
    else
    {
        if(caret >= length()) return false;
        int end = word ? wordend(caret) : caret + 1;
        replace(caret, end - caret, "", 0);
    }
    return true;
}

void TextBuffer::replace(int pos, int count, const char *str, int len)
{
    text.remove(pos, count);
    if(len > 0) text.insert(pos, str, len);
    caret = anchor = pos + len;
}
