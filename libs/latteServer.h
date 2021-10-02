#include "ae.h"
#include "connection.h"
#include "zmalloc.h"
typedef struct latteClient {
    int fd;
    int flags;
    connection* conn;
} latteClient;
typedef latteClient* (*createClientFunc)(connection* conn);
typedef struct latteServer {
    aeEventLoop* el;
    ConnectionType* type;
    latteClient* clients;
    createClientFunc createClient;
    int port;
    char neterr[256];
    int fd;
} latteServer;


latteServer* createServer(int port);
int runServer(latteServer* server);
