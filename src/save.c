/*
 * save.c - saving without losing anything, and closing documents
 *
 * tico16 writes NAME.$$$ and renames it over NAME.  That would drop a
 * Macintosh file's resource fork and Finder information, so tico68
 * writes the data fork of the existing file where it stands.  The order
 * of events is what makes that safe:
 *
 *  1. Write everything to a temporary file.  If that fails, for lack of
 *     space or otherwise, the original hasn't been touched.
 *  2. Grow the original to its new length.  Its old bytes stay put, and
 *     this claims the disk space, so step 3 can't run out of it.
 *  3. Overwrite the original and set its final length.
 *  4. Delete the temporary file.
 *
 * Only a hardware failure can stop step 3, and then the temporary file
 * is complete, and the caller says where it is.  The cost is room for a
 * second copy of the file while saving.
 *
 * Save As over another file is different: see save_replacing.
 *
 * Portable C: the file system comes in as save_ops, so the failures can
 * be tested on the host.
 */

#include <string.h>
#include "tico.h"

typedef struct {
    unsigned (*write)(void *ctx, const char *buf, unsigned len);
    void *ctx;
    char *buf;
    unsigned cap, used;
    long total;
} outbuf;

static int out_flush(outbuf *o)
{
    unsigned n = o->used;

    o->used = 0;
    return n == 0 || o->write(o->ctx, o->buf, n) == n;
}

static int out_put(void *ctx, const char *buf, unsigned len)
{
    outbuf *o = ctx;
    unsigned n;

    o->total += len;
    while (len > 0) {
        if (o->used == o->cap && !out_flush(o))
            return 0;
        n = o->cap - o->used;
        if (n > len)
            n = len;
        memcpy(o->buf + o->used, buf, n);
        o->used += n;
        buf += n;
        len -= n;
    }
    return 1;
}

static int write_all(const document *d, const save_ops *ops,
                     unsigned (*write)(void *, const char *, unsigned),
                     char *buf, unsigned cap, long *total)
{
    outbuf o;

    o.write = write;
    o.ctx = ops->ctx;
    o.buf = buf;
    o.cap = cap;
    o.used = 0;
    o.total = 0;
    if (!text_write(&d->buf, d->format, d->ctrlz, out_put, &o) ||
        !out_flush(&o))
        return 0;
    *total = o.total;
    return 1;
}

/* Write the temporary file; 0 if that fails, leaving none behind. */
static int write_temp(const document *d, const save_ops *ops, char *buf,
                      unsigned cap, long *total)
{
    int ok;

    if (!ops->temp_create(ops->ctx))
        return 0;
    ok = write_all(d, ops, ops->temp_write, buf, cap, total);
    if (!ops->temp_close(ops->ctx))
        ok = 0;
    if (!ok)
        ops->temp_delete(ops->ctx);
    return ok;
}

int save_in_place(const document *d, const save_ops *ops, char *buf,
                  unsigned cap)
{
    long total, old, wrote;
    int ok;

    /* a locked file stops here, before the work of the temporary file */
    if (!ops->target_open(ops->ctx, &old))
        return SAVE_FAILED;
    if (!write_temp(d, ops, buf, cap, &total)) {
        ops->target_close(ops->ctx);
        return SAVE_FAILED;
    }
    if (total > old && !ops->target_set_length(ops->ctx, total)) {
        ops->target_set_length(ops->ctx, old);
        ops->target_close(ops->ctx);
        ops->temp_delete(ops->ctx);
        return SAVE_FAILED;
    }
    ok = write_all(d, ops, ops->target_write, buf, cap, &wrote) &&
         wrote == total && ops->target_set_length(ops->ctx, total);
    if (!ops->target_close(ops->ctx))
        ok = 0;
    if (!ok)
        return SAVE_KEPT_TEMP;
    ops->temp_delete(ops->ctx);
    return SAVE_OK;
}

/* After a failure, with the target as it was. */
static int drop_temp(const save_ops *ops)
{
    return ops->temp_delete(ops->ctx) ? SAVE_FAILED : SAVE_LEFT_TEMP;
}

/*
 * Save As over another file: the user has chosen to replace it, so
 * unlike a save it doesn't keep its forks.  But it stays until the text
 * is in its place, in case that can't be done:
 *
 *  1. Write everything to a temporary file.
 *  2. Rename the old file out of the way, as a backup.
 *  3. Rename the temporary file to the name.
 *  4. Delete the backup.
 *
 * If step 3 fails the backup is renamed back.  Should that fail too
 * there is no file by that name, and the caller must say where both
 * of the others are.
 */
int save_replacing(const document *d, const save_ops *ops, char *buf,
                   unsigned cap)
{
    long total;

    if (!write_temp(d, ops, buf, cap, &total))
        return SAVE_FAILED;
    if (!ops->target_backup(ops->ctx))
        return drop_temp(ops);
    if (!ops->temp_install(ops->ctx)) {
        if (!ops->target_restore(ops->ctx))
            return SAVE_KEPT_BOTH;
        return drop_temp(ops);
    }
    return ops->backup_delete(ops->ctx) ? SAVE_OK : SAVE_KEPT_BACKUP;
}

/*
 * Close a document, asking first if it has changes.  0 if it stays
 * open: the user cancelled, or the save failed.
 */
int close_document(void *doc, const close_ops *ops)
{
    if (ops->is_modified(ops->ctx, doc)) {
        switch (ops->ask(ops->ctx, doc)) {
        case ASK_SAVE:
            if (!ops->save(ops->ctx, doc))
                return 0;
            break;
        case ASK_DISCARD:
            break;
        default:
            return 0;
        }
    }
    ops->dispose(ops->ctx, doc);
    return 1;
}
