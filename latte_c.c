#include <locale.h>
#include <time.h>
#include "latte_c.h"
#include "setproctitle.h"
#include "config.h"
#include "time.h"
#include "libs/utils.h"
#include "module.h"


struct redisEtlServer server; /* Server global state */

/* We take a cached value of the unix time in the global state because with
 * virtual memory and aging there is to store the current time in objects at
 * every object access, and accuracy is not needed. To access a global var is
 * a lot faster than calling time(NULL).
 *
 * This function should be fast because it is called at every command execution
 * in call(), so it is possible to decide if to update the daylight saving
 * info or not using the 'update_daylight_info' argument. Normally we update
 * such info only when calling this function from serverCron() but not when
 * calling it from call(). */
void updateCachedTime(int update_daylight_info) {
    server.ustime = ustime();
    server.mstime = server.ustime / 1000;
    server.unixtime = server.mstime / 1000;

    /* To get information about daylight saving time, we need to call
     * localtime_r and cache the result. However calling localtime_r in this
     * context is safe since we will never fork() while here, in the main
     * thread. The logging function will call a thread safe version of
     * localtime that has no locks. */
    if (update_daylight_info) {
        struct tm tm;
        time_t ut = server.unixtime;
        localtime_r(&ut,&tm);
        server.daylight_active = tm.tm_isdst;
    }
}

void initEtlServerConfig(void) {
    int j;

    updateCachedTime(1);
    // getRandomHexChars(server.runid,CONFIG_RUN_ID_SIZE);
    // server.runid[CONFIG_RUN_ID_SIZE] = '\0';
    // changeReplicationId();
    // clearReplicationId2();

}
int main(int argc, char **argv) {
    struct timeval tv;
    int j;
#ifdef INIT_SETPROCTITLE_REPLACEMENT
    //修改程序名 由于参数和环境变量在同一块内存里  如果程序名修改的太长 会覆盖后面参数数据
    //所以会重新拷贝出参数
    spt_init(argc, argv);
#endif
    /**
        影响字符比较（字符排序），具体来说就是影响 <string.h> 头文件中的 strcoll() 和 strxfrm() 函数。
        在默认的地域设置中（设置为"C"），比较字符大小其实比较的是字符的内码，C语言一般使用 ASCII 编码，此时比较的就是字符的 ASCII 码值；但是在其它的地域设置中，可能会有不同的比较方式，例如在中文环境下就可以按照拼音来对字符进行比较和排序。
        Windows 和 Linux 都支持按拼音排序，但是 Mac OS 不支持；Windows 甚至还支持按照笔画来排序，不过需要修改“控制面板 --> 区域和语言”里面的设置。
    */
    setlocale(LC_COLLATE,"");
    tzset(); /* Populates 'timezone' global. */
    //申请不出memory情况的报错处理
    // zmalloc_set_oom_handler(redisOutOfMemoryHandler);
    //初始化随机开始值
    srand(time(NULL)^getpid());
    gettimeofday(&tv,NULL);
    crc64_init();
    
    uint8_t hashseed[16];
    getRandomBytes(hashseed,sizeof(hashseed));
    dictSetHashFunctionSeed(hashseed);
    // server.sentinel_mode =  
    initEtlServerConfig();
    // ACLInit();
    moduleInitModulesSystem();
    // tlsInit();
    server.executable = getAbsolutePath(argv[0]);
    server.exec_argv = zmalloc(sizeof(char*)*(argc+1));
    server.exec_argv[argc] = NULL;
    for (j = 0; j < argc; j++) server.exec_argv[j] = zstrdup(argv[j]);
    /* We need to init sentinel right now as parsing the configuration file
     * in sentinel mode will have the effect of populating the sentinel
     * data structures with master nodes to monitor. */
    // if (server.sentinel_mode) {
    //     initSentinelConfig();
    //     initSentinel();
    // }
    /* Check if we need to start in redis-check-rdb/aof mode. We just execute
     * the program main. However the program is part of the Redis executable
     * so that we can easily execute an RDB check on loading errors. */
    // if (strstr(argv[0],"redis-check-rdb") != NULL)
    //     redis_check_rdb_main(argc,argv,NULL);
    // else if (strstr(argv[0],"redis-check-aof") != NULL)
    //     redis_check_aof_main(argc,argv);
    if (argc >= 2) {
        j = 1; /* First option to parse in argv[] */
        sds options = sdsempty();
        char *configfile = NULL;
        /* Handle special options --help and --version */
        if (strcmp(argv[1], "-v") == 0 ||
            strcmp(argv[1], "--version") == 0) version();
        if (strcmp(argv[1], "--help") == 0 ||
            strcmp(argv[1], "-h") == 0) usage();
        // if (strcmp(argv[1], "--test-memory") == 0) {
        //     if (argc == 3) {
        //         memtest(atoi(argv[2]),50);
        //         exit(0);
        //     } else {
        //         fprintf(stderr,"Please specify the amount of memory to test in megabytes.\n");
        //         fprintf(stderr,"Example: ./redis-server --test-memory 4096\n\n");
        //         exit(1);
        //     }
        // }
        /* First argument is the config file name? */
        if (argv[j][0] != '-' || argv[j][1] != '-') {
            configfile = argv[j];
            server.configfile = getAbsolutePath(configfile);
            /* Replace the config file in server.exec_argv with
             * its absolute path. */
            zfree(server.exec_argv[j]);
            server.exec_argv[j] = zstrdup(server.configfile);
            j++;
        }
        /* All the other options are parsed and conceptually appended to the
         * configuration file. For instance --port 6380 will generate the
         * string "port 6380\n" to be parsed after the actual file name
         * is parsed, if any. */
        while(j != argc) {
            if (argv[j][0] == '-' && argv[j][1] == '-') {
                /* Option name */
                if (!strcmp(argv[j], "--check-rdb")) {
                    /* Argument has no options, need to skip for parsing. */
                    j++;
                    continue;
                }
                if (sdslen(options)) options = sdscat(options,"\n");
                options = sdscat(options,argv[j]+2);
                options = sdscat(options," ");
            } else {
                /* Option argument */
                options = sdscatrepr(options,argv[j],strlen(argv[j]));
                options = sdscat(options," ");
            }
            j++;
        }
        // if (server.sentinel_mode && configfile && *configfile == '-') {
        //     serverLog(LL_WARNING,
        //         "Sentinel config from STDIN not allowed.");
        //     serverLog(LL_WARNING,
        //         "Sentinel needs config file on disk to save state.  Exiting...");
        //     exit(1);
        // }
        resetServerSaveParams();
        loadServerConfig(configfile,options);
        sdsfree(options);
    }
    // server.supervised = redisIsSupervised(server.supervised_mode);
    // int background = server.daemonize && !server.supervised;
    // if (background) daemonize();

    //start server log
    // serverLog(LL_WARNING, "oO0OoO0OoO0Oo Redis is starting oO0OoO0OoO0Oo");
    // serverLog(LL_WARNING,
    //     "Redis version=%s, bits=%d, commit=%s, modified=%d, pid=%d, just started",
    //         REDIS_VERSION,
    //         (sizeof(long) == 8) ? 64 : 32,
    //         redisGitSHA1(),
    //         strtol(redisGitDirty(),NULL,10) > 0,
    //         (int)getpid());
    // if (argc == 1) {
    //     serverLog(LL_WARNING, "Warning: no config file specified, using the default config. In order to specify a config file use %s /path/to/%s.conf", argv[0], server.sentinel_mode ? "sentinel" : "redis");
    // } else {
    //     serverLog(LL_WARNING, "Configuration loaded");
    // }
    
    readOOMScoreAdj();
    initServer();
    // 暂时不处理
    // if (background || server.pidfile) createPidFile();
    redisSetProcTitle(argv[0]);
    redisAsciiArt();
    checkTcpBacklogSettings();
    // if (!server.sentinel_mode) {
    if (1) {
        /* Things not needed when running in Sentinel mode. */
        serverLog(LL_WARNING,"Server initialized");
    #ifdef __linux__
        linuxMemoryWarnings();
    #if defined (__arm64__)
        if (linuxMadvFreeForkBugCheck()) {
            serverLog(LL_WARNING,"WARNING Your kernel has a bug that could lead to data corruption during background save. Please upgrade to the latest stable kernel.");
            if (!checkIgnoreWarning("ARM64-COW-BUG")) {
                serverLog(LL_WARNING,"Redis will now exit to prevent data corruption. Note that it is possible to suppress this warning by setting the following config: ignore-warnings ARM64-COW-BUG");
                exit(1);
            }
        }
    #endif /* __arm64__ */
    #endif /* __linux__ */
        moduleLoadFromQueue();
        ACLLoadUsersAtStartup();
        InitServerLast();
        loadDataFromDisk();
        // if (server.cluster_enabled) {
        //     if (verifyClusterConfigWithData() == C_ERR) {
        //         serverLog(LL_WARNING,
        //             "You can't have keys in a DB different than DB 0 when in "
        //             "Cluster mode. Exiting.");
        //         exit(1);
        //     }
        // }
        // if (server.ipfd_count > 0 || server.tlsfd_count > 0)
        //     serverLog(LL_NOTICE,"Ready to accept connections");
        // if (server.sofd > 0)
        //     serverLog(LL_NOTICE,"The server is now ready to accept connections at %s", server.unixsocket);
        // if (server.supervised_mode == SUPERVISED_SYSTEMD) {
        //     if (!server.masterhost) {
        //         redisCommunicateSystemd("STATUS=Ready to accept connections\n");
        //         redisCommunicateSystemd("READY=1\n");
        //     } else {
        //         redisCommunicateSystemd("STATUS=Waiting for MASTER <-> REPLICA sync\n");
        //     }
        // }
    } else {
        InitServerLast();
        sentinelIsRunning();
        // if (server.supervised_mode == SUPERVISED_SYSTEMD) {
        //     redisCommunicateSystemd("STATUS=Ready to accept connections\n");
        //     redisCommunicateSystemd("READY=1\n");
        // }
    }
    /* Warning the user about suspicious maxmemory setting. */
    // if (server.maxmemory > 0 && server.maxmemory < 1024*1024) {
    //     serverLog(LL_WARNING,"WARNING: You specified a maxmemory value that is less than 1MB (current value is %llu bytes). Are you sure this is what you really want?", server.maxmemory);
    // }

    // redisSetCpuAffinity(server.server_cpulist);
    setOOMScoreAdj(-1);

    aeMain(server.el);
    aeDeleteEventLoop(server.el);
    return 0;
}
