/*
 * prefs.c - the preferences file's contents
 *
 * Lines of key=value.  Anything missing, unknown or out of range keeps
 * its default, so a damaged file costs settings and nothing else.
 * Portable C.
 */

#include <string.h>
#include "tico.h"

static const char *const bar_names[] = {"show", "hide"};

void prefs_default(prefs *p)
{
    p->profile = PROFILE_MAC;
    p->title_bar = BAR_SHOW;
    p->shortcut_bar = BAR_SHOW;
    strcpy(p->font, "Monaco");
    p->size = 9;
}

static int same(const char *s, int n, const char *word)
{
    return (int)strlen(word) == n && memcmp(s, word, n) == 0;
}

static void set_bar(int *bar, const char *v, int n)
{
    int i;

    for (i = 0; i < BAR_COUNT; i++)
        if (same(v, n, bar_names[i]))
            *bar = i;
}

static void set_pref(prefs *p, const char *k, int klen, const char *v, int n)
{
    int i, size = 0;

    if (same(k, klen, "profile")) {
        for (i = 0; i < PROFILE_COUNT; i++)
            if (same(v, n, profile_name(i)))
                p->profile = i;
    } else if (same(k, klen, "title")) {
        set_bar(&p->title_bar, v, n);
    } else if (same(k, klen, "shortcuts")) {
        set_bar(&p->shortcut_bar, v, n);
    } else if (same(k, klen, "font")) {
        if (n > 0 && n < FONT_NAME_MAX) {
            memcpy(p->font, v, n);
            p->font[n] = 0;
        }
    } else if (same(k, klen, "size")) {
        for (i = 0; i < n && i < 3; i++) {
            if (v[i] < '0' || v[i] > '9')
                return;
            size = size * 10 + (v[i] - '0');
        }
        if (n <= 3 && size >= 6 && size <= 72)
            p->size = size;
    }
}

void prefs_parse(prefs *p, const char *text, long len)
{
    long i = 0, start, eq;

    while (i < len) {
        start = i;
        eq = -1;
        while (i < len && text[i] != '\r' && text[i] != '\n') {
            if (text[i] == '=' && eq < 0)
                eq = i;
            i++;
        }
        if (eq > start && i - eq - 1 < 64 && eq - start < 16)
            set_pref(p, text + start, (int)(eq - start), text + eq + 1,
                     (int)(i - eq - 1));
        i++;
    }
}

/* The file's text, in out; its length. */
int prefs_format(const prefs *p, char *out, int cap)
{
    char *d = out;

    if (cap < 128)
        return 0;
    d = put_s(put_s(d, "profile="), profile_name(p->profile));
    d = put_s(put_s(d, "\rtitle="), bar_names[p->title_bar]);
    d = put_s(put_s(d, "\rshortcuts="), bar_names[p->shortcut_bar]);
    d = put_s(put_s(d, "\rfont="), p->font);
    d = put_l(put_s(d, "\rsize="), p->size, 0);
    d = put_s(d, "\r");
    return (int)(d - out);
}
