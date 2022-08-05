
#include "../sds/sds.h"
#include "../dict/dict.h"
#include "../list/list.h"
typedef int redisDb;
typedef char robj;


#define REQUEST_LEVEL_SVR 0
#define REQUEST_LEVEL_DB 1
#define REQUEST_LEVEL_KEY 2


typedef struct requestListeners {
  list *listeners;                  /* list of listenters */
  struct requestListeners *parent;  /* parent lisenter */
  int nlisteners;                   /* # of listeners of current and childs */
  int level;                        /* key/db/svr */
  union {
      struct {
          int dbnum;
          struct requestListeners **dbs;
      } svr;
      struct {
          redisDb db;
          dict *keys;
      } db;
      struct {
          robj *key;
      } key;
  };
} requestListeners;


requestListeners* requestBindListeners(requestListeners *root, redisDb db, robj *key, int create);
requestListeners *srvRequestListenersCreate(int dbnum, requestListeners *parent);
void *dictFetchValue(dict *d, const void *key);

void requestListenersLink(requestListeners *listeners);
void requestListenersPush(requestListeners *listeners,
        requestListener *listener);