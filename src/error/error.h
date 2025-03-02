#ifndef __LATTE_ERROR_H
#define __LATTE_ERROR_H


#include "sds/sds.h"
#include "zmalloc/zmalloc.h"
#include <errno.h>    // 包含errno和错误码
#include <dirent.h>
#include "utils/error.h"
typedef enum latte_error_code_enum {
    COk = 0, //操作正常
    CNotFound = 1, //没找到相关项
    CCorruption = 2, //异常崩溃
    CNotSupported = 3, //不支持
    CInvalidArgument = 4, //非法参数
    CIOError = 5, //I/O操作错误   剩下的类型错误  继续扩展
    CMergeInProgress = 6,
    CIncomplete = 7,
    CShutdownInProgress = 8,
    CTimedOut = 9,
    CAborted = 10,
    CBusy = 11,
    CExpired = 12,
    CTryAgain = 13,
    CCompactionTooLarge = 14,
    CColumnFamilyDropped = 15,
    CThread = 16,
    CMaxCode
} latte_error_code_enum;

typedef enum latte_error_subcode_enum {
    CNode = 0,
    CMutexTimeout = 1,
    CLockTimeout = 2,
    CLockLimit = 3,
    CNoSpace = 4,
    CDeadlock = 5,
    CStaleFile = 6,
    CMemoryLimit = 7,
    CSpaceLimit = 8,
    CPathNotFound = 9,
    CMergeOperandsInsufficientCapacity = 10,
    CManualCompactionPaused = 11,
    COverwritten = 12,
    CTxnNotPrepared = 13,
    CIOFenced = 14,
    CMergeOperatorFailed = 15,
    CMergeOperandThresholdExceeded = 16,
    CMaxSubCode
} latte_error_subcode_enum;


typedef enum latte_error_serverity_enum {
    CNoError = 0,
    CSoftError = 1,
    CHardError = 2,
    CFatalError = 3,
    CUnrecoverableError = 4,
    CMaxSeverity
} latte_error_serverity_enum;

typedef struct latte_error_t {
    latte_error_code_enum code;
    latte_error_subcode_enum subcode;
    latte_error_serverity_enum sev;
    bool retryable;
    bool data_loss;
    unsigned char scope;
    sds_t state;
} latte_error_t;

static latte_error_t Ok = {COk, CNode, CNoError, false, false, 0, NULL};

latte_error_t* errno_io_new(char* context, ...);
latte_error_t* error_new(latte_error_code_enum code, char* state, char* message, ...);
void error_delete(latte_error_t* error);
int error_is_ok(latte_error_t* error);
int error_is_not_found(latte_error_t* error);
latte_error_t* io_error_new(char* state, char* message);



#endif