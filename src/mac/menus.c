/*
 * menus.c - the menu bar
 *
 * Menus and keys both end up in app_command.  Keys are not given to
 * MenuKey: the active profile decides what a key does, so the Command
 * letters here are only labels, and they change with the profile.  A
 * Command-V menu item can't appear in legacy mode, where that key is
 * Page Down.
 *
 * Text Format is a run of items in the File menu rather than a submenu:
 * the Plus's ROM has no hierarchical menus to rely on.
 */

#include <string.h>
#include "mac.h"

enum {
    MENU_APPLE = 128, MENU_FILE, MENU_EDIT, MENU_GO, MENU_FONT, MENU_WINDOW
};

#define HEADER (-1)     /* a label, never enabled */

typedef struct {
    short menu;
    const char *name;
    short cmd;
} mitem;

/* Standard Edit menu order: SystemEdit counts on it for desk accessories */
static const mitem items[] = {
    {MENU_APPLE, "About tico68\311", CMD_ABOUT},

    {MENU_FILE, "New", CMD_NEW},
    {MENU_FILE, "Open\311", CMD_OPEN},
    {MENU_FILE, "-", 0},
    {MENU_FILE, "Close", CMD_CLOSE},
    {MENU_FILE, "Save", CMD_SAVE},
    {MENU_FILE, "Save As\311", CMD_SAVE_AS},
    {MENU_FILE, "-", 0},
    {MENU_FILE, "Text Format:", HEADER},
    {MENU_FILE, "   Macintosh (CR)", CMD_FORMAT_MAC},
    {MENU_FILE, "   DOS / Windows (CRLF)", CMD_FORMAT_DOS},
    {MENU_FILE, "   Unix (LF)", CMD_FORMAT_UNIX},
    {MENU_FILE, "-", 0},
    {MENU_FILE, "Quit", CMD_QUIT},

    {MENU_EDIT, "Undo", CMD_UNDO},
    {MENU_EDIT, "-", 0},
    {MENU_EDIT, "Cut", CMD_CUT},
    {MENU_EDIT, "Copy", CMD_COPY},
    {MENU_EDIT, "Paste", CMD_PASTE},
    {MENU_EDIT, "Clear", CMD_CLEAR},
    {MENU_EDIT, "Select All", CMD_SELECT_ALL},
    {MENU_EDIT, "-", 0},
    {MENU_EDIT, "Cut Line", CMD_CUT_LINE},
    {MENU_EDIT, "Copy Line", CMD_COPY_LINE},
    {MENU_EDIT, "Paste Line", CMD_PASTE_LINE},
    {MENU_EDIT, "Set Mark", CMD_MARK},
    {MENU_EDIT, "-", 0},
    {MENU_EDIT, "Location", CMD_LOCATION},
    {MENU_EDIT, "Preferences\311", CMD_PREFS},

    {MENU_GO, "Page Up", CMD_PAGE_UP},
    {MENU_GO, "Page Down", CMD_PAGE_DOWN},
    {MENU_GO, "-", 0},
    {MENU_GO, "Start of Line", CMD_HOME},
    {MENU_GO, "End of Line", CMD_END},
    {MENU_GO, "First Line", CMD_FIRST_LINE},
    {MENU_GO, "Last Line", CMD_LAST_LINE},
    {MENU_GO, "-", 0},
    {MENU_GO, "Previous Word", CMD_PREV_WORD},
    {MENU_GO, "Next Word", CMD_NEXT_WORD},
};
#define ITEM_COUNT ((int)(sizeof items / sizeof items[0]))

static MenuHandle apple_menu, font_menu, window_menu;
static const short sizes[] = {9, 10, 12, 14, 18, 24};
#define SIZE_COUNT ((int)(sizeof sizes / sizeof sizes[0]))
static int font_count;

static void to_pascal(Str255 out, const char *in)
{
    int n = (int)strlen(in);

    out[0] = (unsigned char)n;
    memcpy(out + 1, in, n);
}

/* Set an item's text literally: AppendMenu would act on ; ^ ! < / ( */
static void append_literal(MenuHandle m, ConstStr255Param text)
{
    AppendMenu(m, "\p ");
    SetMenuItemText(m, CountMItems(m), text);
}

static int item_number(int index)
{
    int i, n = 0;

    for (i = 0; i <= index; i++)
        if (items[i].menu == items[index].menu)
            n++;
    /* About is the first of the Apple menu */
    return n;
}

static void build_font_menu(void)
{
    MenuHandle all = NewMenu(250, "\pall");
    short n, i, id;
    Str255 name;
    GrafPtr port;

    /* the list of fonts, then only those of fixed width */
    AppendResMenu(all, 'FONT');
    n = CountMItems(all);
    GetPort(&port);
    for (i = 1; i <= n; i++) {
        GetMenuItemText(all, i, name);
        GetFNum(name, &id);
        if (font_is_fixed(id))
            append_literal(font_menu, name);
    }
    DisposeMenu(all);
    font_count = CountMItems(font_menu);
    AppendMenu(font_menu, "\p(-");
    for (i = 0; i < SIZE_COUNT; i++) {
        NumToString(sizes[i], name);
        name[++name[0]] = ' ';
        name[++name[0]] = 'p';
        name[++name[0]] = 't';
        append_literal(font_menu, name);
    }
}

void menus_init(void)
{
    MenuHandle m;
    Str255 text;
    int i;

    apple_menu = NewMenu(MENU_APPLE, "\p\024");
    InsertMenu(apple_menu, 0);
    InsertMenu(NewMenu(MENU_FILE, "\pFile"), 0);
    InsertMenu(NewMenu(MENU_EDIT, "\pEdit"), 0);
    InsertMenu(NewMenu(MENU_GO, "\pGo"), 0);
    font_menu = NewMenu(MENU_FONT, "\pFont");
    InsertMenu(font_menu, 0);
    window_menu = NewMenu(MENU_WINDOW, "\pWindow");
    InsertMenu(window_menu, 0);

    for (i = 0; i < ITEM_COUNT; i++) {
        m = GetMenuHandle(items[i].menu);
        if (items[i].name[0] == '-') {
            AppendMenu(m, "\p(-");
        } else {
            to_pascal(text, items[i].name);
            append_literal(m, text);
        }
    }
    AppendMenu(apple_menu, "\p(-");
    AppendResMenu(apple_menu, 'DRVR');
    build_font_menu();
    menus_profile_changed();
    DrawMenuBar();
}

/* Labels follow the profile. */
void menus_profile_changed(void)
{
    MenuHandle m;
    Str255 text;
    char label[64], *p;
    int i, key, base;
    int simple;

    for (i = 0; i < ITEM_COUNT; i++) {
        if (items[i].cmd <= 0)
            continue;
        m = GetMenuHandle(items[i].menu);
        key = key_for_cmd(app_prefs.profile, items[i].cmd);
        base = KEY_BASE(key);
        simple = key != 0 && base >= KEY_CMD(0) && base < KEY_META(0) &&
                 (key & KF_SHIFT) == 0;
        p = put_s(label, items[i].name);
        if (key != 0 && !simple) {
            p = put_s(p, "  ");
            key_label(p, key);
        }
        to_pascal(text, label);
        SetMenuItemText(m, item_number(i), text);
        SetItemCmd(m, item_number(i), simple ? (short)(base & 0xFF) : 0);
    }
}

static void set_enabled(MenuHandle m, int item, int on)
{
    if (on)
        EnableItem(m, item);
    else
        DisableItem(m, item);
}

static int enabled_now(int cmd, docwin *dw)
{
    switch (cmd) {
    case CMD_NEW:
    case CMD_OPEN:
    case CMD_QUIT:
    case CMD_ABOUT:
    case CMD_PREFS:
        return 1;
    case CMD_UNDO:
        return 0;
    case CMD_CUT:
    case CMD_COPY:
    case CMD_CLEAR:
        return dw != NULL && doc_has_selection(&dw->doc);
    case CMD_PASTE_LINE:
        return dw != NULL && cutbuf.top != NULL;
    default:
        return dw != NULL;
    }
}

/* Before the menus are pulled down. */
void menus_update(void)
{
    docwin *dw = front_doc();
    MenuHandle m;
    int i, n, cmd;
    short fmt = dw != NULL ? dw->doc.format : 0;
    Str255 name;
    char want[FONT_NAME_MAX + 1];

    for (i = 0; i < ITEM_COUNT; i++) {
        cmd = items[i].cmd;
        if (cmd == 0)
            continue;
        m = GetMenuHandle(items[i].menu);
        n = item_number(i);
        set_enabled(m, n, cmd != HEADER && enabled_now(cmd, dw));
        if (cmd == CMD_FORMAT_MAC)
            CheckItem(m, n, fmt == FMT_MAC);
        else if (cmd == CMD_FORMAT_DOS)
            CheckItem(m, n, fmt == FMT_DOS);
        else if (cmd == CMD_FORMAT_UNIX)
            CheckItem(m, n, fmt == FMT_UNIX);
    }
    for (i = 1; i <= font_count; i++) {
        GetMenuItemText(font_menu, i, name);
        memcpy(want, name + 1, name[0]);
        want[name[0]] = 0;
        CheckItem(font_menu, i, strcmp(want, app_prefs.font) == 0);
    }
    for (i = 0; i < SIZE_COUNT; i++)
        CheckItem(font_menu, font_count + 2 + i, sizes[i] == app_prefs.size);
    menus_windows_changed();
}

/* The Window menu, rebuilt: oldest document first. */
void menus_windows_changed(void)
{
    docwin *dw, *front = front_doc();
    int i, n = 0;
    short count = CountMItems(window_menu);
    docwin *list[64];

    for (i = count; i > 0; i--)
        DeleteMenuItem(window_menu, i);
    for (dw = doc_list; dw != NULL && n < 64; dw = dw->next)
        list[n++] = dw;
    for (i = n - 1; i >= 0; i--) {
        append_literal(window_menu, list[i]->name);
        if (list[i] == front)
            CheckItem(window_menu, n - i, 1);
    }
}

int menu_of_cmd(int cmd)
{
    int i;

    for (i = 0; i < ITEM_COUNT; i++)
        if (items[i].cmd == cmd)
            return items[i].menu;
    return 0;
}

static void choose_font(int item)
{
    Str255 name;

    if (item <= font_count) {
        GetMenuItemText(font_menu, item, name);
        if (name[0] >= FONT_NAME_MAX)
            return;
        memcpy(app_prefs.font, name + 1, name[0]);
        app_prefs.font[name[0]] = 0;
    } else {
        app_prefs.size = sizes[item - font_count - 2];
    }
    fonts_changed();
    prefs_store(&app_prefs);
}

static void choose_window(int item)
{
    docwin *dw;
    int n = 0, target;

    for (dw = doc_list; dw != NULL; dw = dw->next)
        n++;
    target = n - item;
    for (dw = doc_list; dw != NULL && target > 0; dw = dw->next)
        target--;
    if (dw != NULL)
        SelectWindow((WindowPtr)dw);
}

void menu_choice(long choice)
{
    int menu = HiWord(choice), item = LoWord(choice), i;
    Str255 name;
    GrafPtr port;

    if (menu == 0)
        return;
    if (menu == MENU_APPLE && item > 1) {
        GetPort(&port);
        GetMenuItemText(apple_menu, item, name);
        OpenDeskAcc(name);
        SetPort(port);
    } else if (menu == MENU_FONT) {
        choose_font(item);
    } else if (menu == MENU_WINDOW) {
        choose_window(item);
    } else if (menu == MENU_EDIT && item <= 6 &&
               SystemEdit(item - 1)) {
        /* a desk accessory took it */
    } else {
        for (i = 0; i < ITEM_COUNT; i++)
            if (items[i].menu == menu && item_number(i) == item) {
                app_command(items[i].cmd, 0);
                break;
            }
    }
    HiliteMenu(0);
}
