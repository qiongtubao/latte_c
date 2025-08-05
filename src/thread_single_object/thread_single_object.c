#include "thread_single_object.h"
#include "dict/dict_plugins.h"
#include "debug/latte_debug.h"
#include "utils/utils.h"

thread_single_entry_t* thread_single_entry_new(void (*destroy)(void* arg)) {
    thread_single_entry_t* entry = (thread_single_entry_t*)zmalloc(sizeof(thread_single_entry_t));
    entry->destroy = destroy;
    return entry;
}

void thread_single_entry_delete(thread_single_entry_t* entry) {
    zfree(entry);
}

void dict_thread_single_object_register_destructor(dict_t* dict, void* value) {
    thread_single_entry_delete((thread_single_entry_t*)value);
}


dict_func_t dict_thread_single_register_object = {
    .hashFunction = dict_char_hash,
    .keyCompare = dict_char_key_compare,
    .valDestructor = dict_thread_single_object_register_destructor,
};


thread_single_object_manager_t* thread_single_object_manager = NULL;

void thread_single_object_module_init() {
    if (thread_single_object_manager != NULL) {
        return;
    }
    thread_single_object_manager = (thread_single_object_manager_t*)zmalloc(sizeof(thread_single_object_manager_t));
    thread_single_object_manager->dict = dict_new(&dict_thread_single_register_object);
    pthread_key_create(&thread_single_object_manager->latte_thread_key, NULL);
}

void thread_single_object_destroy(char* name, void* value) {
    dict_entry_t* entry = dict_find(thread_single_object_manager->dict, name);
    latte_assert(entry != NULL) ;
    thread_single_entry_t* entry_obj = (thread_single_entry_t*)dict_get_entry_val(entry);
    latte_assert(entry_obj != NULL);
    latte_assert(entry_obj->destroy != NULL);
    entry_obj->destroy(value);  
}

void thread_single_object_module_destroy() {
    if (thread_single_object_manager == NULL) {
        return;
    }
    dict_t* object = (dict_t*)pthread_getspecific(thread_single_object_manager->latte_thread_key);
    if (object != NULL) {
        latte_iterator_t* iter = dict_get_latte_iterator(object);
        while (latte_iterator_has_next(iter)) {
            latte_pair_t* val = latte_iterator_next(iter);
            if (val != NULL) {
                thread_single_object_destroy(val->key, val->value);
            }
        }
        dict_delete(object);
        pthread_setspecific(thread_single_object_manager->latte_thread_key, NULL);
    }
    
    
    if (thread_single_object_manager != NULL) {
        pthread_key_delete(thread_single_object_manager->latte_thread_key);
        dict_delete(thread_single_object_manager->dict);
        zfree(thread_single_object_manager);
        thread_single_object_manager = NULL;
    }

    
}
void thread_single_object_register(char* name, void (*destroy)(void* arg)) {
    latte_assert(thread_single_object_manager != NULL);
    thread_single_entry_t* entry = thread_single_entry_new(destroy);
    entry->destroy = destroy;
    latte_assert_with_info(DICT_OK == dict_add(thread_single_object_manager->dict, name, entry), "thread_single_object_register failed");
}

void thread_single_object_unregister(char* name) {
    latte_assert(thread_single_object_manager != NULL);
    int result = dict_delete_key(thread_single_object_manager->dict, name);
    latte_assert_with_info(DICT_OK == result , "thread_single_object_unregister failed");
    if (dict_size(thread_single_object_manager->dict) == 0) {
        thread_single_object_module_destroy();
    }
}


dict_func_t dict_thread_single_object_func = {
    .hashFunction = dict_char_hash,
    .keyCompare = dict_char_key_compare
};


void* thread_single_object_get(char* name) {
    latte_assert_with_info(thread_single_object_manager != NULL, "thread_single_object_manager is not initialized");
    dict_t* object = (dict_t*)pthread_getspecific(thread_single_object_manager->latte_thread_key);
    if (object == NULL) {
        return NULL;
    }
    dict_entry_t* entry = dict_find(object, name);
    if (entry == NULL) {
        return NULL;
    }
    return dict_get_entry_val(entry);
}



int thread_single_object_delete(char* name) {
    latte_assert_with_info(thread_single_object_manager != NULL, "thread_single_object_manager is not initialized");
    dict_t* object = (dict_t*)pthread_getspecific(thread_single_object_manager->latte_thread_key);
    if (object == NULL) {
        return 0;
    }
    dict_entry_t* entry = dict_find(object, name);
    void* value = dict_get_entry_val(entry);
    if (value == NULL) {
        return 0;
    }
    thread_single_object_destroy(name, value);
    dict_delete_key(object, name);
    if (dict_size(object) == 0) {
        dict_delete(object);
        pthread_setspecific(thread_single_object_manager->latte_thread_key, NULL);
    }
    return 1;
}

void* thread_single_object_set(char* name, void* value) {
    latte_assert_with_info(thread_single_object_manager != NULL, "thread_single_object_manager is not initialized");
    dict_entry_t* entry = dict_find(thread_single_object_manager->dict, name);
    latte_assert_with_info(entry != NULL, "thread_single_object_set failed");
    dict_t* object = (dict_t*)pthread_getspecific(thread_single_object_manager->latte_thread_key);
    if (object == NULL) {
        object = dict_new(&dict_thread_single_object_func);
        pthread_setspecific(thread_single_object_manager->latte_thread_key, object);
    }
    dict_add(object, name, value);
    return value;
}