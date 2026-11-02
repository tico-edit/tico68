/*
 * prefsfile.c - the preferences file, and the dialog that edits it
 *
 * The settings live in a small text file: in the Preferences folder
 * where FindFolder exists, otherwise in the System Folder.  Nothing
 * here is fatal: without a readable file the defaults stand.
 */

#include <string.h>
#include "mac.h"

#define PREFS_NAME "\ptico68 Prefs"

static int have_trap(unsigned short trap, TrapType kind)
{
    return NGetTrapAddress(trap, kind) !=
           NGetTrapAddress(0xA89F, kToolboxTrapType);
}

static int prefs_folder(short *vref, long *dir)
{
    SysEnvRec env;
    long response;

    if (have_trap(0xA1AD, kOSTrapType) &&
        Gestalt(gestaltFindFolderAttr, &response) == noErr &&
        (response & 1) &&
        FindFolder(-32768 /* kOnSystemDisk */, kPreferencesFolderType, 1, vref,
                   dir) == noErr)
        return 1;
    if (SysEnvirons(1, &env) != noErr)
        return 0;
    return file_wd_location(env.sysVRefNum, vref, dir);
}

void prefs_load(prefs *p)
{
    HParamBlockRec pb;
    ParamBlockRec io;
    short vref;
    long dir;
    char text[256];

    prefs_default(p);
    if (!prefs_folder(&vref, &dir))
        return;
    memset(&pb, 0, sizeof pb);
    pb.ioParam.ioNamePtr = (StringPtr)PREFS_NAME;
    pb.ioParam.ioVRefNum = vref;
    pb.ioParam.ioPermssn = 1;
    pb.fileParam.ioDirID = dir;
    if (PBHOpenSync(&pb) != noErr)
        return;
    memset(&io, 0, sizeof io);
    io.ioParam.ioRefNum = pb.ioParam.ioRefNum;
    io.ioParam.ioBuffer = text;
    io.ioParam.ioReqCount = sizeof text;
    io.ioParam.ioPosMode = fsAtMark;
    PBReadSync(&io);
    prefs_parse(p, text, io.ioParam.ioActCount);
    PBCloseSync(&io);
}

void prefs_store(const prefs *p)
{
    HParamBlockRec pb;
    ParamBlockRec io;
    short vref;
    long dir;
    char text[256];
    int n = prefs_format(p, text, sizeof text);

    if (n == 0 || !prefs_folder(&vref, &dir))
        return;
    memset(&pb, 0, sizeof pb);
    pb.fileParam.ioNamePtr = (StringPtr)PREFS_NAME;
    pb.fileParam.ioVRefNum = vref;
    pb.fileParam.ioDirID = dir;
    if (PBHCreateSync(&pb) == noErr) {
        memset(&pb, 0, sizeof pb);
        pb.fileParam.ioNamePtr = (StringPtr)PREFS_NAME;
        pb.fileParam.ioVRefNum = vref;
        pb.fileParam.ioDirID = dir;
        if (PBHGetFInfoSync(&pb) == noErr) {
            pb.fileParam.ioFlFndrInfo.fdType = 'pref';
            pb.fileParam.ioFlFndrInfo.fdCreator = CREATOR;
            pb.fileParam.ioDirID = dir;
            PBHSetFInfoSync(&pb);
        }
    }
    memset(&pb, 0, sizeof pb);
    pb.ioParam.ioNamePtr = (StringPtr)PREFS_NAME;
    pb.ioParam.ioVRefNum = vref;
    pb.ioParam.ioPermssn = 3;
    pb.fileParam.ioDirID = dir;
    if (PBHOpenSync(&pb) != noErr)
        return;
    memset(&io, 0, sizeof io);
    io.ioParam.ioRefNum = pb.ioParam.ioRefNum;
    io.ioParam.ioBuffer = text;
    io.ioParam.ioReqCount = n;
    io.ioParam.ioPosMode = fsFromStart;
    PBWriteSync(&io);
    io.ioParam.ioMisc = (Ptr)(long)n;
    PBSetEOFSync(&io);
    PBCloseSync(&io);
    memset(&io, 0, sizeof io);
    io.ioParam.ioVRefNum = vref;
    PBFlushVolSync(&io);
}

/* ------------------------------------------------------------------ */
/* the dialog: tico68.r's DLOG 128 */

enum {
    ITEM_OK = 1, ITEM_CANCEL, ITEM_OUTLINE,
    ITEM_KEYS = 5,              /* three radio buttons, after a label */
    ITEM_TITLE = 9,             /* two, after another */
    ITEM_BAR = 12               /* and again */
};

static pascal void outline_item(DialogPtr dlg, short item)
{
    short type;
    Handle h;
    Rect r;

    GetDialogItem(dlg, ITEM_OK, &type, &h, &r);
    InsetRect(&r, -4, -4);
    PenSize(3, 3);
    FrameRoundRect(&r, 16, 16);
    PenNormal();
    (void)item;
}

static void set_group(DialogPtr dlg, int first, int count, int chosen)
{
    short type;
    Handle h;
    Rect r;
    int i;

    for (i = 0; i < count; i++) {
        GetDialogItem(dlg, first + i, &type, &h, &r);
        SetControlValue((ControlHandle)h, i == chosen);
    }
}

void prefs_dialog(void)
{
    DialogPtr dlg = GetNewDialog(128, NULL, (WindowPtr)-1);
    int keys = app_prefs.profile, title = app_prefs.title_bar;
    int bar = app_prefs.shortcut_bar, changed;
    short item = 0, type;
    Handle h;
    Rect r;

    if (dlg == NULL)
        return;
    GetDialogItem(dlg, ITEM_OUTLINE, &type, &h, &r);
    SetDialogItem(dlg, ITEM_OUTLINE, type, (Handle)outline_item, &r);
    set_group(dlg, ITEM_KEYS, PROFILE_COUNT, keys);
    set_group(dlg, ITEM_TITLE, BAR_COUNT, title);
    set_group(dlg, ITEM_BAR, BAR_COUNT, bar);
    ShowWindow(dlg);
    while (item != ITEM_OK && item != ITEM_CANCEL) {
        ModalDialog(NULL, &item);
        if (item >= ITEM_KEYS && item < ITEM_KEYS + PROFILE_COUNT) {
            keys = item - ITEM_KEYS;
            set_group(dlg, ITEM_KEYS, PROFILE_COUNT, keys);
        } else if (item >= ITEM_TITLE && item < ITEM_TITLE + BAR_COUNT) {
            title = item - ITEM_TITLE;
            set_group(dlg, ITEM_TITLE, BAR_COUNT, title);
        } else if (item >= ITEM_BAR && item < ITEM_BAR + BAR_COUNT) {
            bar = item - ITEM_BAR;
            set_group(dlg, ITEM_BAR, BAR_COUNT, bar);
        }
    }
    DisposeDialog(dlg);
    if (item != ITEM_OK)
        return;
    changed = keys != app_prefs.profile || title != app_prefs.title_bar ||
              bar != app_prefs.shortcut_bar;
    app_prefs.profile = keys;
    app_prefs.title_bar = title;
    app_prefs.shortcut_bar = bar;
    if (changed) {
        prefs_store(&app_prefs);
        menus_profile_changed();
        fonts_changed();
    }
}
