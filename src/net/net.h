

#include <sys/types.h>

#define ANET_OK 0
#define ANET_ERR -1
#define ANET_ERR_LEN 256

/* Flags used with certain functions. */
#define ANET_NONE 0
#define ANET_IP_ONLY (1<<0)

#if defined(__sun) || defined(_AIX)
#define AF_LOCAL AF_UNIX
#endif

#ifdef _AIX
#undef ip_len
#endif

/* FD to address string conversion types */
#define FD_TO_PEER_NAME 0
#define FD_TO_SOCK_NAME 1

// server
int anetTcpServer(char *err, int port, char *bindaddr, int backlog);

//client 
int anetTcpNonBlockConnect(char *err, const char *addr, int port);