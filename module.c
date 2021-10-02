#include "module.h"
#include "libs/dict_plus.h"
list *moduleUnblockedClients;
list *loadmodule_quue; 
dict* modules;
dictType modulesDictType = {
    dictSdsCaseHash,
    NULL,
    NULL,
    dictSdsKeyCaseCompare,
    dictSdsDestructor,
    NULL
};
void moduleInitModulesSystem(void) {
    moduleUnblockedClients = listCreate();
    loadmodule_quue = listCreate();
    modules = dictCreate(&modulesDictType, NULL);

}