
#include "latte_c.h"
#include "libs/sds.h"
#include "libs/log.h"
#include "config.h"
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
// void resetServerSaveParams(struct redisEtlServer *srv) {
//     //几秒保存一次  多大保存一次
//     zfree(srv->saveparams);
//     srv->saveparams = NULL;
//     srv->saveparamslen = 0;
// }
void loadServerConfigFromString(char *config) {
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

        /* Skip comments and blank lines */
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
        /* Execute config directives */
        if (!strcasecmp(argv[0],"slaveof") && argc == 3) {
            slaveof_linenum = linenum;
            server.masterhost = sdsnew(argv[1]);
            server.masterport = atoi(argv[2]);
            server.repl_state = REPL_STATE_CONNECT;
        } else if (!strcasecmp(argv[0],"etl-type") && argc == 2) {
            server.etl_type =
                configEnumGetValue(etl_type_enum, argv[1]);
            if (server.etl_type == INT_MIN) {
                err = "Invalid maxmemory policy";
                goto loaderr;
            }
        } else {
            err = "Bad directive or wrong number of arguments"; goto loaderr;
        } 
        sdsfreesplitres(argv,argc);
    }
    sdsfreesplitres(lines,totlines);
    return;
loaderr:
    fprintf(stderr, "\n*** FATAL CONFIG FILE ERROR ***\n");
    fprintf(stderr, "Reading the configuration file, at line %d\n", linenum);
    fprintf(stderr, ">>> '%s'\n", lines[i]);
    fprintf(stderr, "%s\n", err);
    exit(1);
}

void loadServerConfig(char *filename, char *options) {
    sds config = sdsempty();
    char buf[CONFIG_MAX_LINE+1];

    /* Load the file content */
    if (filename) {
        FILE *fp;

        if (filename[0] == '-' && filename[1] == '\0') {
            fp = stdin;
        } else {
            if ((fp = fopen(filename,"r")) == NULL) {
                serverLog(LL_WARNING,
                    "Fatal error, can't open config file '%s'", filename);
                exit(1);
            }
        }
        while(fgets(buf,CONFIG_MAX_LINE+1,fp) != NULL)
            config = sdscat(config,buf);
        if (fp != stdin) fclose(fp);
    }
    /* Append the additional options */
    if (options) {
        config = sdscat(config,"\n");
        config = sdscat(config,options);
    }
    loadServerConfigFromString(config);
    sdsfree(config);
}

void resetServerSaveParams() {
    // zfree(config.saveparams);
    // server.saveparams = NULL;
    // server.saveparamslen = 0;
}

