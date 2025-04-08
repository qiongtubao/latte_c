
#ifndef __LATTE_OBJECT_LIST_H
#define __LATTE_OBJECT_LIST_H
#include "object.h"

latte_object_t* quick_list_object_new(int fill, int compress);
//已经被淘汰了
latte_object_t* zip_list_object_new();
latte_object_t* list_pack_object_new();


#endif