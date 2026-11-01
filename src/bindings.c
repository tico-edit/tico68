/*
 * bindings.c - keys to commands, one table per profile
 *
 * Every profile lands on the same commands.  The legacy and modern
 * tables come from tico's src/keymap.rs (install_main_defaults and
 * install_modern_overrides); see docs/KEYBINDINGS.md.  Only commands
 * that exist are listed.  CMD_NONE in the modern table means tico
 * moved that key to a command tico68 doesn't have, so the legacy
 * binding must not show through.  Portable C.
 */

#include "tico.h"

typedef struct {
    int key, cmd;
} bind;

/* Ways to reach what the Plus keyboard has no keys for, in every profile
 * that shares this table: Option with the arrows. */
#define OPT_ARROWS \
    {KEY_LEFT | KF_OPT, CMD_HOME}, {KEY_RIGHT | KF_OPT, CMD_END}, \
    {KEY_UP | KF_OPT, CMD_PAGE_UP}, {KEY_DOWN | KF_OPT, CMD_PAGE_DOWN}

/* Keys that do the same everywhere. */
static const bind common_map[] = {
    {KEY_UP, CMD_UP}, {KEY_DOWN, CMD_DOWN},
    {KEY_LEFT, CMD_LEFT}, {KEY_RIGHT, CMD_RIGHT},
    {KEY_HOME, CMD_HOME}, {KEY_END, CMD_END},
    {KEY_PGUP, CMD_PAGE_UP}, {KEY_PGDN, CMD_PAGE_DOWN},
    {KEY_RETURN, CMD_ENTER}, {KEY_ENTER, CMD_ENTER},
    {KEY_TAB, CMD_TAB}, {KEY_BSP, CMD_BACKSPACE}, {KEY_DEL, CMD_DELETE},
    {0, 0}
};

static const bind legacy_map[] = {
    {KEY_CMD('X'), CMD_EXIT}, {KEY_F2, CMD_EXIT},
    {KEY_CMD('O'), CMD_SAVE_AS}, {KEY_F3, CMD_SAVE_AS},
    {KEY_CMD('S'), CMD_SAVE},
    {KEY_CMD('R'), CMD_OPEN},
    {KEY_CMD('K'), CMD_CUT_LINE}, {KEY_F9, CMD_CUT_LINE},
    {KEY_CMD('U'), CMD_PASTE_LINE}, {KEY_F10, CMD_PASTE_LINE},
    {KEY_META('6'), CMD_COPY_LINE}, {KEY_META('^'), CMD_COPY_LINE},
    {KEY_META('A'), CMD_MARK}, {KEY_CMD('6'), CMD_MARK},
    {KEY_CMD('^'), CMD_MARK},
    {KEY_CMD('C'), CMD_LOCATION}, {KEY_F11, CMD_LOCATION},
    {KEY_CMD('P'), CMD_UP}, {KEY_CMD('N'), CMD_DOWN},
    {KEY_CMD('A'), CMD_HOME}, {KEY_CMD('E'), CMD_END},
    {KEY_CMD('Y'), CMD_PAGE_UP}, {KEY_CMD('V'), CMD_PAGE_DOWN},
    {KEY_META('\\'), CMD_FIRST_LINE}, {KEY_HOME | KF_CMD, CMD_FIRST_LINE},
    {KEY_META('/'), CMD_LAST_LINE}, {KEY_END | KF_CMD, CMD_LAST_LINE},
    {KEY_LEFT | KF_CMD, CMD_PREV_WORD}, {KEY_META(' '), CMD_PREV_WORD},
    {KEY_RIGHT | KF_CMD, CMD_NEXT_WORD}, {KEY_CMD(' '), CMD_NEXT_WORD},
    {KEY_LEFT | KF_META, CMD_PREV_WINDOW}, {KEY_META(','), CMD_PREV_WINDOW},
    {KEY_META('<'), CMD_PREV_WINDOW},
    {KEY_RIGHT | KF_META, CMD_NEXT_WINDOW}, {KEY_META('.'), CMD_NEXT_WINDOW},
    {KEY_META('>'), CMD_NEXT_WINDOW},
    {KEY_CMD('D'), CMD_DELETE}, {KEY_CMD('H'), CMD_BACKSPACE},
    {KEY_CMD('I'), CMD_TAB}, {KEY_CMD('M'), CMD_ENTER},
    OPT_ARROWS,
    {0, 0}
};

/* install_modern_overrides; searched before legacy_map. */
static const bind modern_map[] = {
    {KEY_CMD('Q'), CMD_EXIT},
    {KEY_CMD('W'), CMD_SAVE_AS},
    {KEY_CMD('O'), CMD_OPEN},
    {KEY_CMD('P'), CMD_LOCATION},
    {KEY_CMD('A'), CMD_MARK},
    {KEY_CMD('X'), CMD_CUT_LINE},
    {KEY_CMD('C'), CMD_COPY_LINE},
    {KEY_CMD('V'), CMD_PASTE_LINE},
    /* Help, Find Next, Find Previous, Replace, Go To Line, Undo, Redo,
     * Execute: all to come */
    {KEY_CMD('H'), CMD_NONE}, {KEY_CMD('G'), CMD_NONE},
    {KEY_CMD('D'), CMD_NONE}, {KEY_CMD('R'), CMD_NONE},
    {KEY_CMD('T'), CMD_NONE}, {KEY_CMD('Z'), CMD_NONE},
    {KEY_CMD('Y'), CMD_NONE}, {KEY_CMD('E'), CMD_NONE},
    {0, 0}
};

static const bind mac_map[] = {
    {KEY_CMD('N'), CMD_NEW},
    {KEY_CMD('O'), CMD_OPEN},
    {KEY_CMD('W'), CMD_CLOSE},
    {KEY_CMD('S'), CMD_SAVE},
    {KEY_CMD('S') | KF_SHIFT, CMD_SAVE_AS},
    {KEY_CMD('Q'), CMD_QUIT},
    {KEY_CMD('X'), CMD_CUT}, {KEY_F2, CMD_CUT},
    {KEY_CMD('C'), CMD_COPY}, {KEY_F3, CMD_COPY},
    {KEY_CMD('V'), CMD_PASTE}, {KEY_F4, CMD_PASTE},
    {KEY_CMD('A'), CMD_SELECT_ALL},
    {KEY_CMD('K'), CMD_CUT_LINE},
    {KEY_CMD('K') | KF_SHIFT, CMD_COPY_LINE},
    {KEY_CMD('U'), CMD_PASTE_LINE},
    {KEY_CMD('L'), CMD_LOCATION},
    {KEY_CMD('`'), CMD_NEXT_WINDOW},
    {KEY_LEFT | KF_CMD, CMD_HOME}, {KEY_RIGHT | KF_CMD, CMD_END},
    {KEY_UP | KF_CMD, CMD_FIRST_LINE}, {KEY_DOWN | KF_CMD, CMD_LAST_LINE},
    {KEY_LEFT | KF_OPT, CMD_PREV_WORD}, {KEY_RIGHT | KF_OPT, CMD_NEXT_WORD},
    {KEY_UP | KF_OPT, CMD_PAGE_UP}, {KEY_DOWN | KF_OPT, CMD_PAGE_DOWN},
    {KEY_HOME | KF_CMD, CMD_FIRST_LINE}, {KEY_END | KF_CMD, CMD_LAST_LINE},
    {0, 0}
};

/* What Command-letter gives with Option held (the character code would
 * be the Option glyph), or under Control: virtual key to unshifted
 * character, for the US layout. */
static const char vkey_chars[] =
    "asdfhgzxcv\0bqweryt123465=97-80]ou[ip\0lj'k;\\,/nm.\0 `";
static const char shift_digits[] = "!@#$%^&*()";

static int shifted(int c)
{
    const char *p;

    if (c >= 'a' && c <= 'z')
        return c - 'a' + 'A';
    switch (c) {
    case '0':
        return ')';
    case '-':
        return '_';
    case '=':
        return '+';
    case '[':
        return '{';
    case ']':
        return '}';
    case '\\':
        return '|';
    case ';':
        return ':';
    case '\'':
        return '"';
    case ',':
        return '<';
    case '.':
        return '>';
    case '/':
        return '?';
    case '`':
        return '~';
    }
    if (c >= '1' && c <= '9') {
        p = shift_digits + (c - '1');
        return *p;
    }
    return c;
}

static int upper(int c)
{
    return c >= 'a' && c <= 'z' ? c - 'a' + 'A' : c;
}

static int function_key(unsigned char vkey)
{
    switch (vkey) {
    case 0x7A:
        return KEY_F1;
    case 0x78:
        return KEY_F2;
    case 0x63:
        return KEY_F3;
    case 0x76:
        return KEY_F4;
    case 0x60:
        return KEY_F5;
    case 0x61:
        return KEY_F6;
    case 0x62:
        return KEY_F7;
    case 0x64:
        return KEY_F8;
    case 0x65:
        return KEY_F9;
    case 0x6D:
        return KEY_F10;
    case 0x67:
        return KEY_F11;
    case 0x6F:
        return KEY_F12;
    }
    return 0;
}

/* The key a character code names by itself, or 0. */
static int special_key(unsigned char c, unsigned char vkey)
{
    switch (c) {
    case 0x1C:
        return KEY_LEFT;
    case 0x1D:
        return KEY_RIGHT;
    case 0x1E:
        return KEY_UP;
    case 0x1F:
        return KEY_DOWN;
    case 0x01:
        return KEY_HOME;
    case 0x04:
        return KEY_END;
    case 0x0B:
        return KEY_PGUP;
    case 0x0C:
        return KEY_PGDN;
    case 0x7F:
        return KEY_DEL;
    case 0x08:
        return KEY_BSP;
    case 0x09:
        return KEY_TAB;
    case 0x0D:
        return KEY_RETURN;
    case 0x03:
        return KEY_ENTER;
    case 0x1B:
        return KEY_ESC;         /* Esc, and Clear on a Plus keyboard */
    case 0x10:
        return function_key(vkey);
    }
    return 0;
}

/*
 * The key for a Toolbox keyDown: character code, virtual key code,
 * and MOD_ flags.  Command is tico's Ctrl, as is a real Control key on
 * keyboards that have one; Command-Option is Meta.  Plain Option is left
 * to the keyboard layout, for typing MacRoman.
 */
int key_from_event(unsigned char c, unsigned char vkey, int mods)
{
    int cmd = (mods & (MOD_CMD | MOD_CTRL)) != 0;
    int meta = cmd && (mods & MOD_OPT);
    int flags = 0, k = 0, ch;

    /* a Control character code is the letter's, not Home or Page Up */
    if (!(mods & MOD_CTRL) || vkey >= sizeof vkey_chars ||
        vkey_chars[vkey] == 0)
        k = special_key(c, vkey);
    if (k != 0) {
        if (cmd)
            flags |= KF_CMD;
        if (mods & MOD_OPT)
            flags |= KF_OPT;
        if (mods & MOD_SHIFT)
            flags |= KF_SHIFT;
        return k | flags;
    }
    if (!cmd)
        return c;
    if (meta || (mods & MOD_CTRL)) {
        ch = vkey < sizeof vkey_chars ? vkey_chars[vkey] : 0;
        if (ch == 0)
            ch = c;
        else if (mods & MOD_SHIFT)
            ch = shifted(ch);
    } else {
        ch = c;
    }
    ch = upper(ch);
    k = meta ? KEY_META(ch) : KEY_CMD(ch);
    /* letters report Shift by being letters; keep it for Mac bindings */
    if (mods & MOD_SHIFT)
        k |= KF_SHIFT;
    return k;
}

/* The key Esc makes of the one after it: tico's Meta. */
int key_after_esc(int key)
{
    if (key < 0x100)
        return KEY_META(upper(key));
    if (KEY_BASE(key) >= KEY_SPECIAL)
        return key | KF_META;
    return key;
}

static const bind *find(const bind *map, int key)
{
    for (; map->key != 0; map++)
        if (map->key == key)
            return map;
    return NULL;
}

/* Tables for a profile, in search order. */
static int profile_maps(int profile, const bind *maps[3])
{
    int n = 0;

    if (profile == PROFILE_MODERN)
        maps[n++] = modern_map;
    if (profile == PROFILE_MAC)
        maps[n++] = mac_map;
    else
        maps[n++] = legacy_map;
    maps[n++] = common_map;
    return n;
}

static const bind *lookup(int profile, int key)
{
    const bind *maps[3], *b;
    int i, n = profile_maps(profile, maps);

    for (i = 0; i < n; i++) {
        b = find(maps[i], key);
        if (b != NULL)
            return b;
    }
    return NULL;
}

static int is_movement(int cmd)
{
    return cmd >= CMD_UP && cmd <= CMD_NEXT_WORD;
}

binding key_lookup(int profile, int key)
{
    binding r;
    const bind *b;
    int plain = key & ~KF_SHIFT;

    r.cmd = CMD_NONE;
    r.extend = 0;
    r.insert = -1;
    if (key >= 0 && key < 0x100) {
        r.insert = key;
        return r;
    }
    b = lookup(profile, key);
    if (b == NULL && plain != key) {
        b = lookup(profile, plain);
        if (b != NULL && KEY_BASE(plain) >= KEY_SPECIAL && is_movement(b->cmd))
            r.extend = 1;
    }
    if (b != NULL)
        r.cmd = b->cmd;
    return r;
}

/*
 * The key to show for a command under a profile, or 0.  Tables list a
 * command's main key first.  Exit counts as Close, which it is but for
 * also leaving after the last document.
 */
int key_for_cmd(int profile, int cmd)
{
    const bind *maps[3], *b;
    int i, n = profile_maps(profile, maps);

    for (i = 0; i < n; i++)
        for (b = maps[i]; b->key != 0; b++)
            if ((b->cmd == cmd || (cmd == CMD_CLOSE && b->cmd == CMD_EXIT)) &&
                lookup(profile, b->key) == b)
                return b->key;
    return 0;
}

/* A key as menus and the shortcut bar show it: 0x11 is the Command sign. */
char *key_label(char *d, int key)
{
    static const char *const names[] = {
        "Up", "Down", "Left", "Right", "Home", "End", "PgUp", "PgDn",
        "Del", "Bsp", "Tab", "Return", "Enter", "Esc",
        "F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8",
        "F9", "F10", "F11", "F12"
    };
    int base = KEY_BASE(key);

    *d = 0;
    if (key == 0)
        return d;
    if (key & KF_SHIFT)
        d = put_s(d, "Sh-");
    if (base >= KEY_SPECIAL) {
        if ((key & KF_META) == KF_META)
            d = put_s(d, "M-");
        else if (key & KF_CMD)
            d = put_s(d, "\021");
        else if (key & KF_OPT)
            d = put_s(d, "Opt-");
        return put_s(d, names[base - KEY_SPECIAL]);
    }
    d = put_s(d, base >= KEY_META(0) ? "M-" : "\021");
    if ((base & 0xFF) == ' ')
        return put_s(d, "Space");
    *d++ = (char)(base & 0xFF);
    *d = 0;
    return d;
}

const char *profile_name(int profile)
{
    switch (profile) {
    case PROFILE_LEGACY:
        return "legacy";
    case PROFILE_MODERN:
        return "modern";
    default:
        return "macintosh";
    }
}
