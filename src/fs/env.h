
#ifndef __LATTE_ENV_H
#define __LATTE_ENV_H
#include "set/lockSet.h"
#include <utils/error.h>
#include "file.h"
typedef struct Env {
    lockSet locks_;
} Env;



Error* envLockFile(Env* env, sds filename, FileLock** lock);
Error* envUnlockFile(Env* env,FileLock* lock);
Env* envCreate();
void envRelease(Env* env);

Error* envNewWritableFile(Env* env, sds filename, WritableFile** file);
Error* envRemoveFile(Env* env, sds filename);
Error* envRenameFile(Env* env, sds oldname, sds newname);
#endif