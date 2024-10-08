#include "config.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "utils/utils.h"
#include "dict_plugins/dict_plugins.h"
void* ll2ptr(long long value) {
    if (sizeof(void*) == sizeof(long long)) {
        return (void*)(value);
    } else {
        return &value;
    }
}

long long ptr2ll(void* value) {
    if (sizeof(void*) == sizeof(long long)) {
        return *(long long*)(&value);
    } else {
        return *(long long *)value;
    }
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

configRule* getConfigRule(struct config* c, char* key) {
    dictEntry* entry = dictFind(c->rules, key);
    if (entry == NULL) return NULL;
    return dictGetVal(entry);
}

void* configGet(config* c, char* key) {
    configRule* rule = getConfigRule(c, key);
    if (rule == NULL) return NULL;
    return rule->value;
}

sds configGetSds(config* c, char* key) {
    return configGet(c, key);
}

int configGetInt(config* c, char* key) {
    return (int)(configGet(c, key));
}

long long configGetLongLong(config* c, char* key) {
    return ((long long)(configGet(c, key)));
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

int configSetLongLong(config* c, char* key, long long value) {
    dictEntry* entry = dictFind(c->rules, key);
    if (entry == NULL) return 0;
    configRule* rule = dictGetVal(entry);
    if (rule->update(rule, rule->value, ll2ptr(value))) {
        rule->value = *(void **)(&value);
    }
    return 1;
}

int registerConfig(config* c, sds key, configRule* rule) {
    return dictAdd(c->rules, key, rule);
}


int loadConfigFromString(config* config, char* configstr, size_t configstrlen) {
    const char *err = NULL;
    int linenum = 0, totlines, i;
    sds *lines;
    lines = sdssplitlen(configstr, configstrlen, "\n",1,&totlines);
    for (i = 0; i < totlines; i++) {
        sds *argv;
        int argc;

        linenum = i+1;
        lines[i] = sdstrim(lines[i]," \t\r\n");
        /* Skip comments and blank lines */
        if (lines[i][0] == '#' || lines[i][0] == '\0') continue;

        /* Split into arguments */
        argv = sdssplitargs(lines[i],&argc);
        if (argv == NULL) {
            err = "Unbalanced quotes in configuration line";
            sdsfreesplitres(argv, argc);
            goto loaderr;
        }
        // sdstolower(argv[0]);
        configRule* rule = getConfigRule(config, argv[0]);
        if (rule == NULL) {
            err = "Bad directive or wrong number of arguments";
            sdsfreesplitres(argv, argc);
            goto loaderr;
        }
        if(!configSetArgv(rule, argv, argc)) {
            sdsfreesplitres(argv, argc);
            err = "load config wrong";
            goto loaderr;
        }
        sdsfreesplitres(argv, argc);
    }
    return 1;
loaderr:
    fprintf(stderr, "\n*** FATAL CONFIG  ERROR ***\n");
    fprintf(stderr, "Reading the configuration, at line %d\n", linenum);
    fprintf(stderr, ">>> '%s'\n", lines[i]);
    fprintf(stderr, "%s\n", err);
    return 0;
}

int configSetArgv(configRule* rule, char** argv, int argc) {
    return rule->load(rule, argv, argc);
}

int loadConfigFromArgv(config* c, char** argv, int argc) {
    sds format_conf_sds = sdsempty();
    int i = 0;
    while(i < argc) {
        if (argv[i][0] == '-' && argv[i][1] == '-') {
            /** Option name **/
            if (sdslen(format_conf_sds)) format_conf_sds = sdscat(format_conf_sds, "\n");
            format_conf_sds = sdscat(format_conf_sds, argv[i] + 2);
            format_conf_sds = sdscat(format_conf_sds, " ");
        } else {
            /* maybe value is some params */
            /* Option argument*/
            format_conf_sds = sdscatrepr(format_conf_sds, argv[i], strlen(argv[i]));
            format_conf_sds = sdscat(format_conf_sds, " ");
        }
        i++;
    }
    int result = loadConfigFromString(c, format_conf_sds, sdslen(format_conf_sds));
    sdsfree(format_conf_sds);
    return result;
}
#define CONFIG_MAX_LINE 1024
int loadConfigFromFile(config* c, char *filename) {
    sds config = sdsempty();
    char buf[CONFIG_MAX_LINE+1];
    FILE *fp;

    /* Load the file content */
    if (filename) {
        if ((fp = fopen(filename,"r")) == NULL) {
            printf(
                    "Fatal error, can't open config file '%s': %s\n",
                    filename, strerror(errno));
            exit(1);
        }
        while(fgets(buf,CONFIG_MAX_LINE+1,fp) != NULL)
            config = sdscat(config,buf);
        fclose(fp);
    }
    int result = loadConfigFromString(c, config, sdslen(config));
    sdsfree(config);
    return result;
}


/* sds config gen*/
int sdsUpdate(configRule* rule, void* old_value, void* new_value) {
    rule->value = new_value;
    return 1;
}

sds sdsWriteConfigSds(sds config, char* key, configRule* rule) {
    return sdscatprintf(config, "%s %s", key, rule->value);
}

void sdsReleaseValue(void* value) {
    if(value) {
        sdsfree(value);
    }
}
int sdsLoadConfig(configRule* rule, char** argv, int argc) {
    if (argc != 2) return 0;
    rule->value = sdsnewlen(argv[1], strlen(argv[1]));
    return 1;
}

/* num config gen*/
int longLongUpdate(configRule* rule, void* old_value, void* new_value) {
    rule->value = new_value;
    return 1;
}

sds longLongWriteConfigSds(sds config, char* key, configRule* rule) {
    long long ll = (long long)rule->value;
    return sdscatprintf(config, "%s %lld", key, ll);
}

#define UNUSED(x) ((void)(x))
void longLongReleaseValue(void* value) {
    UNUSED(value);
}

int longLongLoadConfig(configRule* rule, char** argv, int argc) {
    if (argc != 2) return 0;
    long long ll = 0;
    if (!string2ll(argv[1], strlen(argv[1]), &ll)) {
        return 0;
    }
    rule->value = (void*)ll;
    return 1;
}



struct arraySds* configGetArray(config* c, char* key) {
    return ((struct arraySds*)(configGet(c, key)));
}

/* sds config gen*/
int arraySdsUpdate(configRule* rule, void* old_value, void* new_value) {
    rule->value = new_value;
    return 1;
}

sds arraySdsWriteConfigSds(sds config, char* key, configRule* rule) {
    struct arraySds* value = (struct arraySds*)rule->value;
    sds result = sdscatprintf(config, "%s", key);
    for(int i = 0; i < value->len; i++) {
        result = sdscatprintf(config, " %s", value->value[i]);
    }
    return result;
}

void arraySdsReleaseValue(void* _value) {
    if (_value == NULL) return;
    struct arraySds* value = (struct arraySds*)_value;
    for(int i = 0; i < value->len; i++) {
        sdsfree(value->value[i]);
    }
    zfree(value->value);
    zfree(value);
}

int arraySdsLoadConfig(configRule* rule, char** argv, int argc) {
    if (argc < 2) return 0;
    sds* array = zmalloc(sizeof(sds*) * (argc - 1));
    struct arraySds* value = zmalloc(sizeof(struct arraySds));
    for(int i = 1; i < argc; i++) {
        array[i - 1] = sdsnew(argv[i]);
    }
    value->len = (argc - 1);
    value->value = array;
    rule->value = value;
    return 1;
}