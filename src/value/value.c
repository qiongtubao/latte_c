#include "value.h"
#include "iterator/iterator.h"

value* valueCreate() {
    value* v = zmalloc(sizeof(value));
    v->type = UNDEFINED;
    v->value.l_value = 0;
    return v;
}

void valueClean(value* v) {
    switch (v->type) {
        case SDSS:
            sdsfree(v->value.sds_value);
            v->value.sds_value = NULL;
            break;
        case LISTTYPS: {
            Iterator* it = vectorGetIterator(v->value.list_value);
            while(iteratorHasNext(it)) {
                value* cv = iteratorNext(it);
                valueRelease(cv);
            }
            iteratorRelease(it);
            vectorRelease(v->value.list_value);
            v->value.list_value = NULL;
        }
            break;
        case MAPTYPES:
            dictRelease(v->value.map_value);
            v->value.map_value = NULL;
        default:
        
    }
}
void valueRelease(value* v) {
    valueClean(v);
    zfree(v);
}

void valueSetSds(value* v, sds s) {
    valueClean(v);
    v->value.sds_value = s;
}
void valueSetLong(value* v, long l) {
    valueClean(v);
    v->value.l_value = l;
}
void valueSetDouble(value* v, double d) {
    valueClean(v);
    v->value.d_value = d;
}
void valueSetBool(value* v, bool b) {
    valueClean(v);
    v->value.bool_value = b;
}
void valueSetArray(value* v, vector* ve) {
    valueClean(v);
    v->value.list_value = ve;
}
void valueSetDict(value* v, dict* d) {
    valueClean(v);
    v->value.map_value = d;
}



sds valueGetSds(value* v) {
    return v->value.sds_value;
}
long valueGetLong(value* v) {
    return v->value.l_value;
}
double valueGetDouble(value* v) {
    return v->value.d_value;
}
bool valueGetBool(value* v) {
    return v->value.bool_value;
}
vector* valueGetArray(value* v) {
    return v->value.list_value;
}
dict* valueGetDict(value* v) {
    return v->value.map_value;
}


