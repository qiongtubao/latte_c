#include "io.h"
/* Define redis_fsync to fdatasync() in Linux and fsync() for all the rest */
#ifdef __linux__
#define latte_fsync fdatasync
#else
#define latte_fsync fsync
#endif
/* --------------------- Stdio file pointer implementation ------------------- */

/* Returns 1 or 0 for success/failure. */
static size_t ioFileWrite(io_t *r, const void *buf, size_t len) {
    size_t retval;

    retval = fwrite(buf,len,1,r->io.file.fp);
    r->io.file.buffered += len;

    if (r->io.file.autosync &&
        r->io.file.buffered >= r->io.file.autosync)
    {
        fflush(r->io.file.fp);
        if (latte_fsync(fileno(r->io.file.fp)) == -1) return 0;
        r->io.file.buffered = 0;
    }
    return retval;
}

/* Returns 1 or 0 for success/failure. */
static size_t ioFileRead(io_t *r, void *buf, size_t len) {
    return fread(buf,len,1,r->io.file.fp);
}

/* Returns read/write position in file. */
static off_t ioFileTell(io_t *r) {
    return ftello(r->io.file.fp);
}

/* Flushes any buffer to target device if applicable. Returns 1 on success
 * and 0 on failures. */
static int ioFileFlush(io_t *r) {
    return (fflush(r->io.file.fp) == 0) ? 1 : 0;
}

static const io_t rioFileIO = {
    ioFileRead,
    ioFileWrite,
    ioFileTell,
    ioFileFlush,
    NULL,           /* update_checksum */
    0,              /* current checksum */
    0,              /* flags */
    0,              /* bytes read or written */
    0,              /* read/write chunk size */
    { { NULL, 0 } } /* union for io-specific vars */
};

void io_init_with_file(io_t *r, FILE *fp) {
    *r = rioFileIO;
    r->io.file.fp = fp;
    r->io.file.buffered = 0;
    r->io.file.autosync = 0;
}

int is_io_file(io_t* r) {
    if (r->write == ioFileWrite) return 1;
    return 0;
}