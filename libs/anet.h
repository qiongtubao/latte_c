

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

int anetTcpConnect(char *err, const char *addr, int port);


int anetNonBlock(char *err, int fd);
int anetBlock(char *err, int fd);
int anetEnableTcpNoDelay(char *err, int fd);
int anetDisableTcpNoDelay(char *err, int fd);
int anetFormatPeer(int fd, char *fmt, size_t fmt_len);
int anetKeepAlive(char *err, int fd, int interval);
int anetRecvTimeout(char *err, int fd, long long ms);
int anetSendTimeout(char *err, int fd, long long ms);
int anetSockName(int fd, char *ip, size_t ip_len, int *port);
int anetTcpNonBlockBestEffortBindConnect(char *err, const char *addr, int port, const char *source_addr);
int anetTcpNonBlockConnect(char *err, const char *addr, int port);
int anetPeerToString(int fd, char *ip, size_t ip_len, int *port);
int anetTcpServer(char *err, int port, char *bindaddr, int backlog);
int anetTcpAccept(char *err, int s, char *ip, size_t ip_len, int *port);

