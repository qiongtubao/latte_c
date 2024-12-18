
#include "test/testhelp.h"
#include "test/testassert.h"

#include <stdio.h>
#include "zmalloc/zmalloc.h"
#include "listeners.h"


int test_listeners() {
    requestListeners* srv = srvRequestListenersCreate(2, NULL);
    requestListeners* es = requestBindListeners(srv, 0, "123", 1);
    // requestListener* listener = requestListenerCreate(db,key,cb,c,pd,pdfree,msgs);
    requestListener* listener = NULL;
    requestListenersPush(es,listener);
    assert(es->level == REQUEST_LEVEL_KEY);
    requestListeners* es1 = requestBindListeners(srv, 0, NULL, 1);
    assert(es1->level == REQUEST_LEVEL_DB);
    requestListeners* es2 = requestBindListeners(srv, 0, "123", 1);
    assert(es2->level == REQUEST_LEVEL_DB);
    return 1;
}

int test_api(void) {
    {
        #ifdef LATTE_TEST
            // ..... private
        #endif
        test_cond("listeners function", 
            test_listeners() == 1);
        
    } test_report()
    return 1;
}

int main() {
    test_api();
    return 0;
}