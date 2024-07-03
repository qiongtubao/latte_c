#include "error.h"

int isOk(Error* error) {
    return error->code == COk;
}
//只有在error不是ok的情况下创建error
Error* errorCreate(Code code, sds state) {
    Error* error = (Error*)zmalloc(sizeof(Error));
    error->code = code;
    error->state = state;
    return error;
}

Error* errnoIoCreate(char* context) {
    Error* error = (Error*)zmalloc(sizeof(Error));
    if (errno == ENOENT) {
        error->code = CNotFound; 
    } else {
        error->code = CIOError; 
    }
    error->state = sdscatprintf(sdsempty(),"%s: %s",context, strerror(errno));
    return error;
}

void errorRelease(Error* error) {
    if (error->state != NULL) {
        sdsfree(error->state);
    }
    zfree(error);
}
