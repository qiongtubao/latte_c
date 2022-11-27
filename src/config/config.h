

#include "sds/sds.h"
#include "dict/dict.h"

typedef struct configRule {
    int *(*init)(struct configRule* rule, void* value);
    int *(*update)(struct configRule* rule, void* old_value, void *new_value);
    sds *(*toSds)(struct configRule* rule);
    void *(*releaseValue)(void* rule);
    void* value;
} configRule;

typedef struct config {
    dict* rules;
} config;
struct config* createConfig();
void releaseConfig(struct config* c);
int registerConfig(config* c, sds key, configRule* rule);
sds configGetSds(config* c, char* key);
int configSetSds(config* c, char* key, sds value);
int configGetInt(config* c, char* key);
int configSetInt(config* c, char* key, int value);
