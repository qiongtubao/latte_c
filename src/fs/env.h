
#ifndef __LATTE_ENV_H
#define __LATTE_ENV_H
#include "set/lockSet.h"
#include <utils/error.h>
#include "file.h"
#include "sds/sds.h"
typedef struct Env {
    lockSet locks_;
} Env;



Error* envLockFile(Env* env, sds filename, FileLock** lock);
Error* envUnlockFile(Env* env,FileLock* lock);
Env* envCreate();
void envRelease(Env* env);
void envWritableFileRelease(Env* env, WritableFile* file);

Error* envWritableFileCreate(Env* env, sds filename, WritableFile** file);
Error* envRemoveFile(Env* env, sds filename);
Error* envRenameFile(Env* env, sds oldname, sds newname);

Error* envWriteSdsToFileSync(Env* env, 
                             sds fname, sds data);


Error* envSequentialFileCreate(Env* env, sds filename, SequentialFile** file);
Error* envReadFileToSds(Env* env, sds fname, sds* data);
#endif