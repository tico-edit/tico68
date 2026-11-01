/*
 * files.c - opening and saving, through the File Manager
 *
 * Files are named by volume, directory ID and name; Standard File's
 * working directories are turned into those at once.  The parameter-
 * block calls are used directly: they are what System 6 has, and they
 * work on every volume format.  save.c decides the order of events;
 * this is its file system.
 */

#include <string.h>
#include "mac.h"

#define IOBUF 4096

#ifndef fsRdPerm
#define fsRdPerm 1
#endif
#ifndef fsRdWrPerm
#define fsRdWrPerm 3
#endif

static char *iobuf;

void file_io_init(void)
{
    iobuf = NewPtr(IOBUF);
}

static OSErr last_err;

static OSErr h_open(short vref, long dir, ConstStr255Param name, int perm,
                    short *ref)
{
    HParamBlockRec pb;
    OSErr err;

    memset(&pb, 0, sizeof pb);
    pb.ioParam.ioNamePtr = (StringPtr)name;
    pb.ioParam.ioVRefNum = vref;
    pb.ioParam.ioPermssn = perm;
    pb.fileParam.ioDirID = dir;
    err = PBHOpenSync(&pb);
    *ref = pb.ioParam.ioRefNum;
    return err;
}

static OSErr h_info(short vref, long dir, ConstStr255Param name,
                    HParamBlockRec *pb)
{
    memset(pb, 0, sizeof *pb);
    pb->fileParam.ioNamePtr = (StringPtr)name;
    pb->fileParam.ioVRefNum = vref;
    pb->fileParam.ioDirID = dir;
    pb->fileParam.ioFDirIndex = 0;
    return PBHGetFInfoSync(pb);
}

static OSErr h_create(short vref, long dir, ConstStr255Param name,
                      OSType type, OSType creator)
{
    HParamBlockRec pb;
    OSErr err;

    memset(&pb, 0, sizeof pb);
    pb.fileParam.ioNamePtr = (StringPtr)name;
    pb.fileParam.ioVRefNum = vref;
    pb.fileParam.ioDirID = dir;
    err = PBHCreateSync(&pb);
    if (err != noErr)
        return err;
    err = h_info(vref, dir, name, &pb);
    if (err != noErr)
        return err;
    pb.fileParam.ioFlFndrInfo.fdType = type;
    pb.fileParam.ioFlFndrInfo.fdCreator = creator;
    pb.fileParam.ioDirID = dir;
    return PBHSetFInfoSync(&pb);
}

static OSErr h_delete(short vref, long dir, ConstStr255Param name)
{
    HParamBlockRec pb;

    memset(&pb, 0, sizeof pb);
    pb.fileParam.ioNamePtr = (StringPtr)name;
    pb.fileParam.ioVRefNum = vref;
    pb.fileParam.ioDirID = dir;
    return PBHDeleteSync(&pb);
}

static OSErr h_rename(short vref, long dir, ConstStr255Param from,
                      ConstStr255Param to)
{
    HParamBlockRec pb;

    memset(&pb, 0, sizeof pb);
    pb.ioParam.ioNamePtr = (StringPtr)from;
    pb.ioParam.ioVRefNum = vref;
    pb.ioParam.ioMisc = (Ptr)to;
    pb.fileParam.ioDirID = dir;
    return PBHRenameSync(&pb);
}

static OSErr f_write(short ref, const char *buf, long len)
{
    ParamBlockRec pb;

    memset(&pb, 0, sizeof pb);
    pb.ioParam.ioRefNum = ref;
    pb.ioParam.ioBuffer = (Ptr)buf;
    pb.ioParam.ioReqCount = len;
    pb.ioParam.ioPosMode = fsAtMark;
    return PBWriteSync(&pb);
}

static OSErr f_close(short ref)
{
    ParamBlockRec pb;

    memset(&pb, 0, sizeof pb);
    pb.ioParam.ioRefNum = ref;
    return PBCloseSync(&pb);
}

static OSErr f_set_eof(short ref, long len)
{
    ParamBlockRec pb;

    memset(&pb, 0, sizeof pb);
    pb.ioParam.ioRefNum = ref;
    pb.ioParam.ioMisc = (Ptr)len;
    return PBSetEOFSync(&pb);
}

static OSErr f_get_eof(short ref, long *len)
{
    ParamBlockRec pb;
    OSErr err;

    memset(&pb, 0, sizeof pb);
    pb.ioParam.ioRefNum = ref;
    err = PBGetEOFSync(&pb);
    *len = (long)pb.ioParam.ioMisc;
    return err;
}

static void flush_volume(short vref)
{
    ParamBlockRec pb;

    memset(&pb, 0, sizeof pb);
    pb.ioParam.ioVRefNum = vref;
    PBFlushVolSync(&pb);
}

int file_wd_location(short wd, short *vref, long *dir)
{
    WDPBRec pb;

    memset(&pb, 0, sizeof pb);
    pb.ioVRefNum = wd;
    pb.ioWDIndex = 0;
    if (PBGetWDInfoSync(&pb) != noErr)
        return 0;
    *vref = pb.ioWDVRefNum;
    *dir = pb.ioWDDirID;
    return 1;
}

/* ------------------------------------------------------------------ */
/* words for the alerts */

static const char *err_text(OSErr err)
{
    switch (err) {
    case dskFulErr:
        return "The disk is full.";
    case wPrErr:
    case vLckdErr:
        return "The disk is locked.";
    case fLckdErr:
        return "The file is locked.";
    case permErr:
    case opWrErr:
    case fBsyErr:
        return "The file is in use.";
    case ioErr:
        return "There was a disk error.";
    case fnfErr:
        return "The file can't be found.";
    case nsvErr:
    case volOffLinErr:
        return "The disk isn't available.";
    case memFullErr:
        return "There isn't enough memory.";
    default:
        return "The disk reported an error.";
    }
}

static void name_text(char *out, ConstStr255Param name)
{
    memcpy(out, name + 1, name[0]);
    out[name[0]] = 0;
}

static void io_error(const char *verb, ConstStr255Param name, OSErr err)
{
    char what[80], *p;

    p = put_s(what, verb);
    p = put_s(p, " \322");
    name_text(p, name);
    p += name[0];
    put_s(p, "\323.");
    app_error(what, err_text(err),
              verb[6] == 's' ? "Your text is safe in its window." : "");
}

/* ------------------------------------------------------------------ */
/* opening */

typedef struct {
    short ref;
} instream;

static int stream_in(void *ctx, char *buf, unsigned cap)
{
    instream *in = ctx;
    ParamBlockRec pb;
    OSErr err;

    memset(&pb, 0, sizeof pb);
    pb.ioParam.ioRefNum = in->ref;
    pb.ioParam.ioBuffer = buf;
    pb.ioParam.ioReqCount = cap;
    pb.ioParam.ioPosMode = fsAtMark;
    err = PBReadSync(&pb);
    if (err != noErr && err != eofErr) {
        last_err = err;
        return -1;
    }
    return (int)pb.ioParam.ioActCount;
}

/*
 * A file that isn't TEXT might be anything; ask before showing its data
 * fork as text.  Saving keeps its type and its resource fork, so this
 * is a question of whether it's useful, not whether it's safe.
 */
static int confirm_type(ConstStr255Param name, OSType type)
{
    char t[5];
    Str255 tstr;

    memcpy(t, &type, 4);
    tstr[0] = 4;
    memcpy(tstr + 1, t, 4);
    ParamText(name, tstr, "\p", "\p");
    return CautionAlert(131, NULL) == 1;
}

static docwin *already_open(short vref, long dir, ConstStr255Param name)
{
    docwin *dw;

    for (dw = doc_list; dw != NULL; dw = dw->next)
        if (dw->has_file && dw->vref == vref && dw->dir == dir &&
            EqualString(dw->name, name, 0, 1))
            return dw;
    return NULL;
}

/* Open a file in a new window; asked says the user picked it just now. */
docwin *file_open(short vref, long dir, ConstStr255Param name, int asked)
{
    HParamBlockRec info;
    instream in;
    docwin *dw;
    OSErr err;
    int result;
    char msg[STATUS_MAX], *p;
    long n;
    Cursor **watch;

    dw = already_open(vref, dir, name);
    if (dw != NULL) {
        SelectWindow((WindowPtr)dw);
        return dw;
    }
    err = h_info(vref, dir, name, &info);
    if (err == noErr)
        err = h_open(vref, dir, name, fsRdPerm, &in.ref);
    if (err != noErr) {
        io_error("Can't open", name, err);
        return NULL;
    }
    if (asked && info.fileParam.ioFlFndrInfo.fdType != 'TEXT' &&
        !confirm_type(name, info.fileParam.ioFlFndrInfo.fdType)) {
        f_close(in.ref);
        return NULL;
    }
    dw = win_new();
    if (dw == NULL) {
        f_close(in.ref);
        io_error("Can't open", name, memFullErr);
        return NULL;
    }
    watch = GetCursor(watchCursor);
    if (watch != NULL)
        SetCursor(*watch);
    result = doc_read(&dw->doc, stream_in, &in, iobuf, IOBUF);
    f_close(in.ref);
    SetCursor(&qd.arrow);
    if (result != READ_OK) {
        win_dispose(dw);
        io_error("Can't open", name,
                 result == READ_NOMEM ? memFullErr : last_err);
        return NULL;
    }
    dw->has_file = 1;
    dw->vref = vref;
    dw->dir = dir;
    memcpy(dw->name, name, name[0] + 1);
    dw->type = info.fileParam.ioFlFndrInfo.fdType;
    dw->creator = info.fileParam.ioFlFndrInfo.fdCreator;
    win_set_name(dw);

    /* nano's "Read N lines (converted from DOS format)" */
    n = dw->doc.buf.count - 1;
    p = put_s(put_l(put_s(msg, "Read "), n, 0), n == 1 ? " line" : " lines");
    if (dw->doc.format == FMT_DOS)
        put_s(p, " (DOS format)");
    else if (dw->doc.format == FMT_UNIX)
        put_s(p, " (Unix format)");
    doc_status(&dw->doc, msg);
    return dw;
}

docwin *file_open_wd(short wd, ConstStr255Param name)
{
    short vref;
    long dir;

    if (!file_wd_location(wd, &vref, &dir)) {
        io_error("Can't open", name, nsvErr);
        return NULL;
    }
    return file_open(vref, dir, name, 0);
}

static Point dialog_where(int width, int height)
{
    Point p;
    const Rect *s = &qd.screenBits.bounds;

    p.h = (s->right - s->left - width) / 2;
    p.v = (s->bottom - s->top - height) / 3;
    return p;
}

/* With no type list Standard File shows invisible files too. */
static pascal Boolean hide_invisible(CInfoPBPtr pb)
{
    return (pb->hFileInfo.ioFlFndrInfo.fdFlags & 0x4000) != 0;
}

/* All files are listed: confirm_type deals with those that aren't TEXT. */
void file_open_dialog(void)
{
    SFReply reply;
    short vref;
    long dir;
    docwin *dw;

    SFGetFile(dialog_where(348, 200), "\p", hide_invisible, -1, NULL, NULL,
              &reply);
    if (!reply.good)
        return;
    if (!file_wd_location(reply.vRefNum, &vref, &dir)) {
        io_error("Can't open", reply.fName, nsvErr);
        return;
    }
    dw = file_open(vref, dir, reply.fName, 1);
    if (dw != NULL)
        ShowWindow((WindowPtr)dw);
}

/* ------------------------------------------------------------------ */
/* saving: save.c's file system */

typedef struct {
    short vref;
    long dir;
    ConstStringPtr name;
    Str63 temp;
    OSType type, creator;
    short temp_ref, target_ref;
    OSErr err;
} savefs;

static int note(savefs *f, OSErr err)
{
    if (err != noErr && f->err == noErr)
        f->err = err;
    return err == noErr;
}

/* "name.tmp", or "name.tmp2" and so on, within HFS's 31 characters. */
static void temp_name(savefs *f, int n)
{
    unsigned char *t = f->temp;
    int len = f->name[0];

    if (len > 26)
        len = 26;
    memcpy(t + 1, f->name + 1, len);
    memcpy(t + 1 + len, ".tmp", 4);
    len += 4;
    if (n > 1)
        t[++len] = (unsigned char)('0' + n % 10);
    t[0] = (unsigned char)len;
}

static int fs_temp_create(void *ctx)
{
    savefs *f = ctx;
    OSErr err = dupFNErr;
    int n;

    for (n = 1; n < 10 && err == dupFNErr; n++) {
        temp_name(f, n);
        err = h_create(f->vref, f->dir, f->temp, f->type, f->creator);
    }
    if (!note(f, err))
        return 0;
    return note(f, h_open(f->vref, f->dir, f->temp, fsRdWrPerm,
                          &f->temp_ref));
}

static int fs_temp_write(void *ctx, const char *buf, unsigned len)
{
    savefs *f = ctx;

    return note(f, f_write(f->temp_ref, buf, len));
}

static int fs_temp_close(void *ctx)
{
    savefs *f = ctx;

    return note(f, f_close(f->temp_ref));
}

static int fs_temp_delete(void *ctx)
{
    savefs *f = ctx;

    return h_delete(f->vref, f->dir, f->temp) == noErr;
}

static int fs_target_open(void *ctx, long *length)
{
    savefs *f = ctx;

    if (!note(f, h_open(f->vref, f->dir, f->name, fsRdWrPerm,
                        &f->target_ref)))
        return 0;
    return note(f, f_get_eof(f->target_ref, length));
}

static int fs_target_set_length(void *ctx, long length)
{
    savefs *f = ctx;

    return note(f, f_set_eof(f->target_ref, length));
}

static int fs_target_write(void *ctx, const char *buf, unsigned len)
{
    savefs *f = ctx;

    return note(f, f_write(f->target_ref, buf, len));
}

static int fs_target_close(void *ctx)
{
    savefs *f = ctx;

    return note(f, f_close(f->target_ref));
}

static int fs_target_replace(void *ctx)
{
    savefs *f = ctx;
    OSErr err = h_delete(f->vref, f->dir, f->name);

    if (err != noErr && err != fnfErr)
        return note(f, err);
    return note(f, h_rename(f->vref, f->dir, f->temp, f->name));
}

static const save_ops mac_ops = {
    NULL, fs_temp_create, fs_temp_write, fs_temp_close, fs_temp_delete,
    fs_target_open, fs_target_set_length, fs_target_write, fs_target_close,
    fs_target_replace
};

static void fs_setup(savefs *f, short vref, long dir, ConstStr255Param name,
                     docwin *dw)
{
    memset(f, 0, sizeof *f);
    f->vref = vref;
    f->dir = dir;
    f->name = name;
    f->type = dw->type;
    f->creator = dw->creator;
}

/* Report how a save went, and tidy up after it. */
static int save_finish(int result, savefs *f, docwin *dw)
{
    char msg[STATUS_MAX];
    long n;

    flush_volume(f->vref);
    if (result == SAVE_FAILED) {
        io_error("Can't save", f->name, f->err);
        return 0;
    }
    if (result == SAVE_KEPT_TEMP) {
        io_error("Can't finish saving", f->name, f->err);
        app_report_recovery(f->temp);
        return 0;
    }
    doc_saved(&dw->doc);
    n = dw->doc.buf.count - 1;
    put_s(put_l(put_s(msg, "Wrote "), n, 0), n == 1 ? " line" : " lines");
    doc_status(&dw->doc, msg);
    return 1;
}

/* Save to the document's own file, keeping its forks and Finder info. */
int file_save(docwin *dw)
{
    savefs f;
    save_ops ops = mac_ops;
    HParamBlockRec info;
    OSErr err;
    int result;

    if (!dw->has_file)
        return file_save_as(dw);
    fs_setup(&f, dw->vref, dw->dir, dw->name, dw);
    ops.ctx = &f;
    err = h_info(dw->vref, dw->dir, dw->name, &info);
    if (err == noErr && (info.fileParam.ioFlAttrib & 1)) {
        /* opening a locked file only says it's busy */
        io_error("Can't save", dw->name, fLckdErr);
        return 0;
    }
    if (err == fnfErr) {
        /* it was deleted behind our back: start it afresh */
        result = save_replacing(&dw->doc, &ops, iobuf, IOBUF);
    } else {
        result = save_in_place(&dw->doc, &ops, iobuf, IOBUF);
    }
    return save_finish(result, &f, dw);
}

int file_save_as(docwin *dw)
{
    SFReply reply;
    short vref;
    long dir;
    savefs f;
    save_ops ops = mac_ops;
    HParamBlockRec info;
    OSErr err;
    int result;

    SFPutFile(dialog_where(304, 184), "\pSave as:", dw->name, NULL, &reply);
    if (!reply.good)
        return 0;
    if (!file_wd_location(reply.vRefNum, &vref, &dir)) {
        io_error("Can't save", reply.fName, nsvErr);
        return 0;
    }
    /* the same file: an ordinary save, keeping its forks */
    if (dw->has_file && dw->vref == vref && dw->dir == dir &&
        EqualString(dw->name, reply.fName, 0, 1))
        return file_save(dw);

    err = h_info(vref, dir, reply.fName, &info);
    if (err == noErr && (info.fileParam.ioFlAttrib & 1)) {
        io_error("Can't save", reply.fName, fLckdErr);
        return 0;
    }
    if (already_open(vref, dir, reply.fName) != NULL) {
        io_error("Can't save", reply.fName, opWrErr);
        return 0;
    }
    /* the new file's type follows the document; a new document is TEXT */
    fs_setup(&f, vref, dir, reply.fName, dw);
    ops.ctx = &f;
    result = save_replacing(&dw->doc, &ops, iobuf, IOBUF);
    if (!save_finish(result, &f, dw))
        return 0;
    dw->has_file = 1;
    dw->vref = vref;
    dw->dir = dir;
    memcpy(dw->name, reply.fName, reply.fName[0] + 1);
    win_set_name(dw);
    menus_windows_changed();
    return 1;
}
