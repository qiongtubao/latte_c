//
// Created by dong on 23-5-22.
//

#ifndef LATTE_C_LISTPACK_H
#define LATTE_C_LISTPACK_H
/* lpInsert() where argument possible values: */
#define LP_BEFORE 0
#define LP_AFTER 1
#define LP_REPLACE 2

#include <assert.h>
#include "zmalloc.h"
#include "utils/utils.h"


#define LP_INTBUF_SIZE 21 /* 20 digits of -2^63 + 1 null term = 21. */

/* We use zmalloc_usable/zrealloc_usable instead of zmalloc/zrealloc
 * to ensure the safe invocation of 'zmalloc_usable_size().
 * See comment in zmalloc_usable_size(). */
#define lp_malloc(sz) zmalloc_usable(sz,NULL)
#define lp_realloc(ptr,sz) zrealloc_usable(ptr,sz,NULL)
#define lp_free zfree
#define lp_malloc_size zmalloc_usable_size

unsigned char *lp_new(size_t capacity);
unsigned char *lp_prepend(unsigned char *lp, unsigned char *s, uint32_t slen);
unsigned char *lp_append(unsigned char *lp, unsigned char *s, uint32_t slen);
size_t lp_bytes(unsigned char *lp);
void lp_repr(unsigned char *lp);
#endif //LATTE_C_LISTPACK_H
