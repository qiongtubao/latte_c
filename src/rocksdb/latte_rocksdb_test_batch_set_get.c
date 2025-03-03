
#include "latte_rocksdb_test.h"
#include "latte_rocksdb.h"
#include "../test/testassert.h"
#include "list/list.h"
#include <pthread.h>
#include "mutex/mutex.h"
#include "utils/utils.h"
#include "vector/vector.h"
#include <unistd.h> // For usleep()
#include <log/log.h>

bool RocksDbIOUringEnable() {
    // 检查是否启用了 IO-uring
    return true;
}

typedef struct batch_queue_t {
    int id;
    pthread_t thread_id;
    list_t* queue;
    size_t max_len;
    latte_mutex_t* mutex;
    pthread_cond_t not_full;
    pthread_cond_t not_empty;
    latte_rocksdb_t *db;
    uint64_t batch_time;
    uint64_t batch_count;
} batch_queue_t;


void batch_queue_init(batch_queue_t* q,int id, latte_rocksdb_t *db, size_t max_len) {
    // batch_queue_t* q = zmalloc(sizeof(batch_queue_t));
    q->thread_id = NULL;
    q->id = id;
    q->db = db;
    q->max_len = max_len;
    q->queue = list_new();
    q->mutex = latte_shared_mutex_new();
    q->batch_time = 0;
    q->batch_count = 0;
    pthread_cond_init(&q->not_full, NULL);
    pthread_cond_init(&q->not_empty, NULL);
}

batch_queue_t* batch_queue_new(int id, latte_rocksdb_t *db, size_t max_len) {
    batch_queue_t* q = zmalloc(sizeof(batch_queue_t));
    batch_queue_init(q, id, db, max_len);
    return q;
}


typedef enum task_type_enum {
    PUT,
    GET
} task_type_enum;

typedef struct task_t {
    task_type_enum type;
    vector_t* task_obj;
} task_t;

task_t* task_new(task_type_enum type, vector_t* task_obj) {
    task_t* t = zmalloc(sizeof(task_t));
    t->type = type;
    t->task_obj = task_obj;
    return t;
}

void task_delete(void* arg) {
    task_t* task = arg;
    switch (task->type)
    {
    case GET: {
        if (task->task_obj != NULL) {
            vector_delete(task->task_obj, latte_rocksdb_query_delete);
            task->task_obj = NULL;
        }
        break;
    }
    
    case PUT:
        if(task->task_obj != NULL) {
            vector_delete(task->task_obj, latte_rocksdb_put_delete);
            task->task_obj = NULL;
        }
        
        break;
    }
    zfree(task);
}

void* swapThreadMain(void* arg) {
    batch_queue_t* batch_queue = arg;
    list_t *processing_reqs = list_new();
    processing_reqs->free = task_delete;
    
    while(true) {
        latte_mutex_lock(batch_queue->mutex);
        
        while(list_length(batch_queue->queue) == 0) {
            pthread_cond_wait(&batch_queue->not_empty, &batch_queue->mutex->supper);
        }
        
        processing_reqs->head = batch_queue->queue->head;
        processing_reqs->tail = batch_queue->queue->tail;
        processing_reqs->len = batch_queue->queue->len;
        batch_queue->queue->head = NULL;
        batch_queue->queue->tail = NULL;
        batch_queue->queue->len = 0;
        latte_mutex_unlock(batch_queue->mutex);
        
        latte_iterator_t* it = list_get_latte_iterator(processing_reqs, LIST_ITERATOR_OPTION_HEAD);
        int exec_count = 0;
        while(latte_iterator_has_next(it)) {
            task_t* task = latte_iterator_next(it);
            switch(task->type) {
                case GET:{
                    long long start_time = ustime();
                    assert(0 == latte_rocksdb_read(batch_queue->db, vector_size(task->task_obj), (latte_rocksdb_query_t **)task->task_obj->data));
                    batch_queue->batch_time += ustime() - start_time;
                    batch_queue->batch_count++;
                    for(int i = 0; i < task->task_obj->count; i++) {
                        latte_rocksdb_query_t* query = task->task_obj->data[i];
                        assert_err(query->key, strcmp(query->value, "x") == 0);
                    }
                }
                break;
                case PUT: {
                    latte_error_t* e = latte_rocksdb_write(batch_queue->db, vector_size(task->task_obj), (latte_rocksdb_put_t **)task->task_obj->data);
                    if(e != NULL && !error_is_ok(e)) {
                        printf("[put-error]%s\n",e->state);
                        error_delete(e);
                    }
                }
                break;
            }
            exec_count++;
        }
        assert(exec_count == list_length(processing_reqs));
        latte_iterator_delete(it);
        
        list_empty(processing_reqs);
    }
    return NULL;
}

#define KEY_SIZE 150000000L
void write_data(rocksdb_column_family_handle_t *cf, size_t thread_num, batch_queue_t* batch_queues) {
    sds value = sds_new("x");
    int64_t batch_size = 10;
    int sleep_count = 0;

    for (int64_t i = 0; i < (KEY_SIZE/batch_size); i++) {
        
        
        vector_t* puts = vector_new();
    
        for(int64_t j = 0; j < batch_size; j++) {
            sds key = sds_cat_printf(sds_empty(),"xxxxxxxxxxx_%010d_%010d", batch_size * i + j, batch_size * i + j);
            vector_push(puts, latte_rocksdb_put_new(
                cf,
                key, 
                sds_dup(value)
            ));
            sds key2 = sds_cat_printf(sds_empty(),"xxxxxxxxxxx_%010d_0123456789", batch_size * i + j);
            vector_push(puts, latte_rocksdb_put_new(
                cf,
                key2, 
                sds_dup(value)
            ));
        }
        bool sender = false;
        while(!sender) {

            for(size_t k = 0; k < thread_num; k++) {
                size_t thread_index = (i + k) % thread_num;
                assert(thread_index < thread_num);
                batch_queue_t* batch_queue = batch_queues + thread_index;

                latte_mutex_lock(batch_queue->mutex);
                if (list_length(batch_queue->queue) >= batch_queue->max_len) {
                    latte_mutex_unlock(batch_queue->mutex);
                    continue;
                }
                list_add_node_tail(batch_queue->queue, task_new(PUT, puts));
                pthread_cond_signal(&batch_queue->not_empty);
                latte_mutex_unlock(batch_queue->mutex);
                sender = true;
                break;
            }
            if (!sender) {
                // printf("sleep %d\n", ++sleep_count);
                usleep(100);
            }
        }
        if (i%((KEY_SIZE/batch_size)/100) == 0) {
            printf("write over %ld%\n", i/((KEY_SIZE/batch_size)/100));
        }

    }
    int retry = 0;
    while(retry < 3) {
        bool is_empty = true;

        for(size_t k = 0; k < thread_num; k++) {
            batch_queue_t* batch_queue = batch_queues + k;
            if (list_length(batch_queue->queue) != 0) {
                is_empty = false;
            }
        }
        if (is_empty) {
            retry ++;
        }
        usleep(100);
    }

}

void read_data(rocksdb_column_family_handle_t *cf, int qps, size_t thread_num, batch_queue_t* batch_queues) {
    long long count = 0;
    int64_t batch_size = 64;
    int64_t batch_all_count = qps * 180;
    long long start_time = ustime();
    while(true) {
        for(int64_t j = 0; j < (batch_all_count/batch_size); j++) {
            vector_t* querys = vector_new();
            for(int64_t k = 0; k < batch_size; k++) {
                long random = random_long(0, KEY_SIZE-1);
                sds key = sds_cat_printf(sds_empty() ,"xxxxxxxxxxx_%010d_%010d", random, random);
                vector_push(querys, latte_rocksdb_query_new(
                    cf,
                    key
                ));
            }

            bool sender = false;
            while(!sender) {
                for(int64_t k = 0; k < thread_num; k++) {
                    int64_t thread_index = (j + k) % thread_num;
                    assert(thread_index < thread_num);
                    batch_queue_t* batch_queue = batch_queues + thread_index;
                    latte_mutex_lock(batch_queue->mutex);
                    if ( list_length(batch_queue->queue) >= batch_queue->max_len) {
                        latte_mutex_unlock(batch_queue->mutex);
                        continue;
                    }
                    list_add_node_tail(batch_queue->queue, task_new(GET, querys));
                    pthread_cond_signal(&batch_queue->not_empty);
                    latte_mutex_unlock(batch_queue->mutex);
                    sender = true;
                    break;
                }
                if (!sender) {
                    usleep(100);
                }
            }
            
        }
        count++;
        long long time =  start_time + 1000 * 1000 - ustime();
        if (time > 0) {
            usleep(time);
            if(count % 60 == 0) {
                for(int64_t k = 0; k < thread_num; k++) {
                    batch_queue_t* batch_queue = batch_queues + k;
                    LATTE_LIB_LOG(LOG_INFO, "thread-%d: batch_time:%llu(us),count:%llu,avg:%llu(us)", k, batch_queue->batch_time, batch_queue->batch_count, batch_queue->batch_time/batch_queue->batch_count);
                }
            }
        }  
        start_time = ustime();
    }
}


int test_rocksdb_batch_set_get() {
    latte_rocksdb_t* db = latte_rocksdb_new();
    latte_rocksdb_family_t* family_info = latte_rocksdb_family_create(db, sds_new("default"));
    latte_error_t * error = latte_rocksdb_open(db, "batch_set_get", 1, &family_info);
    assert(error_is_ok(error));

    
    size_t thread_num = 1;
    batch_queue_t* ts = zmalloc(sizeof(batch_queue_t) * thread_num);
    for(size_t i = 0; i < thread_num; i++) {
        batch_queue_t* t = ts + i;
        batch_queue_init(t, i, db, 100UL);
        if (pthread_create(&t->thread_id, NULL, swapThreadMain, t)) {
            return -1;
        }
        
    }
    
    // write_data(family_info->cf_handle, thread_num, ts);
    // printf("write data over\n");
    // error = latte_rocksdb_flush(db);
    // assert(error_is_ok(error));
    // printf("flush data over\n");
    read_data(family_info->cf_handle, 20, thread_num, ts);

    return 1;
}