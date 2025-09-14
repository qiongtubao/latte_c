#include "listpack.h"
#include "zmalloc/zmalloc.h"




#define list_pack_set_taotal_bytes(p,v) do { \
    (p)[0] = (v)&0xff; \
    (p)[1] = ((v)>>8)&0xff; \
    (p)[2] = ((v)>>16)&0xff; \
    (p)[3] = ((v)>>24)&0xff; \
} while(0)

#define list_pack_set_num_elements(p,v) do { \
    (p)[4] = (v)&0xff; \
    (p)[5] = ((v)>>8)&0xff; \
} while(0)

#define LP_HDR_SIZE 6
#define LP_EOF 0xFF
/* 
    创建空list_pack_t* 对象
    总长度 7 = total_bytes(4)  + num_elements(2) + EOF(1)
*/
list_pack_t* list_pack_new(size_t capacity) {
    list_pack_t* lp = zmalloc(capacity > LP_HDR_SIZE + 1? capacity: LP_HDR_SIZE+ 1);
    if (lp == NULL) return NULL;
    list_pack_set_taotal_bytes(lp, LP_HDR_SIZE + 1); /* 设置长度为7  */
    list_pack_set_num_elements(lp, 0); /*设置个数 0 */
    lp[LP_HDR_SIZE] = LP_EOF;
    return lp;
}

void list_pack_delete(list_pack_t* lp) {
    zfree(lp);
}

void list_pack_free_generic(void* lp) {
    list_pack_delete((list_pack_t*)lp);
}


#define list_pack_get_total_bytes(p)           (((uint32_t)(p)[0]<<0) | \
                                      ((uint32_t)(p)[1]<<8) | \
                                      ((uint32_t)(p)[2]<<16) | \
                                      ((uint32_t)(p)[3]<<24))

/*内存收缩*/
unsigned char* list_pack_shrink_to_fit(list_pack_t* lp) {
    size_t size = list_pack_get_total_bytes(lp);
    if (size < zmalloc_size(lp)) {
        return zrealloc(lp, size);
    } else {
        return lp;
    }
}