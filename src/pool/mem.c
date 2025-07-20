#include "mem.h"
#include "zmalloc/zmalloc.h"
#include "set/hash_set.h"
#include "log/log.h"
#include "utils/utils.h"

hash_set_func_t used_set_func = {
    .hashFunction = dict_ptr_hash,
    .keyCompare = dict_ptr_key_compare
};

mem_pool_t* mem_pool_new(sds name) {
    mem_pool_t* pool = zmalloc(sizeof(mem_pool_t));
    if (pool == NULL) return NULL;
    pool->mutex = latte_recursive_mutex_new();
    if (pool->mutex == NULL) {
        zfree(pool);
        return NULL;
    }
    pool->name = name;
    pool->size = 0;
    pool->item_size = 0;
    pool->item_num_per_pool = 0;
    pool->dynamic = false;
    pool->pools = NULL;
    pool->used = NULL;
    pool->frees = NULL;
    return pool;
}

int mem_pool_delete(mem_pool_t* pool) {
    if(list_length(pool->frees) != pool->size) {
        return 0;
    }

    mem_pool_clean_up(pool);
    latte_mutex_delete(pool->mutex);
    zfree(pool);
    return 1;
}

int mem_pool_extend(mem_pool_t* mem_pool) {
    if (mem_pool->dynamic == false) {
        LATTE_LIB_LOG(LOG_ERROR, "Disable dynamic extend memory pool, but begin to extend, this->name:%s", mem_pool->name);
        return -1;
    }
    latte_mutex_lock(mem_pool->mutex);
    //申请一块大内存
    void* pool = zmalloc(mem_pool->item_num_per_pool * mem_pool->item_size);
    if (pool == NULL) {
        latte_mutex_unlock(mem_pool->mutex);
        LATTE_LIB_LOG(LOG_ERROR, "Failed to extend memory pool, this->size:%d, item_num_per_pool:%d, this->name:%s.",
              mem_pool->size, mem_pool->item_num_per_pool, mem_pool->name);
        return -1;
    }

    //大内存块保存起来
    list_add_node_tail(mem_pool->pools, pool);
    mem_pool->size += mem_pool->item_num_per_pool;
    //把拆分成小对象添加到空闲组里
    for(int i =0 ; i< mem_pool->item_num_per_pool;i++) {
        list_add_node_tail(mem_pool->frees, pool + i * mem_pool->item_size);
    }

    latte_mutex_unlock(mem_pool->mutex);
    LATTE_LIB_LOG(LOG_INFO, "extend one pool, this->size:%d, item_num_per_pool:%d, this->name:%s.",
           mem_pool->size, mem_pool->item_num_per_pool, mem_pool->name);
    return 0;
}

void* mem_pool_alloc(mem_pool_t* mem_pool) {
    void* result = NULL;
    latte_mutex_lock(mem_pool->mutex);
    if (list_length(mem_pool->frees) == 0) {
        if (mem_pool->dynamic == false) {
            goto end;
        }
        if (mem_pool_extend(mem_pool) < 0) {
            goto end;
        }
    }
    result = list_rpop(mem_pool->frees);
    latte_assert_with_info(set_add(mem_pool->used, result) == 1, "used add item fail");
end:
    latte_mutex_unlock(mem_pool->mutex);
    return result;
}

void mem_pool_free(mem_pool_t* mem_pool, void* result) {
    latte_mutex_lock(mem_pool->mutex);
    int num = set_remove(mem_pool->used, result); //0 删除失败 1表示删除成功
    if (num == 0) {
        //为了打印消息后结束提前解锁    这里可以写的优雅一点 以后改改
        latte_mutex_unlock(mem_pool->mutex);
        LATTE_LIB_LOG(LOG_WARN, "No entry of %p in %s.", result, mem_pool->name);
        // print_stacktrace();
        return;
    }
    list_add_node_tail(mem_pool->frees,result);
    latte_mutex_unlock(mem_pool->mutex);
    return;
}

void mem_pool_clean_up(mem_pool_t* mem_pool) {
    if (list_length(mem_pool->pools) == 0) { //正常情况下应该有数据
        LATTE_LIB_LOG(LOG_WARN,"Begin to do cleanup, but there is no memory pool, this->name:%s!", mem_pool->name);
        return ;
    }
    latte_mutex_lock(mem_pool->mutex);
    if (mem_pool->used != NULL) {
        set_delete(mem_pool->used);
        mem_pool->used = NULL;
    }
    
    if (mem_pool->frees) {
        list_delete(mem_pool->frees);
        mem_pool->frees = NULL;
    }
    
    if (mem_pool->pools) {
        list_delete(mem_pool->pools);
        mem_pool->pools = NULL;
    }
    mem_pool->size = 0;
    latte_mutex_unlock(mem_pool->mutex);
    LATTE_LIB_LOG(LOG_INFO,"Successfully do cleanup, this->name:%s.", mem_pool->name);
}

int mem_pool_init(mem_pool_t* mem_pool, int item_size, bool dynamic, int pool_num, int item_num_per_pool) {
    if (item_size <= 0 || pool_num <= 0 || item_num_per_pool <= 0) {
        LATTE_LIB_LOG(LOG_ERROR,"Invalid arguments, item_size:%d, pool_num:%d, item_num_per_pool:%d, this->name:%s.",
        item_size, pool_num, item_num_per_pool, mem_pool->name);
        return -1;
    }
    
    mem_pool->item_size = item_size;
    mem_pool->item_num_per_pool = item_num_per_pool;
    mem_pool->dynamic = true;
    mem_pool->used = hash_set_new(&used_set_func);
    if (mem_pool->used == NULL) {
        LATTE_LIB_LOG(LOG_ERROR, "mem_pool new used fail\n");
        return -1;
    }
    mem_pool->pools = list_new();
    if (mem_pool->pools == NULL) {
        LATTE_LIB_LOG(LOG_ERROR, "mem_pool new pools fail\n");
        mem_pool_clean_up(mem_pool);
        return -1;
    }
    mem_pool->frees = list_new();
    if (mem_pool->frees == NULL) {
        LATTE_LIB_LOG(LOG_ERROR, "mem_pool new frees fail\n");
        mem_pool_clean_up(mem_pool);
        return -1;
    }
    for (int i = 0; i < pool_num; i++) {
        if (mem_pool_extend(mem_pool) < 0) {
            mem_pool_clean_up(mem_pool);
            return -1;
        }
    }
    mem_pool->dynamic  = dynamic;
    LATTE_LIB_LOG(LOG_INFO, "Extend one pool, this->size:%d, item_size:%d, item_num_per_pool:%d, this->name:%s.",
      mem_pool->size, item_size, item_num_per_pool, mem_pool->name);
    return 0;

}


//**********


// #include "utils_mem_pool.h"
// #include <assert.h>
// #include "log.h"

// memPoolSimple* memPoolSimpleCreate() {
//     memPoolSimple* pool = zmalloc(sizeof(memPoolSimple));
//     return pool;
// }

// int memPoolSimple_extend(memPoolSimple* simple) {
//     if (simple->dynamic == false) {
//         miniDBServerLog(LOG_ERROR, "Disable dynamic extend memory pool, but begin to extend, this->name:%s", simple->name);
//         return -1;
//     }

//     latte_mutex_lock(simple->mutex);
//     void* pool = zmalloc(simple->item_num_per_pool * simple->class_size);
//     if (pool == NULL) {
//         latte_mutex_unlock(simple->mutex);
//         miniDBServerLog(LOG_ERROR, "Failed to extend memory pool, this->size:%d, item_num_per_pool:%d, this->name:%s.",
//               simple->size, simple->item_num_per_pool, simple->name);
//         return -1;
//     }
//     list_add_node_tail(simple->pools, pool);
//     simple->size += simple->item_num_per_pool;
//     for(int i =0 ; i< simple->item_num_per_pool;i++) {
//         list_add_node_tail(simple->frees, pool + i * simple->class_size);
//     }

//     latte_mutex_unlock(simple->mutex);
//     miniDBServerLog(LOG_INFO, "Extend one pool, this->size:%d, item_num_per_pool:%d, this->name:%s.",
//            simple->size, simple->item_num_per_pool, simple->name);
//     return 0;
// }

// void* memPoolSimple_alloc(memPoolSimple* simple) {
//     void* result = NULL;
//     latte_mutex_lock(simple->mutex);
//     if (list_length(simple->frees) == 0) {
//         if (simple->dynamic == false) {
//             goto end;
//         }
//         if (memPoolSimple_extend(simple) < 0) {
//             goto end;
//         }
//     }
//     result = list_pop(simple->frees);
//     assert(set_add(simple->used, result) == 1);
// end:
//     latte_mutex_unlock(simple->mutex);
//     return result;
// }

// void memPoolSimple_free(memPoolSimple* simple, void* result) {
//     latte_mutex_lock(simple->mutex);
//     int num = set_remove(simple->used, result); //0 删除失败 1表示删除成功
//     if (num == 0) {
//         //为了打印消息后结束提前解锁    这里可以写的优雅一点 以后改改
//         latte_mutex_unlock(simple->mutex);
//         miniDBServerLog(LOG_WARN, "No entry of %p in %s.", result, simple->name);
//         // print_stacktrace();
//         return;
//     }
//     list_add_node_tail(simple->frees,result);
//     latte_mutex_unlock(simple->mutex);
//     return;
// }



// memPoolItem* memPoolItemCreate(const char* type) {
//     memPoolItem* item = zmalloc(sizeof(memPoolItem));
//     item->name = sds_new(type);
//     return item;
// }

// int memPoolItemExtend(memPoolItem* item) {
//     if (item->dynamic == false) {
//         miniDBServerLog(LOG_WARN,"Disable dynamic extend memory pool, but begin to extend, this->name:%s", item->name);
//         return -1;
//     }
//     latte_mutex_lock(item->mutex);
//     void* pool = zmalloc(item->item_size * item->item_num_per_pool);
//     if (pool == NULL) {
//         latte_mutex_unlock(item->mutex);
//         miniDBServerLog(LOG_ERROR, "Failed to extend memory pool, this->size:%d, item_num_per_pool:%d, this->name:%s.",
//             item->size,
//             item->item_num_per_pool,
//             item->name);
//         return -1;
//     }
//     list_add_node_tail(item->pools, pool);
//     item->size += item->item_num_per_pool;
//     for (int i = 0; i < item->item_num_per_pool; i++) {
//         char *point = (char *)pool + i * item->item_size;
//         list_add_node_tail(item->frees, point);
//     }
//     latte_mutex_unlock(item->mutex);
//     miniDBServerLog(LOG_ERROR, "Extend one pool, this->size:%d, item_size:%d, item_num_per_pool:%d, this->name:%s.",
//       item->size,
//       item->item_size,
//       item->item_num_per_pool,
//       item->name);
//     return 0;
// } 

// void memPoolItemCleanup(memPoolItem* item) {
//     if (list_length(item->pools) == 0) { //正常情况下应该有数据
//         miniDBServerLog(LOG_WARN,"Begin to do cleanup, but there is no memory pool, this->name:%s!", item->name);
//         return 0;
//     }
//     latte_mutex_lock(item->mutex);

//     list_delete(item->used);
//     list_delete(item->frees);
//     item->size = 0;

//     latte_iterator_t* it= list_get_latte_iterator(item->pools, 0);
//     while(latte_iterator_has_next(it)) {
//         void* pool = latte_iterator_next(it);
//         zfree(pool);
//     }
//     latte_iterator_delete(it);

//     list_delete(item->pools);
//     latte_mutex_unlock(item->mutex);
//     miniDBServerLog(LOG_INFO,"Successfully do cleanup, this->name:%s.", item->name);
// }

// int memPoolItemInit(memPoolItem* item, int item_size, bool dynamic, int pool_num, int item_num_per_pool) {
//     if (list_length(item->pools) != 0) { //初始化时 应该没有数据
//         miniDBServerLog(LOG_WARN,"Memory pool has been initialized, but still begin to be initialized, this->name:%s.", item->name);
//         return 0;
//     }
//     if (item_size <= 0 || pool_num <= 0 || item_num_per_pool <= 0) {
//         miniDBServerLog(LOG_ERROR,"Invalid arguments, item_size:%d, pool_num:%d, item_num_per_pool:%d, this->name:%s.",
//         item_size, pool_num, item_num_per_pool, item->name);
//         return -1;
//     }
//     item->item_size = item_size;
//     item->item_num_per_pool = item_num_per_pool;
//     item->dynamic = true;
//     for (int i = 0; i < pool_num; i++) {
//         if (memPoolItemExtend(item) < 0) {
//             memPoolItemCleanup(item);
//             return -1;
//         }
//     }
//     item->dynamic  = dynamic;
//     miniDBServerLog(LOG_INFO, "Extend one pool, this->size:%d, item_size:%d, item_num_per_pool:%d, this->name:%s.",
//       item->size, item_size, item_num_per_pool, item->name);
//     return 0;
// }

