/*
 * Host-native tests for doc.c, bindings.c, bars.c, prefs.c and save.c.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../src/tico.h"

static int pass, fail;

/* plat_alloc can be told to start failing, for the out-of-memory paths */
static int alloc_left = -1;

void *plat_alloc(unsigned size)
{
    if (alloc_left == 0)
        return NULL;
    if (alloc_left > 0)
        alloc_left--;
    return malloc(size);
}

void plat_free(void *p)
{
    free(p);
}

static void check(const char *what, int ok)
{
    if (ok) {
        pass++;
    } else {
        fail++;
        printf("FAIL: %s\n", what);
    }
}

static void check_int(const char *what, long got, long want)
{
    if (got == want) {
        pass++;
    } else {
        fail++;
        printf("FAIL: %s: got %ld, want %ld\n", what, got, want);
    }
}

static void show(const char *s, unsigned n)
{
    unsigned i;

    for (i = 0; i < n; i++) {
        if ((unsigned char)s[i] >= 0x20 && s[i] != 0x7F)
            putchar(s[i]);
        else
            printf("\\x%02x", (unsigned char)s[i]);
    }
}

static void check_bytes(const char *what, const char *got, unsigned gn,
                        const char *want, unsigned wn)
{
    if (gn == wn && memcmp(got, want, gn) == 0) {
        pass++;
        return;
    }
    fail++;
    printf("FAIL: %s: got \"", what);
    show(got, gn);
    printf("\", want \"");
    show(want, wn);
    printf("\"\n");
}

/* ------------------------------------------------------------------ */
/* helpers */

#define OUTMAX 4096

typedef struct {
    char buf[OUTMAX];
    unsigned len;
} sink;

static int to_sink(void *ctx, const char *buf, unsigned len)
{
    sink *s = ctx;

    if (s->len + len > OUTMAX)
        return 0;
    memcpy(s->buf + s->len, buf, len);
    s->len += len;
    return 1;
}

typedef struct {
    const char *data;
    unsigned len, pos, chunk;
    int fail_at;
} source;

static int from_source(void *ctx, char *buf, unsigned cap)
{
    source *s = ctx;
    unsigned n = s->len - s->pos;

    if (s->fail_at >= 0 && s->pos >= (unsigned)s->fail_at)
        return -1;
    if (n > cap)
        n = cap;
    if (s->chunk != 0 && n > s->chunk)
        n = s->chunk;
    memcpy(buf, s->data + s->pos, n);
    s->pos += n;
    return (int)n;
}

static int load_doc(document *d, const char *text, unsigned n, unsigned chunk)
{
    source s;
    char io[64];

    s.data = text;
    s.len = n;
    s.pos = 0;
    s.chunk = chunk;
    s.fail_at = -1;
    doc_init(d);
    return doc_read(d, from_source, &s, io, sizeof io);
}

static void load_str(document *d, const char *text)
{
    check_int("load", load_doc(d, text, (unsigned)strlen(text), 0), READ_OK);
}

/* The document, serialized in its format. */
static void expect(const char *what, const document *d, const char *want)
{
    sink s;

    s.len = 0;
    text_write(&d->buf, d->format, d->ctrlz, to_sink, &s);
    check_bytes(what, s.buf, s.len, want, (unsigned)strlen(want));
}

/* Every line links properly and the magic line is there. */
static void check_shape(const char *what, const document *d)
{
    const line *l, *prev = NULL;
    long n = 0, cur_seen = 0;

    for (l = d->buf.top; l != NULL; prev = l, l = l->next) {
        n++;
        if (l->prev != prev)
            cur_seen = -1000000;
        if (l == d->cur && n == d->cur_no)
            cur_seen++;
    }
    check_int(what, n, d->buf.count);
    check(what, d->buf.bot == prev && prev->len == 0);
    check_int(what, cur_seen, 1);
    check(what, d->cur_x <= d->cur->len);
}

static void type_str(document *d, const char *s)
{
    for (; *s; s++) {
        if (*s == '\r')
            doc_exec(d, CMD_ENTER, 0);
        else
            doc_type(d, *s);
    }
}

/* ------------------------------------------------------------------ */
/* round trips */

static void roundtrip(const char *what, const char *raw, unsigned n,
                      int format)
{
    document d;
    sink s;
    unsigned chunk;

    /* 1 byte at a time, then a size that splits things differently */
    for (chunk = 1; chunk <= 3; chunk += 2) {
        check_int(what, load_doc(&d, raw, n, chunk), READ_OK);
        check_int(what, d.format, format);
        check_int(what, d.modified, 0);
        s.len = 0;
        text_write(&d.buf, d.format, d.ctrlz, to_sink, &s);
        check_bytes(what, s.buf, s.len, raw, n);
        doc_free(&d);
    }
}

static void test_roundtrips(void)
{
    static const char nul[] = "a\0b\r\0\0\r";
    static const char high[] = "caf\216 \245\377\r\200\r";
    static const char ctrl[] = "\001\002\t\033\177\r";
    document d;

    roundtrip("CR", "one\rtwo\r", 8, FMT_MAC);
    roundtrip("CRLF", "one\r\ntwo\r\n", 10, FMT_DOS);
    roundtrip("LF", "one\ntwo\n", 8, FMT_UNIX);
    roundtrip("empty", "", 0, FMT_MAC);
    roundtrip("tabs", "\ta\tb\r", 5, FMT_MAC);
    roundtrip("nul", nul, sizeof nul - 1, FMT_MAC);
    roundtrip("high", high, sizeof high - 1, FMT_MAC);
    roundtrip("control", ctrl, sizeof ctrl - 1, FMT_MAC);
    roundtrip("ctrl-z", "a\r\n\032", 4, FMT_DOS);
    roundtrip("ctrl-z LF", "a\n\032", 3, FMT_UNIX);
    roundtrip("only ctrl-z", "\032", 1, FMT_MAC);
    roundtrip("blank lines", "\r\r\r", 3, FMT_MAC);

    /* a Ctrl+Z elsewhere is text */
    load_str(&d, "a\032b\n");
    check_int("inner ctrl-z", d.ctrlz, 0);
    expect("inner ctrl-z", &d, "a\032b\n");
    doc_free(&d);

    /* no final newline: one is added, as in nano */
    load_str(&d, "no newline");
    check_int("no break is Mac", d.format, FMT_MAC);
    expect("final newline added", &d, "no newline\r");
    doc_free(&d);

    /* mixed: the first break decides, and the rest come out uniform */
    load_str(&d, "a\nb\r\nc\n");
    check_int("mixed format", d.format, FMT_UNIX);
    expect("mixed output", &d, "a\nb\nc\n");
    doc_free(&d);

    /* read errors empty the document rather than half-load it */
    {
        source s;
        char io[8];

        s.data = "abcdefghijklmnop\r";
        s.len = 17;
        s.pos = 0;
        s.chunk = 0;
        s.fail_at = 8;
        doc_init(&d);
        check_int("read error", doc_read(&d, from_source, &s, io, 8),
                  READ_ERR);
        check_int("read error empties", d.buf.count, 1);
        check_shape("read error shape", &d);
        doc_free(&d);
    }
}

/* ------------------------------------------------------------------ */
/* formats */

static void test_formats(void)
{
    document a, b;

    doc_init(&a);
    check_int("new is Mac", a.format, FMT_MAC);
    check_int("new unmodified", a.modified, 0);
    doc_set_format(&a, FMT_UNIX);
    check_int("empty unchanged", a.modified, 0);
    doc_free(&a);

    load_str(&a, "x\ry\r");
    load_str(&b, "p\nq\n");
    doc_set_format(&a, FMT_DOS);
    check_int("format modifies", a.modified, 1);
    check_int("text cursor untouched", a.cur_no, 1);
    expect("conversion", &a, "x\r\ny\r\n");
    check_int("other doc format", b.format, FMT_UNIX);
    check_int("other doc clean", b.modified, 0);
    expect("other doc output", &b, "p\nq\n");
    doc_set_format(&a, FMT_MAC);
    check_int("back again", a.modified, 0);

    doc_set_format(&a, FMT_UNIX);
    doc_saved(&a);
    check_int("saved clean", a.modified, 0);
    check_int("saved disk format", a.disk_format, FMT_UNIX);

    type_str(&a, "z");
    doc_set_format(&a, FMT_MAC);
    doc_set_format(&a, FMT_UNIX);
    check_int("edit stays modified", a.modified, 1);
    doc_free(&a);
    doc_free(&b);
}

/* ------------------------------------------------------------------ */
/* editing */

static void test_editing(void)
{
    document d;

    doc_init(&d);
    type_str(&d, "hello\rworld");
    expect("typing", &d, "hello\rworld\r");
    check_int("modified", d.modified, 1);
    check_shape("typing", &d);

    doc_exec(&d, CMD_HOME, 0);
    doc_exec(&d, CMD_BACKSPACE, 0);
    expect("join", &d, "helloworld\r");
    check_shape("join", &d);
    doc_exec(&d, CMD_DELETE, 0);
    expect("delete", &d, "helloorld\r");
    doc_exec(&d, CMD_END, 0);
    doc_exec(&d, CMD_DELETE, 0);
    expect("magic line survives", &d, "helloorld\r");
    check_shape("magic", &d);
    doc_exec(&d, CMD_TAB, 0);
    expect("tab", &d, "helloorld\t\r");
    doc_free(&d);

    /* cut and paste by line */
    load_str(&d, "a\rb\rc\r");
    doc_exec(&d, CMD_CUT_LINE, 0);
    doc_exec(&d, CMD_CUT_LINE, 0);
    expect("cut run", &d, "c\r");
    check_shape("cut run", &d);
    doc_exec(&d, CMD_DOWN, 0);
    doc_exec(&d, CMD_PASTE_LINE, 0);
    expect("paste run", &d, "c\ra\rb\r");
    check_shape("paste run", &d);
    doc_exec(&d, CMD_FIRST_LINE, 0);
    doc_exec(&d, CMD_CUT_LINE, 0);
    doc_exec(&d, CMD_DOWN, 0);
    doc_exec(&d, CMD_CUT_LINE, 0);
    doc_exec(&d, CMD_PASTE_LINE, 0);
    expect("run broken by moving", &d, "a\rb\r");
    doc_free(&d);

    /* nothing on the magic line to cut */
    doc_init(&d);
    doc_exec(&d, CMD_CUT_LINE, 0);
    check_bytes("nothing cut", d.status, (unsigned)strlen(d.status),
                "Nothing was cut", 15);
    doc_free(&d);

    /* the cut buffer goes between documents */
    {
        document e;

        load_str(&d, "shared\r");
        doc_init(&e);
        doc_exec(&d, CMD_COPY_LINE, 0);
        doc_exec(&e, CMD_PASTE_LINE, 0);
        expect("copy across", &e, "shared\r");
        expect("source untouched", &d, "shared\r");
        check_int("source clean", d.modified, 0);
        doc_free(&d);
        doc_free(&e);
    }
}

static void test_movement(void)
{
    document d;
    long i;

    load_str(&d, "abcdef\rxy\r\tq\r");
    d.rows = 10;
    doc_exec(&d, CMD_END, 0);
    doc_exec(&d, CMD_DOWN, 0);
    check_int("short line", d.cur_x, 2);
    doc_exec(&d, CMD_DOWN, 0);
    check_int("into tab", d.cur_x, 0);
    doc_exec(&d, CMD_UP, 0);
    doc_exec(&d, CMD_UP, 0);
    check_int("column remembered", d.cur_x, 6);
    doc_exec(&d, CMD_RIGHT, 0);
    check_int("wrap right", d.cur_no, 2);
    doc_exec(&d, CMD_LEFT, 0);
    check_int("wrap left", d.cur_no, 1);
    doc_exec(&d, CMD_LAST_LINE, 0);
    check_int("last line", d.cur_no, 4);
    doc_exec(&d, CMD_FIRST_LINE, 0);
    check_int("first line", d.cur_no, 1);
    doc_free(&d);

    /* pages are two lines short of a screenful */
    {
        char text[400];
        char *p = text;

        for (i = 0; i < 100; i++)
            p = put_s(p, "x\r");
        load_str(&d, text);
    }
    d.rows = 10;
    doc_exec(&d, CMD_PAGE_DOWN, 0);
    check_int("page down", d.cur_no, 9);
    check_int("page keeps row", d.top_no, 9);
    doc_exec(&d, CMD_PAGE_UP, 0);
    check_int("page up", d.cur_no, 1);
    doc_exec(&d, CMD_LAST_LINE, 0);
    doc_scroll_to_cursor(&d);
    check_int("scrolled to end", d.top_no, 92);
    doc_free(&d);

    load_str(&d, "one two_3  four\r\rfive\r");
    doc_exec(&d, CMD_NEXT_WORD, 0);
    check_int("next word", d.cur_x, 4);
    doc_exec(&d, CMD_NEXT_WORD, 0);
    check_int("underscore and digit", d.cur_x, 11);
    doc_exec(&d, CMD_NEXT_WORD, 0);
    check_int("next line word", d.cur_no, 3);
    doc_exec(&d, CMD_PREV_WORD, 0);
    check_int("prev word line", d.cur_no, 1);
    check_int("prev word", d.cur_x, 11);
    doc_free(&d);

    /* columns: tabs and caret notation */
    load_str(&d, "a\tb\001c\r");
    check_int("col after tab", doc_col(d.cur, 2), 8);
    check_int("col after ctrl", doc_col(d.cur, 4), 11);
    check_int("x at col", doc_x_at(d.cur, 9), 3);
    check_int("x inside tab", doc_x_at(d.cur, 4), 1);
    {
        char out[32];
        int n = doc_render(d.cur, 0, 32, out);

        check_bytes("render", out, n, "a       b^Ac", 12);
        n = doc_render(d.cur, 9, 2, out);
        check_bytes("render clipped", out, n, "^A", 2);
        n = doc_render(d.cur, 10, 1, out);
        check_bytes("render half caret", out, n, "A", 1);
    }
    doc_free(&d);

    /* sideways, as in nano */
    {
        char text[200];

        memset(text, 'x', 190);
        text[190] = '\r';
        text[191] = 0;
        load_str(&d, text);
    }
    d.cols = 80;
    d.rows = 10;
    check_int("widest measured", d.widest, 190);
    doc_exec(&d, CMD_END, 0);
    doc_scroll_to_cursor(&d);
    check(("cursor in view"), doc_col(d.cur, d.cur_x) >= d.left &&
          doc_col(d.cur, d.cur_x) < d.left + d.cols);
    doc_exec(&d, CMD_HOME, 0);
    doc_scroll_to_cursor(&d);
    check_int("back to left", d.left, 0);
    doc_free(&d);
}

static void test_lookup(void)
{
    document d;
    long i, no;
    line *l, *walk;
    char text[1000];
    char *p = text;

    for (i = 0; i < 200; i++)
        p = put_s(p, "ab\r");
    load_str(&d, text);
    for (i = 0; i < 50; i++) {
        no = (i * 37) % 201 + 1;
        l = doc_line_at(&d, no);
        walk = d.buf.top;
        while (--no > 0)
            walk = walk->next;
        check("line at", l == walk);
    }
    check("line 0", doc_line_at(&d, 0) == NULL);
    check("line past end", doc_line_at(&d, 202) == NULL);
    doc_free(&d);
}

/* ------------------------------------------------------------------ */
/* the selection */

static void test_selection(void)
{
    document d;
    sink s;
    long from, to;

    load_str(&d, "alpha\rbeta\rgamma\r");
    doc_exec(&d, CMD_RIGHT, 1);
    doc_exec(&d, CMD_RIGHT, 1);
    check("shift selects", doc_has_selection(&d));
    check_int("size", doc_selection_size(&d), 2);
    doc_exec(&d, CMD_DOWN, 1);
    check_int("size with break", doc_selection_size(&d), 8);
    s.len = 0;
    doc_selection_out(&d, to_sink, &s);
    check_bytes("selection out", s.buf, s.len, "alpha\rbe", 8);

    check("first cols", doc_selection_cols(&d, d.buf.top, 1, &from, &to));
    check_int("first from", from, 0);
    check_int("first to past end", to, 6);
    check("last cols", doc_selection_cols(&d, d.cur, 2, &from, &to));
    check_int("last to", to, 2);
    check("not selected", !doc_selection_cols(&d, d.buf.bot, 4, &from, &to));

    doc_exec(&d, CMD_DELETE, 0);
    expect("delete across lines", &d, "ta\rgamma\r");
    check_shape("delete across lines", &d);
    check("selection gone", !doc_has_selection(&d));
    doc_free(&d);

    /* backwards selection, then typing over it */
    load_str(&d, "abcdef\r");
    doc_exec(&d, CMD_END, 0);
    doc_exec(&d, CMD_LEFT, 1);
    doc_exec(&d, CMD_LEFT, 1);
    doc_type(&d, 'Z');
    expect("type over", &d, "abcdZ\r");
    check_shape("type over", &d);
    doc_free(&d);

    /* an arrow collapses to the end of the selection */
    load_str(&d, "abcdef\r");
    doc_exec(&d, CMD_RIGHT, 1);
    doc_exec(&d, CMD_RIGHT, 1);
    doc_exec(&d, CMD_LEFT, 0);
    check_int("left collapses", d.cur_x, 0);
    check("no selection", !doc_has_selection(&d));
    doc_exec(&d, CMD_RIGHT, 1);
    doc_exec(&d, CMD_RIGHT, 0);
    check_int("right collapses", d.cur_x, 1);
    doc_free(&d);

    /* select all, then the magic line must come back */
    load_str(&d, "a\rb\r");
    doc_exec(&d, CMD_SELECT_ALL, 0);
    check_int("select all", doc_selection_size(&d), 4);
    doc_exec(&d, CMD_DELETE, 0);
    expect("all gone", &d, "");
    check_shape("all gone", &d);
    type_str(&d, "n");
    expect("still works", &d, "n\r");
    check_shape("still works", &d);
    doc_free(&d);

    /* delete a selection ending in the magic line */
    load_str(&d, "keep\rdrop\r");
    doc_click(&d, 1, 2, 0);
    doc_click(&d, 3, 0, 1);
    doc_exec(&d, CMD_CLEAR, 0);
    expect("into magic line", &d, "ke\r");
    check_shape("into magic line", &d);
    doc_free(&d);

    /* the mark: movement extends without Shift */
    load_str(&d, "marked text\r");
    doc_exec(&d, CMD_MARK, 0);
    check_int("marking", d.marking, 1);
    doc_exec(&d, CMD_NEXT_WORD, 0);
    check_int("mark extends", doc_selection_size(&d), 7);
    doc_exec(&d, CMD_CUT_LINE, 0);
    expect("cut region", &d, "text\r");
    check_int("mark gone", d.marking, 0);
    check_shape("cut region", &d);
    doc_exec(&d, CMD_END, 0);
    doc_exec(&d, CMD_PASTE_LINE, 0);
    expect("paste region", &d, "textmarked \r");
    doc_free(&d);

    /* M-6 with a selection copies only it */
    load_str(&d, "abc\r");
    doc_exec(&d, CMD_RIGHT, 1);
    doc_exec(&d, CMD_COPY_LINE, 0);
    expect("copy leaves text", &d, "abc\r");
    check("copy clears", !doc_has_selection(&d));
    doc_exec(&d, CMD_LAST_LINE, 0);
    doc_exec(&d, CMD_PASTE_LINE, 0);
    expect("paste copied", &d, "abc\ra\r");
    doc_free(&d);

    /* word selection */
    load_str(&d, "one two three\r");
    doc_select_word(&d, 1, 5);
    check_int("word", doc_selection_size(&d), 3);
    check_int("word start", d.anchor_x, 4);
    check_int("word end", d.cur_x, 7);
    doc_free(&d);

    /* clicks past the ends clamp */
    load_str(&d, "ab\r");
    doc_click(&d, 99, 99, 0);
    check_int("clamped line", d.cur_no, 2);
    doc_click(&d, 1, 99, 0);
    check_int("clamped col", d.cur_x, 2);
    doc_free(&d);
}

static void test_paste_text(void)
{
    document d;

    load_str(&d, "[]\r");
    doc_exec(&d, CMD_RIGHT, 0);
    check("paste", doc_paste_text(&d, "1\r2\n3\r\n4", 8));
    expect("all breaks", &d, "[1\r2\r3\r4]\r");
    check_shape("all breaks", &d);
    check_int("cursor after", d.cur_no, 4);
    check_int("cursor col", d.cur_x, 1);
    doc_free(&d);

    load_str(&d, "abc\r");
    doc_exec(&d, CMD_SELECT_ALL, 0);
    check("paste over", doc_paste_text(&d, "new", 3));
    expect("replaced", &d, "new\r");
    check_shape("replaced", &d);
    doc_free(&d);

    doc_init(&d);
    check("paste with final break", doc_paste_text(&d, "x\r", 2));
    expect("final break", &d, "x\r");
    check_shape("final break", &d);
    doc_free(&d);
}

static void test_status(void)
{
    document d;
    int i;

    load_str(&d, "line one\rtwo\r");
    doc_exec(&d, CMD_LOCATION, 0);
    check_bytes("location", d.status, (unsigned)strlen(d.status),
                "line 1/3 (33%), col  1/ 9 ( 11%), char  0/13 ( 0%)", 50);
    for (i = 0; i < 19; i++)
        doc_status_tick(&d);
    check("status lasts", d.status[0] != 0);
    doc_status_tick(&d);
    check("status wiped", d.status[0] == 0);
    doc_free(&d);
}

static void test_out_of_memory(void)
{
    document d;
    int i;

    /* run the pool dry, then every edit must fail cleanly */
    load_str(&d, "abc\rdef\r");
    alloc_left = 0;
    for (i = 0; i < 20000; i++)
        if (!doc_type(&d, 'x'))
            break;
    check("ran out", i < 20000);
    check("said so", d.status[0] != 0);
    check_shape("out of memory", &d);
    doc_free(&d);
    alloc_left = -1;

    /* a failed load leaves an empty, usable document */
    {
        static char big[20000];
        document e;
        source s;
        char io[512];

        memset(big, 'y', sizeof big);
        for (i = 0; i < 20000; i += 3)
            big[i] = '\r';
        s.data = big;
        s.len = sizeof big;
        s.pos = 0;
        s.chunk = 0;
        s.fail_at = -1;
        doc_init(&e);
        alloc_left = 0;
        /* burn off whatever the free lists hold */
        while (pool_alloc(24) != NULL || pool_alloc(8) != NULL)
            ;
        check_int("load out of memory", doc_read(&e, from_source, &s, io,
                                                 sizeof io), READ_NOMEM);
        alloc_left = -1;
    }
}

/* ------------------------------------------------------------------ */
/* bindings */

/* What a Mac sends for Command-letter. */
static int cmd_key(int c)
{
    return key_from_event((unsigned char)c, 0, MOD_CMD);
}

static void test_legacy(void)
{
    static const struct {
        int key, cmd;
    } t[] = {
        {'y', CMD_PAGE_UP}, {'v', CMD_PAGE_DOWN}, {'u', CMD_PASTE_LINE},
        {'k', CMD_CUT_LINE}, {'x', CMD_EXIT}, {'o', CMD_SAVE_AS},
        {'s', CMD_SAVE}, {'r', CMD_OPEN}, {'c', CMD_LOCATION},
        {'p', CMD_UP}, {'n', CMD_DOWN}, {'a', CMD_HOME}, {'e', CMD_END},
        {'d', CMD_DELETE}, {'h', CMD_BACKSPACE}, {'i', CMD_TAB},
        {'m', CMD_ENTER},
    };
    unsigned i;

    for (i = 0; i < sizeof t / sizeof t[0]; i++)
        check_int("legacy key", key_lookup(PROFILE_LEGACY,
                                           cmd_key(t[i].key)).cmd, t[i].cmd);
    /* The design's confirmed requirements, spelled out. */
    check("legacy V is not paste",
          key_lookup(PROFILE_LEGACY, cmd_key('v')).cmd != CMD_PASTE &&
          key_lookup(PROFILE_LEGACY, cmd_key('v')).cmd != CMD_PASTE_LINE);
    check_int("legacy Shift-V too",
              key_lookup(PROFILE_LEGACY,
                         key_from_event('V', 0, MOD_CMD | MOD_SHIFT)).cmd,
              CMD_PAGE_DOWN);
    /* tico has no such keys */
    check_int("legacy Q", key_lookup(PROFILE_LEGACY, cmd_key('q')).cmd,
              CMD_NONE);
    check_int("legacy W", key_lookup(PROFILE_LEGACY, cmd_key('w')).cmd,
              CMD_NONE);
    check_int("legacy M-6", key_lookup(PROFILE_LEGACY, KEY_META('6')).cmd,
              CMD_COPY_LINE);
    check_int("legacy M-\\", key_lookup(PROFILE_LEGACY, KEY_META('\\')).cmd,
              CMD_FIRST_LINE);
    check_int("legacy M-/", key_lookup(PROFILE_LEGACY, KEY_META('/')).cmd,
              CMD_LAST_LINE);
    check_int("legacy M-A", key_lookup(PROFILE_LEGACY, KEY_META('A')).cmd,
              CMD_MARK);
}

static void test_modern(void)
{
    static const struct {
        int key, cmd;
    } t[] = {
        {'q', CMD_EXIT}, {'w', CMD_SAVE_AS}, {'o', CMD_OPEN},
        {'p', CMD_LOCATION}, {'a', CMD_MARK}, {'x', CMD_CUT_LINE},
        {'c', CMD_COPY_LINE}, {'v', CMD_PASTE_LINE},
        /* moved to things that don't exist yet: no legacy show-through */
        {'y', CMD_NONE}, {'z', CMD_NONE}, {'e', CMD_NONE}, {'d', CMD_NONE},
        {'h', CMD_NONE}, {'g', CMD_NONE}, {'r', CMD_NONE}, {'t', CMD_NONE},
        /* unchanged from legacy */
        {'s', CMD_SAVE}, {'k', CMD_CUT_LINE}, {'u', CMD_PASTE_LINE},
        {'n', CMD_DOWN},
    };
    unsigned i;

    for (i = 0; i < sizeof t / sizeof t[0]; i++)
        check_int("modern key", key_lookup(PROFILE_MODERN,
                                           cmd_key(t[i].key)).cmd, t[i].cmd);
    /* A Plus keyboard must still have a way to the start of a line. */
    check_int("modern Option-Left", key_lookup(PROFILE_MODERN,
              KEY_LEFT | KF_OPT).cmd, CMD_HOME);
    check_int("modern Option-Down", key_lookup(PROFILE_MODERN,
              KEY_DOWN | KF_OPT).cmd, CMD_PAGE_DOWN);
}

static void test_mac_profile(void)
{
    static const struct {
        int key, cmd;
    } t[] = {
        {'n', CMD_NEW}, {'o', CMD_OPEN}, {'s', CMD_SAVE}, {'w', CMD_CLOSE},
        {'q', CMD_QUIT}, {'x', CMD_CUT}, {'c', CMD_COPY}, {'v', CMD_PASTE},
        {'a', CMD_SELECT_ALL}, {'l', CMD_LOCATION}, {'k', CMD_CUT_LINE},
        {'u', CMD_PASTE_LINE}, {'y', CMD_NONE},
    };
    unsigned i;
    binding b;

    for (i = 0; i < sizeof t / sizeof t[0]; i++)
        check_int("mac key", key_lookup(PROFILE_MAC, cmd_key(t[i].key)).cmd,
                  t[i].cmd);
    check_int("mac Save As", key_lookup(PROFILE_MAC,
              key_from_event('S', 1, MOD_CMD | MOD_SHIFT)).cmd, CMD_SAVE_AS);
    check_int("mac cmd-up", key_lookup(PROFILE_MAC,
              key_from_event(0x1E, 0x7E, MOD_CMD)).cmd, CMD_FIRST_LINE);

    b = key_lookup(PROFILE_MAC, key_from_event(0x1D, 0x7C, MOD_SHIFT));
    check_int("shift arrow", b.cmd, CMD_RIGHT);
    check_int("shift extends", b.extend, 1);
    b = key_lookup(PROFILE_MAC,
                   key_from_event(0x1D, 0x7C, MOD_SHIFT | MOD_CMD));
    check_int("shift cmd arrow", b.cmd, CMD_END);
    check_int("shift cmd extends", b.extend, 1);
    b = key_lookup(PROFILE_MAC, key_from_event('a', 0, 0));
    check_int("plain inserts", b.insert, 'a');
    b = key_lookup(PROFILE_MAC, key_from_event(0xBF, 0x1F, MOD_OPT));
    check_int("option inserts high byte", b.insert, 0xBF);
}

static void test_events(void)
{
    check_int("arrow", key_from_event(0x1C, 0x7B, 0), KEY_LEFT);
    check_int("return", key_from_event(0x0D, 0x24, 0), KEY_RETURN);
    check_int("enter", key_from_event(0x03, 0x4C, 0), KEY_ENTER);
    check_int("backspace", key_from_event(0x08, 0x33, 0), KEY_BSP);
    check_int("forward delete", key_from_event(0x7F, 0x75, 0), KEY_DEL);
    check_int("page down", key_from_event(0x0C, 0x79, 0), KEY_PGDN);
    check_int("esc", key_from_event(0x1B, 0x35, 0), KEY_ESC);
    check_int("Plus Clear is esc", key_from_event(0x1B, 0x47, 0), KEY_ESC);
    check_int("F9", key_from_event(0x10, 0x65, 0), KEY_F9);
    check_int("cmd letter", key_from_event('k', 0x28, MOD_CMD), KEY_CMD('K'));
    /* with Option the character code is the Option glyph */
    check_int("cmd-opt letter", key_from_event(0xBF, 0x1F, MOD_CMD | MOD_OPT),
              KEY_META('O'));
    check_int("cmd-opt 6", key_from_event(0xA4, 0x16, MOD_CMD | MOD_OPT),
              KEY_META('6'));
    check_int("cmd-opt-shift 6",
              key_from_event(0xDF, 0x16, MOD_CMD | MOD_OPT | MOD_SHIFT) &
              ~KF_SHIFT, KEY_META('^'));
    check_int("cmd-opt backslash",
              key_from_event(0xC7, 0x2A, MOD_CMD | MOD_OPT),
              KEY_META('\\'));
    check_int("cmd-opt slash",
              key_from_event(0xD6, 0x2C, MOD_CMD | MOD_OPT), KEY_META('/'));
    /* a real Control key: ^A's character code is Home's */
    check_int("control a", key_from_event(0x01, 0x00, MOD_CTRL), KEY_CMD('A'));
    check_int("control k", key_from_event(0x0B, 0x28, MOD_CTRL), KEY_CMD('K'));
    check_int("control arrow", key_from_event(0x1C, 0x7B, MOD_CTRL),
              KEY_LEFT | KF_CMD);
    /* Esc then a key is Meta */
    check_int("esc letter", key_after_esc('a'), KEY_META('A'));
    check_int("esc digit", key_after_esc('6'), KEY_META('6'));
    check_int("esc arrow", key_after_esc(KEY_LEFT), KEY_LEFT | KF_META);
    check_int("meta window", key_lookup(PROFILE_LEGACY,
              key_after_esc(KEY_RIGHT)).cmd, CMD_NEXT_WINDOW);
}

static void test_labels(void)
{
    char s[16];

    key_label(s, key_for_cmd(PROFILE_LEGACY, CMD_PAGE_DOWN));
    check_bytes("page down label", s, (unsigned)strlen(s), "\021V", 2);
    key_label(s, key_for_cmd(PROFILE_LEGACY, CMD_COPY_LINE));
    check_bytes("meta label", s, (unsigned)strlen(s), "M-6", 3);
    key_label(s, key_for_cmd(PROFILE_MAC, CMD_PASTE));
    check_bytes("mac paste label", s, (unsigned)strlen(s), "\021V", 2);
    key_label(s, key_for_cmd(PROFILE_MAC, CMD_SAVE_AS));
    check_bytes("shift label", s, (unsigned)strlen(s), "Sh-\021S", 5);
    /* a menu must not offer a key the profile doesn't have */
    check_int("no legacy paste key", key_for_cmd(PROFILE_LEGACY, CMD_PASTE), 0);
    check_int("no legacy quit key", key_for_cmd(PROFILE_LEGACY, CMD_QUIT), 0);
    check_int("no legacy new key", key_for_cmd(PROFILE_LEGACY, CMD_NEW), 0);
    check_int("legacy close is exit", key_for_cmd(PROFILE_LEGACY, CMD_CLOSE),
              KEY_CMD('X'));
    check_int("modern close is exit", key_for_cmd(PROFILE_MODERN, CMD_CLOSE),
              KEY_CMD('Q'));
    /* overridden legacy keys aren't offered under modern */
    check_int("modern up isn't P", key_for_cmd(PROFILE_MODERN, CMD_UP),
              KEY_UP);
    /* modern gives ^A away, so a Plus is left with Option-Left */
    check_int("modern home", key_for_cmd(PROFILE_MODERN, CMD_HOME),
              KEY_LEFT | KF_OPT);
    check_int("modern exit", key_for_cmd(PROFILE_MODERN, CMD_EXIT),
              KEY_CMD('Q'));
}

/* Every bound key's menu label agrees with the table, in every profile. */
static void test_key_round_trip(void)
{
    int p, cmd, k;

    for (p = 0; p < PROFILE_COUNT; p++)
        for (cmd = CMD_UP; cmd < CMD_COUNT; cmd++) {
            k = key_for_cmd(p, cmd);
            if (k != 0 && cmd != CMD_CLOSE)
                check_int("key round trip", key_lookup(p, k).cmd, cmd);
        }
}

/* ------------------------------------------------------------------ */
/* bars */

static void test_bars(void)
{
    char row[81];
    bar_item items[BAR_MAX];
    int n, i, p, width;

    bar_title(row, 80, "notes", 1);
    check_bytes("title left", row, 13, "  tico68 " VERSION, 13);
    check_bytes("title state", row + 70, 8, "Modified", 8);
    bar_title(row, 80, "notes", 0);
    check("title centred", memcmp(row + 39, "notes", 5) == 0);
    bar_title(row, 12, "a long file name", 0);
    check("title squeezed", memcmp(row, "...", 3) == 0);

    n = bar_shortcuts(PROFILE_LEGACY, 80, items);
    check_int("all fit", n, 8);
    check_bytes("exit first", items[0].key, items[0].keylen, "\021X", 2);
    check_bytes("exit desc", items[0].desc, 4, "Exit", 4);
    n = bar_shortcuts(PROFILE_MAC, 80, items);
    check_bytes("mac close", items[0].key, items[0].keylen, "\021W", 2);
    check_bytes("mac paste", items[3].key, items[3].keylen, "\021V", 2);
    n = bar_shortcuts(PROFILE_MODERN, 80, items);
    check_bytes("modern exit", items[0].key, items[0].keylen, "\021Q", 2);

    /* never past the edge, whatever the width */
    for (p = 0; p < PROFILE_COUNT; p++)
        for (width = 1; width < 100; width++) {
            n = bar_shortcuts(p, width, items);
            for (i = 0; i < n; i++)
                check("bar fits", items[i].col + items[i].keylen + 1 +
                      (int)strlen(items[i].desc) <= width);
        }
    check_int("narrow shows fewer", bar_shortcuts(PROFILE_LEGACY, 30, items),
              4);
    check_int("very narrow shows none", bar_shortcuts(PROFILE_LEGACY, 5, items),
              0);
}

/* ------------------------------------------------------------------ */
/* prefs */

static void test_prefs(void)
{
    prefs p, q;
    char text[256];
    int n;

    prefs_default(&p);
    check_int("default profile", p.profile, PROFILE_MAC);
    check_int("default title", p.title_bar, BAR_SHOW);
    check_int("default shortcuts", p.shortcut_bar, BAR_SHOW);
    check_int("default size", p.size, 9);
    check("default font", strcmp(p.font, "Monaco") == 0);

    p.profile = PROFILE_LEGACY;
    p.title_bar = BAR_HIDE;
    p.shortcut_bar = BAR_HIDE;
    strcpy(p.font, "Courier");
    p.size = 12;
    n = prefs_format(&p, text, sizeof text);
    prefs_default(&q);
    prefs_parse(&q, text, n);
    check_int("round profile", q.profile, PROFILE_LEGACY);
    check_int("round title", q.title_bar, BAR_HIDE);
    check_int("round shortcuts", q.shortcut_bar, BAR_HIDE);
    check_int("round size", q.size, 12);
    check("round font", strcmp(q.font, "Courier") == 0);

    prefs_default(&q);
    prefs_parse(&q, "profile=nonsense\rsize=9000\rtitle=maybe\r"
                "font=\rjunk\r=x\rfutureoption=1\n", 60);
    check_int("bad profile", q.profile, PROFILE_MAC);
    check_int("bad size", q.size, 9);
    check_int("bad title", q.title_bar, BAR_SHOW);
    check("bad font", strcmp(q.font, "Monaco") == 0);

    /* a file from when the bars had an automatic setting */
    prefs_default(&q);
    prefs_parse(&q, "title=auto\rshortcuts=auto\r", 26);
    check_int("old auto title", q.title_bar, BAR_SHOW);
    check_int("old auto shortcuts", q.shortcut_bar, BAR_SHOW);

    prefs_default(&q);
    prefs_parse(&q, "\0\377\0\377garbage", 11);
    check_int("binary garbage", q.size, 9);
    prefs_parse(&q, "profile=mod", 11);
    check_int("truncated", q.profile, PROFILE_MAC);
    prefs_parse(&q, "", 0);
    check_int("empty", q.profile, PROFILE_MAC);
}

/* ------------------------------------------------------------------ */
/* saving, against a pretend file system */

#define FILEMAX 512

typedef struct {
    char target[FILEMAX];
    long target_len, target_pos;
    int target_exists;
    char temp[FILEMAX];
    long temp_len;
    int temp_exists;
    char backup[FILEMAX];
    long backup_len;
    int backup_exists;
    long disk_free;             /* bytes */
    /* failure switches */
    int locked;
    int fail_target_write_at;   /* bytes in; -1 for never */
    int short_temp_write_at;    /* the same, no error, just a short count */
    int short_target_write_at;
    int fail_temp_delete;
    int fail_backup, fail_install, fail_restore, fail_backup_delete;
    int calls_after_open;
} fakefs;

static int fs_temp_create(void *ctx)
{
    fakefs *f = ctx;

    f->temp_exists = 1;
    f->temp_len = 0;
    return 1;
}

static unsigned fs_temp_write(void *ctx, const char *buf, unsigned len)
{
    fakefs *f = ctx;

    if ((long)len > f->disk_free)
        return 0;
    if (f->short_temp_write_at >= 0 &&
        f->temp_len + (long)len > f->short_temp_write_at)
        len = (unsigned)(f->short_temp_write_at - f->temp_len);
    memcpy(f->temp + f->temp_len, buf, len);
    f->temp_len += len;
    f->disk_free -= len;
    return len;
}

static int fs_temp_close(void *ctx)
{
    (void)ctx;
    return 1;
}

static int fs_temp_delete(void *ctx)
{
    fakefs *f = ctx;

    if (f->fail_temp_delete)
        return 0;
    f->disk_free += f->temp_len;
    f->temp_exists = 0;
    return 1;
}

static int fs_target_open(void *ctx, long *length)
{
    fakefs *f = ctx;

    if (f->locked)
        return 0;
    *length = f->target_len;
    f->target_pos = 0;
    return 1;
}

static int fs_target_set_length(void *ctx, long length)
{
    fakefs *f = ctx;
    long grow = length - f->target_len;

    if (grow > f->disk_free)
        return 0;
    f->disk_free -= grow;
    f->target_len = length;
    return 1;
}

static unsigned fs_target_write(void *ctx, const char *buf, unsigned len)
{
    fakefs *f = ctx;

    if (f->fail_target_write_at >= 0 &&
        f->target_pos + (long)len > f->fail_target_write_at)
        return 0;
    if (f->short_target_write_at >= 0 &&
        f->target_pos + (long)len > f->short_target_write_at)
        len = (unsigned)(f->short_target_write_at - f->target_pos);
    memcpy(f->target + f->target_pos, buf, len);
    f->target_pos += len;
    return len;
}

static int fs_target_close(void *ctx)
{
    (void)ctx;
    return 1;
}

static int fs_target_backup(void *ctx)
{
    fakefs *f = ctx;

    if (f->fail_backup)
        return 0;
    if (!f->target_exists)
        return 1;
    memcpy(f->backup, f->target, f->target_len);
    f->backup_len = f->target_len;
    f->backup_exists = 1;
    f->target_exists = 0;
    return 1;
}

static int fs_temp_install(void *ctx)
{
    fakefs *f = ctx;

    if (f->fail_install)
        return 0;
    memcpy(f->target, f->temp, f->temp_len);
    f->target_len = f->temp_len;
    f->target_exists = 1;
    f->temp_exists = 0;
    return 1;
}

static int fs_target_restore(void *ctx)
{
    fakefs *f = ctx;

    if (f->fail_restore)
        return 0;
    if (!f->backup_exists)
        return 1;
    memcpy(f->target, f->backup, f->backup_len);
    f->target_len = f->backup_len;
    f->target_exists = 1;
    f->backup_exists = 0;
    return 1;
}

static int fs_backup_delete(void *ctx)
{
    fakefs *f = ctx;

    if (f->fail_backup_delete)
        return 0;
    f->backup_exists = 0;
    return 1;
}

static const save_ops fake_ops = {
    NULL, fs_temp_create, fs_temp_write, fs_temp_close, fs_temp_delete,
    fs_target_open, fs_target_set_length, fs_target_write, fs_target_close,
    fs_target_backup, fs_temp_install, fs_target_restore, fs_backup_delete
};

static void fs_init(fakefs *f, const char *contents, long free_space)
{
    memset(f, 0, sizeof *f);
    f->target_len = (long)strlen(contents);
    memcpy(f->target, contents, f->target_len);
    f->target_exists = 1;
    f->disk_free = free_space;
    f->fail_target_write_at = -1;
    f->short_temp_write_at = -1;
    f->short_target_write_at = -1;
}

static void test_save(void)
{
    document d;
    fakefs f;
    save_ops ops = fake_ops;
    char io[16];

    ops.ctx = &f;

    /* growing */
    load_str(&d, "old\r");
    type_str(&d, "new and longer ");
    fs_init(&f, "old\r", 1000);
    check_int("save grows", save_in_place(&d, &ops, io, sizeof io), SAVE_OK);
    check_bytes("grown", f.target, f.target_len, "new and longer old\r", 19);
    check_int("temp gone", f.temp_exists, 0);
    doc_free(&d);

    /* shrinking */
    load_str(&d, "s\r");
    fs_init(&f, "much longer original\r", 1000);
    check_int("save shrinks", save_in_place(&d, &ops, io, sizeof io), SAVE_OK);
    check_bytes("shrunk", f.target, f.target_len, "s\r", 2);
    doc_free(&d);

    /* locked: nothing happens at all */
    load_str(&d, "text\r");
    fs_init(&f, "orig\r", 1000);
    f.locked = 1;
    check_int("locked", save_in_place(&d, &ops, io, sizeof io), SAVE_FAILED);
    check_bytes("locked unchanged", f.target, f.target_len, "orig\r", 5);
    check_int("locked no temp", f.temp_exists, 0);
    expect("locked keeps buffer", &d, "text\r");
    doc_free(&d);

    /* no room for the temporary file */
    load_str(&d, "0123456789012345678901234567890123456789\r");
    fs_init(&f, "orig\r", 20);
    check_int("disk full", save_in_place(&d, &ops, io, sizeof io),
              SAVE_FAILED);
    check_bytes("full unchanged", f.target, f.target_len, "orig\r", 5);
    check_int("full no temp", f.temp_exists, 0);
    check_int("full frees", f.disk_free, 20);
    expect("full keeps buffer", &d,
           "0123456789012345678901234567890123456789\r");
    doc_free(&d);

    /* room for the temporary file, but then not to grow the original */
    load_str(&d, "0123456789012345678901234567890123456789\r");
    fs_init(&f, "orig\r", 50);
    check_int("no room to grow", save_in_place(&d, &ops, io, sizeof io),
              SAVE_FAILED);
    check_bytes("grow failed unchanged", f.target, f.target_len, "orig\r", 5);
    check_int("grow failed no temp", f.temp_exists, 0);
    doc_free(&d);

    /* failing part way through the original: the temp file is the copy */
    load_str(&d, "0123456789012345678901234567890123456789\r");
    fs_init(&f, "orig\r", 1000);
    f.fail_target_write_at = 20;
    check_int("write fails", save_in_place(&d, &ops, io, sizeof io),
              SAVE_KEPT_TEMP);
    check_int("temp kept", f.temp_exists, 1);
    check_bytes("temp complete", f.temp, f.temp_len,
                "0123456789012345678901234567890123456789\r", 41);
    expect("write fails keeps buffer", &d,
           "0123456789012345678901234567890123456789\r");
    doc_free(&d);

    /* a short write with no error: the same, for each file */
    load_str(&d, "0123456789012345678901234567890123456789\r");
    fs_init(&f, "orig\r", 1000);
    f.short_temp_write_at = 20;
    check_int("short temp", save_in_place(&d, &ops, io, sizeof io),
              SAVE_FAILED);
    check_bytes("short temp unchanged", f.target, f.target_len, "orig\r", 5);
    check_int("short temp deleted", f.temp_exists, 0);

    fs_init(&f, "orig\r", 1000);
    f.short_target_write_at = 20;
    check_int("short target", save_in_place(&d, &ops, io, sizeof io),
              SAVE_KEPT_TEMP);
    check_int("short target temp kept", f.temp_exists, 1);
    check_bytes("short target temp complete", f.temp, f.temp_len,
                "0123456789012345678901234567890123456789\r", 41);

    fs_init(&f, "orig\r", 1000);
    f.short_temp_write_at = 20;
    check_int("short replace", save_replacing(&d, &ops, io, sizeof io),
              SAVE_FAILED);
    check_bytes("short replace unchanged", f.target, f.target_len,
                "orig\r", 5);
    check_int("short replace no backup", f.backup_exists, 0);
    doc_free(&d);

    /* Save As over another file */
    load_str(&d, "replacement\r");
    fs_init(&f, "victim\r", 1000);
    check_int("replace", save_replacing(&d, &ops, io, sizeof io), SAVE_OK);
    check_bytes("replaced", f.target, f.target_len, "replacement\r", 12);
    check_int("replace no temp", f.temp_exists, 0);
    check_int("replace no backup", f.backup_exists, 0);

    /* the same with nothing there to replace */
    fs_init(&f, "", 1000);
    f.target_exists = 0;
    check_int("replace nothing", save_replacing(&d, &ops, io, sizeof io),
              SAVE_OK);
    check_bytes("replace nothing wrote", f.target, f.target_len,
                "replacement\r", 12);

    fs_init(&f, "victim\r", 5);
    check_int("replace full", save_replacing(&d, &ops, io, sizeof io),
              SAVE_FAILED);
    check_bytes("replace full unchanged", f.target, f.target_len,
                "victim\r", 7);
    check_int("replace full no temp", f.temp_exists, 0);

    /* can't move the old file aside */
    fs_init(&f, "victim\r", 1000);
    f.fail_backup = 1;
    check_int("backup fails", save_replacing(&d, &ops, io, sizeof io),
              SAVE_FAILED);
    check_bytes("backup fails unchanged", f.target, f.target_len,
                "victim\r", 7);
    check_int("backup fails target", f.target_exists, 1);
    check_int("backup fails no temp", f.temp_exists, 0);

    fs_init(&f, "victim\r", 1000);
    f.fail_backup = f.fail_temp_delete = 1;
    check_int("backup fails, temp stuck",
              save_replacing(&d, &ops, io, sizeof io), SAVE_LEFT_TEMP);
    check_bytes("temp stuck unchanged", f.target, f.target_len,
                "victim\r", 7);
    check_int("temp stuck reported", f.temp_exists, 1);

    /* can't rename the new one into place: the old one comes back */
    fs_init(&f, "victim\r", 1000);
    f.fail_install = 1;
    check_int("install fails", save_replacing(&d, &ops, io, sizeof io),
              SAVE_FAILED);
    check_int("install fails target", f.target_exists, 1);
    check_bytes("install fails restored", f.target, f.target_len,
                "victim\r", 7);
    check_int("install fails no backup", f.backup_exists, 0);
    check_int("install fails no temp", f.temp_exists, 0);

    fs_init(&f, "victim\r", 1000);
    f.fail_install = f.fail_temp_delete = 1;
    check_int("install fails, temp stuck",
              save_replacing(&d, &ops, io, sizeof io), SAVE_LEFT_TEMP);
    check_bytes("restored, temp stuck", f.target, f.target_len,
                "victim\r", 7);
    check_int("restored, temp reported", f.temp_exists, 1);

    fs_init(&f, "", 1000);
    f.target_exists = 0;
    f.fail_install = 1;
    check_int("install fails, no old file",
              save_replacing(&d, &ops, io, sizeof io), SAVE_FAILED);
    check_int("no old file, none made", f.target_exists, 0);

    /* and it can't: nothing is lost, and both files are there */
    fs_init(&f, "victim\r", 1000);
    f.fail_install = f.fail_restore = 1;
    check_int("restore fails", save_replacing(&d, &ops, io, sizeof io),
              SAVE_KEPT_BOTH);
    check_int("restore fails backup", f.backup_exists, 1);
    check_bytes("restore fails old file", f.backup, f.backup_len,
                "victim\r", 7);
    check_int("restore fails temp", f.temp_exists, 1);
    check_bytes("restore fails new text", f.temp, f.temp_len,
                "replacement\r", 12);

    /* saved, with the old file left over */
    fs_init(&f, "victim\r", 1000);
    f.fail_backup_delete = 1;
    check_int("backup stuck", save_replacing(&d, &ops, io, sizeof io),
              SAVE_KEPT_BACKUP);
    check_bytes("backup stuck saved", f.target, f.target_len,
                "replacement\r", 12);
    check_int("backup stuck reported", f.backup_exists, 1);
    doc_free(&d);
}

/* ------------------------------------------------------------------ */
/* closing */

typedef struct {
    int modified;
    int answer;
    int save_works;
    int asked, saved, disposed;
} fakedoc;

static int fd_ask(void *ctx, void *doc)
{
    fakedoc *f = doc;

    (void)ctx;
    f->asked++;
    return f->answer;
}

static int fd_save(void *ctx, void *doc)
{
    fakedoc *f = doc;

    (void)ctx;
    f->saved++;
    return f->save_works;
}

static void fd_dispose(void *ctx, void *doc)
{
    fakedoc *f = doc;

    (void)ctx;
    f->disposed++;
}

static int fd_modified(void *ctx, void *doc)
{
    fakedoc *f = doc;

    (void)ctx;
    return f->modified;
}

static const close_ops fake_close = {
    NULL, fd_ask, fd_save, fd_dispose, fd_modified
};

static void test_close(void)
{
    fakedoc f;

    memset(&f, 0, sizeof f);
    check_int("clean closes", close_document(&f, &fake_close), 1);
    check_int("clean no prompt", f.asked, 0);
    check_int("clean disposed", f.disposed, 1);

    memset(&f, 0, sizeof f);
    f.modified = 1;
    f.answer = ASK_CANCEL;
    check_int("cancel keeps", close_document(&f, &fake_close), 0);
    check_int("cancel asked", f.asked, 1);
    check_int("cancel kept", f.disposed, 0);

    memset(&f, 0, sizeof f);
    f.modified = 1;
    f.answer = ASK_DISCARD;
    check_int("discard closes", close_document(&f, &fake_close), 1);
    check_int("discard no save", f.saved, 0);

    memset(&f, 0, sizeof f);
    f.modified = 1;
    f.answer = ASK_SAVE;
    f.save_works = 1;
    check_int("save closes", close_document(&f, &fake_close), 1);
    check_int("saved", f.saved, 1);

    /* a failed save mustn't lose the document */
    memset(&f, 0, sizeof f);
    f.modified = 1;
    f.answer = ASK_SAVE;
    check_int("save fails", close_document(&f, &fake_close), 0);
    check_int("save fails kept", f.disposed, 0);
}

int main(void)
{
    test_roundtrips();
    test_formats();
    test_editing();
    test_movement();
    test_lookup();
    test_selection();
    test_paste_text();
    test_status();
    test_out_of_memory();
    test_legacy();
    test_modern();
    test_mac_profile();
    test_events();
    test_labels();
    test_key_round_trip();
    test_bars();
    test_prefs();
    test_save();
    test_close();
    printf("%d passed, %d failed\n", pass, fail);
    return fail != 0;
}
