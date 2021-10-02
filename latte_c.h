#include <stdlib.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <limits.h>
#include <unistd.h>
#include <errno.h>
#include <inttypes.h>
#include <pthread.h>
#include <syslog.h>
#include <netinet/in.h>
#include <signal.h>
#include "libs/ae.h"
#include "./libs/dict.h"
#include "./libs/connection.h"
#include "libs/utils.h"
#include "libs/log.h"


/* Slave replication state. Used in server.repl_state for slaves to remember
 * what to do next. */
#define REPL_STATE_NONE 0 /* No active replication */
#define REPL_STATE_CONNECT 1 /* Must connect to master */
#define REPL_STATE_CONNECTING 2 /* Connecting to master */
/* --- Handshake states, must be ordered --- */
#define REPL_STATE_RECEIVE_PONG 3 /* Wait for PING reply */
#define REPL_STATE_SEND_GID 4 /* Send Gid to master */
#define REPL_STATE_RECEIVE_GID 5 /* Wait for Gid reply */
#define REPL_STATE_SEND_CRDT_AUTH 6 /* Send Namespace to master */
#define REPL_STATE_RECEIVE_CRDT_AUTH 7 /* Wait for Namespace reply */
#define REPL_STATE_SEND_AUTH 8 /* Send AUTH to master */
#define REPL_STATE_RECEIVE_AUTH 9 /* Wait for AUTH reply */
#define REPL_STATE_SEND_PORT 10 /* Send REPLCONF listening-port */
#define REPL_STATE_RECEIVE_PORT 11 /* Wait for REPLCONF reply */
#define REPL_STATE_SEND_IP 12 /* Send REPLCONF ip-address */
#define REPL_STATE_RECEIVE_IP 13 /* Wait for REPLCONF reply */
#define REPL_STATE_SEND_CAPA 14 /* Send REPLCONF capa */
#define REPL_STATE_RECEIVE_CAPA 15 /* Wait for REPLCONF reply */
#define REPL_STATE_SEND_VC 16 /* Send Vector Clock */
#define REPL_STATE_SEND_CRDT 17
#define REPL_STATE_RECEIVE_CRDT 18
#define REPL_STATE_RECEIVE_VC 19 /* Wait for Vector Clock reply */
#define REPL_STATE_SEND_BACKFLOW 20
#define REPL_STATE_RECEIVE_BACKFLOW 21
#define REPL_STATE_SEND_PSYNC 22 /* Send PSYNC */
#define REPL_STATE_RECEIVE_PSYNC 23 /* Wait for PSYNC reply */
/* Error codes */
#define C_OK                    0
#define C_ERR                   -1
/* --- End of handshake states --- */
#define REPL_STATE_TRANSFER 24 /* Receiving .rdb from master */
#define REPL_STATE_CONNECTED 25 /* Connected to master */
/* Get the first bind addr or NULL */
#define NET_FIRST_BIND_ADDR (server.bindaddr_count ? server.bindaddr[0] : NULL)
/* Using the following macro you can run code inside serverCron() with the
 * specified period, specified in milliseconds.
 * The actual resolution depends on server.hz. */
#define run_with_period(_ms_) if ((_ms_ <= 1000/server.hz) || !(server.cronloops%((_ms_)/(1000/server.hz))))

// #include "libs/latteassert.h"
// #define latteAssertWithInfo(_c,_o,_e) ((_e)?(void)0 : (_latteAssertWithInfo(_c,_o,#_e,__FILE__,__LINE__),_exit(1)))
// #define latteAssert(_e) ((_e)?(void)0 : (_latteAssert(#_e,__FILE__,__LINE__),_exit(1)))
// #define lattePanic(...) _lattePanic(__FILE__,__LINE__,__VA_ARGS__),_exit(1)




struct saveparam {
    time_t seconds;
    int changes;
};
#define LRU_BITS 24
typedef struct redisObject {
    unsigned type:4;
    unsigned encoding:4;
    unsigned lru:LRU_BITS; /* LRU time (relative to global lru_clock) or
                            * LFU data (least significant 8 bits frequency
                            * and most significant 16 bits access time). */
    int refcount;
    void *ptr;
} robj;
/* Client flags */
#define CLIENT_SLAVE (1<<0)   /* This client is a repliaca */
typedef struct client {
    uint64_t id;            /* Client incremental unique ID. */
    connection *conn;
    robj **argv;            /* Arguments of current command. */
    uint64_t flags; /* The actual flags, obtained from the 'sflags' field. */
    /* Use a function to determine keys arguments in a command line.
     * Used for Redis Cluster redirect. */
} client;
typedef struct Instance {
    char *masterhost;               /* Hostname of master */
    int masterport;                 /* Port of master */
    client *client;
} Instance;
struct redisEtlServer {
    char *executable;       /* Absolute executable file path. */
    char *configfile;   /* Absolute config file path, or NULL */
    char **exec_argv;   /* Executable argv vector (copy). */
    // struct saveparam *saveparams;   /* Save points array for RDB */
    // int saveparamslen;              /* Number of saving points */
    char *masterhost;               /* Hostname of master */
    int masterport;                 /* Port of master */
    int repl_state;          /* Replication status if the instance is a slave */
    int etl_type;           /* etl data type */
    aeEventLoop * el;
    /* Limits */
    unsigned int maxclients;            /* Max number of simultaneous clients */
    /* time cache */
    int daylight_active;        /* Currently in daylight saving time. */
    time_t unixtime;    /* Unix time sampled every cron cycle. */
    ustime_t ustime;   /* 'unixtime' in microseconds. */
    mstime_t mstime;   /* Like 'unixtime' but with milliseconds resolution. */
    int hz;             /* serverCron() calls frequency in hertz */
    int cronloops;      /* Number of times the cron function run */
    dict* commands;
    dict* orig_commands;


    size_t initial_memory_usage;/* Bytes used after initialization. */

    char *bio_cpulist; /* cpu affinity list of bio thread. */
} redisEtlServer;
void loadServerConfig(char *filename, char *options);
extern struct redisEtlServer server;
struct sharedObjectsStruct {
    robj *ok;
};
extern struct sharedObjectsStruct shared;
void initEtlServer();

#define MAX_KEYS_BUFFER 256

/* A result structure for the various getkeys function calls. It lists the
 * keys as indices to the provided argv.
 */
typedef struct {
    int keysbuf[MAX_KEYS_BUFFER];       /* Pre-allocated buffer, to save heap allocations */
    int *keys;                          /* Key indices array, points to keysbuf or heap */
    int numkeys;                        /* Number of key indices return */
    int size;                           /* Available array size */
} getKeysResult;
#define GETKEYS_RESULT_INIT { {0}, NULL, 0, MAX_KEYS_BUFFER }

typedef void redisCommandProc(client *c);
typedef int redisGetKeysProc(struct redisCommand *cmd, robj **argv, int argc, getKeysResult *result);
struct redisCommand {
    char *name;
    redisCommandProc *proc;
    int arity;
    char *sflags;   /* Flags as string representation, one char per flag. */
    uint64_t flags; /* The actual flags, obtained from the 'sflags' field. */
    /* Use a function to determine keys arguments in a command line.
     * Used for Redis Cluster redirect. */
    redisGetKeysProc *getkeys_proc;
    /* What keys should be loaded in background when calling this command? */
    int firstkey; /* The first argument that's a key (0 = no keys) */
    int lastkey;  /* The last argument that's a key */
    int keystep;  /* The step between first and last key */
    long long microseconds, calls;
    int id;     /* Command ID. This is a progressive ID starting from 0 that
                   is assigned at runtime, and is used in order to check
                   ACLs. A connection is able to execute a given command if
                   the user associated to the connection has this command
                   bit set in the bitmap of allowed commands. */
};
//commands
void replicaofCommand(client *c);
void redisSetCpuAffinity(const char *cpulist) ;

