
#ifndef __LATTE_ENV_H
#define __LATTE_ENV_H
#include "set/set.h"
#include "mutex/mutex.h"
#include <utils/error.h>
#include "file.h"
#include "sds/sds.h"
typedef struct Env {
    latte_mutex_t* set_lock;
    set_t* set;
} Env;



Error* envLockFile(Env* env, sds_t filename, FileLock** lock);
Error* envUnlockFile(Env* env,FileLock* lock);
Env* envCreate();
void envRelease(Env* env);
void envWritableFileRelease(Env* env, WritableFile* file);

Error* envWritableFileCreate(Env* env, sds_t filename, WritableFile** file);
Error* envRemoveFile(Env* env, sds_t filename);
Error* envRenameFile(Env* env, sds_t oldname, sds_t newname);

Error* envWriteSdsToFileSync(Env* env, 
                             sds_t fname, sds_t data);


Error* envSequentialFileCreate(Env* env, sds_t filename, SequentialFile** file);
Error* envReadFileToSds(Env* env, sds_t fname, sds* data);
#endif