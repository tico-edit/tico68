/*
 * doc.c - one open document: text, cursor, selection and view state
 *
 * The editing commands are tico16's editor.c with its globals moved into
 * a document, its DOS event loop and screen code taken out, and a
 * selection added for the mouse and the clipboard.  nano's mark is the
 * same selection, made sticky.  Portable C.
 */

#include <string.h>
#include "tico.h"

lines cutbuf;
static int cut_run;             /* the last command was a line cut */

/* ------------------------------------------------------------------ */
/* messages, built in place of sprintf, which would bring all of stdio */

char *put_s(char *d, const char *s)
{
    while (*s)
        *d++ = *s++;
    *d = 0;
    return d;
}

/* v in decimal, right-aligned in width columns, as "%*ld" */
char *put_l(char *d, long v, int width)
{
    char t[12];
    int n = 0;

    do {
        t[n++] = (char)('0' + v % 10);
        v /= 10;
    } while (v > 0);
    while (n < width--)
        *d++ = ' ';
    while (n > 0)
        *d++ = t[--n];
    *d = 0;
    return d;
}

static int digits(long n)
{
    int d = 1;

    while (n >= 10) {
        n /= 10;
        d++;
    }
    return d;
}

static long labs_diff(long a, long b)
{
    return a > b ? a - b : b - a;
}

/* 100 * n / total, without overflowing on large files */
static long percent(long n, long total)
{
    if (n < 0x7FFFFFFFL / 100)
        return 100 * n / total;
    return n / (total / 100);
}

/* ------------------------------------------------------------------ */
/* columns */

static int char_width(unsigned char c, long col)
{
    if (c == '\t')
        return TABSIZE - (int)(col % TABSIZE);
    if (c < 0x20 || c == 0x7F)
        return 2;
    return 1;
}

/* Display column of byte x of l. */
long doc_col(const line *l, unsigned x)
{
    long col = 0;
    unsigned i;

    for (i = 0; i < x; i++)
        col += char_width((unsigned char)l->text[i], col);
    return col;
}

/* nano's actual_x: the byte at or just before display column goal. */
unsigned doc_x_at(const line *l, long goal)
{
    long col = 0, next;
    unsigned i;

    for (i = 0; i < l->len; i++) {
        next = col + char_width((unsigned char)l->text[i], col);
        if (next > goal)
            break;
        col = next;
    }
    return i;
}

/* nano 8.7.1's get_page_start (tico's scroll_horizontal_to_cursor). */
static long page_start(long column, int width)
{
    if (column == 0 || column + 2 < width)
        return 0;
    if (width > 8)
        return column - 6 - (column - 6) % (width - 8);
    return column - (width - 2);
}

/*
 * The cells of l from column left, in out; the count of them.  Control
 * bytes are caret notation, as in tico; bytes 128-255 go through as they
 * are, so they show as MacRoman glyphs.
 */
int doc_render(const line *l, long left, int cols, char *out)
{
    long col = 0, right = left + cols;
    unsigned i;
    int w, j, n = 0;
    unsigned char c;

    for (i = 0; l != NULL && i < l->len && col < right; i++) {
        c = (unsigned char)l->text[i];
        w = char_width(c, col);
        if (col + w > left) {
            for (j = 0; j < w; j++) {
                if (col + j < left || col + j >= right)
                    continue;
                if (c == '\t')
                    out[n++] = ' ';
                else if (c < 0x20 || c == 0x7F)
                    out[n++] = (char)(j == 0 ? '^' : c ^ 0x40);
                else
                    out[n++] = (char)c;
            }
        }
        col += w;
    }
    return n;
}

/* ------------------------------------------------------------------ */
/* status */

static void set_status(document *d, const char *msg, int err)
{
    strncpy(d->status, msg, STATUS_MAX - 1);
    d->status[STATUS_MAX - 1] = 0;
    d->status_err = err;
    d->status_count = 20;
    d->title_dirty = 1;
}

void doc_status(document *d, const char *msg)
{
    set_status(d, msg, 0);
}

void doc_alert(document *d, const char *msg)
{
    set_status(d, msg, 1);
}

/* nano's blank_it_when_expired */
void doc_status_tick(document *d)
{
    if (d->status_count > 0 && --d->status_count == 0) {
        d->status[0] = 0;
        d->title_dirty = 1;
    }
}

/* ------------------------------------------------------------------ */
/* marking things for redraw */

static void mark_line(document *d, long n)
{
    int i;

    for (i = 0; i < 3; i++)
        if (d->dirty[i] == n)
            return;
    for (i = 0; i < 3; i++)
        if (d->dirty[i] == NO_LINE) {
            d->dirty[i] = n;
            return;
        }
    if (n < d->redraw_from)
        d->redraw_from = n;
    if (n > d->redraw_to || d->redraw_to == NO_LINE)
        d->redraw_to = n;
}

static void mark_range(document *d, long a, long b)
{
    long t;

    if (a > b) {
        t = a;
        a = b;
        b = t;
    }
    if (a == b) {
        mark_line(d, a);
        return;
    }
    if (a < d->redraw_from)
        d->redraw_from = a;
    if (d->redraw_to < b)
        d->redraw_to = b;
}

/* Every line from n to the end: the lines have shifted. */
static void mark_from(document *d, long n)
{
    if (n < d->redraw_from)
        d->redraw_from = n;
    d->redraw_to = 0x7FFFFFFEL;
    d->hint = NULL;
}

void doc_view_changed(document *d)
{
    d->full_redraw = 1;
}

static void update_modified(document *d)
{
    int m = d->edited ||
            (d->format != d->disk_format && d->buf.count > 1);

    if (m != d->modified) {
        d->modified = m;
        d->title_dirty = 1;
        /* with the bars hidden, the strip is what says "Modified" */
        if (m && !d->status_err) {
            d->status[0] = 0;
            d->status_count = 0;
        }
    }
}

static void set_edited(document *d)
{
    d->edited = 1;
    update_modified(d);
}

static void note_width(document *d, const line *l)
{
    long w = doc_col(l, l->len);

    if (w > d->widest)
        d->widest = w;
}

/* note_width for a line of plain characters, the usual kind; 0 if it
   holds a character wider than one column. */
static int plain_width(document *d, const line *l)
{
    const unsigned char *p = (const unsigned char *)l->text;
    unsigned i;

    for (i = 0; i < l->len; i++)
        if (p[i] < 0x20 || p[i] == 0x7F)
            return 0;
    if ((long)l->len > d->widest)
        d->widest = l->len;
    return 1;
}

/* ------------------------------------------------------------------ */
/* setting up */

int doc_init(document *d)
{
    memset(d, 0, sizeof *d);
    lines_init(&d->buf);
    d->format = d->disk_format = FMT_MAC;
    if (!lines_append(&d->buf, NULL, 0))
        return 0;
    d->cur = d->buf.top;
    d->cur_no = d->top_no = 1;
    d->rows = 24;
    d->cols = 80;
    d->full_redraw = 1;
    d->redraw_from = d->redraw_to = NO_LINE;
    d->dirty[0] = d->dirty[1] = d->dirty[2] = NO_LINE;
    return 1;
}

void doc_free(document *d)
{
    lines_clear(&d->buf);
    d->cur = d->anchor = d->hint = NULL;
}

/*
 * Replace the text with a file's.  Everything but the file's last byte
 * goes to the reader as it comes: a Ctrl+Z in that position is kept
 * aside rather than read as text, as in tico16.  On failure the document
 * is left empty.
 */
int doc_read(document *d, text_in in, void *ctx, char *iobuf, unsigned cap)
{
    reader r;
    const line *l;
    int pending = -1, got = 0, result = READ_OK;
    char c;

    lines_clear(&d->buf);
    reader_start(&r, &d->buf);
    while (!r.failed && (got = in(ctx, iobuf, cap)) > 0) {
        if (pending >= 0) {
            c = (char)pending;
            reader_feed(&r, &c, 1);
        }
        pending = (unsigned char)iobuf[got - 1];
        reader_feed(&r, iobuf, (unsigned)got - 1);
    }
    d->ctrlz = 0;
    if (pending == 0x1A && got == 0 && !r.failed) {
        d->ctrlz = 1;
    } else if (pending >= 0 && !r.failed) {
        c = (char)pending;
        reader_feed(&r, &c, 1);
    }
    if (got < 0)
        result = READ_ERR;
    if (!reader_finish(&r) && result == READ_OK)
        result = READ_NOMEM;
    if (result != READ_OK) {
        lines_clear(&d->buf);
        lines_append(&d->buf, NULL, 0);
    }
    /* no line break to go by: write Macintosh, like a new document */
    d->format = d->disk_format = r.format == FMT_NONE ? FMT_MAC : r.format;
    d->cur = d->buf.top;
    d->cur_no = d->top_no = 1;
    d->cur_x = 0;
    d->want = 0;
    d->left = 0;
    d->anchor = NULL;
    d->marking = 0;
    d->hint = NULL;
    d->edited = d->modified = 0;
    d->widest = 0;
    for (l = d->buf.top; l != NULL; l = l->next)
        if (!plain_width(d, l))
            note_width(d, l);
    d->full_redraw = 1;
    return result;
}

void doc_saved(document *d)
{
    d->disk_format = d->format;
    d->edited = 0;
    update_modified(d);
}

/* Takes effect on the next save; the text doesn't change. */
void doc_set_format(document *d, int format)
{
    d->format = format;
    update_modified(d);
    d->title_dirty = 1;
}

/* ------------------------------------------------------------------ */
/* finding lines */

line *doc_line_at(document *d, long no)
{
    line *l;
    long at, best;

    if (no < 1 || no > d->buf.count)
        return NULL;
    l = d->buf.top;
    at = 1;
    best = no - 1;
    if (d->buf.count - no < best) {
        l = d->buf.bot;
        at = d->buf.count;
        best = d->buf.count - no;
    }
    if (labs_diff(d->cur_no, no) < best) {
        l = d->cur;
        at = d->cur_no;
        best = labs_diff(d->cur_no, no);
    }
    if (d->hint != NULL && labs_diff(d->hint_no, no) < best) {
        l = d->hint;
        at = d->hint_no;
    }
    while (at < no) {
        l = l->next;
        at++;
    }
    while (at > no) {
        l = l->prev;
        at--;
    }
    d->hint = l;
    d->hint_no = no;
    return l;
}

/* ------------------------------------------------------------------ */
/* the selection */

/* Is the anchor before the cursor? */
static int anchor_first(const document *d)
{
    if (d->anchor_no != d->cur_no)
        return d->anchor_no < d->cur_no;
    return d->anchor_x < d->cur_x;
}

int doc_has_selection(const document *d)
{
    return d->anchor != NULL &&
           (d->anchor != d->cur || d->anchor_x != d->cur_x);
}

static void sel_bounds(const document *d, line **s, unsigned *sx, long *sn,
                       line **e, unsigned *ex, long *en)
{
    if (anchor_first(d)) {
        *s = d->anchor;
        *sx = d->anchor_x;
        *sn = d->anchor_no;
        *e = d->cur;
        *ex = d->cur_x;
        *en = d->cur_no;
    } else {
        *s = d->cur;
        *sx = d->cur_x;
        *sn = d->cur_no;
        *e = d->anchor;
        *ex = d->anchor_x;
        *en = d->anchor_no;
    }
}

void doc_clear_selection(document *d)
{
    if (d->anchor != NULL)
        mark_range(d, d->anchor_no, d->cur_no);
    d->anchor = NULL;
    d->marking = 0;
}

static void drop_anchor(document *d)
{
    d->anchor = d->cur;
    d->anchor_no = d->cur_no;
    d->anchor_x = d->cur_x;
}

/* The columns of line l (number no) that are selected. */
int doc_selection_cols(const document *d, const line *l, long no,
                       long *from, long *to)
{
    line *s, *e;
    unsigned sx, ex;
    long sn, en;

    if (!doc_has_selection(d))
        return 0;
    sel_bounds(d, &s, &sx, &sn, &e, &ex, &en);
    if (no < sn || no > en)
        return 0;
    *from = no == sn ? doc_col(l, sx) : 0;
    /* one column past the end shows the line break is selected */
    *to = no == en ? doc_col(l, ex) : doc_col(l, l->len) + 1;
    return 1;
}

/* Calls out with the selected bytes; lines are joined with CR. */
int doc_selection_out(const document *d, text_out out, void *ctx)
{
    line *s, *e;
    const line *l;
    unsigned sx, ex, from, to;
    long sn, en;

    if (!doc_has_selection(d))
        return 1;
    sel_bounds(d, &s, &sx, &sn, &e, &ex, &en);
    for (l = s; ; l = l->next) {
        from = l == s ? sx : 0;
        to = l == e ? ex : l->len;
        if (to > from && !out(ctx, l->text + from, to - from))
            return 0;
        if (l == e)
            return 1;
        if (!out(ctx, "\r", 1))
            return 0;
    }
}

static int count_bytes(void *ctx, const char *buf, unsigned len)
{
    (void)buf;
    *(long *)ctx += len;
    return 1;
}

long doc_selection_size(const document *d)
{
    long n = 0;

    doc_selection_out(d, count_bytes, &n);
    return n;
}

/* The selection as lines: one more line than it has line breaks. */
int doc_selection_lines(const document *d, lines *dst)
{
    line *s, *e;
    const line *l;
    unsigned sx, ex, from, to;
    long sn, en;

    sel_bounds(d, &s, &sx, &sn, &e, &ex, &en);
    for (l = s; ; l = l->next) {
        from = l == s ? sx : 0;
        to = l == e ? ex : l->len;
        if (!lines_append(dst, l->text + from, to - from))
            return 0;
        if (l == e)
            return 1;
    }
}

static void out_of_memory(document *d)
{
    doc_alert(d, "Out of memory");
}

/* Delete the selection; 0 if out of memory, which changes nothing. */
int doc_delete_selection(document *d)
{
    line *s, *e, *l, *next;
    unsigned sx, ex;
    long sn, en;

    if (!doc_has_selection(d)) {
        d->anchor = NULL;
        d->marking = 0;
        return 1;
    }
    sel_bounds(d, &s, &sx, &sn, &e, &ex, &en);
    if (s == e) {
        line_erase(s, sx, ex - sx);
        mark_line(d, sn);
    } else {
        if (!line_insert(s, s->len, e->text + ex, e->len - ex)) {
            out_of_memory(d);
            return 0;
        }
        line_erase(s, sx, s->len - sx - (e->len - ex));
        for (l = s->next; ; l = next) {
            next = l->next;
            lines_unlink(&d->buf, l);
            if (l == e) {
                line_free(l);
                break;
            }
            line_free(l);
        }
        mark_from(d, sn);
    }
    d->cur = s;
    d->cur_no = sn;
    d->cur_x = sx;
    d->want = doc_col(s, sx);
    d->anchor = NULL;
    d->marking = 0;
    /* the magic line went with the selection */
    if (!lines_ensure_magic(&d->buf))
        out_of_memory(d);
    set_edited(d);
    return 1;
}

/* ------------------------------------------------------------------ */
/* moving */

static void update_want(document *d)
{
    d->want = doc_col(d->cur, d->cur_x);
}

static void go_up(document *d)
{
    if (d->cur->prev) {
        d->cur = d->cur->prev;
        d->cur_no--;
        d->cur_x = doc_x_at(d->cur, d->want);
    }
}

static void go_down(document *d)
{
    if (d->cur->next) {
        d->cur = d->cur->next;
        d->cur_no++;
        d->cur_x = doc_x_at(d->cur, d->want);
    }
}

static void go_left(document *d)
{
    if (d->cur_x > 0) {
        d->cur_x--;
    } else if (d->cur->prev) {
        d->cur = d->cur->prev;
        d->cur_no--;
        d->cur_x = d->cur->len;
    }
    update_want(d);
}

static void go_right(document *d)
{
    if (d->cur_x < d->cur->len) {
        d->cur_x++;
    } else if (d->cur->next) {
        d->cur = d->cur->next;
        d->cur_no++;
        d->cur_x = 0;
    }
    update_want(d);
}

static void go_first_line(document *d)
{
    d->cur = d->buf.top;
    d->cur_no = 1;
    d->cur_x = 0;
    d->want = 0;
}

static void go_last_line(document *d)
{
    d->cur = d->buf.bot;
    d->cur_no = d->buf.count;
    d->cur_x = 0;
    d->want = 0;
}

/*
 * nano's do_page_up/do_page_down: two lines short of a screenful,
 * keeping the cursor on the same screen row and aiming for the same
 * column; without that many lines left, to the start of the first line
 * or the end of the last.
 */
static void go_page(document *d, int forward)
{
    long mustmove = d->rows < 3 ? 1 : d->rows - 2;
    long row = d->cur_no - d->top_no, i;

    if (!forward && d->cur_no - 1 < mustmove) {
        go_first_line(d);
        return;
    }
    if (forward && d->buf.count - d->cur_no < mustmove) {
        d->cur = d->buf.bot;
        d->cur_no = d->buf.count;
        d->cur_x = d->cur->len;
        update_want(d);
        return;
    }
    for (i = 0; i < mustmove; i++) {
        d->cur = forward ? d->cur->next : d->cur->prev;
        d->cur_no += forward ? 1 : -1;
    }
    d->cur_x = doc_x_at(d->cur, d->want);
    d->top_no = d->cur_no - row;
    if (d->top_no < 1)
        d->top_no = 1;
}

/* Bytes 128-255 count as letters: they are, in MacRoman. */
static int word_byte(unsigned char c)
{
    return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') ||
           (c >= 'a' && c <= 'z') || c == '_' || c >= 0x80;
}

static int at_word(const document *d)
{
    return d->cur_x < d->cur->len &&
           word_byte((unsigned char)d->cur->text[d->cur_x]);
}

/* nano's do_next_word: past this word, to the start of the next. */
static void go_next_word(document *d)
{
    while (at_word(d))
        d->cur_x++;
    for (;;) {
        if (d->cur_x >= d->cur->len) {
            if (d->cur->next == NULL)
                break;
            d->cur = d->cur->next;
            d->cur_no++;
            d->cur_x = 0;
        } else if (at_word(d)) {
            break;
        } else {
            d->cur_x++;
        }
    }
    update_want(d);
}

/* nano's do_prev_word: to the start of this word or the one before. */
static void go_prev_word(document *d)
{
    for (;;) {
        if (d->cur_x == 0) {
            if (d->cur->prev == NULL)
                break;
            d->cur = d->cur->prev;
            d->cur_no--;
            d->cur_x = d->cur->len;
        } else if (word_byte((unsigned char)d->cur->text[d->cur_x - 1])) {
            break;
        } else {
            d->cur_x--;
        }
    }
    while (d->cur_x > 0 &&
           word_byte((unsigned char)d->cur->text[d->cur_x - 1]))
        d->cur_x--;
    update_want(d);
}

static void move_cursor(document *d, int cmd, int extend)
{
    long old_no = d->cur_no;
    int collapse = 0;

    if (extend || d->marking) {
        if (d->anchor == NULL)
            drop_anchor(d);
    } else if (doc_has_selection(d)) {
        /* Left and Right land at the ends of the selection */
        if (cmd == CMD_LEFT || cmd == CMD_RIGHT) {
            collapse = (cmd == CMD_LEFT) == anchor_first(d);
            if (collapse) {
                d->cur = d->anchor;
                d->cur_no = d->anchor_no;
                d->cur_x = d->anchor_x;
                update_want(d);
            }
            mark_range(d, old_no, d->cur_no);
            doc_clear_selection(d);
            return;
        }
        doc_clear_selection(d);
    } else {
        d->anchor = NULL;
    }

    switch (cmd) {
    case CMD_UP:
        go_up(d);
        break;
    case CMD_DOWN:
        go_down(d);
        break;
    case CMD_LEFT:
        go_left(d);
        break;
    case CMD_RIGHT:
        go_right(d);
        break;
    case CMD_HOME:
        d->cur_x = 0;
        update_want(d);
        break;
    case CMD_END:
        d->cur_x = d->cur->len;
        update_want(d);
        break;
    case CMD_PAGE_UP:
        go_page(d, 0);
        break;
    case CMD_PAGE_DOWN:
        go_page(d, 1);
        break;
    case CMD_FIRST_LINE:
        go_first_line(d);
        break;
    case CMD_LAST_LINE:
        go_last_line(d);
        break;
    case CMD_PREV_WORD:
        go_prev_word(d);
        break;
    case CMD_NEXT_WORD:
        go_next_word(d);
        break;
    }
    if (d->anchor != NULL) {
        mark_range(d, old_no, d->cur_no);
        if (!d->marking && !doc_has_selection(d))
            d->anchor = NULL;
    }
}

/* ------------------------------------------------------------------ */
/* the mouse */

/* Put the cursor at a line and column, or extend the selection there. */
void doc_click(document *d, long no, long col, int extend)
{
    line *l;
    long old_no = d->cur_no;

    if (no < 1)
        no = 1;
    if (no > d->buf.count)
        no = d->buf.count;
    l = doc_line_at(d, no);
    if (extend) {
        if (d->anchor == NULL)
            drop_anchor(d);
    } else {
        doc_clear_selection(d);
    }
    d->cur = l;
    d->cur_no = no;
    d->cur_x = doc_x_at(l, col);
    update_want(d);
    if (d->anchor != NULL)
        mark_range(d, old_no, no);
    cut_run = 0;
}

void doc_select_word(document *d, long no, long col)
{
    line *l;

    doc_click(d, no, col, 0);
    l = d->cur;
    if (!at_word(d))
        return;
    while (d->cur_x > 0 && word_byte((unsigned char)l->text[d->cur_x - 1]))
        d->cur_x--;
    drop_anchor(d);
    while (at_word(d))
        d->cur_x++;
    update_want(d);
    mark_line(d, d->cur_no);
}

/* ------------------------------------------------------------------ */
/* editing */

static int insert_byte(document *d, char c)
{
    if (!line_insert(d->cur, d->cur_x, &c, 1)) {
        out_of_memory(d);
        return 0;
    }
    d->cur_x++;
    update_want(d);
    set_edited(d);
    mark_line(d, d->cur_no);
    note_width(d, d->cur);
    /* typing on the magic line makes a new one */
    if (d->cur == d->buf.bot) {
        if (!lines_ensure_magic(&d->buf))
            out_of_memory(d);
        d->hint = NULL;
    }
    return 1;
}

static void do_enter(document *d)
{
    if (!lines_split(&d->buf, d->cur, d->cur_x)) {
        out_of_memory(d);
        return;
    }
    mark_from(d, d->cur_no);
    d->cur = d->cur->next;
    d->cur_no++;
    d->cur_x = 0;
    d->want = 0;
    set_edited(d);
}

/* nano's do_deletion(FORWARD). */
static void do_delete(document *d)
{
    if (d->cur_x < d->cur->len) {
        line_erase(d->cur, d->cur_x, 1);
        mark_line(d, d->cur_no);
    } else if (d->cur->next) {
        /* don't eat the magic line from the end of a non-empty line */
        if (d->cur->next == d->buf.bot && d->cur_x != 0)
            return;
        if (!lines_join(&d->buf, d->cur)) {
            out_of_memory(d);
            return;
        }
        mark_from(d, d->cur_no);
    } else {
        return;
    }
    set_edited(d);
}

static void do_backspace(document *d)
{
    if (d->cur_x > 0) {
        d->cur_x--;
        update_want(d);
        do_delete(d);
    } else if (d->cur->prev) {
        go_left(d);
        do_delete(d);
    }
}

static void cut_to_cutbuf(document *d, line *l)
{
    lines_unlink(&d->buf, l);
    lines_link_after(&cutbuf, cutbuf.bot->prev, l);
}

/* Replace the cut buffer with the selection; 0 if out of memory. */
static int selection_to_cutbuf(document *d)
{
    lines_clear(&cutbuf);
    if (doc_selection_lines(d, &cutbuf))
        return 1;
    lines_clear(&cutbuf);
    out_of_memory(d);
    return 0;
}

/* ^K: nano's cut_text; ^K's in a row add up.  A selection is cut whole. */
static void do_cut(document *d)
{
    line *l = d->cur;

    if (doc_has_selection(d)) {
        if (selection_to_cutbuf(d) && doc_delete_selection(d))
            doc_status_tick(d);
        return;
    }
    d->anchor = NULL;
    d->marking = 0;
    if (d->cur->next == NULL) {
        doc_status(d, "Nothing was cut");
        return;
    }
    if (!cut_run) {
        lines_clear(&cutbuf);
        if (!lines_append(&cutbuf, NULL, 0)) {
            out_of_memory(d);
            return;
        }
    }
    d->cur = d->cur->next;
    cut_to_cutbuf(d, l);
    d->cur_x = 0;
    d->want = 0;
    cut_run = 1;
    mark_from(d, d->cur_no);
    set_edited(d);
}

/* M-6: tico's do_copy, the current line or the selection. */
static void do_copy(document *d)
{
    if (doc_has_selection(d)) {
        if (selection_to_cutbuf(d))
            doc_status(d, "Copied");
        doc_clear_selection(d);
        return;
    }
    lines_clear(&cutbuf);
    if (!lines_append(&cutbuf, d->cur->text, d->cur->len) ||
        !lines_append(&cutbuf, NULL, 0)) {
        lines_clear(&cutbuf);
        out_of_memory(d);
        return;
    }
    doc_status(d, "Copied");
}

/*
 * Insert a list of lines at the cursor.  The new lines are built first,
 * and only then is the cursor line changed, so running out of memory
 * changes nothing.
 */
void doc_insert_lines(document *d, const lines *src)
{
    lines add;
    line *first, *last, *l, *next;
    long n;

    if (src->top == NULL)
        return;
    first = src->top;
    last = src->bot;
    if (first == last) {
        if (!line_insert(d->cur, d->cur_x, first->text, first->len)) {
            out_of_memory(d);
            return;
        }
        d->cur_x += first->len;
        mark_line(d, d->cur_no);
        note_width(d, d->cur);
    } else {
        lines_init(&add);
        for (l = first->next; l != last; l = l->next)
            if (!lines_append(&add, l->text, l->len))
                break;
        if (l != last || !lines_append(&add, last->text, last->len) ||
            !line_insert(add.bot, add.bot->len, d->cur->text + d->cur_x,
                         d->cur->len - d->cur_x) ||
            !line_insert(d->cur, d->cur_x, first->text, first->len)) {
            lines_clear(&add);
            out_of_memory(d);
            return;
        }
        d->cur->len = d->cur_x + first->len;
        note_width(d, d->cur);
        mark_from(d, d->cur_no);
        n = add.count;
        for (l = add.top; l != NULL; l = next) {
            next = l->next;
            l->prev = l->next = NULL;
            lines_link_after(&d->buf, d->cur, l);
            d->cur = l;
            note_width(d, l);
        }
        d->cur_no += n;
        d->cur_x = last->len;
    }
    update_want(d);
    set_edited(d);
    if (!lines_ensure_magic(&d->buf))
        out_of_memory(d);
    d->hint = NULL;
}

/* ^U: the cut buffer at the cursor, over any selection. */
static void do_paste(document *d)
{
    if (cutbuf.top == NULL)
        return;
    if (!doc_delete_selection(d))
        return;
    doc_insert_lines(d, &cutbuf);
    doc_status(d, "Pasted");
}

/* Scrap text at the cursor, over any selection; CR, LF or CR LF break. */
int doc_paste_text(document *d, const char *text, long len)
{
    lines add;
    long i = 0, start = 0;
    int ok = 1;

    lines_init(&add);
    for (i = 0; i < len && ok; i++) {
        if (text[i] != '\r' && text[i] != '\n')
            continue;
        ok = i - start <= (long)LINE_MAX_LEN &&
             lines_append(&add, text + start, (unsigned)(i - start));
        if (text[i] == '\r' && i + 1 < len && text[i + 1] == '\n')
            i++;
        start = i + 1;
    }
    if (ok)
        ok = len - start <= (long)LINE_MAX_LEN &&
             lines_append(&add, text + start, (unsigned)(len - start));
    if (ok)
        ok = doc_delete_selection(d);
    else
        out_of_memory(d);
    if (ok)
        doc_insert_lines(d, &add);
    lines_clear(&add);
    return ok;
}

/* ^C: nano's report_cursor_position. */
static void do_location(document *d)
{
    char msg[STATUS_MAX], *p;
    const line *l;
    long column = doc_col(d->cur, d->cur_x) + 1;
    long fullwidth = doc_col(d->cur, d->cur->len) + 1;
    long sum = d->cur_x, total = lines_bytes(&d->buf);

    for (l = d->buf.top; l != d->cur; l = l->next)
        sum += l->len + 1;
    /* "line %*ld/%ld (%2d%%), col %2ld/%2ld (%3d%%), char %*ld/%ld (%2d%%)" */
    p = put_l(put_s(msg, "line "), d->cur_no, digits(d->buf.count));
    p = put_l(put_s(p, "/"), d->buf.count, 0);
    p = put_l(put_s(p, " ("), percent(d->cur_no, d->buf.count), 2);
    p = put_l(put_s(p, "%), col "), column, 2);
    p = put_l(put_s(p, "/"), fullwidth, 2);
    p = put_l(put_s(p, " ("), percent(column, fullwidth), 3);
    p = put_l(put_s(p, "%), char "), sum, digits(total));
    p = put_l(put_s(p, "/"), total, 0);
    p = put_l(put_s(p, " ("), total > 0 ? percent(sum, total) : 0, 2);
    put_s(p, "%)");
    doc_status(d, msg);
}

static void do_mark(document *d)
{
    if (d->marking) {
        doc_clear_selection(d);
        doc_status(d, "Mark Unset");
    } else {
        doc_clear_selection(d);
        drop_anchor(d);
        d->marking = 1;
        doc_status(d, "Mark Set");
    }
}

static void select_all(document *d)
{
    doc_clear_selection(d);
    d->anchor = d->buf.top;
    d->anchor_no = 1;
    d->anchor_x = 0;
    d->cur = d->buf.bot;
    d->cur_no = d->buf.count;
    d->cur_x = 0;
    d->want = 0;
    mark_range(d, 1, d->cur_no);
}

/* A character from the keyboard, over any selection. */
int doc_type(document *d, char c)
{
    cut_run = 0;
    if (!doc_delete_selection(d))
        return 0;
    return insert_byte(d, c);
}

/*
 * Do a command on the document.  0 if it isn't one of the document's
 * own: the application's commands come back to the caller.
 */
int doc_exec(document *d, int cmd, int extend)
{
    if (cmd != CMD_CUT_LINE)
        cut_run = 0;
    if (cmd >= CMD_UP && cmd <= CMD_NEXT_WORD) {
        move_cursor(d, cmd, extend);
        return 1;
    }
    switch (cmd) {
    case CMD_ENTER:
        if (doc_delete_selection(d))
            do_enter(d);
        break;
    case CMD_TAB:
        doc_type(d, '\t');
        break;
    case CMD_BACKSPACE:
    case CMD_DELETE:
    case CMD_CLEAR:
        if (doc_has_selection(d))
            doc_delete_selection(d);
        else if (cmd == CMD_BACKSPACE)
            do_backspace(d);
        else if (cmd == CMD_DELETE)
            do_delete(d);
        break;
    case CMD_CUT_LINE:
        do_cut(d);
        break;
    case CMD_COPY_LINE:
        do_copy(d);
        break;
    case CMD_PASTE_LINE:
        do_paste(d);
        break;
    case CMD_MARK:
        do_mark(d);
        break;
    case CMD_SELECT_ALL:
        select_all(d);
        break;
    case CMD_LOCATION:
        do_location(d);
        break;
    default:
        return 0;
    }
    return 1;
}

/* Keep the cursor on screen: nano's smooth scrolling. */
void doc_scroll_to_cursor(document *d)
{
    long col = doc_col(d->cur, d->cur_x);

    if (d->cur_no < d->top_no)
        d->top_no = d->cur_no;
    else if (d->cur_no >= d->top_no + d->rows)
        d->top_no = d->cur_no - d->rows + 1;
    if (d->top_no < 1)
        d->top_no = 1;
    if (col > d->widest)
        d->widest = col;
    if (col < d->left || col >= d->left + d->cols)
        d->left = page_start(col, d->cols);
}
