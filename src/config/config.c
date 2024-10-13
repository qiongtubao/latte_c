#include "config.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "utils/utils.h"
#include "dict/dict_plugins.h"
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



dict_func_t ruleDictType = {
    dictCharHash,
    NULL,
    NULL,
    dictCharKeyCompare,
    dictSdsDestructor,
    NULL,
    NULL
};

struct config* createConfig() {
    config* c = zmalloc(sizeof(config));
    c->rules = dict_new(&ruleDictType);
    return c;
}

void releaseConfig(config* c) {
    dict_iterator_t* di = dict_get_iterator(c->rules);
    dict_entry_t*de;
    while ((de = dict_next(di)) != NULL) {
        configRule* rule = dict_get_val(de);
        rule->releaseValue(rule->value);
        rule->value = NULL;
    }
    dict_iterator_delete(di);
    dict_delete(c->rules);
    zfree(c);
}

configRule* getConfigRule(struct config* c, char* key) {
    dict_entry_t* entry = dict_find(c->rules, key);
    if (entry == NULL) return NULL;
    return dict_get_val(entry);
}

void* configGet(config* c, char* key) {
    configRule* rule = getConfigRule(c, key);
    if (rule == NULL) return NULL;
    return rule->value;
}

sds_t configGetSds(config* c, char* key) {
    return configGet(c, key);
}

int configGetInt(config* c, char* key) {
    return (int)(configGet(c, key));
}

long long configGetLongLong(config* c, char* key) {
    return ((long long)(configGet(c, key)));
}

int configSetSds(config* c, char* key, sds_t value) {
    dict_entry_t* entry = dict_find(c->rules, key);
    if (entry == NULL) return 0;
    configRule* rule = dict_get_val(entry);
    if (rule->update(rule, rule->value, value)) {
        rule->value = value;
    }
    return 1;
}

int configSetInt(config* c, sds_t key, int value) {
    dict_entry_t* entry = dict_find(c->rules, key);
    if (entry == NULL) return 0;
    configRule* rule = dict_get_val(entry);
    if (rule->update(rule, rule->value, &value)) {
        rule->value = &value;
    }
    return 1;
}

int configSetLongLong(config* c, char* key, long long value) {
    dict_entry_t* entry = dict_find(c->rules, key);
    if (entry == NULL) return 0;
    configRule* rule = dict_get_val(entry);
    if (rule->update(rule, rule->value, ll2ptr(value))) {
        rule->value = *(void **)(&value);
    }
    return 1;
}

int registerConfig(config* c, sds_t key, configRule* rule) {
    return dict_add(c->rules, key, rule);
}


int loadConfigFromString(config* config, char* configstr, size_t configstrlen) {
    const char *err = NULL;
    int linenum = 0, totlines, i;
    sds_t *lines;
    lines = sds_split_len(configstr, configstrlen, "\n",1,&totlines);
    for (i = 0; i < totlines; i++) {
        sds_t *argv;
        int argc;

        linenum = i+1;
        lines[i] = sds_trim(lines[i]," \t\r\n");
        /* Skip comments and blank lines */
        if (lines[i][0] == '#' || lines[i][0] == '\0') continue;

        /* Split into arguments */
        argv = sds_split_args(lines[i],&argc);
        if (argv == NULL) {
            err = "Unbalanced quotes in configuration line";
            sds_free_splitres(argv, argc);
            goto loaderr;
        }
        // sdstolower(argv[0]);
        configRule* rule = getConfigRule(config, argv[0]);
        if (rule == NULL) {
            err = "Bad directive or wrong number of arguments";
            sds_free_splitres(argv, argc);
            goto loaderr;
        }
        if(!configSetArgv(rule, argv, argc)) {
            sds_free_splitres(argv, argc);
            err = "load config wrong";
            goto loaderr;
        }
        sds_free_splitres(argv, argc);
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
    sds_t format_conf_sds = sds_empty();
    int i = 0;
    while(i < argc) {
        if (argv[i][0] == '-' && argv[i][1] == '-') {
            /** Option name **/
            if (sds_len(format_conf_sds)) format_conf_sds = sds_cat(format_conf_sds, "\n");
            format_conf_sds = sds_cat(format_conf_sds, argv[i] + 2);
            format_conf_sds = sds_cat(format_conf_sds, " ");
        } else {
            /* maybe value is some params */
            /* Option argument*/
            format_conf_sds = sds_catrepr(format_conf_sds, argv[i], strlen(argv[i]));
            format_conf_sds = sds_cat(format_conf_sds, " ");
        }
        i++;
    }
    int result = loadConfigFromString(c, format_conf_sds, sds_len(format_conf_sds));
    sds_free(format_conf_sds);
    return result;
}
#define CONFIG_MAX_LINE 1024
int loadConfigFromFile(config* c, char *filename) {
    sds_t config = sds_empty();
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
            config = sds_cat(config,buf);
        fclose(fp);
    }
    int result = loadConfigFromString(c, config, sds_len(config));
    sds_free(config);
    return result;
}


/* sds_t config gen*/
int sdsUpdate(configRule* rule, void* old_value, void* new_value) {
    rule->value = new_value;
    return 1;
}

sds_t sdsWriteConfigSds(sds_t config, char* key, configRule* rule) {
    return sds_cat_printf(config, "%s %s", key, rule->value);
}

void sdsReleaseValue(void* value) {
    if(value) {
        sds_free(value);
    }
}
int sdsLoadConfig(configRule* rule, char** argv, int argc) {
    if (argc != 2) return 0;
    rule->value = sds_new_len(argv[1], strlen(argv[1]));
    return 1;
}

/* num config gen*/
int longLongUpdate(configRule* rule, void* old_value, void* new_value) {
    rule->value = new_value;
    return 1;
}

sds_t longLongWriteConfigSds(sds_t config, char* key, configRule* rule) {
    long long ll = (long long)rule->value;
    return sds_cat_printf(config, "%s %lld", key, ll);
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

/* sds_t config gen*/
int arraySdsUpdate(configRule* rule, void* old_value, void* new_value) {
    rule->value = new_value;
    return 1;
}

sds_t arraySdsWriteConfigSds(sds_t config, char* key, configRule* rule) {
    struct arraySds* value = (struct arraySds*)rule->value;
    sds_t result = sds_cat_printf(config, "%s", key);
    for(int i = 0; i < value->len; i++) {
        result = sds_cat_printf(config, " %s", value->value[i]);
    }
    return result;
}

void arraySdsReleaseValue(void* _value) {
    if (_value == NULL) return;
    struct arraySds* value = (struct arraySds*)_value;
    for(int i = 0; i < value->len; i++) {
        sds_free(value->value[i]);
    }
    zfree(value->value);
    zfree(value);
}

int arraySdsLoadConfig(configRule* rule, char** argv, int argc) {
    if (argc < 2) return 0;
    sds* array = zmalloc(sizeof(sds*) * (argc - 1));
    struct arraySds* value = zmalloc(sizeof(struct arraySds));
    for(int i = 1; i < argc; i++) {
        array[i - 1] = sds_new(argv[i]);
    }
    value->len = (argc - 1);
    value->value = array;
    rule->value = value;
    return 1;
}