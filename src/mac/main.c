/*
 * main.c - the application: startup, the event loop, commands
 *
 * Everything a key or a menu does comes through app_command, whatever
 * profile is active.  There is no waiting for keystrokes: each event is
 * handled and control comes straight back here.
 */

#include <string.h>
#include "mac.h"

/* What must stay free for the Toolbox: dialogs, Standard File, the scrap. */
#define RESERVE 96000L

prefs app_prefs;
docwin *doc_list;

static int quitting;
static int meta_pending;        /* Esc seen: the next key is Meta */
static int have_wne;

/* ------------------------------------------------------------------ */
/* the pool's memory */

void *plat_alloc(unsigned size)
{
    if (MaxBlock() < (long)size + RESERVE)
        return NULL;
    return NewPtr(size);
}

void plat_free(void *p)
{
    DisposePtr(p);
}

/* ------------------------------------------------------------------ */
/* alerts */

static void to_pascal(Str255 out, const char *in)
{
    int n = (int)strlen(in);

    out[0] = (unsigned char)n;
    memcpy(out + 1, in, n);
}

void app_error(const char *what, const char *why, const char *more)
{
    Str255 a, b, c;

    to_pascal(a, what);
    to_pascal(b, why);
    to_pascal(c, more);
    ParamText(a, b, c, "\p");
    InitCursor();
    StopAlert(129, NULL);
}

void app_report_recovery(ConstStr255Param temp)
{
    ParamText(temp, "\p", "\p", "\p");
    StopAlert(132, NULL);
}

docwin *front_doc(void)
{
    WindowPtr w = FrontWindow();

    if (w == NULL || ((WindowPeek)w)->windowKind != userKind)
        return NULL;
    return (docwin *)w;
}

static int is_doc(WindowPtr w)
{
    return w != NULL && ((WindowPeek)w)->windowKind == userKind;
}

/* ------------------------------------------------------------------ */
/* closing */

/* tico68.r's ALRT 128 */
static int close_ask(void *ctx, void *doc)
{
    docwin *dw = doc;

    (void)ctx;
    win_hide_caret(dw);
    SelectWindow((WindowPtr)dw);
    /* let it come to the front on screen before the question */
    {
        EventRecord e;

        while (GetNextEvent(updateMask | activMask, &e)) {
            if (e.what == updateEvt)
                win_update((docwin *)e.message);
        }
    }
    ParamText(dw->name, "\p", "\p", "\p");
    InitCursor();
    switch (CautionAlert(128, NULL)) {
    case 1:
        return ASK_SAVE;
    case 3:
        return ASK_DISCARD;
    default:
        return ASK_CANCEL;
    }
}

static int close_save(void *ctx, void *doc)
{
    (void)ctx;
    return file_save(doc);
}

static void close_dispose(void *ctx, void *doc)
{
    (void)ctx;
    win_dispose(doc);
}

static int close_modified(void *ctx, void *doc)
{
    (void)ctx;
    return ((docwin *)doc)->doc.modified;
}

static const close_ops mac_close = {
    NULL, close_ask, close_save, close_dispose, close_modified
};

static void quit_app(void)
{
    /* newest first: each stays open until it's safe to close */
    while (doc_list != NULL)
        if (!close_document(doc_list, &mac_close))
            return;
    quitting = 1;
}

/* ------------------------------------------------------------------ */
/* the clipboard */

static int to_handle(void *ctx, const char *buf, unsigned len)
{
    char **p = ctx;

    memcpy(*p, buf, len);
    *p += len;
    return 1;
}

static int copy_to_scrap(docwin *dw)
{
    long size = doc_selection_size(&dw->doc);
    Handle h = NewHandle(size);
    char *p;

    if (h == NULL) {
        doc_alert(&dw->doc, "Out of memory");
        return 0;
    }
    HLock(h);
    p = *h;
    doc_selection_out(&dw->doc, to_handle, &p);
    ZeroScrap();
    PutScrap(size, 'TEXT', *h);
    DisposeHandle(h);
    return 1;
}

static void paste_scrap(docwin *dw)
{
    Handle h = NewHandle(0);
    long offset, size;

    if (h == NULL) {
        doc_alert(&dw->doc, "Out of memory");
        return;
    }
    size = GetScrap(h, 'TEXT', &offset);
    if (size > 0) {
        HLock(h);
        doc_paste_text(&dw->doc, *h, size);
    } else if (size != noTypeErr) {
        doc_alert(&dw->doc, "Out of memory");
    }
    DisposeHandle(h);
}

/* ------------------------------------------------------------------ */
/* commands */

static void new_document(void)
{
    static int untitled;
    docwin *dw = win_new();
    Str255 num;
    int i;

    if (dw == NULL) {
        app_error("Can't make a new document.", "There isn't enough memory.",
                  "");
        return;
    }
    if (++untitled > 1) {
        NumToString(untitled, num);
        dw->name[++dw->name[0]] = ' ';
        for (i = 1; i <= num[0]; i++)
            dw->name[++dw->name[0]] = num[i];
        win_set_name(dw);
    }
    ShowWindow((WindowPtr)dw);
    SelectWindow((WindowPtr)dw);
    menus_windows_changed();
}

static void switch_window(int forward)
{
    docwin *dw = front_doc(), *d, *prev = NULL, *last = NULL;

    if (dw == NULL)
        return;
    for (d = doc_list; d != NULL; d = d->next) {
        if (d->next == dw)
            prev = d;
        last = d;
    }
    /* the list runs newest first */
    d = forward ? prev : dw->next;
    if (d == NULL)
        d = forward ? last : doc_list;
    if (d != dw)
        SelectWindow((WindowPtr)d);
}

void app_command(int cmd, int extend)
{
    docwin *dw = front_doc();

    switch (cmd) {
    case CMD_NEW:
        new_document();
        break;
    case CMD_OPEN:
        file_open_dialog();
        break;
    case CMD_QUIT:
        quit_app();
        break;
    case CMD_ABOUT:
        ParamText("\p" VERSION, "\p", "\p", "\p");
        Alert(130, NULL);
        break;
    case CMD_PREFS:
        prefs_dialog();
        break;
    case CMD_UNDO:
        break;
    case CMD_NEXT_WINDOW:
    case CMD_PREV_WINDOW:
        switch_window(cmd == CMD_NEXT_WINDOW);
        break;
    default:
        if (dw == NULL)
            break;
        switch (cmd) {
        case CMD_CLOSE:
            close_document(dw, &mac_close);
            menus_windows_changed();
            return;
        case CMD_EXIT:
            /* tico's ^X: closing the last buffer leaves the editor */
            if (close_document(dw, &mac_close) && doc_list == NULL)
                quitting = 1;
            menus_windows_changed();
            return;
        case CMD_SAVE:
            file_save(dw);
            break;
        case CMD_SAVE_AS:
            file_save_as(dw);
            break;
        case CMD_CUT:
            if (doc_has_selection(&dw->doc) && copy_to_scrap(dw))
                doc_delete_selection(&dw->doc);
            break;
        case CMD_COPY:
            if (doc_has_selection(&dw->doc))
                copy_to_scrap(dw);
            break;
        case CMD_PASTE:
            paste_scrap(dw);
            break;
        case CMD_FORMAT_MAC:
            doc_set_format(&dw->doc, FMT_MAC);
            break;
        case CMD_FORMAT_DOS:
            doc_set_format(&dw->doc, FMT_DOS);
            break;
        case CMD_FORMAT_UNIX:
            doc_set_format(&dw->doc, FMT_UNIX);
            break;
        default:
            doc_exec(&dw->doc, cmd, extend);
            break;
        }
        /* a dialog may have changed what is in front */
        dw = front_doc();
        if (dw != NULL) {
            doc_scroll_to_cursor(&dw->doc);
            win_refresh(dw);
        }
        return;
    }
}

/* ------------------------------------------------------------------ */
/* keys */

static void report_unbound(docwin *dw, int key)
{
    char msg[40], *p = put_s(msg, "Unbound key: ");

    key_label(p, key & ~KF_SHIFT);
    doc_alert(&dw->doc, msg);
}

/* Menu items a key can reach are dealt with just as if chosen. */
static void run_key_command(int cmd, int extend)
{
    int menu = menu_of_cmd(cmd);

    menus_update();
    /* movement has no need of a flash in the menu bar */
    if (menu != 0 && !(cmd >= CMD_UP && cmd <= CMD_NEXT_WORD))
        HiliteMenu(menu);
    app_command(cmd, extend);
    HiliteMenu(0);
}

static void handle_key(EventRecord *e)
{
    unsigned char c = e->message & charCodeMask;
    unsigned char vkey = (e->message & keyCodeMask) >> 8;
    int mods = 0, key;
    docwin *dw;
    binding b;

    /* a desk accessory in front gets the standard Edit keys */
    if (!is_doc(FrontWindow()) && FrontWindow() != NULL) {
        if (e->modifiers & cmdKey) {
            switch (c | 0x20) {
            case 'z':
                SystemEdit(0);
                break;
            case 'x':
                SystemEdit(2);
                break;
            case 'c':
                SystemEdit(3);
                break;
            case 'v':
                SystemEdit(4);
                break;
            }
        }
        return;
    }

    if (e->modifiers & cmdKey)
        mods |= MOD_CMD;
    if (e->modifiers & optionKey)
        mods |= MOD_OPT;
    if (e->modifiers & shiftKey)
        mods |= MOD_SHIFT;
    if (e->modifiers & 0x1000)        /* controlKey */
        mods |= MOD_CTRL;
    key = key_from_event(c, vkey, mods);

    /* Esc is the way to Meta, except where Mac conventions rule */
    if (app_prefs.profile != PROFILE_MAC) {
        if (meta_pending) {
            meta_pending = 0;
            if (KEY_BASE(key) == KEY_ESC)
                return;
            key = key_after_esc(key);
        } else if (KEY_BASE(key) == KEY_ESC) {
            meta_pending = 1;
            return;
        }
    }

    ObscureCursor();
    dw = front_doc();
    if (dw != NULL)
        doc_status_tick(&dw->doc);
    b = key_lookup(app_prefs.profile, key);
    if (b.cmd != CMD_NONE) {
        run_key_command(b.cmd, b.extend);
    } else if (dw == NULL) {
        return;
    } else if (b.insert >= 0) {
        if (b.insert == 0x1B)
            return;
        doc_type(&dw->doc, (char)b.insert);
        doc_scroll_to_cursor(&dw->doc);
    } else if (KEY_BASE(key) < KEY_SPECIAL) {
        report_unbound(dw, key);
    }
    /* with more keys waiting, draw once they've all been taken in */
    if (front_doc() != NULL) {
        EventRecord next;

        if (!EventAvail(keyDownMask | autoKeyMask, &next))
            win_refresh(front_doc());
    }
}

/* ------------------------------------------------------------------ */
/* events */

static void handle_mouse(EventRecord *e)
{
    WindowPtr w;
    short part = FindWindow(e->where, &w);

    switch (part) {
    case inMenuBar:
        menus_update();
        menu_choice(MenuSelect(e->where));
        break;
    case inSysWindow:
        SystemClick(e, w);
        break;
    case inDrag:
        DragWindow(w, e->where, &qd.screenBits.bounds);
        break;
    case inContent:
        if (w != FrontWindow())
            SelectWindow(w);
        else if (is_doc(w))
            win_click((docwin *)w, e);
        break;
    case inGrow:
        if (is_doc(w))
            win_grow((docwin *)w, e->where);
        break;
    case inGoAway:
        if (TrackGoAway(w, e->where) && is_doc(w)) {
            close_document(w, &mac_close);
            menus_windows_changed();
        }
        break;
    case inZoomIn:
    case inZoomOut:
        if (is_doc(w))
            win_zoom((docwin *)w, e->where, part);
        break;
    }
}

static void handle_event(EventRecord *e)
{
    WindowPtr w;

    switch (e->what) {
    case mouseDown:
        handle_mouse(e);
        break;
    case keyDown:
    case autoKey:
        handle_key(e);
        break;
    case updateEvt:
        w = (WindowPtr)e->message;
        if (is_doc(w))
            win_update((docwin *)w);
        break;
    case activateEvt:
        w = (WindowPtr)e->message;
        if (is_doc(w)) {
            win_activate((docwin *)w, (e->modifiers & activeFlag) != 0);
            menus_windows_changed();
        }
        break;
    }
}

/* WaitNextEvent exists if its trap isn't the unimplemented one. */
static int trap_exists(unsigned short trap)
{
    return NGetTrapAddress(trap, kToolboxTrapType) !=
           NGetTrapAddress(0xA89F, kToolboxTrapType);
}

static void open_startup_files(void)
{
    short message, count, i;
    AppFile f;
    docwin *dw;

    CountAppFiles(&message, &count);
    for (i = 1; i <= count; i++) {
        GetAppFiles(i, &f);
        dw = file_open_wd(f.vRefNum, f.fName);
        if (dw != NULL) {
            ShowWindow((WindowPtr)dw);
            SelectWindow((WindowPtr)dw);
        }
        ClrAppFiles(i);
    }
    if (doc_list == NULL)
        new_document();
    menus_windows_changed();
}

int main(void)
{
    EventRecord e;
    int i, got;
    docwin *dw;

    MaxApplZone();
    for (i = 0; i < 8; i++)
        MoreMasters();
    InitGraf(&qd.thePort);
    InitFonts();
    InitWindows();
    InitMenus();
    TEInit();
    InitDialogs(NULL);
    InitCursor();
    FlushEvents(everyEvent, 0);

    have_wne = trap_exists(0xA860);
    file_io_init();
    lines_init(&cutbuf);
    prefs_load(&app_prefs);
    SetPort(qd.thePort);
    menus_init();
    open_startup_files();

    while (!quitting) {
        if (have_wne) {
            got = WaitNextEvent(everyEvent, &e, 6, NULL);
        } else {
            SystemTask();
            got = GetNextEvent(everyEvent, &e);
        }
        if (got)
            handle_event(&e);
        dw = front_doc();
        if (dw != NULL)
            win_idle(dw);
    }
    return 0;
}
