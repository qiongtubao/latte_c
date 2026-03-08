/* anet.c - 网络封装实现文件
 * 
 * Latte C 库网络组件：简化网络编程
 * 
 * 核心功能：
 * 1. 通用 TCP/UDP 套接字封装
 * 2. 非阻塞 I/O 设置
 * 3. 地址解析和绑定
 * 4. 连接管理 (accept/connect)
 * 5. 读写操作封装
 * 6. 错误处理统一
 * 
 * 主要函数：
 * - anetTcpServer: 创建 TCP 服务器
 * - anetTcpClient: 创建 TCP 客户端
 * - anetNonBlock: 设置非阻塞模式
 * - anetTcpNoDelay: 禁用 Nagle 算法
 * - anetAddr: 地址解析
 * 
 * 作者：自动注释生成
 * 日期：2026-03-08
 */

#include "fmacros.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <net/if.h>
#include <sys/un.h>

#include "anet.h"
#include "log/log.h"

#define ANET_NOTUSED(V) ((void) V)
