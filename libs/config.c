#include "config.h"
#include <fcntl.h>
#include <sys/stat.h>
#include <stdio.h>
#include "log.h"
#include <errno.h>
#include <limits.h>
#include "adlist.h"
#include "string.h"
#define CONFIG_MAX_LINE 1024
#define CONFIG_VERSION "1.0.0"
#include "dict_plus.h"

standardConfig* findStandardConfig(Config* c, const char* name, char** err) {
    sds key = sdsnew(name);
    dictEntry* de = dictFind(c->configs, key);
    sdsfree(key);
    if (!de) {
        *err = "no config"; 
        return NULL;
    }
    standardConfig* config =  dictGetVal(de);
    return config;
}
static dictType configDict = {
    dictSdsHash,
    NULL,
    NULL,
    dictSdsKeyCompare,
    dictSdsDestructor,
    NULL
};
/*-----------------------------------------------------------------------------
 * Config file parsing
 *----------------------------------------------------------------------------*/

int yesnotoi(char *s) {
    if (!strcasecmp(s,"yes")) return 1;
    else if (!strcasecmp(s,"no")) return 0;
    else return -1;
}
static int boolConfigSet(typeData* data, int argc, sds* values, int update, char **err) {
    if(argc != 2) {
        *err = "argument error";
        return 0;
    } 
    int yn = yesnotoi(values[1]);
    if (yn == -1) {
        *err = "argument must be 'yes' or 'no'";
        return 0;
    }
    
    if (data->yesno.is_valid_fn && !data->yesno.is_valid_fn(yn, err)) {
        return 0;
    }
    // int prev = *(data.yesno.config);
    // *(data.yesno.config) = yn;
    int prev = data->yesno.data;
    data->yesno.data = yn;
    if (update && data->yesno.update_fn && !data->yesno.update_fn(yn, prev, err)) {
        // *(data.yesno.config) = prev;
        data->yesno.data = prev;
        return 0;
    }
    return 1;
}

Config* createConfig() {
    Config* config = zmalloc(sizeof(Config));
    config->configs = dictCreate(&configDict, NULL);
    return config;
}

void loadConfigFromString(struct Config* c, char *config) {
    char *err = NULL;
    int linenum = 0, totlines, i;
    int slaveof_linenum = 0;
    sds *lines;
    lines = sdssplitlen(config,strlen(config),"\n",1,&totlines);
    for (i = 0; i < totlines; i++) {
        sds *argv;
        int argc;

        linenum = i+1;
        lines[i] = sdstrim(lines[i]," \t\r\n");
        if (lines[i][0] == '#' || lines[i][0] == '\0') continue;
        /* Split into arguments */
        argv = sdssplitargs(lines[i],&argc);
        if (argv == NULL) {
            err = "Unbalanced quotes in configuration line";
            goto loaderr;
        }
        /* Skip this line if the resulting command vector is empty. */
        if (argc == 0) {
            sdsfreesplitres(argv,argc);
            continue;
        }
        sdstolower(argv[0]);
        /* Iterate the configs that are standard */
        dictEntry* de = dictFind(c->configs, argv[0]);
        if (de == NULL) {
            goto loaderr;
        }
        standardConfig* cf = dictGetVal(de);
        if (!cf->interface.set(&cf->data, argc, argv, 0, &err)) {
            goto loaderr;
        }
        sdsfreesplitres(argv,argc);
        continue;
    }
    sdsfreesplitres(lines,totlines);
    return;
loaderr:
    fprintf(stderr, "\n*** FATAL CONFIG FILE ERROR (Redis %s) ***\n",
        CONFIG_VERSION);
    fprintf(stderr, "Reading the configuration file, at line %d\n", linenum);
    fprintf(stderr, ">>> '%s'\n", lines[i]);
    fprintf(stderr, "%s\n", err);
    exit(1);
}

void loadConfig(struct Config* c, char* filename, char *options) {
    sds config = sdsempty();
    char buf[CONFIG_MAX_LINE+1];
    if (filename) {
        FILE *fp;
        if (filename[0] == '-' && filename[1] == '\0') {
            fp = stdin;
        } else {
            if ((fp = fopen(filename,"r")) == NULL) {
                serverLog(LL_WARNING,
                    "Fatal error, can't open config file '%s': %s",
                    filename, strerror(errno));
                exit(1);
            }
        }
        while(fgets(buf,CONFIG_MAX_LINE+1,fp) != NULL)
            config = sdscat(config,buf);
        if (fp != stdin) fclose(fp);
        if (options) {
            config = sdscat(config,"\n");
            config = sdscat(config,options);
        }
        loadConfigFromString(c, config);
        sdsfree(config);
    }
}

void initConfig(Config* c , standardConfig configs[]) {
    for(standardConfig *config = configs; config->name != NULL; config++) {
        dictAdd(c->configs, sdsnew(config->name), config);
        config->interface.init(&config->data);
    }
}

#define embedCommonConfig(config_name, config_alias, is_modifiable) \
    .name = (config_name), \
    .alias = (config_alias), \
    .modifiable = (is_modifiable),

#define embedConfigInterface(initfn, setfn, getfn, rewritefn) .interface = { \
    .init = (initfn), \
    .set = (setfn), \
    .get = (getfn), \
    .rewrite = (rewritefn) \
},

/* Bool Configs */
static void boolConfigInit(typeData data) {
    data.yesno.data = data.yesno.default_value;
    // *data.yesno.config = data.yesno.default_value;
}





static sds boolConfigGet(typeData data) {
    return sdsnew(data.yesno.data ? "yes" : "no");
}
/* The config rewrite state. */
struct rewriteConfigState {
    dict *option_to_line; /* Option -> list of config file lines map */
    dict *rewritten;      /* Dictionary of already processed options */
    int numlines;         /* Number of lines in current config */
    sds *lines;           /* Current lines as an array of sds strings */
    int has_tail;         /* True if we already added directives that were
                             not present in the original config file. */
    int force_all;        /* True if we want all keywords to be force
                             written. Currently only used for testing. */
};
/* Add the specified option to the set of processed options.
 * This is useful as only unused lines of processed options will be blanked
 * in the config file, while options the rewrite process does not understand
 * remain untouched. */
void rewriteConfigMarkAsProcessed(struct rewriteConfigState *state, const char *option) {
    sds opt = sdsnew(option);

    if (dictAdd(state->rewritten,opt,NULL) != DICT_OK) sdsfree(opt);
}
/* Append the new line to the current configuration state. */
void rewriteConfigAppendLine(struct rewriteConfigState *state, sds line) {
    state->lines = zrealloc(state->lines, sizeof(char*) * (state->numlines+1));
    state->lines[state->numlines++] = line;
}
#define REDIS_CONFIG_REWRITE_SIGNATURE "# Generated by CONFIG REWRITE"

/* Rewrite the specified configuration option with the new "line".
 * It progressively uses lines of the file that were already used for the same
 * configuration option in the old version of the file, removing that line from
 * the map of options -> line numbers.
 *
 * If there are lines associated with a given configuration option and
 * "force" is non-zero, the line is appended to the configuration file.
 * Usually "force" is true when an option has not its default value, so it
 * must be rewritten even if not present previously.
 *
 * The first time a line is appended into a configuration file, a comment
 * is added to show that starting from that point the config file was generated
 * by CONFIG REWRITE.
 *
 * "line" is either used, or freed, so the caller does not need to free it
 * in any way. */
void rewriteConfigRewriteLine(struct rewriteConfigState *state, const char *option, sds line, int force) {
    sds o = sdsnew(option);
    list *l = dictFetchValue(state->option_to_line,o);

    rewriteConfigMarkAsProcessed(state,option);

    if (!l && !force && !state->force_all) {
        /* Option not used previously, and we are not forced to use it. */
        sdsfree(line);
        sdsfree(o);
        return;
    }

    if (l) {
        listNode *ln = listFirst(l);
        int linenum = (long) ln->value;

        /* There are still lines in the old configuration file we can reuse
         * for this option. Replace the line with the new one. */
        listDelNode(l,ln);
        if (listLength(l) == 0) dictDelete(state->option_to_line,o);
        sdsfree(state->lines[linenum]);
        state->lines[linenum] = line;
    } else {
        /* Append a new line. */
        if (!state->has_tail) {
            rewriteConfigAppendLine(state,
                sdsnew(REDIS_CONFIG_REWRITE_SIGNATURE));
            state->has_tail = 1;
        }
        rewriteConfigAppendLine(state,line);
    }
    sdsfree(o);
}

/* Rewrite a yes/no option. */
void rewriteConfigYesNoOption(struct rewriteConfigState *state, const char *option, int value, int defvalue) {
    int force = value != defvalue;
    sds line = sdscatprintf(sdsempty(),"%s %s",option,
        value ? "yes" : "no");

    rewriteConfigRewriteLine(state,option,line,force);
}

static void boolConfigRewrite(typeData* data, const char *name, struct rewriteConfigState *state) {
    rewriteConfigYesNoOption(state, name,*(data->yesno.config), data->yesno.default_value);
}

#define createBoolConfig(name, alias, modifiable, default, is_valid, update) {\
    embedCommonConfig(name, alias, modifiable) \
    embedConfigInterface(boolConfigInit, boolConfigSet, boolConfigGet, boolConfigRewrite) \
    .data.yesno = { \
        .default_value = (default), \
        .is_valid_fn = (is_valid), \
        .update_fn = (update), \
    } \
}

int configGetBoolean(Config* c, const char* name, char** err) {
    standardConfig* config =  findStandardConfig(c, name, err);
    if(config == NULL) return -1;
    return config->data.yesno.data;
}

sds configGetSds(Config* c, const char* name, char** err) {
    standardConfig* config = findStandardConfig(c, name, err);
    if(config == NULL) return NULL;
    return config->data.string.data;
}

/* String Configs */
static void stringConfigInit(typeData* data) {
    if (data->string.convert_empty_to_null) {
        data->string.data = data->string.default_value ? sdsnew(data->string.default_value) : NULL;
    } else {
        data->string.data = sdsnew(data->string.default_value);
    }
}

static int stringConfigSet(typeData* data, int argv, sds* argc, int update, char **err) {
    if (data->string.is_valid_fn && !data->string.is_valid_fn(argc[1], err))
        return 0;
    sds prev = data->string.data;
    if (data->string.convert_empty_to_null) {
        data->string.data = argc[1] ? sdsnew(argc[1]) : NULL;
    } else {
        data->string.data = sdsnew(argc[1]);
    }
    if (update && data->string.update_fn && !data->string.update_fn(data->string.data, prev, err)) {
        sdsfree(data->string.data);
        data->string.data = prev;
        return 0;
    }
    sdsfree(prev);
    return 1;
}

static sds stringConfigGet(typeData* data) {
    return data->string.data;
}

/* Rewrite a string option. */
void rewriteConfigStringOption(struct rewriteConfigState *state, const char *option, char *value, const char *defvalue) {
    int force = 1;
    sds line;

    /* String options set to NULL need to be not present at all in the
     * configuration file to be set to NULL again at the next reboot. */
    if (value == NULL) {
        rewriteConfigMarkAsProcessed(state,option);
        return;
    }

    /* Set force to zero if the value is set to its default. */
    if (defvalue && strcmp(value,defvalue) == 0) force = 0;

    line = sdsnew(option);
    line = sdscatlen(line, " ", 1);
    line = sdscatrepr(line, value, strlen(value));

    rewriteConfigRewriteLine(state,option,line,force);
}

static void stringConfigRewrite(typeData* data, const char *name, struct rewriteConfigState *state) {
    rewriteConfigStringOption(state, name, data->string.data, data->string.default_value);
}

#define createStringConfig(name, alias, modifiable, empty_to_null, default, is_valid, update) { \
    embedCommonConfig(name, alias, modifiable) \
    embedConfigInterface(stringConfigInit, stringConfigSet, stringConfigGet, stringConfigRewrite) \
    .data.string = { \
        .data = NULL, \
        .default_value = (default), \
        .is_valid_fn = (is_valid), \
        .update_fn = (update), \
        .convert_empty_to_null = (empty_to_null), \
    } \
}


configEnum loglevel_enum[] = {
    {"debug", LL_DEBUG},
    {"verbose", LL_VERBOSE},
    {"notice", LL_NOTICE},
    {"warning", LL_WARNING},
    {NULL,0}
};

/*-----------------------------------------------------------------------------
 * Configs that fit one of the major types and require no special handling
 *----------------------------------------------------------------------------*/
//to do?
#define LOADBUF_SIZE 256
// static char loadbuf[LOADBUF_SIZE];

/* Enum configs */
static void enumConfigInit(typeData data) {
    data.enumd.data = data.enumd.default_value;
}

/*-----------------------------------------------------------------------------
 * Enum access functions
 *----------------------------------------------------------------------------*/

/* Get enum value from name. If there is no match INT_MIN is returned. */
int configEnumGetValue(configEnum *ce, char *name) {
    while(ce->name != NULL) {
        if (!strcasecmp(ce->name,name)) return ce->val;
        ce++;
    }
    return INT_MIN;
}

/* Get enum name from value. If no match is found NULL is returned. */
const char *configEnumGetName(configEnum *ce, int val) {
    while(ce->name != NULL) {
        if (ce->val == val) return ce->name;
        ce++;
    }
    return NULL;
}
/* Wrapper for configEnumGetName() returning "unknown" instead of NULL if
 * there is no match. */
const char *configEnumGetNameOrUnknown(configEnum *ce, int val) {
    const char *name = configEnumGetName(ce,val);
    return name ? name : "unknown";
}

sds enumConfigGet(typeData* data) {
    return sdsnew(configEnumGetNameOrUnknown(data->enumd.enum_value,data->enumd.data));
}

/* Rewrite an enumeration option. It takes as usually state and option name,
 * and in addition the enumeration array and the default value for the
 * option. */
void rewriteConfigEnumOption(struct rewriteConfigState *state, const char *option, int value, configEnum *ce, int defval) {
    sds line;
    const char *name = configEnumGetNameOrUnknown(ce,value);
    int force = value != defval;

    line = sdscatprintf(sdsempty(),"%s %s",option,name);
    rewriteConfigRewriteLine(state,option,line,force);
}

void enumConfigRewrite(typeData* data, const char *name, struct rewriteConfigState *state) {
    rewriteConfigEnumOption(state, name, data->enumd.data, data->enumd.enum_value, data->enumd.default_value);
}



int configGetEnum(Config* c, const char* name, char** err) {
    standardConfig* config = findStandardConfig(c, name, err);
    if(config == NULL) return 0;
    return config->data.enumd.data;
}

static int enumConfigSet(typeData* data, int argv, sds* value, int update, char **err) {
    int enumval = configEnumGetValue(data->enumd.enum_value, value[1]);
    char loadbuf[LOADBUF_SIZE];
    if (enumval == INT_MIN) {
        sds enumerr = sdsnew("argument must be one of the following: ");
        configEnum *enumNode = data->enumd.enum_value;
        while(enumNode->name != NULL) {
            enumerr = sdscatlen(enumerr, enumNode->name,
                                strlen(enumNode->name));
            enumerr = sdscatlen(enumerr, ", ", 2);
            enumNode++;
        }
        sdsrange(enumerr,0,-3); /* Remove final ", ". */

        strncpy(loadbuf, enumerr, LOADBUF_SIZE);
        loadbuf[LOADBUF_SIZE - 1] = '\0';

        sdsfree(enumerr);
        *err = loadbuf;
        return 0;
    }
    if (data->enumd.is_valid_fn && !data->enumd.is_valid_fn(enumval, err))
        return 0;
    int prev = data->enumd.data;
    data->enumd.data = enumval;
    if (update && data->enumd.update_fn && !data->enumd.update_fn(enumval, prev, err)) {
        data->enumd.data = prev;
        return 0;
    }
    return 1;
}

#define createEnumConfig(name, alias, modifiable, enum, default, is_valid, update) { \
    embedCommonConfig(name, alias, modifiable) \
    embedConfigInterface(enumConfigInit, enumConfigSet, enumConfigGet, enumConfigRewrite) \
    .data.enumd = { \
        .default_value = (default), \
        .is_valid_fn = (is_valid), \
        .update_fn = (update), \
        .enum_value = (enum), \
    } \
}

#define INTEGER_CONFIG 0

#define GET_NUMERIC_TYPE(val) \
    if (data->numeric.numeric_type == NUMERIC_TYPE_INT) { \
        val = data->numeric.data.i; \
    } else if (data->numeric.numeric_type == NUMERIC_TYPE_UINT) { \
        val = data->numeric.data.ui; \
    } else if (data->numeric.numeric_type == NUMERIC_TYPE_LONG) { \
        val = data->numeric.data.l; \
    } else if (data->numeric.numeric_type == NUMERIC_TYPE_ULONG) { \
        val = data->numeric.data.ul; \
    } else if (data->numeric.numeric_type == NUMERIC_TYPE_LONG_LONG) { \
        val = data->numeric.data.ll; \
    } else if (data->numeric.numeric_type == NUMERIC_TYPE_ULONG_LONG) { \
        val = data->numeric.data.ull; \
    } else if (data->numeric.numeric_type == NUMERIC_TYPE_SIZE_T) { \
        val = (data->numeric.data.st); \
    } else if (data->numeric.numeric_type == NUMERIC_TYPE_SSIZE_T) { \
        val = (data->numeric.data.sst); \
    } else if (data->numeric.numeric_type == NUMERIC_TYPE_OFF_T) { \
        val = (data->numeric.data.ot); \
    } else if (data->numeric.numeric_type == NUMERIC_TYPE_TIME_T) { \
        val = (data->numeric.data.tt); \
    }

/* Gets a 'long long val' and sets it into the union, using a macro to get
 * compile time type check. */
#define SET_NUMERIC_TYPE(val) \
    if (data->numeric.numeric_type == NUMERIC_TYPE_INT) { \
        data->numeric.data.i = (int) val; \
    } else if (data->numeric.numeric_type == NUMERIC_TYPE_UINT) { \
        data->numeric.data.ui = (unsigned int) val; \
    } else if (data->numeric.numeric_type == NUMERIC_TYPE_LONG) { \
        data->numeric.data.l = (long) val; \
    } else if (data->numeric.numeric_type == NUMERIC_TYPE_ULONG) { \
        data->numeric.data.ul = (unsigned long) val; \
    } else if (data->numeric.numeric_type == NUMERIC_TYPE_LONG_LONG) { \
        data->numeric.data.ll = (long long) val; \
    } else if (data->numeric.numeric_type == NUMERIC_TYPE_ULONG_LONG) { \
        data->numeric.data.ull = (unsigned long long) val; \
    } else if (data->numeric.numeric_type == NUMERIC_TYPE_SIZE_T) { \
        data->numeric.data.st = (size_t) val; \
    } else if (data->numeric.numeric_type == NUMERIC_TYPE_SSIZE_T) { \
        data->numeric.data.sst = (ssize_t) val; \
    } else if (data->numeric.numeric_type == NUMERIC_TYPE_OFF_T) { \
        data->numeric.data.ot = (off_t) val; \
    } else if (data->numeric.numeric_type == NUMERIC_TYPE_TIME_T) { \
        data->numeric.data.tt = (time_t) val; \
    }

void numericConfigInit(typeData* data) {
    SET_NUMERIC_TYPE(data->numeric.default_value)
}

/* Convert a string representing an amount of memory into the number of
 * bytes, so for instance memtoll("1Gb") will return 1073741824 that is
 * (1024*1024*1024).
 *
 * On parsing error, if *err is not NULL, it's set to 1, otherwise it's
 * set to 0. On error the function return value is 0, regardless of the
 * fact 'err' is NULL or not. */
long long memtoll(const char *p, int *err) {
    const char *u;
    char buf[128];
    long mul; /* unit multiplier */
    long long val;
    unsigned int digits;

    if (err) *err = 0;

    /* Search the first non digit character. */
    u = p;
    if (*u == '-') u++;
    while(*u && isdigit(*u)) u++;
    if (*u == '\0' || !strcasecmp(u,"b")) {
        mul = 1;
    } else if (!strcasecmp(u,"k")) {
        mul = 1000;
    } else if (!strcasecmp(u,"kb")) {
        mul = 1024;
    } else if (!strcasecmp(u,"m")) {
        mul = 1000*1000;
    } else if (!strcasecmp(u,"mb")) {
        mul = 1024*1024;
    } else if (!strcasecmp(u,"g")) {
        mul = 1000L*1000*1000;
    } else if (!strcasecmp(u,"gb")) {
        mul = 1024L*1024*1024;
    } else {
        if (err) *err = 1;
        return 0;
    }

    /* Copy the digits into a buffer, we'll use strtoll() to convert
     * the digit (without the unit) into a number. */
    digits = u-p;
    if (digits >= sizeof(buf)) {
        if (err) *err = 1;
        return 0;
    }
    memcpy(buf,p,digits);
    buf[digits] = '\0';

    char *endptr;
    errno = 0;
    val = strtoll(buf,&endptr,10);
    if ((val == 0 && errno == EINVAL) || *endptr != '\0') {
        if (err) *err = 1;
        return 0;
    }
    return val*mul;
}

int numericBoundaryCheck(typeData* data, long long ll, char **err) {
    char* loadbuf[LOADBUF_SIZE];
    if (data->numeric.numeric_type == NUMERIC_TYPE_ULONG_LONG ||
        data->numeric.numeric_type == NUMERIC_TYPE_UINT ||
        data->numeric.numeric_type == NUMERIC_TYPE_SIZE_T) {
        /* Boundary check for unsigned types */
        unsigned long long ull = ll;
        unsigned long long upper_bound = data->numeric.upper_bound;
        unsigned long long lower_bound = data->numeric.lower_bound;
        if (ull > upper_bound || ull < lower_bound) {
            snprintf(loadbuf, LOADBUF_SIZE,
                "argument must be between %llu and %llu inclusive",
                lower_bound,
                upper_bound);
            *err = loadbuf;
            return 0;
        }
    } else {
        /* Boundary check for signed types */
        if (ll > data->numeric.upper_bound || ll < data->numeric.lower_bound) {
            snprintf(loadbuf, LOADBUF_SIZE,
                "argument must be between %lld and %lld inclusive",
                data->numeric.lower_bound,
                data->numeric.upper_bound);
            *err = loadbuf;
            return 0;
        }
    }
    return 1;
}

int numericConfigSet(typeData* data,int argv, sds* value, int update, char **err) {
    long long ll, prev = 0;
    if (data->numeric.is_memory) {
        int memerr;
        ll = memtoll(value[1], &memerr);
        if (memerr || ll < 0) {
            *err = "argument must be a memory value";
            return 0;
        }
    } else {
        if (!string2ll(value[1], sdslen(value[1]),&ll)) {
            *err = "argument couldn't be parsed into an integer" ;
            return 0;
        }
    }

    if (!numericBoundaryCheck(data, ll, err))
        return 0;

    if (data->numeric.is_valid_fn && !data->numeric.is_valid_fn(ll, err))
        return 0;

    GET_NUMERIC_TYPE(prev)
    SET_NUMERIC_TYPE(ll)

    if (update && data->numeric.update_fn && !data->numeric.update_fn(ll, prev, err)) {
        SET_NUMERIC_TYPE(prev)
        return 0;
    }
    return 1;
}

int configGetInt(Config* c, const char* name, char** err) {
    standardConfig* config =  findStandardConfig(c, name, err);
    if(config == NULL) return -1;
    return config->data.numeric.data.i;
}

sds numericConfigGet(typeData* data) {
    char buf[128];
    long long value = 0;

    GET_NUMERIC_TYPE(value)

    int length = ll2string(buf, sizeof(buf), value);
    return sdsnewlen(buf, length);
}

/* Write the long long 'bytes' value as a string in a way that is parsable
 * inside redis.conf. If possible uses the GB, MB, KB notation. */
int rewriteConfigFormatMemory(char *buf, size_t len, long long bytes) {
    int gb = 1024*1024*1024;
    int mb = 1024*1024;
    int kb = 1024;

    if (bytes && (bytes % gb) == 0) {
        return snprintf(buf,len,"%lldgb",bytes/gb);
    } else if (bytes && (bytes % mb) == 0) {
        return snprintf(buf,len,"%lldmb",bytes/mb);
    } else if (bytes && (bytes % kb) == 0) {
        return snprintf(buf,len,"%lldkb",bytes/kb);
    } else {
        return snprintf(buf,len,"%lld",bytes);
    }
}

/* Rewrite a simple "option-name <bytes>" configuration option. */
void rewriteConfigBytesOption(struct rewriteConfigState *state, const char *option, long long value, long long defvalue) {
    char buf[64];
    int force = value != defvalue;
    sds line;

    rewriteConfigFormatMemory(buf,sizeof(buf),value);
    line = sdscatprintf(sdsempty(),"%s %s",option,buf);
    rewriteConfigRewriteLine(state,option,line,force);
}

/* Rewrite a numerical (long long range) option. */
void rewriteConfigNumericalOption(struct rewriteConfigState *state, const char *option, long long value, long long defvalue) {
    int force = value != defvalue;
    sds line = sdscatprintf(sdsempty(),"%s %lld",option,value);

    rewriteConfigRewriteLine(state,option,line,force);
}

void numericConfigRewrite(typeData* data, const char *name, struct rewriteConfigState *state) {
    long long value = 0;

    GET_NUMERIC_TYPE(value)

    if (data->numeric.is_memory) {
        rewriteConfigBytesOption(state, name, value, data->numeric.default_value);
    } else {
        rewriteConfigNumericalOption(state, name, value, data->numeric.default_value);
    }
}

#define embedCommonNumericalConfig(name, alias, modifiable, lower, upper, default, memory, is_valid, update) { \
    embedCommonConfig(name, alias, modifiable) \
    embedConfigInterface(numericConfigInit, numericConfigSet, numericConfigGet, numericConfigRewrite) \
    .data.numeric = { \
        .lower_bound = (lower), \
        .upper_bound = (upper), \
        .default_value = (default), \
        .is_valid_fn = (is_valid), \
        .update_fn = (update), \
        .is_memory = (memory),

#define createIntConfig(name, alias, modifiable, lower, upper, default, memory, is_valid, update) \
    embedCommonNumericalConfig(name, alias, modifiable, lower, upper, default, memory, is_valid, update) \
        .numeric_type = NUMERIC_TYPE_INT, \
        .data.i = 0\
    } \
}

#define IMMUTABLE_CONFIG 0 
#define ALLOW_EMPTY_STRING 0
#if defined(CONFIG_TEST_MAIN)
standardConfig configs[] = {
    /* Bool configs */
    createBoolConfig("a", NULL, IMMUTABLE_CONFIG, 1, NULL, NULL),
    /* String Configs */
    createStringConfig("b", NULL, IMMUTABLE_CONFIG, ALLOW_EMPTY_STRING, "", NULL, NULL),
    /* Enum Configs */
    createEnumConfig("log_level", NULL, IMMUTABLE_CONFIG, loglevel_enum, LL_NOTICE, NULL, NULL),
    /* Integer configs */
    createIntConfig("c", NULL, IMMUTABLE_CONFIG, 1, INT_MAX, 16, INTEGER_CONFIG, NULL, NULL),
    {NULL}
};

    // /* Integer configs */
    // createIntConfig("databases", NULL, IMMUTABLE_CONFIG, 1, INT_MAX, server.dbnum, 16, INTEGER_CONFIG, NULL, NULL),
    // /* Unsigned int configs */
    // createUIntConfig("maxclients", NULL, MODIFIABLE_CONFIG, 1, UINT_MAX, server.maxclients, 10000, INTEGER_CONFIG, NULL, updateMaxclients),
    // /* Unsigned Long configs */
    // createULongConfig("active-defrag-max-scan-fields", NULL, MODIFIABLE_CONFIG, 1, LONG_MAX, server.active_defrag_max_scan_fields, 1000, INTEGER_CONFIG, NULL, NULL), /* Default: keys with more than 1000 fields will be processed separately */
    //  /* Long Long configs */
    // createLongLongConfig("lua-time-limit", NULL, MODIFIABLE_CONFIG, 0, LONG_MAX, server.lua_time_limit, 5000, INTEGER_CONFIG, NULL, NULL),/* milliseconds */
    // /* Unsigned Long Long configs */
    // createULongLongConfig("maxmemory", NULL, MODIFIABLE_CONFIG, 0, ULLONG_MAX, server.maxmemory, 0, MEMORY_CONFIG, NULL, updateMaxmemory),
    // /* Size_t configs */
    // createSizeTConfig("hash-max-ziplist-entries", NULL, MODIFIABLE_CONFIG, 0, LONG_MAX, server.hash_max_ziplist_entries, 512, INTEGER_CONFIG, NULL, NULL),
int ramdonIndex(int start, int end){
    int dis = end - start;
    return rand() % dis + start;
}
int main() {
    Config* c = createConfig();
    initConfig(c, configs);
    // addIntConfig(c, "a", "alias-a", true, 0, 10, setA, 0, NULL);
    loadConfigFromString(c, "a yes\r\nb yes\r\nlog_level notice\r\nc 100\r\n slave 1 100\r\n");
    char *err = NULL;

    int value = configGetBoolean(c, "a", &err);
    if(err == NULL) {
        printf("value: %d\n", value);
    } else {
        printf("err: %s\n", err);
    }

    sds b = configGetSds(c, "b", &err);
    if(err == NULL) {
        printf("b value: %s\n", b);
    } else {
        printf("b err: %s\n", err);
    }
    int log_level = configGetEnum(c, "log_level", &err);
    if (err == NULL) {
        printf("log_level value: %d\n", log_level);
    } else {
        printf("log_level err: %s\n", err);
    }
    int cv = configGetInt(c, "c", &err);
    if (err == NULL) {
        printf("c value: %d\n", cv);
    } else {
        printf("c err: %s\n", err);
    }
    return 1;
}
#endif
