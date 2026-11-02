/*
 * win.c - document windows: layout, drawing, scrolling and the mouse
 *
 * Everything goes through QuickDraw and the Window and Control Managers,
 * and layout comes from the window's size, never the machine's.  Text is
 * drawn a row at a time in srcCopy, so a row can be redrawn without
 * erasing it first and a keystroke doesn't flicker.
 */

#include <string.h>
#include "mac.h"

#define MARGIN      4           /* left of the text */
#define SBAR        16          /* scroll bar, overlapping the window edge */
#define MIN_HBAR    80          /* horizontal bar's share of the bottom */
#define MAX_COLS    480         /* cells in a row */
#define MAX_ROWS    50          /* of a new window */
#define DEFAULT_COLS 80
#define STAGGER_STEPS 6
#define MIN_WIDTH   200
#define MIN_HEIGHT  100
#define BAR_FONT    4           /* Monaco */
#define BAR_SIZE    9

/* The document font's metrics, and the bars'. */
static short font_id;
static int cell_w, line_h, font_ascent, font_leading;
static int bar_w, bar_h, bar_ascent;
static int stagger;

static char cells[MAX_COLS + 1];

static void use_doc_font(void)
{
    TextFont(font_id);
    TextSize(app_prefs.size);
    TextFace(0);
}

static void use_bar_font(void)
{
    TextFont(BAR_FONT);
    TextSize(BAR_SIZE);
    TextFace(0);
}

/*
 * Measuring needs a port with fonts of its own: changing the current
 * port's font at startup would change the Window Manager's, and with it
 * the menu bar's.
 */
static void use_measuring_port(GrafPtr *save)
{
    static GrafPort port;
    static int opened;

    GetPort(save);
    if (!opened) {
        OpenPort(&port);
        opened = 1;
    }
    SetPort(&port);
}

/* A font is fixed-width if its narrowest and widest glyphs agree. */
int font_is_fixed(short id)
{
    GrafPtr save;
    int fixed;

    use_measuring_port(&save);
    TextFont(id);
    TextSize(app_prefs.size);
    fixed = CharWidth('i') == CharWidth('W') &&
            CharWidth('l') == CharWidth('m');
    SetPort(save);
    return fixed;
}

static void measure(void)
{
    FontInfo fi;
    Str255 name;
    GrafPtr save;
    int i;

    /* the saved font may be gone: fall back on Monaco, then the system's */
    name[0] = (unsigned char)strlen(app_prefs.font);
    for (i = 0; i < name[0]; i++)
        name[i + 1] = (unsigned char)app_prefs.font[i];
    GetFNum(name, &font_id);
    if (font_id == 0 || !font_is_fixed(font_id))
        font_id = BAR_FONT;
    use_measuring_port(&save);
    use_doc_font();
    GetFontInfo(&fi);
    cell_w = CharWidth('0');
    font_ascent = fi.ascent;
    font_leading = fi.leading;
    line_h = fi.ascent + fi.descent + fi.leading;
    use_bar_font();
    GetFontInfo(&fi);
    bar_w = CharWidth('0');
    bar_h = fi.ascent + fi.descent + fi.leading + 1;
    bar_ascent = fi.ascent;
    SetPort(save);
}

/* The font changed, or the bars' settings: everything moves. */
void fonts_changed(void)
{
    docwin *dw;

    measure();
    for (dw = doc_list; dw != NULL; dw = dw->next) {
        SetPort((GrafPtr)dw);
        use_doc_font();
        win_layout(dw);
        win_redraw_all(dw);
    }
}

/* ------------------------------------------------------------------ */
/* layout */

static const Rect *port_rect(docwin *dw)
{
    return &((GrafPtr)dw)->portRect;
}

void win_layout(docwin *dw)
{
    const Rect *r = port_rect(dw);
    document *d = &dw->doc;
    int width = r->right - r->left, height = r->bottom - r->top;
    int bottom = r->bottom - SBAR + 1, top = r->top;
    int strip_right, hbar_w = width - SBAR + 1;
    int title_h = bar_h + 1, keys_h = bar_h * 2 + 2;

    dw->title_on = app_prefs.title_bar == BAR_SHOW;
    if (dw->title_on) {
        SetRect(&dw->title, r->left, top, r->right - SBAR + 1, top + title_h);
        top += title_h;
    }
    dw->bar_on = app_prefs.shortcut_bar == BAR_SHOW;
    if (dw->bar_on) {
        SetRect(&dw->keys, r->left, bottom - keys_h, r->right - SBAR + 1,
                bottom);
        bottom -= keys_h;
    }
    SetRect(&dw->text, r->left + MARGIN, top, r->right - SBAR + 1, bottom);

    d->rows = (dw->text.bottom - dw->text.top) / line_h;
    d->cols = (dw->text.right - dw->text.left) / cell_w;
    if (d->rows < 1)
        d->rows = 1;
    if (d->cols < 1)
        d->cols = 1;
    if (d->cols > MAX_COLS)
        d->cols = MAX_COLS;

    /* the status strip shares the bottom with the horizontal scroll bar */
    strip_right = hbar_w - MIN_HBAR;
    if (strip_right < 0)
        strip_right = 0;
    SetRect(&dw->strip, r->left, r->bottom - SBAR + 1, strip_right,
            r->bottom);

    MoveControl(dw->vbar, r->right - SBAR + 1, r->top - 1);
    SizeControl(dw->vbar, SBAR, height - SBAR + 3);
    MoveControl(dw->hbar, strip_right - 1, r->bottom - SBAR + 1);
    SizeControl(dw->hbar, width - SBAR + 3 - strip_right, SBAR);
}

void win_redraw_all(docwin *dw)
{
    InvalRect(port_rect(dw));
    dw->doc.full_redraw = 1;
}

/* ------------------------------------------------------------------ */
/* the caret */

static int caret_rect(docwin *dw, Rect *r)
{
    document *d = &dw->doc;
    long col = doc_col(d->cur, d->cur_x);
    long row = d->cur_no - d->top_no;
    int x;

    if (row < 0 || row >= d->rows || col < d->left ||
        col > d->left + d->cols)
        return 0;
    x = dw->text.left + (int)(col - d->left) * cell_w - 1;
    SetRect(r, x, dw->text.top + (int)row * line_h, x + 1,
            dw->text.top + ((int)row + 1) * line_h);
    return 1;
}

/*
 * The document can move while the caret is showing, so hiding it goes by
 * where it was drawn, not by where the document says it is now.  caret_on
 * means a bar is on the screen: a caret out of view isn't on.
 */
static void show_caret(docwin *dw)
{
    if (!dw->caret_on && caret_rect(dw, &dw->caret_at)) {
        InvertRect(&dw->caret_at);
        dw->caret_on = 1;
    }
}

void win_hide_caret(docwin *dw)
{
    if (dw->caret_on) {
        SetPort((GrafPtr)dw);
        InvertRect(&dw->caret_at);
        dw->caret_on = 0;
    }
}

/* ------------------------------------------------------------------ */
/* drawing */

static void draw_row(docwin *dw, int row, const line *l, long no)
{
    document *d = &dw->doc;
    int n = 0, x = dw->text.left, y = dw->text.top + row * line_h;
    long from, to;
    Rect r;

    if (l != NULL)
        n = doc_render(l, d->left, d->cols, cells);
    if (n > 0) {
        TextMode(srcCopy);
        MoveTo(x, y + font_ascent);
        DrawText(cells, 0, n);
        TextMode(srcOr);
    }
    SetRect(&r, x + n * cell_w, y, dw->text.right, y + line_h);
    EraseRect(&r);
    if (n > 0 && font_leading > 0) {
        SetRect(&r, x, y + line_h - font_leading, x + n * cell_w, y + line_h);
        EraseRect(&r);
    }
    if (l != NULL && ((WindowPeek)dw)->hilited &&
        doc_selection_cols(d, l, no, &from, &to)) {
        if (from < d->left)
            from = d->left;
        if (to > d->left + d->cols)
            to = d->left + d->cols;
        if (to > from) {
            SetRect(&r, x + (int)(from - d->left) * cell_w, y,
                    x + (int)(to - d->left) * cell_w, y + line_h);
            InvertRect(&r);
        }
    }
}

/* Monaco has no Command sign, so the bars draw their own. */
static void draw_command_sign(int x, int baseline)
{
    static const unsigned char rows[7] = {
        0x63, 0x55, 0x3E, 0x14, 0x3E, 0x55, 0x63
    };
    int r, c;

    for (r = 0; r < 7; r++)
        for (c = 0; c < 7; c++)
            if (rows[r] & (0x40 >> c)) {
                MoveTo(x + c - 1, baseline - 6 + r);
                Line(0, 0);
            }
}

/* Text in the bar font, n cells of it. */
static void put_text(int x, int y, const char *s, int n)
{
    int i, start = 0;

    for (i = 0; i <= n; i++) {
        if (i < n && s[i] != '\021')
            continue;
        if (i > start) {
            MoveTo(x + start * bar_w, y);
            DrawText((Ptr)s, start, i - start);
        }
        if (i < n)
            draw_command_sign(x + i * bar_w, y);
        start = i + 1;
    }
}

static void draw_title(docwin *dw)
{
    Rect r = dw->title;
    char name[64];
    int cols = (r.right - r.left) / bar_w;

    memcpy(name, dw->name + 1, dw->name[0]);
    name[dw->name[0]] = 0;
    if (cols > MAX_COLS)
        cols = MAX_COLS;
    bar_title(cells, cols, name, dw->doc.modified);
    use_bar_font();
    EraseRect(&r);
    put_text(r.left, r.top + bar_ascent, cells, cols);
    InvertRect(&r);
    use_doc_font();
}

static void draw_keys(docwin *dw)
{
    bar_item items[BAR_MAX];
    Rect r = dw->keys;
    int cols = (r.right - r.left - 2) / bar_w, n, i, x, y;

    EraseRect(&r);
    MoveTo(r.left, r.top);
    LineTo(r.right - 1, r.top);
    n = bar_shortcuts(app_prefs.profile, cols, items);
    use_bar_font();
    for (i = 0; i < n; i++) {
        x = r.left + 2 + items[i].col * bar_w;
        y = r.top + 2 + items[i].row * bar_h;
        put_text(x, y + bar_ascent, items[i].key, items[i].keylen);
        put_text(x + (items[i].keylen + 1) * bar_w, y + bar_ascent,
                 items[i].desc, (int)strlen(items[i].desc));
        {
            Rect k;

            SetRect(&k, x - 1, y, x + items[i].keylen * bar_w, y + bar_h - 1);
            InvertRect(&k);
        }
    }
    use_doc_font();
}

/*
 * Bottom left: what tico's status line says, and otherwise the file's
 * format, so neither costs a row of text.
 */
static void draw_strip(docwin *dw)
{
    document *d = &dw->doc;
    Rect r = dw->strip;
    const char *s = d->status;
    char idle[32], *p;
    int cols;

    if (r.right - r.left < bar_w)
        return;
    MoveTo(r.left, r.top);
    LineTo(r.right, r.top);
    r.top++;
    EraseRect(&r);
    if (s[0] == 0) {
        p = put_s(idle, d->format == FMT_MAC ? "Macintosh (CR)" :
                  d->format == FMT_DOS ? "DOS (CRLF)" : "Unix (LF)");
        if (d->modified)
            put_s(p, "  Modified");
        s = idle;
    }
    use_bar_font();
    cols = (r.right - r.left - MARGIN) / bar_w;
    if ((int)strlen(s) < cols)
        cols = (int)strlen(s);
    TextFace(d->status_err && s == d->status ? bold : 0);
    put_text(r.left + MARGIN, r.top + bar_ascent, s, cols);
    use_doc_font();
}

/* Set the scroll bars from the document. */
static void update_bars(docwin *dw)
{
    document *d = &dw->doc;
    long max = d->buf.count - d->rows, value = d->top_no - 1;
    short lim = 0, val = 0;

    if (max < 0)
        max = 0;
    /* bars count in 16 bits; long files scale */
    if (max > 32000) {
        lim = 32000;
        val = (short)(value / (max / 32000 + 1));
    } else {
        lim = (short)max;
        val = (short)(value < max ? value : max);
    }
    if (GetControlMaximum(dw->vbar) != lim)
        SetControlMaximum(dw->vbar, lim);
    if (GetControlValue(dw->vbar) != val)
        SetControlValue(dw->vbar, val);

    max = d->widest + 2 - d->cols;
    if (max < d->left)
        max = d->left;
    lim = (short)(max < 0 ? 0 : max > 32000 ? 32000 : max);
    val = (short)(d->left > 32000 ? 32000 : d->left);
    if (GetControlMaximum(dw->hbar) != lim)
        SetControlMaximum(dw->hbar, lim);
    if (GetControlValue(dw->hbar) != val)
        SetControlValue(dw->hbar, val);
}

/* The rows that have changed, and whatever else the document asks for. */
void win_refresh(docwin *dw)
{
    document *d = &dw->doc;
    long scrolled = d->top_no - dw->drawn_top, no;
    const line *l;
    int row, full = d->full_redraw || d->left != dw->drawn_left;
    int first = 0, last = 0;        /* rows ScrollRect left blank */
    int have_blank = 0;

    SetPort((GrafPtr)dw);
    win_hide_caret(dw);
    use_doc_font();

    if (!full && scrolled != 0) {
        if (scrolled >= d->rows || -scrolled >= d->rows) {
            full = 1;
        } else {
            RgnHandle rgn = NewRgn();
            Rect r, blank;

            SetRect(&r, dw->text.left - MARGIN, dw->text.top,
                    dw->text.right, dw->text.top + d->rows * line_h);
            ScrollRect(&r, 0, (short)(-scrolled * line_h), rgn);
            /* the update event catches what was covered; the rows we're
             * about to draw needn't wait for it */
            InvalRgn(rgn);
            DisposeRgn(rgn);
            if (scrolled > 0) {
                first = d->rows - (int)scrolled;
                last = d->rows;
            } else {
                first = 0;
                last = (int)-scrolled;
            }
            SetRect(&blank, r.left, dw->text.top + first * line_h, r.right,
                    dw->text.top + last * line_h);
            ValidRect(&blank);
            have_blank = 1;
        }
    }

    l = doc_line_at(d, d->top_no);
    no = d->top_no;
    for (row = 0; row < d->rows; row++, no++) {
        if (full || (have_blank && row >= first && row < last) ||
            (no >= d->redraw_from && no <= d->redraw_to) ||
            no == d->dirty[0] || no == d->dirty[1] || no == d->dirty[2])
            draw_row(dw, row, l, no);
        if (l != NULL)
            l = l->next;
    }

    if (d->title_dirty || full) {
        if (dw->title_on)
            draw_title(dw);
        draw_strip(dw);
    }
    update_bars(dw);

    dw->drawn_top = d->top_no;
    dw->drawn_left = d->left;
    d->full_redraw = 0;
    d->title_dirty = 0;
    d->redraw_from = d->redraw_to = NO_LINE;
    d->dirty[0] = d->dirty[1] = d->dirty[2] = NO_LINE;

    if (((WindowPeek)dw)->hilited && !doc_has_selection(d))
        show_caret(dw);
    dw->caret_when = TickCount();
}

static void draw_grow(docwin *dw)
{
    RgnHandle save = NewRgn();
    const Rect *p = port_rect(dw);
    Rect r;

    GetClip(save);
    SetRect(&r, p->right - SBAR + 1, p->bottom - SBAR + 1, p->right,
            p->bottom);
    ClipRect(&r);
    DrawGrowIcon((WindowPtr)dw);
    SetClip(save);
    DisposeRgn(save);
}

void win_update(docwin *dw)
{
    Rect r;
    document *d = &dw->doc;
    const line *l;
    long no;
    int row;

    win_hide_caret(dw);
    SetPort((GrafPtr)dw);
    BeginUpdate((WindowPtr)dw);
    use_doc_font();
    /* what the rows don't cover */
    SetRect(&r, dw->text.left - MARGIN, dw->text.top, dw->text.left,
            dw->text.bottom);
    EraseRect(&r);
    SetRect(&r, dw->text.left, dw->text.top + d->rows * line_h, dw->text.right,
            dw->text.bottom);
    EraseRect(&r);
    l = doc_line_at(d, d->top_no);
    no = d->top_no;
    for (row = 0; row < d->rows; row++, no++) {
        draw_row(dw, row, l, no);
        if (l != NULL)
            l = l->next;
    }
    if (dw->title_on)
        draw_title(dw);
    if (dw->bar_on)
        draw_keys(dw);
    draw_strip(dw);
    DrawControls((WindowPtr)dw);
    draw_grow(dw);
    EndUpdate((WindowPtr)dw);
}

/* ------------------------------------------------------------------ */
/* windows */

docwin *win_new(void)
{
    docwin *dw;
    const Rect *s = &qd.screenBits.bounds;
    Rect r, none = {0, 0, 0, 0};
    int width, rows;

    dw = (docwin *)NewPtrClear(sizeof *dw);
    if (dw == NULL)
        return NULL;
    if (!doc_init(&dw->doc)) {
        DisposePtr((Ptr)dw);
        return NULL;
    }
    if (cell_w == 0)
        measure();
    /* each window steps down and right; on a small screen they get
     * smaller instead of landing on top of one another */
    r.left = s->left + 4 + stagger * 18;
    r.top = s->top + 40 + stagger * 18;
    width = DEFAULT_COLS * cell_w + MARGIN + SBAR + 1;
    r.right = r.left + width;
    if (r.right > s->right - 4)
        r.right = s->right - 4;
    rows = MAX_ROWS;
    r.bottom = r.top + rows * line_h + SBAR - 1;
    if (r.bottom > s->bottom - 4)
        r.bottom = s->bottom - 4;
    stagger = (stagger + 1) % STAGGER_STEPS;

    memcpy(dw->name, "\puntitled", 9);
    dw->type = 'TEXT';
    dw->creator = CREATOR;

    NewWindow(&dw->rec, &r, dw->name, 0, 8 /* zoomDocProc */, (WindowPtr)-1, 1, 0);
    SetPort((GrafPtr)dw);
    dw->vbar = NewControl((WindowPtr)dw, &none, "\p", 1, 0, 0, 0,
                          scrollBarProc, 0);
    dw->hbar = NewControl((WindowPtr)dw, &none, "\p", 1, 0, 0, 0,
                          scrollBarProc, 0);
    use_doc_font();
    win_layout(dw);
    dw->drawn_top = 1;

    dw->next = doc_list;
    doc_list = dw;
    return dw;
}

void win_dispose(docwin *dw)
{
    docwin **p;

    for (p = &doc_list; *p != NULL; p = &(*p)->next)
        if (*p == dw) {
            *p = dw->next;
            break;
        }
    doc_free(&dw->doc);
    CloseWindow((WindowPtr)dw);
    DisposePtr((Ptr)dw);
}

void win_set_name(docwin *dw)
{
    SetWTitle((WindowPtr)dw, dw->name);
    dw->doc.title_dirty = 1;
}

void win_activate(docwin *dw, int active)
{
    SetPort((GrafPtr)dw);
    HiliteControl(dw->vbar, active ? 0 : 255);
    HiliteControl(dw->hbar, active ? 0 : 255);
    draw_grow(dw);
    if (!active)
        win_hide_caret(dw);
    /* the selection shows only in the active window */
    if (doc_has_selection(&dw->doc))
        dw->doc.full_redraw = 1;
    if (active)
        win_refresh(dw);
    else if (dw->doc.full_redraw)
        win_refresh(dw);
}

void win_grow(docwin *dw, Point where)
{
    Rect limits;
    long size;

    SetRect(&limits, MIN_WIDTH, MIN_HEIGHT, 0x7FFF, 0x7FFF);
    size = GrowWindow((WindowPtr)dw, where, &limits);
    if (size == 0)
        return;
    win_hide_caret(dw);
    SizeWindow((WindowPtr)dw, LoWord(size), HiWord(size), 1);
    win_layout(dw);
    doc_scroll_to_cursor(&dw->doc);
    win_redraw_all(dw);
}

void win_zoom(docwin *dw, Point where, short part)
{
    if (!TrackBox((WindowPtr)dw, where, part))
        return;
    win_hide_caret(dw);
    SetPort((GrafPtr)dw);
    EraseRect(port_rect(dw));
    ZoomWindow((WindowPtr)dw, part, 0);
    win_layout(dw);
    doc_scroll_to_cursor(&dw->doc);
    win_redraw_all(dw);
}

/* ------------------------------------------------------------------ */
/* scrolling, which moves the view and leaves the cursor be */

static void set_top(docwin *dw, long top)
{
    document *d = &dw->doc;
    long max = d->buf.count - d->rows + 1;

    if (top > max)
        top = max;
    if (top < 1)
        top = 1;
    d->top_no = top;
}

static void set_left(docwin *dw, long left)
{
    if (left < 0)
        left = 0;
    dw->doc.left = left;
}

static docwin *tracking;

static pascal void scroll_action(ControlHandle c, short part)
{
    docwin *dw = tracking;
    document *d = &dw->doc;
    int vertical = c == dw->vbar;
    long page = vertical ? d->rows - 2 : d->cols - 8, step;

    if (page < 1)
        page = 1;
    switch (part) {
    case inUpButton:
        step = -1;
        break;
    case inDownButton:
        step = 1;
        break;
    case inPageUp:
        step = -page;
        break;
    case inPageDown:
        step = page;
        break;
    default:
        return;
    }
    if (vertical)
        set_top(dw, d->top_no + step);
    else
        set_left(dw, d->left + step);
    win_refresh(dw);
}

static void click_bar(docwin *dw, ControlHandle c, short part, Point p)
{
    document *d = &dw->doc;
    long max;

    if (part != inThumb) {
        tracking = dw;
        TrackControl(c, p, NewControlActionUPP(scroll_action));
        return;
    }
    if (TrackControl(c, p, NULL) == 0)
        return;
    if (c == dw->vbar) {
        max = d->buf.count - d->rows;
        if (max > 32000)
            set_top(dw, (long)GetControlValue(c) * (max / 32000 + 1) + 1);
        else
            set_top(dw, (long)GetControlValue(c) + 1);
    } else {
        set_left(dw, GetControlValue(c));
    }
    win_refresh(dw);
}

/* ------------------------------------------------------------------ */
/* the mouse in the text */

static void locate(docwin *dw, Point p, long *no, long *col)
{
    document *d = &dw->doc;
    int y = p.v - dw->text.top, x = p.h - dw->text.left;

    *no = d->top_no + (y < 0 ? -1 : y / line_h);
    *col = d->left + (x < 0 ? 0 : (x + cell_w / 2) / cell_w);
}

static void click_text(docwin *dw, EventRecord *e, Point p)
{
    static long last_when;
    static long last_no, last_col;
    document *d = &dw->doc;
    long no, col;
    int shift = (e->modifiers & shiftKey) != 0;

    locate(dw, p, &no, &col);
    if (!shift && no == last_no && col == last_col &&
        e->when - last_when <= (long)LMGetDoubleTime()) {
        doc_select_word(d, no, col);
        win_refresh(dw);
        last_when = 0;
        return;
    }
    last_when = e->when;
    last_no = no;
    last_col = col;
    doc_click(d, no, col, shift);
    win_refresh(dw);

    /* a drag; past the top or bottom it scrolls */
    while (StillDown()) {
        GetMouse(&p);
        locate(dw, p, &no, &col);
        if (p.v < dw->text.top)
            set_top(dw, d->top_no - 1);
        else if (p.v >= dw->text.top + d->rows * line_h) {
            set_top(dw, d->top_no + 1);
            no = d->top_no + d->rows;
        }
        if (no > d->top_no + d->rows - 1)
            no = d->top_no + d->rows - 1;
        if (no < d->top_no)
            no = d->top_no;
        if (no == d->cur_no && col == doc_col(d->cur, d->cur_x) &&
            d->top_no == dw->drawn_top)
            continue;
        doc_click(d, no, col, 1);
        win_refresh(dw);
    }
}

void win_click(docwin *dw, EventRecord *e)
{
    Point p = e->where;
    ControlHandle c;
    short part;

    SetPort((GrafPtr)dw);
    GlobalToLocal(&p);
    part = FindControl(p, (WindowPtr)dw, &c);
    win_hide_caret(dw);
    if (part != 0)
        click_bar(dw, c, part, p);
    else if (PtInRect(p, &dw->text))
        click_text(dw, e, p);
    win_refresh(dw);
}

/* Null events: blink the caret, and keep the I-beam over the text. */
void win_idle(docwin *dw)
{
    Point p;
    long now = TickCount();

    SetPort((GrafPtr)dw);
    GetMouse(&p);
    if (PtInRect(p, &dw->text))
        SetCursor(*GetCursor(iBeamCursor));
    else
        SetCursor(&qd.arrow);
    if (doc_has_selection(&dw->doc)) {
        win_hide_caret(dw);
        return;
    }
    if (now - dw->caret_when >= (long)LMGetCaretTime()) {
        if (dw->caret_on)
            win_hide_caret(dw);
        else
            show_caret(dw);
        dw->caret_when = now;
    }
}
