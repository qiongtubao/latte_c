
#ifndef __LATTE_INT_SET_H
#define __LATTE_INT_SET_H
#include <stdint.h>
#include <string.h>

/* Note that these encodings are ordered, so:
 * INTSET_ENC_INT16 < INTSET_ENC_INT32 < INTSET_ENC_INT64. */
#define INTSET_ENC_INT16 (sizeof(int16_t))
#define INTSET_ENC_INT32 (sizeof(int32_t))
#define INTSET_ENC_INT64 (sizeof(int64_t))

typedef struct int_set_t {
    uint32_t encoding;
    uint32_t length;
    int8_t contents[];
} int_set_t;

int_set_t* int_set_new();
int_set_t* int_set_add(int_set_t* is, int64_t value, uint8_t *success);
int_set_t *int_set_remove(int_set_t *is, int64_t value, int *success);
uint8_t int_set_find(int_set_t *is, int64_t value);
int64_t int_set_random(int_set_t *is);
uint8_t int_set_get(int_set_t *is, uint32_t pos, int64_t *value);
uint32_t int_set_len(const int_set_t *is);
size_t int_set_blob_len(int_set_t *is);
int int_set_validate_integrity(const unsigned char *is, size_t size, int deep);

uint8_t _int_set_value_encoding(int64_t v);
uint8_t int_set_search(int_set_t *is, int64_t value, uint32_t *pos);
#endif