#include "config.h"



uint64_t dictSdsHash(const void *key) {
    return dictGenHashFunction((unsigned char*)key, strlen((char*)key));
}

int dictCharKeyCompare(void* privdata, const void *key1,
    const void *key2) {
    int l1, l2;
    DICT_NOTUSED(privdata);
    l1 = strlen(key1);
    l2 = strlen(key2);
    if (l1 != l2) return 0;
    return memcmp(key1, key2, l1) == 0;
}
void dictSdsDestructor(void *privdata, void *val)
{
    DICT_NOTUSED(privdata);
    sdsfree(val);
}
dictType ruleDictType = {
    dictSdsHash,
    NULL,
    NULL,
    dictCharKeyCompare,
    dictSdsDestructor,
    NULL,
    NULL
};

struct config* createConfig() {
    config* c = zmalloc(sizeof(config));
    c->rules = dictCreate(&ruleDictType);
    return c;
}

void releaseConfig(config* c) {
    dictIterator* di = dictGetIterator(c->rules);
    dictEntry *de;
    while ((de = dictNext(di)) != NULL) {
        configRule* rule = dictGetVal(de);
        rule->releaseValue(rule->value);
        rule->value = NULL;
    }
    dictReleaseIterator(di);
    dictRelease(c->rules);
    zfree(c);
}

void* configGet(config* c, char* key) {
    dictEntry* entry = dictFind(c->rules, key);
    if (entry == NULL) return NULL;
    configRule* rule = dictGetVal(entry);
    return rule->value;
}
sds configGetSds(config* c, char* key) {
    return configGet(c, key);
}

int configGetInt(config* c, char* key) {
    return (*(int*)(configGet(c, key)));
}

int configSetSds(config* c, char* key, sds value) {
    dictEntry* entry = dictFind(c->rules, key);
    if (entry == NULL) return 0;
    configRule* rule = dictGetVal(entry);
    if (rule->update(rule, rule->value, value)) {
        rule->value = value;
    }
    return 1;
}

int configSetInt(config* c, sds key, int value) {
    dictEntry* entry = dictFind(c->rules, key);
    if (entry == NULL) return 0;
    configRule* rule = dictGetVal(entry);
    if (rule->update(rule, rule->value, &value)) {
        rule->value = &value;
    }
    return 1;
}


int registerConfig(config* c, sds key, configRule* rule) {
    return dictAdd(c->rules, key, rule);
}