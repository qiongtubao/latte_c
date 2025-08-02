#include "thread_single_object.h"
#include "dict/dict_plugins.h"
#include "debug/latte_debug.h"

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

void thread_single_object_manager_init() {
    if (thread_single_object_manager != NULL) {
        return;
    }
    thread_single_object_manager = (thread_single_object_manager_t*)zmalloc(sizeof(thread_single_object_manager_t));
    thread_single_object_manager->dict = dict_new(&dict_thread_single_register_object);
    pthread_key_create(&thread_single_object_manager->latte_thread_key, NULL);
}

void thread_single_object_manager_destroy() {
    if (thread_single_object_manager != NULL) {
        dict_delete(thread_single_object_manager->dict);
        zfree(thread_single_object_manager);
        thread_single_object_manager = NULL;
    }
}

void thread_single_object_register(sds name, void (*destroy)(void* arg)) {
    latte_assert(thread_single_object_manager != NULL);
    thread_single_entry_t* entry = thread_single_entry_new(destroy);
    dict_add(thread_single_object_manager->dict, name, entry);
}


dict_func_t dict_thread_single_object_func = {
    .hashFunction = dict_char_hash,
    .keyCompare = dict_char_key_compare
};


void* thread_single_object_get(char* name) {
    thread_single_object_manager_init();
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

void thread_single_object_delete(char* name) {
    thread_single_object_manager_init();
    dict_t* object = (dict_t*)pthread_getspecific(thread_single_object_manager->latte_thread_key);
    if (object == NULL) {
        return;
    }
    dict_entry_t* entry = dict_find(object, name);
    void* value = dict_get_entry_val(entry);
    if (value == NULL) {
        return;
    }
    entry = dict_find(thread_single_object_manager->dict, name);
    latte_assert(entry != NULL) ;
    thread_single_entry_t* entry_obj = (thread_single_entry_t*)dict_get_entry_val(entry);
    entry_obj->destroy(value);
    dict_delete_key(object, name);
}

void* thread_single_object_set(char* name, void* value) {
    thread_single_object_manager_init();
    dict_t* object = (dict_t*)pthread_getspecific(thread_single_object_manager->latte_thread_key);
    if (object == NULL) {
        object = dict_new(&dict_thread_single_object_func);
        pthread_setspecific(thread_single_object_manager->latte_thread_key, object);
    }
    dict_add(object, name, value);
    return value;
}