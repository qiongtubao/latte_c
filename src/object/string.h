
#ifndef __LATTE_OBJECT_STRING_H
#define __LATTE_OBJECT_STRING_H
#include <sds/sds.h>
#include "object.h"
#define OBJ_SHARED_INTEGERS 10000
typedef struct shared_integers_objects_t {
    latte_object_t* integers[OBJ_SHARED_INTEGERS];
} shared_integers_objects_t;
struct shared_integers_objects_t shared_integers;

void init_shared_integers();

latte_object_t *raw_string_object_new(const char *ptr, size_t len);
latte_object_t *embedded_string_object_new(const char *ptr, size_t len) ;
latte_object_t *string_object_new(const char* ptr, size_t len) ;
latte_object_t *try_raw_string_object_new(const char* ptr, size_t len);
latte_object_t *try_string_object_new(const char* ptr, size_t len);
latte_object_t *string_object_from_long_long_with_options_new(long long value, int valueobj);
latte_object_t *string_object_from_long_long(long long value);
latte_object_t *string_object_from_long_long_for_value(long long value);
latte_object_t *string_object_from_long_double(long double value, int humanfriendly);
latte_object_t *dup_string_object(const latte_object_t* o);
void trim_string_object_if_needed(latte_object_t* o);
int compare_string_objects_with_flags(latte_object_t* o, latte_object_t *b, int flags);
int compare_string_objects(latte_object_t* a, latte_object_t* b);
int collate_string_objects(latte_object_t* a, latte_object_t* b);
int equal_string_objects(latte_object_t* a, latte_object_t* b);
size_t string_object_len(latte_object_t* o);
int get_double_from_object(const latte_object_t* o, double *target);
// int get_double_from_object_or_reply()
int get_long_double_from_object(latte_object_t* o, long double *target);
int get_long_long_from_object(latte_object_t* o, long long *target);
int get_sds_from_object(latte_object_t* o, sds* target);

latte_object_t* try_object_encoding(latte_object_t* o);
latte_object_t* get_decode_object(latte_object_t* o);
#endif
