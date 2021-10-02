//
// Created by 周国栋 on 2021/4/1.
//
#include "latte_c.h"
#include <stdio.h>
//#include "libs/MathFunctions.h"
#include "./libs/atomicvar.h"
#include "./libs/latteassert.h"
#include "./libs/sds.h"
#include "./libs/zmalloc.h"
#include "./libs/utils.h"
#include "./libs/connection.h"
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>

#include <assert.h>
#include <ctype.h>
#include <limits.h>
#include <math.h>
#include <unistd.h>
#include <sys/time.h>
#include <float.h>
#include <stdint.h>
#include <errno.h>
#include <float.h>
#include "libs/syncio.h"
#include <sys/socket.h>
#include "libs/anet.h"
#include "libs/latteServer.h"
#include <sys/mman.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
//#include "csapp.h"
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "libs/log.h"
#include "libs/rax.h"
/* Global vars */
struct redisEtlServer server; /* Server global state */

#define CONFIG_MIN_RESERVED_FDS 32
#define CONFIG_FDSET_INCR (CONFIG_MIN_RESERVED_FDS+96)
/* Static server configuration */
#define CONFIG_DEFAULT_HZ        10      /* Time interrupt calls/sec. */
#define CONFIG_MIN_HZ            1
#define CONFIG_MAX_HZ            500
struct sharedObjectsStruct shared;

/* This structure represents a Redis user. This is useful for ACLs, the
 * user is associated to the connection after the connection is authenticated.
 * If there is no associated user, the connection uses the default user. */
#define USER_COMMAND_BITS_COUNT 1024    /* The total number of command bits
                                           in the user structure. The last valid
                                           command ID we can set in the user
                                           is USER_COMMAND_BITS_COUNT-1. */
/* This is our timer interrupt, called server.hz times per second.
 * Here is where we do a number of things that need to be done asynchronously.
 * For instance:
 *
 * - Active expired keys collection (it is also performed in a lazy way on
 *   lookup).
 * - Software watchdog.
 * - Update some statistic.
 * - Incremental rehashing of the DBs hash tables.
 * - Triggering BGSAVE / AOF rewrite, and handling of terminated children.
 * - Clients timeout of different kinds.
 * - Replication reconnection.
 * - Many more...
 *
 * Everything directly called here will be called server.hz times per second,
 * so in order to throttle execution of things we want to do less frequently
 * a macro is used: run_with_period(milliseconds) { .... }
 */

int serverCron(struct aeEventLoop *eventLoop, long long id, void *clientData) {
    int j;
    UNUSED(eventLoop);
    UNUSED(id);
    UNUSED(clientData);
    /* Software watchdog: deliver the SIGALRM that will reach the signal
     * handler if we don't return here fast enough. */
    // if (server.watchdog_period) watchdogScheduleSignal(server.watchdog_period);

    /* Update the time cache. */
    // updateCachedTime(1);

    // server.hz = server.config_hz;
    /* Adapt the server.hz value to the number of configured clients. If we have
     * many clients, we want to call serverCron() with an higher frequency. */
    // if (server.dynamic_hz) {
    //     while (listLength(server.clients) / server.hz >
    //            MAX_CLIENTS_PER_CLOCK_TICK)
    //     {
    //         server.hz *= 2;
    //         if (server.hz > CONFIG_MAX_HZ) {
    //             server.hz = CONFIG_MAX_HZ;
    //             break;
    //         }
    //     }
    // }
}

/* Command flags. Please check the command table defined in the server.c file
 * for more information about the meaning of every flag. */
#define CMD_WRITE (1ULL<<0)            /* "write" flag */
#define CMD_READONLY (1ULL<<1)         /* "read-only" flag */
#define CMD_DENYOOM (1ULL<<2)          /* "use-memory" flag */
#define CMD_MODULE (1ULL<<3)           /* Command exported by module. */
#define CMD_ADMIN (1ULL<<4)            /* "admin" flag */
#define CMD_PUBSUB (1ULL<<5)           /* "pub-sub" flag */
#define CMD_NOSCRIPT (1ULL<<6)         /* "no-script" flag */
#define CMD_RANDOM (1ULL<<7)           /* "random" flag */
#define CMD_SORT_FOR_SCRIPT (1ULL<<8)  /* "to-sort" flag */
#define CMD_LOADING (1ULL<<9)          /* "ok-loading" flag */
#define CMD_STALE (1ULL<<10)           /* "ok-stale" flag */
#define CMD_SKIP_MONITOR (1ULL<<11)    /* "no-monitor" flag */
#define CMD_SKIP_SLOWLOG (1ULL<<12)    /* "no-slowlog" flag */
#define CMD_ASKING (1ULL<<13)          /* "cluster-asking" flag */
#define CMD_FAST (1ULL<<14)            /* "fast" flag */
#define CMD_NO_AUTH (1ULL<<15)         /* "no-auth" flag */

/* Command flags used by the module system. */
#define CMD_MODULE_GETKEYS (1ULL<<16)  /* Use the modules getkeys interface. */
#define CMD_MODULE_NO_CLUSTER (1ULL<<17) /* Deny on Redis Cluster. */

/* Command flags that describe ACLs categories. */
#define CMD_CATEGORY_KEYSPACE (1ULL<<18)
#define CMD_CATEGORY_READ (1ULL<<19)
#define CMD_CATEGORY_WRITE (1ULL<<20)
#define CMD_CATEGORY_SET (1ULL<<21)
#define CMD_CATEGORY_SORTEDSET (1ULL<<22)
#define CMD_CATEGORY_LIST (1ULL<<23)
#define CMD_CATEGORY_HASH (1ULL<<24)
#define CMD_CATEGORY_STRING (1ULL<<25)
#define CMD_CATEGORY_BITMAP (1ULL<<26)
#define CMD_CATEGORY_HYPERLOGLOG (1ULL<<27)
#define CMD_CATEGORY_GEO (1ULL<<28)
#define CMD_CATEGORY_STREAM (1ULL<<29)
#define CMD_CATEGORY_PUBSUB (1ULL<<30)
#define CMD_CATEGORY_ADMIN (1ULL<<31)
#define CMD_CATEGORY_FAST (1ULL<<32)
#define CMD_CATEGORY_SLOW (1ULL<<33)
#define CMD_CATEGORY_BLOCKING (1ULL<<34)
#define CMD_CATEGORY_DANGEROUS (1ULL<<35)
#define CMD_CATEGORY_CONNECTION (1ULL<<36)
#define CMD_CATEGORY_TRANSACTION (1ULL<<37)
#define CMD_CATEGORY_SCRIPTING (1ULL<<38)

struct ACLCategoryItem {
    const char *name;
    uint64_t flag;
} ACLCommandCategories[] = {
    {"keyspace", CMD_CATEGORY_KEYSPACE},
    {"read", CMD_CATEGORY_READ},
    {"write", CMD_CATEGORY_WRITE},
    {"set", CMD_CATEGORY_SET},
    {"sortedset", CMD_CATEGORY_SORTEDSET},
    {"list", CMD_CATEGORY_LIST},
    {"hash", CMD_CATEGORY_HASH},
    {"string", CMD_CATEGORY_STRING},
    {"bitmap", CMD_CATEGORY_BITMAP},
    {"hyperloglog", CMD_CATEGORY_HYPERLOGLOG},
    {"geo", CMD_CATEGORY_GEO},
    {"stream", CMD_CATEGORY_STREAM},
    {"pubsub", CMD_CATEGORY_PUBSUB},
    {"admin", CMD_CATEGORY_ADMIN},
    {"fast", CMD_CATEGORY_FAST},
    {"slow", CMD_CATEGORY_SLOW},
    {"blocking", CMD_CATEGORY_BLOCKING},
    {"dangerous", CMD_CATEGORY_DANGEROUS},
    {"connection", CMD_CATEGORY_CONNECTION},
    {"transaction", CMD_CATEGORY_TRANSACTION},
    {"scripting", CMD_CATEGORY_SCRIPTING},
    {NULL,0} /* Terminator. */
};

/* Given the category name the command returns the corresponding flag, or
 * zero if there is no match. */
uint64_t ACLGetCommandCategoryFlagByName(const char *name) {
    for (int j = 0; ACLCommandCategories[j].flag != 0; j++) {
        if (!strcasecmp(name,ACLCommandCategories[j].name)) {
            return ACLCommandCategories[j].flag;
        }
    }
    return 0; /* No match. */
}

/* Parse the flags string description 'strflags' and set them to the
 * command 'c'. If the flags are all valid C_OK is returned, otherwise
 * C_ERR is returned (yet the recognized flags are set in the command). */
int populateCommandTableParseFlags(struct redisCommand *c, char *strflags) {
    int argc;
    sds *argv;

    /* Split the line into arguments for processing. */
    argv = sdssplitargs(strflags,&argc);
    if (argv == NULL) return C_ERR;

    for (int j = 0; j < argc; j++) {
        char *flag = argv[j];
        if (!strcasecmp(flag,"write")) {
            c->flags |= CMD_WRITE|CMD_CATEGORY_WRITE;
        } else if (!strcasecmp(flag,"read-only")) {
            c->flags |= CMD_READONLY|CMD_CATEGORY_READ;
        } else if (!strcasecmp(flag,"use-memory")) {
            c->flags |= CMD_DENYOOM;
        } else if (!strcasecmp(flag,"admin")) {
            c->flags |= CMD_ADMIN|CMD_CATEGORY_ADMIN|CMD_CATEGORY_DANGEROUS;
        } else if (!strcasecmp(flag,"pub-sub")) {
            c->flags |= CMD_PUBSUB|CMD_CATEGORY_PUBSUB;
        } else if (!strcasecmp(flag,"no-script")) {
            c->flags |= CMD_NOSCRIPT;
        } else if (!strcasecmp(flag,"random")) {
            c->flags |= CMD_RANDOM;
        } else if (!strcasecmp(flag,"to-sort")) {
            c->flags |= CMD_SORT_FOR_SCRIPT;
        } else if (!strcasecmp(flag,"ok-loading")) {
            c->flags |= CMD_LOADING;
        } else if (!strcasecmp(flag,"ok-stale")) {
            c->flags |= CMD_STALE;
        } else if (!strcasecmp(flag,"no-monitor")) {
            c->flags |= CMD_SKIP_MONITOR;
        } else if (!strcasecmp(flag,"no-slowlog")) {
            c->flags |= CMD_SKIP_SLOWLOG;
        } else if (!strcasecmp(flag,"cluster-asking")) {
            c->flags |= CMD_ASKING;
        } else if (!strcasecmp(flag,"fast")) {
            c->flags |= CMD_FAST | CMD_CATEGORY_FAST;
        } else if (!strcasecmp(flag,"no-auth")) {
            c->flags |= CMD_NO_AUTH;
        } else {
            /* Parse ACL categories here if the flag name starts with @. */
            uint64_t catflag;
            if (flag[0] == '@' &&
                (catflag = ACLGetCommandCategoryFlagByName(flag+1)) != 0)
            {
                c->flags |= catflag;
            } else {
                sdsfreesplitres(argv,argc);
                serverLog(LL_WARNING, "args error %s \n", flag);
                return C_ERR;
            }
        }
    }
    /* If it's not @fast is @slow in this binary world. */
    if (!(c->flags & CMD_CATEGORY_FAST)) c->flags |= CMD_CATEGORY_SLOW;

    sdsfreesplitres(argv,argc);
    return C_OK;
}


void moduleCommand(client *c) {
}
struct redisCommand redisCommandTable[] = {
    {"module",moduleCommand,-2,"admin no-script",0,NULL,0,0,0,0,0,0}
};
unsigned long ACLGetCommandID(const char *cmdname) {
    static rax *map = NULL;
    static unsigned long nextid = 0;
    sds lowername = sdsnew(cmdname);
    sdstolower(lowername);
    if(map == NULL) map = raxNew();
    void *id = raxFind(map, (unsigned char*)lowername,sdslen(lowername));
    if (id != raxNotFound) {
        sdsfree(lowername);
        return (unsigned long)id;
    } 
    raxInsert(map,(unsigned char*)lowername,strlen(lowername),
              (void*)nextid,NULL);
    sdsfree(lowername);
    unsigned long thisid = nextid;
    nextid++;
    /* We never assign the last bit in the user commands bitmap structure,
     * this way we can later check if this bit is set, understanding if the
     * current ACL for the user was created starting with a +@all to add all
     * the possible commands and just subtracting other single commands or
     * categories, or if, instead, the ACL was created just adding commands
     * and command categories from scratch, not allowing future commands by
     * default (loaded via modules). This is useful when rewriting the ACLs
     * with ACL SAVE. */
    if (nextid == USER_COMMAND_BITS_COUNT-1) nextid++;
    return thisid;
}

void populateCommandTable() {
    int j;
    int numcommands = sizeof(redisCommandTable)/sizeof(struct redisCommand);
    for(j = 0; j < numcommands; j++) {
        struct redisCommand *c = redisCommandTable + j;
        int retval1, retval2;
        /* Translate the command string flags description into an actual
         * set of flags. */
        if (populateCommandTableParseFlags(c,c->sflags) == C_ERR)
            panic("Unsupported command flag");
        c->id = ACLGetCommandID(c->name); /* Assign the ID used for ACL. */
        retval1 = dictAdd(server.commands, sdsnew(c->name), c);
        /* Populate an additional dictionary that will be unaffected
         * by rename-command statements in redis.conf. */
        retval2 = dictAdd(server.orig_commands, sdsnew(c->name), c);
        // assert(retval1 == DICT_OK && retval2 == DICT_OK);
    }
}
uint64_t dictSdsHash(const void *key) {
    return dictGenHashFunction((unsigned char*)key, sdslen((char*)key));
}

uint64_t dictSdsCaseHash(const void *key) {
    return dictGenCaseHashFunction((unsigned char*)key, sdslen((char*)key));
}
/* A case insensitive version used for the command lookup table and other
 * places where case insensitive non binary-safe comparison is needed. */
int dictSdsKeyCaseCompare(void *privdata, const void *key1,
        const void *key2)
{
    DICT_NOTUSED(privdata);

    return strcasecmp(key1, key2) == 0;
}
void dictSdsDestructor(void *privdata, void *val)
{
    DICT_NOTUSED(privdata);

    sdsfree(val);
}
/* Command table. sds string -> command struct pointer. */
dictType commandTableDictType = {
    dictSdsCaseHash,            /* hash function */
    NULL,                       /* key dup */
    NULL,                       /* val dup */
    dictSdsKeyCaseCompare,      /* key compare */
    dictSdsDestructor,          /* key destructor */
    NULL                        /* val destructor */
};



void initEtlServer() {
    server.hz = CONFIG_DEFAULT_HZ;
    server.cronloops = 0;
    server.el = aeCreateEventLoop(1024+CONFIG_FDSET_INCR);
    if (server.el == NULL) {
        serverLog(LL_WARNING,
            "Failed creating the event loop. Error message: '%s'",
            strerror(errno));
        exit(1);
    }
    /* Create the timer callback, this is our way to process many background
     * operations incrementally, like clients timeout, eviction of unaccessed
     * expired keys and so forth. */
    if (aeCreateTimeEvent(server.el, 1, serverCron, NULL, NULL) == AE_ERR) {
        panic("Can't create event loop timers.");
        exit(1);
    }
    server.commands = dictCreate(&commandTableDictType, NULL);
    server.orig_commands = dictCreate(&commandTableDictType, NULL);
    populateCommandTable();
}


void redisSetCpuAffinity(const char *cpulist) {

}

void InitServerLast() {
    // bioInit();
    // initThreadedIO();
    // set_jemalloc_bg_thread(server.jemalloc_bg_thread);
    // server.initial_memory_usage = zmalloc_used_memory();
}

int main(int argc, char **argv) {
    // struct  timeval tv;
    // int j;
    // setlocale(LC_COLLATE, "");
    // zmalloc_set_oom_handler(redisOutOfMemoryHandler);
    // srand(time(NULL)^getpid());
    // gettimeofday(&tv, NULL);
    // crc64_init();

    // char hashseed[16];
    // getRandomHexChars(hashseed, sizeof(hashseed));
    // dictSetHashFunctionSeed((uint8_t*)hashseed);
    int j;
    /* Store the executable path and arguments in a safe place in order
     * to be able to restart the server later. */
    server.executable = getAbsolutePath(argv[0]);
    server.exec_argv = zmalloc(sizeof(char*)*(argc+1));
    server.exec_argv[argc] = NULL;
    for (j = 0; j < argc; j++) server.exec_argv[j] = zstrdup(argv[j]);

    if (argc >= 2) {
        j = 1; /* First option to parse in argv[] */
        sds options = sdsempty();
        char *configfile = NULL;
        // if (strcmp(argv[1], "-v") == 0 || 
        //     strcmp(argv[1], "--version") == 0) version();
        // if(strcmp(argv[1], "--help") == 0 ||
        //     strcmp(argv[1], "-h") == 0) usage();
        // if(strcmp(argv[1], "--test-memory") == 0) {
        //     if(argc == 3) {
        //         memtest(atoi(argv[2]), 50);
        //         exit(0);
        //     } else {
        //         fprintf(stderr,"Please specify the amount of memory to test in megabytes.\n");
        //         fprintf(stderr,"Example: ./redis-server --test-memory 4096\n\n");
        //         exit(1);
        //     }
        // } 
        if (argv[j][0] != '-' || argv[j][1] != '-') {
            configfile = argv[j];
            server.configfile = getAbsolutePath(configfile);
            /* Replace the config file in server.exec_argv with
             * its absoulte path. */
            zfree(server.exec_argv[j]);
            server.exec_argv[j] = zstrdup(server.configfile);
            j++;
        }
        
        while(j != argc) {
            if(argv[j][0] == '-' && argv[j][1] == '-') {
                if(!strcmp(argv[j], "--check-rdb")) {
                    j++;
                    continue;
                }
                if (sdslen(options)) options = sdscat(options, "\n");
                options = sdscat(options, argv[j] + 2);
                options = sdscat(options, " ");
            } else {
                options = sdscatrepr(options, argv[j], strlen(argv[j]));
                options = sdscat(options, " ");
            }
            j++;
        }
        // if(configfile && *configfile == '-') {
        //     serverLog(LL_WARNING,
        //         "crdt_etl config from STDIN not allowed.");
        //     serverLog(LL_WARNING,
        //         "crdt_etl needs config file on disk to save state.  Exiting...");
        //     exit(1);
        // }
        // resetServerSaveParams(&server);
        loadServerConfig(configfile,options);
        sdsfree(options);
    }
    // initServer();
    
    initEtlServer();
    //
    // moduleLoadFromQueue();
    InitServerLast();
    //

    aeMain(server.el);
    
    aeDeleteEventLoop(server.el);
    return 0;
}