/*
 * mac.h - what the Macintosh side of tico68 shares
 */

#ifndef MAC_H
#define MAC_H

#include <Multiverse.h>
#include "../tico.h"

#define CREATOR 'Tc68'

/* One open document.  The window record comes first, so a docwin * is
 * its WindowPtr. */
typedef struct docwin {
    WindowRecord rec;
    struct docwin *next;
    document doc;

    /* the file; has_file is 0 for a document never saved */
    int has_file;
    short vref;
    long dir;
    Str63 name;
    OSType type, creator;

    /* layout */
    ControlHandle vbar, hbar;
    int title_on, bar_on;       /* tico's bars, as laid out */
    Rect title, text, keys, strip;

    /* what is on the screen */
    int caret_on;
    Rect caret_at;              /* where, while caret_on */
    long caret_when;
    long drawn_top, drawn_left;
} docwin;

/* main.c */
extern prefs app_prefs;
extern docwin *doc_list;
docwin *front_doc(void);
void app_command(int cmd, int extend);
void app_error(const char *what, const char *why, const char *more);
void app_report_recovery(ConstStr255Param temp);
void app_report(const char *text);

/* win.c */
void fonts_changed(void);
int font_is_fixed(short id);
docwin *win_new(void);
void win_dispose(docwin *dw);
void win_set_name(docwin *dw);
void win_layout(docwin *dw);
void win_refresh(docwin *dw);
void win_update(docwin *dw);
void win_activate(docwin *dw, int active);
void win_click(docwin *dw, EventRecord *e);
void win_grow(docwin *dw, Point where);
void win_zoom(docwin *dw, Point where, short part);
void win_idle(docwin *dw);
void win_hide_caret(docwin *dw);
void win_redraw_all(docwin *dw);

/* menus.c */
void menus_init(void);
void menus_profile_changed(void);
void menus_update(void);
void menus_windows_changed(void);
void menu_choice(long choice);
int menu_of_cmd(int cmd);

/* files.c */
void file_io_init(void);
void file_open_dialog(void);
docwin *file_open(short vref, long dir, ConstStr255Param name, int asked);
docwin *file_open_wd(short wd, ConstStr255Param name);
int file_save(docwin *dw);
int file_save_as(docwin *dw);
int file_wd_location(short wd, short *vref, long *dir);

/* prefsfile.c */
void prefs_load(prefs *p);
void prefs_store(const prefs *p);
void prefs_dialog(void);

#endif
