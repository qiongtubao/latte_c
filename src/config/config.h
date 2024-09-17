
#ifndef __LATTE_CONFIG_H
#define __LATTE_CONFIG_H

#include "sds/sds.h"
#include "dict/dict.h"

typedef struct configRule {
    int (*update)(struct configRule* rule, void* old_value, void *new_value);
    sds (*writeConfigSds)(sds config, char* key,struct configRule* rule);
    void (*releaseValue)(void* rule);
    int (*load)(struct configRule* rule, char** argv, int argc);
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
int configSetLongLong(config* c, char* key, long long value);
long long configGetLongLong(config* c, char* key);
int configSetArgv(configRule* c, char** argv, int argc);
/** load config **/
int loadConfigFromString(config* config, char* configstr, size_t configstrlen);
int loadConfigFromArgv(config* config, char** argv, int argc);
int loadConfigFromFile(config* c, char *filename);

/* num attribute */
int longLongUpdate(configRule* rule, void* old_value, void* new_value);
sds longLongWriteConfigSds(sds config, char* key, configRule* rule);
void longLongReleaseValue(void* value);
int longLongLoadConfig(configRule* rule, char** argv, int argc);


/* sds attribute */
int sdsUpdate(configRule* rule, void* old_value, void* new_value);
sds sdsWriteConfigSds(sds config, char* key, configRule* rule);
void sdsReleaseValue(void* value);
int sdsLoadConfig(configRule* rule, char** argv, int argc);

typedef struct arraySds {
    int len;
    void** value;
} arraySds;
/* array attribute */
int arraySdsUpdate(configRule* rule, void* old_value, void* new_value);
sds arraySdsWriteConfigSds(sds config, char* key, configRule* rule);
int arraySdsLoadConfig(configRule* rule, char** argv, int argc);
void arraySdsReleaseValue(void* value);
struct arraySds* configGetArray(config* c, char* key);

/* object attribute */
int objectUpdate(configRule* rule, void* old_value, void* new_value);

#define LL_CONFIG_INIT(v) \
    { .update = longLongUpdate, \
    .writeConfigSds = longLongWriteConfigSds,\
     .releaseValue = longLongReleaseValue, \
     .load = longLongLoadConfig,\
      .value = v }

#define SDS_CONFIG_INIT(v) \
    { .update = sdsUpdate, \
    .writeConfigSds = sdsWriteConfigSds, \
    .releaseValue = sdsReleaseValue, \
    .load = sdsLoadConfig, \
    .value = v }
#endif