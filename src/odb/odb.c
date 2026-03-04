/**
 * odb 实现：oio 的 buffer/file 后端，以及字符串/二进制/数字的序列化。
 */
#include "odb/odb.h"
#include "zmalloc/zmalloc.h"
#include <string.h>

/* ---------- Buffer 后端 ---------- */
static size_t oio_buffer_read(oio *o, void *buf, size_t len) {
    size_t avail = sds_len(o->io.buffer.ptr) - (size_t)o->io.buffer.pos;
    if (avail < len)
        return 0;
    memcpy(buf, o->io.buffer.ptr + o->io.buffer.pos, len);
    o->io.buffer.pos += (off_t)len;
    o->processed_bytes += len;
    return len;
}

static size_t oio_buffer_write(oio *o, const void *buf, size_t len) {
    o->io.buffer.ptr = sds_cat_len(o->io.buffer.ptr, buf, len);
    if (!o->io.buffer.ptr)
        return 0;
    o->io.buffer.pos += (off_t)len;
    o->processed_bytes += len;
    return len;
}

static off_t oio_buffer_tell(oio *o) {
    return o->io.buffer.pos;
}

static int oio_buffer_flush(oio *o) {
    (void)o;
    return 1;
}

oio *odb_oio_create_buffer(void) {
    oio *o = (oio *)zmalloc(sizeof(oio));
    if (!o) return NULL;
    memset(o, 0, sizeof(*o));
    o->read = oio_buffer_read;
    o->write = oio_buffer_write;
    o->tell = oio_buffer_tell;
    o->flush = oio_buffer_flush;
    o->io.buffer.ptr = sds_empty();
    o->io.buffer.pos = 0;
    o->max_processing_chunk = 1024 * 1024;
    if (!o->io.buffer.ptr) {
        zfree(o);
        return NULL;
    }
    return o;
}

/* ---------- File 后端 ---------- */
static size_t oio_file_read(oio *o, void *buf, size_t len) {
    size_t n = fread(buf, 1, len, o->io.file.fp);
    if (n != len && ferror(o->io.file.fp))
        return 0;
    o->processed_bytes += n;
    return n;
}

static size_t oio_file_write(oio *o, const void *buf, size_t len) {
    size_t n = fwrite(buf, 1, len, o->io.file.fp);
    if (n != len)
        return 0;
    o->processed_bytes += n;
    return n;
}

static off_t oio_file_tell(oio *o) {
    return (off_t)ftello(o->io.file.fp);
}

static int oio_file_flush(oio *o) {
    return fflush(o->io.file.fp) == 0 ? 1 : 0;
}

oio *odb_oio_create_file(FILE *fp) {
    if (!fp) return NULL;
    oio *o = (oio *)zmalloc(sizeof(oio));
    if (!o) return NULL;
    memset(o, 0, sizeof(*o));
    o->read = oio_file_read;
    o->write = oio_file_write;
    o->tell = oio_file_tell;
    o->flush = oio_file_flush;
    o->io.file.fp = fp;
    o->io.file.buffered = 0;
    o->io.file.autosync = 0;
    o->max_processing_chunk = 1024 * 1024;
    return o;
}

void odb_oio_free(oio *o) {
    if (!o) return;
    if (o->io.buffer.ptr) {
        sds_delete(o->io.buffer.ptr);
        o->io.buffer.ptr = NULL;
    }
    zfree(o);
}

void odb_oio_buffer_rewind(oio *o) {
    if (o && o->io.buffer.ptr)
        o->io.buffer.pos = 0;
}

/* 小端写入/读取，兼容常见 x86/arm64 */
static void put_u32_le(unsigned char *p, uint32_t v) {
    p[0] = (unsigned char)(v);
    p[1] = (unsigned char)(v >> 8);
    p[2] = (unsigned char)(v >> 16);
    p[3] = (unsigned char)(v >> 24);
}
static uint32_t get_u32_le(const unsigned char *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static int odb_write_len32_le(oio *o, uint32_t len) {
    unsigned char buf[4];
    put_u32_le(buf, len);
    return o->write(o, buf, 4) == 4 ? 1 : 0;
}

static int odb_read_len32_le(oio *o, uint32_t *len) {
    unsigned char buf[4];
    if (o->read(o, buf, 4) != 4)
        return 0;
    *len = get_u32_le(buf);
    return 1;
}

/* ---------- 字符串 ---------- */
int odb_write_string(oio *o, const void *s, size_t len) {
    if (len > 0xFFFFFFFFu) return 0;
    if (!odb_write_len32_le(o, (uint32_t)len)) return 0;
    if (len && o->write(o, s, len) != len) return 0;
    return 1;
}

sds odb_read_string(oio *o) {
    uint32_t len;
    if (!odb_read_len32_le(o, &len)) return NULL;
    sds s = sds_new_len(SDS_NOINIT, len);
    if (!s) return NULL;
    if (len && o->read(o, s, len) != len) {
        sds_delete(s);
        return NULL;
    }
    return s;
}

/* ---------- 二进制 ---------- */
int odb_write_binary(oio *o, const void *buf, size_t len) {
    if (len > 0xFFFFFFFFu) return 0;
    if (!odb_write_len32_le(o, (uint32_t)len)) return 0;
    if (len && o->write(o, buf, len) != len) return 0;
    return 1;
}

int odb_read_binary(oio *o, void **out, size_t *out_len) {
    uint32_t len;
    if (!odb_read_len32_le(o, &len)) return -1;
    if (len == 0) {
        *out = NULL;
        *out_len = 0;
        return 0;
    }
    void *p = zmalloc(len);
    if (!p) return -1;
    if (o->read(o, p, len) != len) {
        zfree(p);
        return -1;
    }
    *out = p;
    *out_len = (size_t)len;
    return 0;
}

/* ---------- 数字（小端） ---------- */
static int odb_write_u16_le(oio *o, uint16_t v) {
    unsigned char buf[2];
    buf[0] = (unsigned char)(v);
    buf[1] = (unsigned char)(v >> 8);
    return o->write(o, buf, 2) == 2 ? 1 : 0;
}
static int odb_write_u32_le(oio *o, uint32_t v) {
    unsigned char buf[4];
    put_u32_le(buf, v);
    return o->write(o, buf, 4) == 4 ? 1 : 0;
}
static int odb_write_u64_le(oio *o, uint64_t v) {
    unsigned char buf[8];
    buf[0] = (unsigned char)(v);
    buf[1] = (unsigned char)(v >> 8);
    buf[2] = (unsigned char)(v >> 16);
    buf[3] = (unsigned char)(v >> 24);
    buf[4] = (unsigned char)(v >> 32);
    buf[5] = (unsigned char)(v >> 40);
    buf[6] = (unsigned char)(v >> 48);
    buf[7] = (unsigned char)(v >> 56);
    return o->write(o, buf, 8) == 8 ? 1 : 0;
}

int odb_write_u8(oio *o, uint8_t v) {
    return o->write(o, &v, 1) == 1 ? 1 : 0;
}
int odb_write_u16(oio *o, uint16_t v) { return odb_write_u16_le(o, v); }
int odb_write_u32(oio *o, uint32_t v) { return odb_write_u32_le(o, v); }
int odb_write_u64(oio *o, uint64_t v) { return odb_write_u64_le(o, v); }

static int read_u8(oio *o, uint8_t *v) {
    return o->read(o, v, 1) == 1 ? 1 : 0;
}
static int odb_read_u16_le(oio *o, uint16_t *v) {
    unsigned char buf[2];
    if (o->read(o, buf, 2) != 2) return 0;
    *v = (uint16_t)buf[0] | ((uint16_t)buf[1] << 8);
    return 1;
}
static int odb_read_u32_le(oio *o, uint32_t *v) {
    unsigned char buf[4];
    if (o->read(o, buf, 4) != 4) return 0;
    *v = get_u32_le(buf);
    return 1;
}
static int odb_read_u64_le(oio *o, uint64_t *v) {
    unsigned char buf[8];
    if (o->read(o, buf, 8) != 8) return 0;
    *v = (uint64_t)buf[0] | ((uint64_t)buf[1] << 8) | ((uint64_t)buf[2] << 16) | ((uint64_t)buf[3] << 24)
       | ((uint64_t)buf[4] << 32) | ((uint64_t)buf[5] << 40) | ((uint64_t)buf[6] << 48) | ((uint64_t)buf[7] << 56);
    return 1;
}

int odb_read_u8(oio *o, uint8_t *v) { return read_u8(o, v); }
int odb_read_u16(oio *o, uint16_t *v) { return odb_read_u16_le(o, v); }
int odb_read_u32(oio *o, uint32_t *v) { return odb_read_u32_le(o, v); }
int odb_read_u64(oio *o, uint64_t *v) { return odb_read_u64_le(o, v); }
