

#include "module.h"
#include "zmalloc/zmalloc.h"

latte_object_t* module_object_new(module_type_t* mt, void* value) {
    module_value_t* mv = zmalloc(sizeof(*mv));
    mv->type = mt;
    mv->value = value;
    return latte_object_new(OBJ_MODULE, mv);
}

void free_module_object(latte_object_t* o) {
    module_value_t *mv = o->ptr;
    mv->type->free(mv->value);
    zfree(mv);
}




size_t object_module_compute_size(latte_object_t* key, latte_object_t* val, size_t sample_size, int dbid) {
    module_value_t *mv = val->ptr;
    module_type_t *mt = mv->type;
    size_t size = 0;
    /* We prefer to use the enhanced version. */
    // 7.x 新增加接口 不知道怎么用
    // if (mt->mem_usage2 != NULL) {
    //     module_key_opt_ctx_t ctx = {key, NULL, dbid, -1};
    //     size = mt->mem_usage2(&ctx, mv->value, sample_size);
    // } else 
    if (mt->mem_usage != NULL) {
        size = mt->mem_usage(mv->value);
    } 

    return size;
}
