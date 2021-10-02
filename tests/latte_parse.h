
#include <stdio.h>
#include "../libs/sds.h"
#include "../libs/zmalloc.h"
#include "../libs/utils.h"

#define VALUE_TYPE_NONE 0
#define VALUE_TYPE_LONGLONG 1
#define VALUE_TYPE_DOUBLE  2
#define VALUE_TYPE_SDS 3
#define VALUE_TYPE_LONGDOUBLE 4
union all_type {
    long long i;
    long double f;
    double d;
    sds s;
} __attribute__ ((packed, aligned(1))) all_type;

typedef struct g_counter_meta {
    unsigned long long gid:4;
    unsigned long long data_type:4;
    unsigned long long vcu: 56;
    union all_type value;
    sds v;
} g_counter_meta;
g_counter_meta* create_g_counter_meta(long long gid, long long vcu);
void free_g_counater_maeta(g_counter_meta* meta);
int g_counter_meta_to_str(g_counter_meta* del, char* buf);
typedef int (*GetGMetaFunc)(void* data, int index, g_counter_meta* value);
int g_counter_metas_to_str(void* data, GetGMetaFunc fun, char*buf, int size);
int gcounter_meta_set_value(g_counter_meta* meta, int type, void* v, int parse);
int str_to_g_counter_metas(char* buf, int len, g_counter_meta** metas);
sds read_value(char* buf, int len, long long* type, union all_type* value, int* offset);
int str_2_value_and_g_counter_metas(sds info, long long*type, union all_type* value, g_counter_meta** g);