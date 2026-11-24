#include "engine.h"
#include "master.h"

#define MAIL_MAXQUEUE 1000
#define MAIL_RETRYMAX 3600

namespace master
{
    // mail the master sends, such as activation links, queued and tried again when the mail server can't take it
    SVAR(0, mastersmtphost, "");
    VAR(0, mastersmtpport, 1, 587, 65535);
    VAR(0, mastersmtpstarttls, 0, 1, 1); // 1 upgrades a plain connection with STARTTLS, 0 is TLS from the start as on port 465
    SVAR(0, mastersmtpuser, "");
    SVAR(0, mastermailfrom, "");
    SVAR(0, mastermailname, "");
    VAR(0, mastermailtries, 1, 6, 50); // attempts at each message before it's given up on
    VAR(0, mastermailretry, 1, 60, 3600); // seconds before the first retry, doubling each time after


    struct mailitem
    {
        char *to, *subject, *text, *html;
        int tries, fetch;
        time_t next;

        mailitem() : to(NULL), subject(NULL), text(NULL), html(NULL), tries(0), fetch(-1), next(0) {}
        ~mailitem() { DELETEA(to); DELETEA(subject); DELETEA(text); DELETEA(html); }
    };

    struct maildef
    {
        char *type;
        scripttemplate subject, text, html;

        maildef(const char *t) : type(newstring(t)) {}
        ~maildef() { DELETEA(type); }

        void set(scripttemplate &part, const char *kind, const char *src) { part.set(tempformatstring("mastermaildef %s %s", type, kind), src); }
    };

    static struct mailstate
    {
        vector<mailitem *> queue;
        mailitem *sending;
        hashtable<const char *, maildef *> defs;

        mailstate() : sending(NULL) {}
    } mails;

    ICOMMAND(0, mastermaildef, "ssss", (char *type, char *subjectcode, char *textcode, char *htmlcode),
    {
        if(!type || !*type) return;
        maildef **existing = mails.defs.access(type);
        if((!subjectcode || !*subjectcode) && (!textcode || !*textcode) && (!htmlcode || !*htmlcode))
        {
            if(existing)
            {
                maildef *d = *existing;
                mails.defs.remove(d->type);
                delete d;
            }
            return;
        }
        maildef *d = existing ? *existing : NULL;
        if(!d)
        {
            d = new maildef(type);
            mails.defs[d->type] = d;
        }
        d->set(d->subject, "subject", subjectcode);
        d->set(d->text, "text", textcode);
        d->set(d->html, "html", htmlcode);
    });

    // a plain address with no name, comments or anything that could start a header of its own
    bool validemail(const char *addr)
    {
        int len = strlen(addr), at = -1;
        if(len < 3 || len > MAIL_MAXADDR) return false;
        loopi(len)
        {
            uchar c = addr[i];
            if(c <= ' ' || c >= 0x7F || strchr("<>()[]\\,;:\"", c)) return false;
            if(c == '@') { if(at >= 0) return false; at = i; }
        }
        if(at <= 0 || at >= len-1 || at > 64) return false;
        const char *domain = addr + at + 1;
        if(!strchr(domain, '.') || domain[0] == '.' || addr[len-1] == '.' || strstr(domain, "..")) return false;
        return true;
    }

    static bool queuemail(const char *to, const char *subject, const char *text, const char *html = NULL)
    {
        if(!validemail(to)) return false;
        if(mails.queue.length() >= MAIL_MAXQUEUE)
        {
            // the notices never have the address it was for
            conoutf(colourred, "Mail queue is full, not sending '%s' to %s", subject, to);
            foldnotice("master", NOTICE_ERROR, "mail full", tempformatstring("Mail queue is full, not sending '%s'", subject));
            return false;
        }
        mailitem *m = new mailitem;
        m->to = newstring(to);
        m->subject = newstring(subject);
        m->text = newstring(text);
        m->html = html && *html ? newstring(html) : NULL;
        mails.queue.add(m);
        return true;
    }

    static char *trimmaillines(char *src)
    {
        if(!src) return NULL;
        vector<char> out;
        const char *p = src;
        bool started = false;
        while(*p)
        {
            const char *eol = strchr(p, '\n');
            const char *end = eol ? eol : p + strlen(p);
            const char *a = p, *b = end;
            while(a < b && (*a == ' ' || *a == '\t' || *a == '\r')) a++;
            while(b > a && (b[-1] == ' ' || b[-1] == '\t' || b[-1] == '\r')) b--;
            if(a < b)
            {
                started = true;
                out.put(a, int(b - a));
                out.add('\n');
            }
            else if(started) out.add('\n');
            if(!eol) break;
            p = eol + 1;
        }
        while(out.length() > 1 && out[out.length() - 1] == '\n' && out[out.length() - 2] == '\n') out.pop();
        DELETEA(src);
        if(out.empty()) return NULL;
        out.add('\0');
        return newstring(out.getbuf());
    }

    bool queuemaildef(const char *type, const char *to, const char *defsubject, const char *deftext, const tagval *extraargs, int numextra)
    {
        tagval args[MAXARGS];
        args[0].setcstr(defsubject);
        int n = 1;
        loopi(min(numextra, MAXARGS - 1)) args[n++] = extraargs[i];
        maildef **dp = mails.defs.access(type);
        maildef *d = dp ? *dp : NULL;
        char *subjectbuf = d ? d->subject.run(args, n) : NULL;
        const char *subject = defsubject;
        if(subjectbuf && *subjectbuf)
        {
            for(char *p = subjectbuf; *p; p++) if(*p == '\r' || *p == '\n') *p = ' ';
            subject = subjectbuf;
            args[0].setcstr(subject);
        }
        char *textbuf = d ? trimmaillines(d->text.run(args, n)) : NULL;
        const char *text = textbuf && *textbuf ? textbuf : NULL;
        if(!text)
        {
            conoutf(colouryellow, "Master mail template '%s' is %s, using fallback text", type, !d ? "not defined" : "empty");
            text = deftext;
        }
        char *htmlbuf = d ? trimmaillines(d->html.run(args, n)) : NULL;
        bool ok = queuemail(to, subject, text, htmlbuf && *htmlbuf ? htmlbuf : NULL);
        DELETEA(subjectbuf);
        DELETEA(textbuf);
        DELETEA(htmlbuf);
        return ok;
    }

    static void mailsent(httpfetch &f)
    {
        mailitem *m = (mailitem *)f.ctx;
        if(m != mails.sending) return;
        mails.sending = NULL;
        m->fetch = -1;
        m->tries++;
        if(f.status == 250)
        {
            conoutf(colourwhite, "Mailed '%s' to %s", m->subject, m->to);
            mails.queue.removeobj(m);
            delete m;
            return;
        }
        if(m->tries >= mastermailtries)
        {
            conoutf(colourred, "Gave up mailing '%s' to %s after %d tries: %s", m->subject, m->to, m->tries, f.error ? f.error : "no answer");
            foldnotice("master", NOTICE_ERROR, "mail gaveup", tempformatstring("Gave up mailing '%s' after %d tries: %s", m->subject, m->tries, f.error ? f.error : "no answer"));
            mails.queue.removeobj(m);
            delete m;
            return;
        }
        int delay = min(mastermailretry << min(m->tries - 1, 10), MAIL_RETRYMAX);
        m->next = time(NULL) + delay;
        conoutf(colourorange, "Couldn't mail '%s' to %s, trying again in %d seconds: %s", m->subject, m->to, delay, f.error ? f.error : "no answer");
        foldnotice("master", NOTICE_WARNING, "mail retry", tempformatstring("Couldn't mail '%s', trying again in %d seconds: %s", m->subject, delay, f.error ? f.error : "no answer"));
    }

    // one message at a time, the oldest first
    void checkmail()
    {
        if(mails.sending || mails.queue.empty() || !*mastersmtphost || !*mastermailfrom) return;
        time_t now = time(NULL);
        mailitem *m = NULL;
        loopv(mails.queue) if(mails.queue[i]->next <= now) { m = mails.queue[i]; break; }
        if(!m) return;
        const char *domain = strchr(mastermailfrom, '@');
        httpmail *hm = new httpmail;
        hm->starttls = mastersmtpstarttls != 0;
        hm->port = mastersmtpport;
        copystring(hm->host, mastersmtphost);
        copystring(hm->helo, domain && domain[1] ? domain + 1 : "localhost");
        hm->user = newstring(mastersmtpuser);
        hm->pass = newstring(mastersecrets.smtppass);
        hm->from = newstring(mastermailfrom);
        hm->to = newstring(m->to);
        time_t t = time(NULL);
        string date;
        strftime(date, sizeof(date), "%a, %d %b %Y %H:%M:%S +0000", gmtime(&t));
        vector<char> msg;
        uint mid[2];
        http::randombytes((uchar *)mid, sizeof(mid));
        string from;
        if(*mastermailname) formatstring(from, "%s <%s>", mastermailname, mastermailfrom);
        else formatstring(from, "<%s>", mastermailfrom);
        if(m->html && *m->html)
        {
            uint bnd[3];
            http::randombytes((uchar *)bnd, sizeof(bnd));
            defformatstring(boundary, "re-%08x%08x%08x", bnd[0], bnd[1], bnd[2]);
            defformatstring(head, "From: %s\nTo: <%s>\nSubject: %s\nDate: %s\nMessage-ID: <%u.%u.%u@%s>\nMIME-Version: 1.0\nContent-Type: multipart/alternative; boundary=\"%s\"\n\n",
                from, m->to, m->subject, date, uint(t), mid[0], mid[1], hm->helo, boundary);
            msg.put(head, strlen(head));
            defformatstring(part1, "--%s\nContent-Type: text/plain; charset=utf-8\nContent-Transfer-Encoding: 8bit\n\n", boundary);
            msg.put(part1, strlen(part1));
            size_t textlen = strlen(m->text);
            msg.put(m->text, textlen);
            if(textlen && m->text[textlen - 1] != '\n') msg.add('\n');
            defformatstring(part2, "\n--%s\nContent-Type: text/html; charset=utf-8\nContent-Transfer-Encoding: 8bit\n\n", boundary);
            msg.put(part2, strlen(part2));
            size_t htmllen = strlen(m->html);
            msg.put(m->html, htmllen);
            if(htmllen && m->html[htmllen - 1] != '\n') msg.add('\n');
            defformatstring(end, "\n--%s--\n", boundary);
            msg.put(end, strlen(end));
        }
        else
        {
            defformatstring(head, "From: %s\nTo: <%s>\nSubject: %s\nDate: %s\nMessage-ID: <%u.%u.%u@%s>\nMIME-Version: 1.0\nContent-Type: text/plain; charset=utf-8\nContent-Transfer-Encoding: 8bit\n\n",
                from, m->to, m->subject, date, uint(t), mid[0], mid[1], hm->helo);
            msg.put(head, strlen(head));
            msg.put(m->text, strlen(m->text));
        }
        msg.add('\0');
        mails.sending = m;
        int id = http::sendmail(hm, msg.getbuf(), mailsent, m); // a failure answers at once, which may have freed it
        if(mails.queue.find(m) >= 0 && mails.sending == m) m->fetch = id;
    }

    void cleanupmail()
    {
        if(mails.sending && mails.sending->fetch >= 0) http::cancel(mails.sending->fetch);
        mails.sending = NULL;
        mails.queue.deletecontents();
        enumerate(mails.defs, maildef *, d, delete d);
        mails.defs.clear();
    }
}
