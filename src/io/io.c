#include "io.h"
#include "crc/crc.h"
#include "utils/utils.h"

/* This function can be installed both in memory and file streams when checksum
 * computation is needed. */
void io_generic_update_checksum(io_t *r, const void *buf, size_t len) {
    crc64_init();
    r->cksum = crc64(r->cksum,buf,len);
}

/* Set the file-based rio object to auto-fsync every 'bytes' file written.
 * By default this is set to zero that means no automatic file sync is
 * performed.
 *
 * This feature is useful in a few contexts since when we rely on OS write
 * buffers sometimes the OS buffers way too much, resulting in too many
 * disk I/O concentrated in very little time. When we fsync in an explicit
 * way instead the I/O pressure is more distributed across time. */
void io_set_auto_sync(io_t *r, off_t bytes) {
    if(is_io_file(r)) return;
    r->io.file.autosync = bytes;
}

/* --------------------------- Higher level interface --------------------------
 *
 * The following higher level functions use lower level rio.c functions to help
 * generating the Redis protocol for the Append Only File. */

/* Write multi bulk count in the format: "*<count>\r\n". */
size_t io_write_bulk_count(io_t *r, char prefix, long count) {
    char cbuf[128];
    int clen;

    cbuf[0] = prefix;
    clen = 1+ll2string(cbuf+1,sizeof(cbuf)-1,count);
    cbuf[clen++] = '\r';
    cbuf[clen++] = '\n';
    if (ioWrite(r,cbuf,clen) == 0) return 0;
    return clen;
}

/* Write binary-safe string in the format: "$<count>\r\n<payload>\r\n". */
size_t io_write_bulk_string(io_t *r, const char *buf, size_t len) {
    size_t nwritten;

    if ((nwritten = io_write_bulk_count(r,'$',len)) == 0) return 0;
    if (len > 0 && ioWrite(r,buf,len) == 0) return 0;
    if (ioWrite(r,"\r\n",2) == 0) return 0;
    return nwritten+len+2;
}

/* Write a long long value in format: "$<count>\r\n<payload>\r\n". */
size_t io_write_bulk_longlong(io_t *r, long long l) {
    char lbuf[32];
    unsigned int llen;

    llen = ll2string(lbuf,sizeof(lbuf),l);
    return io_write_bulk_string(r,lbuf,llen);
}

/* Write a double value in the format: "$<count>\r\n<payload>\r\n" */
size_t io_write_bulk_double(io_t *r, double d) {
    char dbuf[128];
    unsigned int dlen;

    dlen = snprintf(dbuf,sizeof(dbuf),"%.17g",d);
    return io_write_bulk_string(r,dbuf,dlen);
}