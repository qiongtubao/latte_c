#include "zmalloc.h"
#include "dict.h"
#include <fcntl.h>
#include <sys/stat.h>
#include "sds.h"
/*-----------------------------------------------------------------------------
 * Config file name-value maps.
 *----------------------------------------------------------------------------*/

typedef struct configEnum {
    const char *name;
    const int val;
} configEnum;
/* Generic config infrastructure function pointers
 * int is_valid_fn(val, err)
 *     Return 1 when val is valid, and 0 when invalid.
 *     Optionally set err to a static error string.
 * int update_fn(val, prev, err)
 *     This function is called only for CONFIG SET command (not at config file parsing)
 *     It is called after the actual config is applied,
 *     Return 1 for success, and 0 for failure.
 *     Optionally set err to a static error string.
 *     On failure the config change will be reverted.
 */

/* Configuration values that require no special handling to set, get, load or
 * rewrite. */
typedef struct boolConfigData {
    int* config; /* The pointer to the server config this value is stored in */
    int data; //data
    const int default_value; /* The default value of the config on rewrite */
    int (*is_valid_fn)(int val, char **err); /* Optional function to check validity of new value (generic doc above) */
    int (*update_fn)(int val, int prev, char **err); /* Optional function to apply new value at runtime (generic doc above) */
} boolConfigData;

typedef struct stringConfigData {
    sds data; /* Pointer to the server config this value is stored in. */
    const char *default_value; /* Default value of the config on rewrite. */
    int (*is_valid_fn)(char* val, char **err); /* Optional function to check validity of new value (generic doc above) */
    int (*update_fn)(char* val, char* prev, char **err); /* Optional function to apply new value at runtime (generic doc above) */
    int convert_empty_to_null; /* Boolean indicating if empty strings should
                                  be stored as a NULL value. */
} stringConfigData;

typedef struct enumConfigData {
    int data; /* The pointer to the server config this value is stored in */
    configEnum *enum_value; /* The underlying enum type this data represents */
    const int default_value; /* The default value of the config on rewrite */
    int (*is_valid_fn)(int val, char **err); /* Optional function to check validity of new value (generic doc above) */
    int (*update_fn)(int val, int prev, char **err); /* Optional function to apply new value at runtime (generic doc above) */
} enumConfigData;

typedef enum numericType {
    NUMERIC_TYPE_INT,
    NUMERIC_TYPE_UINT,
    NUMERIC_TYPE_LONG,
    NUMERIC_TYPE_ULONG,
    NUMERIC_TYPE_LONG_LONG,
    NUMERIC_TYPE_ULONG_LONG,
    NUMERIC_TYPE_SIZE_T,
    NUMERIC_TYPE_SSIZE_T,
    NUMERIC_TYPE_OFF_T,
    NUMERIC_TYPE_TIME_T,
} numericType;

typedef struct numericConfigData {
    union {
        int i;
        unsigned int ui;
        long l;
        unsigned long ul;
        long long ll;
        unsigned long long ull;
        size_t st;
        ssize_t sst;
        off_t ot;
        time_t tt;
    } data; /* The pointer to the numeric config this value is stored in */
    int is_memory; /* Indicates if this value can be loaded as a memory value */
    numericType numeric_type; /* An enum indicating the type of this value */
    long long lower_bound; /* The lower bound of this numeric value */
    long long upper_bound; /* The upper bound of this numeric value */
    const long long default_value; /* The default value of the config on rewrite */
    int (*is_valid_fn)(long long val, char **err); /* Optional function to check validity of new value (generic doc above) */
    int (*update_fn)(long long val, long long prev, char **err); /* Optional function to apply new value at runtime (generic doc above) */
} numericConfigData;

typedef union typeData {
    boolConfigData yesno;
    stringConfigData string;
    enumConfigData enumd;
    numericConfigData numeric;
} typeData;

typedef struct typeInterface {
    /* Called on server start, to init the server with default value */
    void (*init)(typeData* data);
    /* Called on server start, should return 1 on success, 0 on error and should set err */
    int (*load)(typeData* data, sds *argc, int argv, char **err);
    /* Called on server startup and CONFIG SET, returns 1 on success, 0 on error
     * and can set a verbose err string, update is true when called from CONFIG SET */
    int (*set)(typeData* data, int argc, sds* values, int update, char **err);
    /* Called on CONFIG GET, required to add output to the client */
    sds (*get)(typeData data);
    /* Called on CONFIG REWRITE, required to rewrite the config state */
    void (*rewrite)(typeData* data, const char *name, struct rewriteConfigState *state);
} typeInterface;

typedef struct standardConfig {
    const char *name; /* The user visible name of this config */
    const char *alias; /* An alias that can also be used for this config */
    const int modifiable; /* Can this value be updated by CONFIG SET? */
    typeInterface interface; /* The function pointers that define the type interface */
    typeData data; /* The type specific data exposed used by the interface */
} standardConfig;

typedef struct Config {
    dict* configs;
} Config;

Config* createConfig();
void loadConfig(Config* config, char* file, char *options);
void loadConfigFromString(struct Config* c, char *config);