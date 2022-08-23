#include "latte_list.h"


latte_list *latteListAddNodeHead(latte_list *list, void *value) {
    if (list == NULL) return NULL;
    return list->type->listAddNode(list, NULL, value, 0);
}

latte_list *latteListAddNodeTail(latte_list *list, void *value) {
    if (list == NULL) return NULL;
    return list->type->listAddNode(list, NULL, value, -1);
}

latte_list *latteListInsertNode(latte_list *list, latte_list_node *old_node, void *value, int after) {
    if (list == NULL) return NULL;
    return list->type->listAddNode(list, old_node, value, after);
}

void *latteListIndex(latte_list *list, int index) {
    if (list == NULL) return NULL;
    return list->type->index(list, index);
}

void* latteListDelByIndex(latte_list* list, int index) {
    if (list == NULL) return NULL;
    return list->type->del(list, index);
}