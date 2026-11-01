/*
 * bars.c - tico's title and shortcut bars, as text
 *
 * What to show and whether to show it; src/mac draws the result.
 * Portable C.
 */

#include <string.h>
#include "tico.h"

/*
 * Whether a bar shows, given its setting, whether it showed last time,
 * the height (pixels) available to text with this bar off, the bar's own
 * height, and the document font's line height.  Automatic shows a bar
 * when it would still leave plenty of rows, and goes by rows rather than
 * by screen: a big font on a big screen has them to spare no more than a
 * Plus does.  The two thresholds keep it from flickering as a window is
 * resized around one row.
 */
int bar_visible(int setting, int was_on, int height, int bar_height,
                int line_height)
{
    int rows;

    if (setting == BAR_SHOW)
        return 1;
    if (setting == BAR_HIDE)
        return 0;
    rows = (height - bar_height) / line_height;
    return rows >= (was_on ? BAR_ROWS_HIDE : BAR_ROWS_SHOW);
}

/* tico's title_bar_text, for one buffer; the version is tico68's. */
void bar_title(char *out, int cols, const char *name, int modified)
{
    char upperleft[24];
    const char *state = modified ? "Modified" : "";
    int pluglen = modified ? 0 : 9;
    int verlen, pathlen, pathwidth, statelen, offset, keep, n;

    strcpy(upperleft, "tico68 " VERSION);
    memset(out, ' ', cols);
    verlen = (int)strlen(upperleft) + 3;
    pathlen = pathwidth = (int)strlen(name);
    statelen = (int)strlen(state) + 2;
    if (statelen > 2)
        pathlen++;
    if (verlen + pathlen + pluglen + statelen <= cols) {
        memcpy(out + 2, upperleft, verlen - 3);
    } else {
        verlen = 2;
        if (verlen + pathlen + pluglen + statelen > cols)
            pluglen = 0;
        if (verlen + pathlen + pluglen + statelen > cols) {
            verlen = 0;
            statelen -= 2;
        }
    }
    offset = 0;
    if (verlen > 0) {
        n = cols - (verlen + pluglen + statelen + pathlen);
        offset = verlen + (n > 0 ? n / 2 : 0);
    }
    if (pathlen + pluglen + statelen <= cols) {
        n = pathwidth;
        if (offset + n > cols)
            n = cols - offset;
        memcpy(out + offset, name, n);
    } else if (5 + statelen <= cols) {
        keep = pathwidth - (3 + pathlen + statelen - cols);
        if (keep < 0)
            keep = 0;
        memcpy(out + offset, "...", 3);
        n = keep;
        if (offset + 3 + n > cols)
            n = cols - offset - 3;
        if (n > 0)
            memcpy(out + offset + 3, name + pathwidth - keep, n);
    }
    if (statelen > 2 && statelen <= cols)
        memcpy(out + cols - statelen, state, strlen(state));
}

typedef struct {
    int cmd;
    const char *desc;
} shortcut;

/* tico's SHORTCUT_PRIORITY order, for commands tico68 has */
static const shortcut tico_keys[] = {
    {CMD_EXIT, "Exit"}, {CMD_SAVE_AS, "Write Out"}, {CMD_CUT_LINE, "Cut"},
    {CMD_PASTE_LINE, "Paste"}, {CMD_LOCATION, "Location"},
    {CMD_COPY_LINE, "Copy"}, {CMD_OPEN, "Read File"}, {CMD_SAVE, "Save"},
};
static const shortcut mac_keys[] = {
    {CMD_CLOSE, "Close"}, {CMD_SAVE, "Save"}, {CMD_CUT, "Cut"},
    {CMD_PASTE, "Paste"}, {CMD_LOCATION, "Location"}, {CMD_COPY, "Copy"},
    {CMD_OPEN, "Open"}, {CMD_QUIT, "Quit"},
};

/*
 * tico's shortcut_bar_layout: two rows of key and description, in
 * columns of equal width.  Only as many columns as fit are used, so a
 * narrow window shows fewer shortcuts rather than clipped ones.
 */
int bar_shortcuts(int profile, int cols, bar_item *items)
{
    const shortcut *keys = profile == PROFILE_MAC ? mac_keys : tico_keys;
    int count = 0, maxk = 0, maxd = 0, width, pairs, shown, i, k, l;
    char *end;

    for (i = 0; i < BAR_MAX; i++) {
        k = key_for_cmd(profile, keys[i].cmd);
        if (k == 0)
            continue;
        end = key_label(items[count].key, k);
        l = (int)(end - items[count].key);
        items[count].keylen = l;
        items[count].desc = keys[i].desc;
        if (l > maxk)
            maxk = l;
        l = (int)strlen(keys[i].desc);
        if (l > maxd)
            maxd = l;
        count++;
    }
    width = maxk + 1 + maxd + 2;
    pairs = cols / width;
    if (pairs > (count + 1) / 2)
        pairs = (count + 1) / 2;
    shown = pairs * 2 < count ? pairs * 2 : count;
    for (i = 0; i < shown; i++) {
        items[i].row = i % 2;
        items[i].col = (i / 2) * width;
    }
    return shown;
}
