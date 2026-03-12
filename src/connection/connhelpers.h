
/*
 * Copyright (c) 2019, Redis Labs
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of Redis nor the names of its contributors may be used
 *     to endorse or promote products derived from this software without
 *     specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __CONNHELPERS_H
#define __CONNHELPERS_H

#include "connection.h"

/**
 * @brief 连接辅助工具函数集
 *
 * 这些辅助函数供不同连接实现（socket/TLS）共用，
 * 实现回调触发机制和引用计数管理，确保在回调内部
 * 安全调用 connClose() 而不会引发悬空指针。
 */

/**
 * @brief 增加连接引用计数
 *
 * 在连接回调内部，框架保证 refs >= 1，确保回调中
 * 调用 connClose() 是安全的（仅标记关闭，不立即释放）。
 * 在其他需要防止连接提前释放的场景也可显式调用。
 * @param conn 目标连接指针
 */
static inline void connIncrRefs(connection *conn) {
    conn->refs++;
}

/**
 * @brief 减少连接引用计数
 *
 * 注意：此函数不提供自动释放逻辑。
 * callHandler() 负责处理常规流程中的引用归零，
 * 显式调用 connIncrRefs() 的地方需由调用方自行处理引用清零。
 * @param conn 目标连接指针
 */
static inline void connDecrRefs(connection *conn) {
    conn->refs--;
}

/**
 * @brief 检查连接是否存在引用（refs > 0）
 * @param conn 目标连接指针
 * @return int refs 值，非零表示有引用
 */
static inline int connHasRefs(connection *conn) {
    return conn->refs;
}

/**
 * @brief 安全调用连接回调的辅助函数
 *
 * 执行步骤：
 *   1. 增加引用计数，防止回调内释放连接
 *   2. 执行回调（若非 NULL）
 *   3. 减少引用计数；若引用归零且已调度关闭，则执行关闭
 *
 * @param el      事件循环指针
 * @param conn    目标连接指针
 * @param handler 待调用的回调函数（可为 NULL）
 * @return int 连接仍存活返回 1，连接已关闭返回 0
 */
static inline int callHandler(struct aeEventLoop *el, connection *conn, ConnectionCallbackFunc handler) {
    connIncrRefs(conn);                         /* 保护连接，防止回调内提前释放 */
    if (handler) handler(conn);                 /* 执行回调 */
    connDecrRefs(conn);                         /* 释放引用 */
    if (conn->flags & CONN_FLAG_CLOSE_SCHEDULED) {
        if (!connHasRefs(conn)) connClose(el, conn); /* 引用归零时执行延迟关闭 */
        return 0;
    }
    return 1;
}

#endif  /* __REDIS_CONNHELPERS_H */
