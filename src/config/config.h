

#include "sds/sds.h"
#include "dict/dict.h"

typedef struct configRule {
    int *(*update)(struct configRule* rule, void* old_value, void *new_value);
    sds *(*writeConfigSds)(struct configRule* rule);
    void *(*releaseValue)(void* rule);
    void *(*load)(struct configRule* rule, char** argv, int argc);
    void* value;
} configRule;

typedef struct config {
    dict* rules;
} config;
struct config* createConfig();
void releaseConfig(config* c);
configRule* getConfigRule(config* c, char* key);
int registerConfig(config* c, sds key, configRule* rule);
sds configGetSds(config* c, char* key);
int configSetSds(config* c, char* key, sds value);
int configGetInt(config* c, char* key);
int configSetInt(config* c, char* key, int value);
int configSetArgv(configRule* c, char** argv, int argc);
/** load config **/
int loadConfigFromString(config* config, char* configstr, size_t configstrlen);
int loadConfigFromArgv(config* config, char** argv, int argc);
int loadConfigFromFile(config* c, char *filename);

/* num attribute */
int numUpdate(configRule* rule, void* old_value, void* new_value);


/* sds attribute */
int sdsUpdate(configRule* rule, void* old_value, void* new_value);
sds sdsWriteConfigSds(sds config, char* key, configRule* rule);
void sdsReleaseValue(void* value);
int sdsLoadConfig(configRule* rule, char** argv, int argc);

/* array attribute */
int arrayUpdate(configRule* rule, void* old_value, void* new_value);


/* object attribute */
int objectUpdate(configRule* rule, void* old_value, void* new_value);
