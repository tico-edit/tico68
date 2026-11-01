/*
 * tico68 - a tico/nano-style text editor for the classic Macintosh
 *
 * Shared declarations for the portable core: the C files in src/ are
 * portable, so the host unit tests build them too.  src/mac/ holds everything
 * that talks to the Toolbox.
 */

#ifndef TICO_H
#define TICO_H

#include <stddef.h>

#ifndef VERSION
#define VERSION "unknown"
#endif

/* ------------------------------------------------------------------ */
/* pool.c: memory for the buffer, and the host's allocator beneath it.  */

void *pool_alloc(unsigned size);
void pool_free(void *p, unsigned size);
unsigned pool_round(unsigned size);

void *plat_alloc(unsigned size);
void plat_free(void *p);

/* ------------------------------------------------------------------ */
/* buffer.c: the text, as a doubly linked list of lines.               */

/*
 * One line of text, without its line break.  The bytes are arbitrary
 * (NULs included); text is NULL while cap is 0.
 */
typedef struct line {
    struct line *prev, *next;
    char *text;
    unsigned len, cap;
} line;

/*
 * A list of lines.  In the edit buffer the last line is always empty:
 * nano's "magic line", which stands for the end of the file, so the
 * file's bytes are every line but the last, each followed by a break.
 */
typedef struct {
    line *top, *bot;
    long count;
} lines;

/* Longest line the buffer can hold. */
#define LINE_MAX_LEN 65000u

line *line_new(const char *text, unsigned len);
void line_free(line *l);
int line_insert(line *l, unsigned at, const char *text, unsigned len);
void line_erase(line *l, unsigned at, unsigned len);

void lines_init(lines *ls);
void lines_clear(lines *ls);
void lines_link_after(lines *ls, line *after, line *l);
void lines_unlink(lines *ls, line *l);
int lines_append(lines *ls, const char *text, unsigned len);
int lines_ensure_magic(lines *ls);
int lines_split(lines *ls, line *l, unsigned at);
int lines_join(lines *ls, line *l);
int lines_copy(lines *dst, const lines *src);
long lines_bytes(const lines *ls);

/* ------------------------------------------------------------------ */
/* text.c: line-ending conversion, as nano 8.7.1 / tico do it.         */

enum { FMT_NONE, FMT_UNIX, FMT_DOS, FMT_MAC };

/*
 * Streaming reader.  Feed it the file's bytes in any number of pieces,
 * then finish it; it builds the buffer's lines (with the magic line).
 */
typedef struct {
    lines *ls;
    line *cur;          /* line being accumulated */
    long breaks;        /* line breaks seen so far */
    int format;         /* FMT_NONE until a break decides it */
    int failed;         /* out of memory, or a line too long */
} reader;

void reader_start(reader *r, lines *ls);
int reader_feed(reader *r, const char *buf, unsigned len);
int reader_finish(reader *r);

/* The bytes that end a line in a format (FMT_NONE writes DOS). */
const char *format_eol(int format);

/* Serialize: calls out(ctx, bytes, len) for each piece of the file. */
typedef int (*text_out)(void *ctx, const char *buf, unsigned len);
int text_write(const lines *ls, int format, int ctrlz, text_out out,
               void *ctx);

/* ------------------------------------------------------------------ */
/* Editor commands: what keys and menus both turn into.                 */

enum {
    CMD_NONE,
    /* movement, which Shift or the mark turns into selection */
    CMD_UP, CMD_DOWN, CMD_LEFT, CMD_RIGHT, CMD_HOME, CMD_END,
    CMD_PAGE_UP, CMD_PAGE_DOWN, CMD_FIRST_LINE, CMD_LAST_LINE,
    CMD_PREV_WORD, CMD_NEXT_WORD,
    /* editing, done by doc_exec */
    CMD_ENTER, CMD_TAB, CMD_BACKSPACE, CMD_DELETE, CMD_CLEAR,
    CMD_CUT_LINE, CMD_COPY_LINE, CMD_PASTE_LINE,
    CMD_MARK, CMD_SELECT_ALL, CMD_LOCATION,
    /* the application's, done in src/mac */
    CMD_CUT, CMD_COPY, CMD_PASTE, CMD_UNDO,
    CMD_NEW, CMD_OPEN, CMD_CLOSE, CMD_EXIT, CMD_SAVE, CMD_SAVE_AS,
    CMD_QUIT, CMD_NEXT_WINDOW, CMD_PREV_WINDOW, CMD_PREFS, CMD_ABOUT,
    CMD_FORMAT_MAC, CMD_FORMAT_DOS, CMD_FORMAT_UNIX,
    CMD_COUNT
};

/* ------------------------------------------------------------------ */
/* doc.c: one open document's text, cursor, selection and view state.  */

#define TABSIZE 8
#define STATUS_MAX 100
#define NO_LINE 0x7FFFFFFFL

typedef struct {
    lines buf;

    /* the cursor */
    line *cur;
    long cur_no;                /* 1-based */
    unsigned cur_x;             /* byte offset in cur */
    long want;                  /* column to aim for going up and down */

    /* the selection runs from here to the cursor; none if anchor is NULL */
    line *anchor;
    long anchor_no;
    unsigned anchor_x;
    int marking;                /* M-A's sticky mark is set */

    /* the view */
    long top_no;                /* first line shown */
    long left;                  /* first column shown */
    int rows, cols;             /* text area size, set by the window */
    long widest;                /* longest line seen, in columns */
    line *hint;                 /* a line whose number we know */
    long hint_no;

    /* what the window must redraw */
    int full_redraw;
    long redraw_from, redraw_to;
    long dirty[3];

    /* the file */
    int format;                 /* output format */
    int disk_format;            /* format of the file on disk */
    int ctrlz;                  /* a trailing Ctrl+Z, kept out of sight */
    int edited;                 /* the text differs from the file */
    int modified;               /* saving would change the file */

    char status[STATUS_MAX];
    int status_err;
    int status_count;           /* keystrokes until it is wiped */
    int title_dirty;            /* modified or status changed */
} document;

/* The cut buffer ^K and ^U share across every document. */
extern lines cutbuf;

/* Reading a file: in() fills buf, returning the count, 0 at the end, or
 * -1 on error. */
typedef int (*text_in)(void *ctx, char *buf, unsigned cap);
enum { READ_OK, READ_ERR, READ_NOMEM };

int doc_init(document *d);
void doc_free(document *d);
int doc_read(document *d, text_in in, void *ctx, char *iobuf, unsigned cap);
void doc_saved(document *d);
void doc_set_format(document *d, int format);

int doc_exec(document *d, int cmd, int extend);
int doc_type(document *d, char c);
void doc_insert_lines(document *d, const lines *src);
int doc_paste_text(document *d, const char *text, long len);
int doc_delete_selection(document *d);

int doc_has_selection(const document *d);
long doc_selection_size(const document *d);
int doc_selection_out(const document *d, text_out out, void *ctx);
int doc_selection_lines(const document *d, lines *dst);
int doc_selection_cols(const document *d, const line *l, long no,
                       long *from, long *to);

void doc_click(document *d, long no, long col, int extend);
void doc_select_word(document *d, long no, long col);
void doc_clear_selection(document *d);

line *doc_line_at(document *d, long no);
long doc_col(const line *l, unsigned x);
unsigned doc_x_at(const line *l, long goal);
int doc_render(const line *l, long left, int cols, char *out);
void doc_scroll_to_cursor(document *d);
void doc_view_changed(document *d);

void doc_status(document *d, const char *msg);
void doc_alert(document *d, const char *msg);
void doc_status_tick(document *d);

char *put_s(char *d, const char *s);
char *put_l(char *d, long v, int width);

/* ------------------------------------------------------------------ */
/* bindings.c: keys to commands, by profile.                           */

enum { PROFILE_LEGACY, PROFILE_MODERN, PROFILE_MAC, PROFILE_COUNT };

/*
 * A key: plain bytes are 0x00-0xFF.  KEY_CMD is Command, or Control
 * where there is one, and stands for tico's Ctrl; KEY_META is
 * Command-Option, or Esc then the key.  Special keys come above those,
 * and take the KF_ flags.  Letters in KEY_CMD and KEY_META are upper case.
 */
#define KEY_CMD(c)  (0x100 | (c))
#define KEY_META(c) (0x200 | (c))
#define KEY_SPECIAL 0x300
enum {
    KEY_UP = KEY_SPECIAL, KEY_DOWN, KEY_LEFT, KEY_RIGHT,
    KEY_HOME, KEY_END, KEY_PGUP, KEY_PGDN, KEY_DEL, KEY_BSP,
    KEY_TAB, KEY_RETURN, KEY_ENTER, KEY_ESC,
    KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5, KEY_F6, KEY_F7, KEY_F8,
    KEY_F9, KEY_F10, KEY_F11, KEY_F12
};
#define KF_CMD      0x1000
#define KF_OPT      0x2000
#define KF_META     (KF_CMD | KF_OPT)
#define KF_SHIFT    0x4000
#define KEY_BASE(k) ((k) & 0xFFF)

/* Modifiers to key_from_event. */
#define MOD_CMD     1
#define MOD_OPT     2
#define MOD_SHIFT   4
#define MOD_CTRL    8

typedef struct {
    int cmd;                    /* CMD_NONE: not bound */
    int extend;                 /* Shift was held on a movement */
    int insert;                 /* or a byte to insert, when not -1 */
} binding;

int key_from_event(unsigned char c, unsigned char vkey, int mods);
int key_after_esc(int key);
binding key_lookup(int profile, int key);
int key_for_cmd(int profile, int cmd);
char *key_label(char *d, int key);
const char *profile_name(int profile);

/* ------------------------------------------------------------------ */
/* bars.c: tico's optional title and shortcut bars.                     */

enum { BAR_AUTO, BAR_SHOW, BAR_HIDE };

/* Automatic mode leaves at least this many text rows, with hysteresis. */
#define BAR_ROWS_SHOW 32
#define BAR_ROWS_HIDE 30

int bar_visible(int setting, int was_on, int height, int bar_height,
                int line_height);
void bar_title(char *out, int cols, const char *name, int modified);

typedef struct {
    char key[8];
    const char *desc;
    int keylen, row, col;
} bar_item;

#define BAR_MAX 8
int bar_shortcuts(int profile, int cols, bar_item *items);

/* ------------------------------------------------------------------ */
/* prefs.c: global settings, as a little text file.                    */

#define FONT_NAME_MAX 32

typedef struct {
    int profile;
    int title_bar, shortcut_bar;
    char font[FONT_NAME_MAX];
    int size;
} prefs;

void prefs_default(prefs *p);
void prefs_parse(prefs *p, const char *text, long len);
int prefs_format(const prefs *p, char *out, int cap);

/* ------------------------------------------------------------------ */
/* save.c: writing a document safely, and closing documents.           */

/*
 * What a safe save needs of the file system.  "Target" is the file being
 * saved over; "temp" is a scratch file on the same volume.  All return 1
 * on success, 0 on failure.
 */
typedef struct {
    void *ctx;
    int (*temp_create)(void *ctx);
    int (*temp_write)(void *ctx, const char *buf, unsigned len);
    int (*temp_close)(void *ctx);
    int (*temp_delete)(void *ctx);
    int (*target_open)(void *ctx, long *length);
    int (*target_set_length)(void *ctx, long length);
    int (*target_write)(void *ctx, const char *buf, unsigned len);
    int (*target_close)(void *ctx);
    int (*target_replace)(void *ctx);   /* temp takes target's place */
} save_ops;

enum {
    SAVE_OK,
    SAVE_FAILED,        /* the file on disk is as it was */
    SAVE_KEPT_TEMP      /* the file may be damaged; temp holds the text */
};

int save_in_place(const document *d, const save_ops *ops, char *buf,
                  unsigned cap);
int save_replacing(const document *d, const save_ops *ops, char *buf,
                   unsigned cap);

enum { ASK_SAVE, ASK_DISCARD, ASK_CANCEL };

typedef struct {
    void *ctx;
    int (*ask)(void *ctx, void *doc);       /* an ASK_ answer */
    int (*save)(void *ctx, void *doc);      /* 1 if it worked */
    void (*dispose)(void *ctx, void *doc);
    int (*is_modified)(void *ctx, void *doc);
} close_ops;

int close_document(void *doc, const close_ops *ops);

#endif
