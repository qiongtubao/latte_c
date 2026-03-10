/**
 * @file raft.h
 * @brief Raft 分布式一致性协议头文件
 * 
 * 本文件定义了 Raft 协议的核心数据结构和接口。
 * Raft 是一种用于管理复制日志的一致性算法，通过选举机制选出 Leader，
 * Leader 负责处理所有客户端请求并复制日志到 Follower 节点。
 */

#ifndef __LATTE_RAFT_H
#define __LATTE_RAFT_H

/* 标准库头文件 */
#include <stdint.h>      /* 用于定义固定大小的整数类型 */
#include <stdbool.h>     /* 用于定义布尔类型 */

/* 项目内部头文件 */
#include "zmalloc/zmalloc.h"  /* 内存分配器封装 */
#include "sds/sds.h"          /* 简单动态字符串 */
#include "list/list.h"        /* 双向链表 */
#include "dict/dict.h"        /* 哈希字典 */
#include "utils/atomic.h"      /* 原子操作 */

/* 返回值定义 */
#define RAFT_OK 0   /* 操作成功 */
#define RAFT_ERR -1 /* 操作失败 */

/**
 * @brief Raft 节点状态枚举
 * 
 * Raft 协议中每个节点有三种状态：
 * - FOLLOWER: 跟随者状态，被动接收 Leader 的日志复制和心跳
 * - CANDIDATE: 候选者状态，在选举过程中主动请求投票
 * - LEADER: 领导者状态，处理所有客户端请求并复制日志
 */
typedef enum {
    RAFT_STATE_FOLLOWER = 0,  /* 跟随者状态：默认状态，接收 Leader 的指令 */
    RAFT_STATE_CANDIDATE,      /* 候选者状态：正在参与选举，请求其他节点投票 */
    RAFT_STATE_LEADER          /* 领导者状态：集群的领导者，处理所有写请求 */
} raft_state_t;

/**
 * @brief 日志条目结构
 * 
 * Raft 日志中的每个条目包含：
 * - term: 该条目被 Leader 接收时的任期号
 * - index: 该条目在日志中的索引位置
 * - data: 实际的数据/命令内容
 * - data_len: 数据的长度
 */
typedef struct raft_log_entry_t {
    uint64_t term;          /* 任期号：该条目被 Leader 接收时的任期（单调递增） */
    uint64_t index;         /* 索引：该条目在日志中的位置（从 1 开始，0 为初始空条目） */
    sds_t data;             /* 数据：实际存储的命令或数据内容（使用 SDS 动态字符串） */
    size_t data_len;       /* 数据长度：data 字段的字节长度 */
} raft_log_entry_t;

/**
 * @brief RequestVote RPC 请求参数
 * 
 * 候选者向其他节点发送投票请求时使用的参数结构。
 * 用于在选举过程中请求其他节点投票支持自己成为 Leader。
 */
typedef struct raft_request_vote_args_t {
    uint64_t term;          /* 候选者的当前任期号 */
    uint64_t candidate_id; /* 候选者的节点 ID（请求投票的节点标识） */
    uint64_t last_log_index; /* 候选者最后一条日志的索引位置 */
    uint64_t last_log_term;  /* 候选者最后一条日志的任期号 */
} raft_request_vote_args_t;

/**
 * @brief RequestVote RPC 响应结果
 * 
 * 节点响应投票请求时返回的结果结构。
 * 包含当前任期号和是否授予投票的决策。
 */
typedef struct raft_request_vote_result_t {
    uint64_t term;          /* 当前任期号：用于候选者更新自己的任期（如果发现更大的任期） */
    bool vote_granted;      /* 是否授予投票：true 表示该节点投票支持候选者 */
} raft_request_vote_result_t;

/**
 * @brief AppendEntries RPC 请求参数
 * 
 * Leader 向 Follower 发送日志复制请求时使用的参数结构。
 * 也可以作为心跳使用（当 entries 为空时）。
 */
typedef struct raft_append_entries_args_t {
    uint64_t term;          /* Leader 的当前任期号 */
    uint64_t leader_id;    /* Leader 的节点 ID：用于 Follower 重定向客户端请求 */
    uint64_t prev_log_index; /* 前一条日志的索引：新日志条目之前的那条日志的索引 */
    uint64_t prev_log_term;  /* 前一条日志的任期：prev_log_index 对应条目的任期号 */
    list_t *entries;        /* 日志条目列表：要存储的新日志条目（空列表表示心跳） */
    uint64_t leader_commit; /* Leader 的提交索引：Leader 已提交的最高日志索引 */
} raft_append_entries_args_t;

/**
 * @brief AppendEntries RPC 响应结果
 * 
 * Follower 响应日志复制请求时返回的结果结构。
 * 包含当前任期号和操作是否成功。
 */
typedef struct raft_append_entries_result_t {
    uint64_t term;          /* 当前任期号：用于 Leader 更新自己的任期（如果发现更大的任期） */
    bool success;          /* 是否成功：true 表示 Follower 包含匹配 prev_log_index 和 prev_log_term 的条目 */
} raft_append_entries_result_t;

/**
 * @brief 持久化状态（所有服务器都需要）
 * 
 * 这些状态必须在响应 RPC 之前持久化到稳定存储（如磁盘），
 * 以确保节点重启后能够恢复正确的状态。
 */
typedef struct raft_persistent_state_t {
    uint64_t current_term;  /* 当前任期号：服务器看到的最新任期（首次启动时为 0，单调递增） */
    uint64_t voted_for;     /* 投票对象：当前任期内收到投票的候选者 ID（如果没有则为 0） */
    list_t *log;            /* 日志条目列表：每个条目包含状态机命令和接收时的任期号 */
} raft_persistent_state_t;

/**
 * @brief 易失状态（所有服务器都需要）
 * 
 * 这些状态在服务器重启后会被重新初始化，不需要持久化。
 */
typedef struct raft_volatile_state_t {
    uint64_t commit_index;  /* 提交索引：已知已提交的最高日志条目索引（初始化为 0，单调递增） */
    uint64_t last_applied;  /* 最后应用索引：已应用到状态机器的最高日志条目索引（初始化为 0，单调递增） */
} raft_volatile_state_t;

/**
 * @brief Leader 的易失状态（仅在 Leader 状态有效）
 * 
 * 这些状态在每次选举后重新初始化，用于跟踪每个 Follower 的日志复制进度。
 */
typedef struct raft_leader_state_t {
    dict_t *next_index;     /* 下一个索引字典：对每个服务器，要发送给该服务器的下一条日志索引（初始化为 Leader 最后日志索引 + 1） */
    dict_t *match_index;    /* 匹配索引字典：对每个服务器，已知在该服务器上已复制的最高日志索引（初始化为 0，单调递增） */
} raft_leader_state_t;

/**
 * @brief Raft 节点主结构
 * 
 * 表示一个 Raft 集群中的节点，包含节点的所有状态信息、配置和回调函数。
 */
typedef struct raft_node_t {
    /* 节点标识 */
    uint64_t node_id;       /* 节点 ID：集群中唯一的节点标识符 */
    
    /* 状态 */
    raft_state_t state;     /* 当前状态：FOLLOWER、CANDIDATE 或 LEADER */
    
    /* 持久化状态 */
    raft_persistent_state_t *persistent;  /* 持久化状态指针：需要持久化到磁盘的状态 */
    
    /* 易失状态 */
    raft_volatile_state_t *volatile_state;  /* 易失状态指针：重启后重新初始化的状态 */
    
    /* Leader 状态（仅在 state == LEADER 时有效） */
    raft_leader_state_t *leader_state;  /* Leader 状态指针：仅在 Leader 状态下使用 */
    
    /* 超时配置（单位：毫秒） */
    uint64_t election_timeout;    /* 选举超时：Follower 等待 Leader 心跳的超时时间，超时后发起选举 */
    uint64_t heartbeat_interval;  /* 心跳间隔：Leader 发送心跳的间隔时间 */
    
    /* 定时器 */
    uint64_t last_heartbeat;    /* 最后心跳时间：上次收到 Leader 心跳的时间戳（毫秒） */
    uint64_t election_deadline; /* 选举截止时间：应该开始新选举的时间戳（毫秒） */
    
    /* 集群配置 */
    list_t *peers;          /* 对等节点列表：集群中其他节点的 ID 列表 */
    
    /* 回调函数 */
    /**
     * @brief 发送 RequestVote RPC 的回调函数
     * @param node 当前节点
     * @param peer_id 目标节点 ID
     * @param args 请求参数
     * @param result 响应结果（输出参数）
     * @return RAFT_OK 成功，RAFT_ERR 失败
     */
    int (*send_request_vote)(struct raft_node_t *node, uint64_t peer_id, 
                             raft_request_vote_args_t *args, 
                             raft_request_vote_result_t *result);
    
    /**
     * @brief 发送 AppendEntries RPC 的回调函数
     * @param node 当前节点
     * @param peer_id 目标节点 ID
     * @param args 请求参数
     * @param result 响应结果（输出参数）
     * @return RAFT_OK 成功，RAFT_ERR 失败
     */
    int (*send_append_entries)(struct raft_node_t *node, uint64_t peer_id,
                               raft_append_entries_args_t *args,
                               raft_append_entries_result_t *result);
    
    /**
     * @brief 应用日志条目到状态机器的回调函数
     * @param node 当前节点
     * @param entry 要应用的日志条目
     * @return RAFT_OK 成功，RAFT_ERR 失败
     */
    int (*apply_log_entry)(struct raft_node_t *node, raft_log_entry_t *entry);
    
    /**
     * @brief 持久化状态的回调函数
     * @param node 当前节点
     * @return RAFT_OK 成功，RAFT_ERR 失败
     */
    int (*persist_state)(struct raft_node_t *node);
    
    /**
     * @brief 加载状态的回调函数
     * @param node 当前节点
     * @return RAFT_OK 成功，RAFT_ERR 失败
     */
    int (*load_state)(struct raft_node_t *node);
    
    /* 统计信息 */
    uint64_t votes_received;    /* 收到的投票数：当前选举中收到的投票数量（包括自己） */
    uint64_t election_count;    /* 选举次数：节点发起的选举总数（用于统计和调试） */
    
    /* 私有数据 */
    void *privdata;  /* 私有数据指针：用户自定义数据，可用于存储额外的上下文信息 */
} raft_node_t;

/* ==================== 函数原型声明 ==================== */

/* ---------- 节点生命周期管理 ---------- */

/**
 * @brief 创建新的 Raft 节点
 * @param node_id 节点 ID
 * @return 成功返回节点指针，失败返回 NULL
 */
raft_node_t *raft_node_create(uint64_t node_id);

/**
 * @brief 删除 Raft 节点并释放所有资源
 * @param node 要删除的节点指针
 */
void raft_node_delete(raft_node_t *node);

/**
 * @brief 初始化 Raft 节点
 * @param node 节点指针（必须已分配内存）
 * @param node_id 节点 ID
 * @return RAFT_OK 成功，RAFT_ERR 失败
 */
int raft_node_init(raft_node_t *node, uint64_t node_id);

/* ---------- 状态管理 ---------- */

/**
 * @brief 将节点转换为 Follower 状态
 * @param node 节点指针
 * @param term 新的任期号（如果大于当前任期则更新）
 * @return RAFT_OK 成功，RAFT_ERR 失败
 */
int raft_become_follower(raft_node_t *node, uint64_t term);

/**
 * @brief 将节点转换为 Candidate 状态并开始选举
 * @param node 节点指针
 * @return RAFT_OK 成功，RAFT_ERR 失败
 */
int raft_become_candidate(raft_node_t *node);

/**
 * @brief 将节点转换为 Leader 状态
 * @param node 节点指针
 * @return RAFT_OK 成功，RAFT_ERR 失败
 */
int raft_become_leader(raft_node_t *node);

/* ---------- 日志操作 ---------- */

/**
 * @brief 追加日志条目到节点日志
 * @param node 节点指针
 * @param data 日志数据（SDS 字符串）
 * @param data_len 数据长度
 * @return RAFT_OK 成功，RAFT_ERR 失败
 */
int raft_log_append(raft_node_t *node, sds_t data, size_t data_len);

/**
 * @brief 获取指定索引的日志条目
 * @param node 节点指针
 * @param index 日志索引
 * @return 成功返回日志条目指针，失败返回 NULL
 */
raft_log_entry_t *raft_log_get(raft_node_t *node, uint64_t index);

/**
 * @brief 获取最后一条日志条目
 * @param node 节点指针
 * @return 成功返回日志条目指针，失败返回 NULL
 */
raft_log_entry_t *raft_log_get_last(raft_node_t *node);

/**
 * @brief 获取最后一条日志的索引
 * @param node 节点指针
 * @return 最后一条日志的索引（如果没有日志则返回 0）
 */
uint64_t raft_log_get_last_index(raft_node_t *node);

/**
 * @brief 获取最后一条日志的任期号
 * @param node 节点指针
 * @return 最后一条日志的任期号（如果没有日志则返回 0）
 */
uint64_t raft_log_get_last_term(raft_node_t *node);

/**
 * @brief 从指定索引开始删除日志条目
 * @param node 节点指针
 * @param index 起始删除索引（包含该索引）
 * @return RAFT_OK 成功，RAFT_ERR 失败
 */
int raft_log_delete_from(raft_node_t *node, uint64_t index);

/**
 * @brief 删除日志条目并释放资源
 * @param entry 要删除的日志条目指针
 */
void raft_log_entry_delete(raft_log_entry_t *entry);

/**
 * @brief 创建新的日志条目
 * @param term 任期号
 * @param index 索引
 * @param data 数据（SDS 字符串，会被复制）
 * @param data_len 数据长度
 * @return 成功返回日志条目指针，失败返回 NULL
 */
raft_log_entry_t *raft_log_entry_create(uint64_t term, uint64_t index, 
                                         sds_t data, size_t data_len);

/* ---------- RPC 处理函数 ---------- */

/**
 * @brief 处理 RequestVote RPC 请求
 * @param node 节点指针
 * @param args 请求参数
 * @param result 响应结果（输出参数）
 * @return RAFT_OK 成功，RAFT_ERR 失败
 */
int raft_handle_request_vote(raft_node_t *node, raft_request_vote_args_t *args, 
                             raft_request_vote_result_t *result);

/**
 * @brief 处理 AppendEntries RPC 请求
 * @param node 节点指针
 * @param args 请求参数
 * @param result 响应结果（输出参数）
 * @return RAFT_OK 成功，RAFT_ERR 失败
 */
int raft_handle_append_entries(raft_node_t *node, raft_append_entries_args_t *args,
                                raft_append_entries_result_t *result);

/* ---------- 选举相关 ---------- */

/**
 * @brief 开始选举过程
 * @param node 节点指针（必须是 Candidate 状态）
 * @return RAFT_OK 成功，RAFT_ERR 失败
 */
int raft_start_election(raft_node_t *node);

/**
 * @brief 节点时钟滴答函数（需要定期调用）
 * @param node 节点指针
 * @param current_time_ms 当前时间（毫秒）
 * @return RAFT_OK 成功，RAFT_ERR 失败
 */
int raft_tick(raft_node_t *node, uint64_t current_time_ms);

/* ---------- 日志复制（仅 Leader 使用） ---------- */

/**
 * @brief 复制日志条目到所有 Follower
 * @param node 节点指针（必须是 Leader 状态）
 * @param data 日志数据
 * @param data_len 数据长度
 * @return RAFT_OK 成功，RAFT_ERR 失败
 */
int raft_replicate_log(raft_node_t *node, sds_t data, size_t data_len);

/**
 * @brief 向所有 Follower 广播 AppendEntries（心跳或日志复制）
 * @param node 节点指针（必须是 Leader 状态）
 * @return RAFT_OK 成功，RAFT_ERR 失败
 */
int raft_broadcast_append_entries(raft_node_t *node);

/**
 * @brief 向指定 Follower 发送 AppendEntries
 * @param node 节点指针（必须是 Leader 状态）
 * @param peer_id 目标 Follower 的节点 ID
 * @return RAFT_OK 成功，RAFT_ERR 失败
 */
int raft_send_append_entries_to_peer(raft_node_t *node, uint64_t peer_id);

/* ---------- 工具函数 ---------- */

/**
 * @brief 获取节点的当前任期号
 * @param node 节点指针
 * @return 当前任期号（如果节点无效则返回 0）
 */
uint64_t raft_get_current_term(raft_node_t *node);

/**
 * @brief 获取节点的当前状态
 * @param node 节点指针
 * @return 节点状态（如果节点无效则返回 RAFT_STATE_FOLLOWER）
 */
raft_state_t raft_get_state(raft_node_t *node);

/**
 * @brief 检查节点是否为 Leader
 * @param node 节点指针
 * @return true 是 Leader，false 不是
 */
bool raft_is_leader(raft_node_t *node);

/**
 * @brief 添加对等节点到集群
 * @param node 节点指针
 * @param peer_id 要添加的节点 ID
 * @return RAFT_OK 成功，RAFT_ERR 失败
 */
int raft_add_peer(raft_node_t *node, uint64_t peer_id);

/**
 * @brief 从集群中移除对等节点
 * @param node 节点指针
 * @param peer_id 要移除的节点 ID
 * @return RAFT_OK 成功，RAFT_ERR 失败
 */
int raft_remove_peer(raft_node_t *node, uint64_t peer_id);

/* ---------- 配置函数 ---------- */

/**
 * @brief 设置选举超时时间
 * @param node 节点指针
 * @param timeout_ms 超时时间（毫秒）
 */
void raft_set_election_timeout(raft_node_t *node, uint64_t timeout_ms);

/**
 * @brief 设置心跳间隔时间
 * @param node 节点指针
 * @param interval_ms 间隔时间（毫秒）
 */
void raft_set_heartbeat_interval(raft_node_t *node, uint64_t interval_ms);

#endif /* __LATTE_RAFT_H */
