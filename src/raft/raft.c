/**
 * @file raft.c
 * @brief Raft 分布式一致性协议核心实现
 * 
 * 本文件实现了 Raft 协议的所有核心功能，包括：
 * - 节点生命周期管理（创建、初始化、删除）
 * - 状态转换（Follower、Candidate、Leader）
 * - 日志操作（追加、查询、删除）
 * - RPC 处理（RequestVote、AppendEntries）
 * - 选举机制
 * - 日志复制机制
 */

/* 头文件包含 */
#include "raft.h"                    /* Raft 协议头文件 */
#include "utils/monotonic.h"         /* 单调时钟，用于获取高精度时间 */
#include "dict/dict.h"               /* 字典/哈希表数据结构 */
#include "dict/dict_plugins.h"       /* 字典插件，提供不同类型的字典实现 */
#include <string.h>                  /* 字符串操作函数 */
#include <stdlib.h>                  /* 标准库函数（malloc、rand 等） */
#include <time.h>                    /* 时间相关函数 */
#include <stdio.h>                   /* 标准输入输出 */

/* ==================== 内部辅助结构和函数 ==================== */

/**
 * @brief 对等节点信息结构（已废弃，保留用于未来扩展）
 * 
 * 原本用于跟踪每个对等节点的日志复制状态，现在使用字典来存储。
 */
typedef struct raft_peer_info_t {
    uint64_t peer_id;      /* 对等节点 ID */
    uint64_t next_index;   /* 下一个要发送的日志索引 */
    uint64_t match_index;  /* 已匹配的最高日志索引 */
} raft_peer_info_t;

/**
 * @brief uint64_t 类型键的哈希函数
 * 
 * 用于在字典中使用 uint64_t 作为键时计算哈希值。
 * 对于 uint64_t，直接返回其值作为哈希值。
 * 
 * @param key 指向 uint64_t 的指针
 * @return 哈希值（即 uint64_t 的值）
 */
static uint64_t dict_uint64_hash(const void *key) {
    const uint64_t *k = (const uint64_t *)key;  /* 将 void* 转换为 uint64_t* */
    return *k;  /* 返回 uint64_t 的值作为哈希值 */
}

/**
 * @brief uint64_t 类型键的比较函数
 * 
 * 用于在字典中比较两个 uint64_t 键是否相等。
 * 
 * @param privdata 字典私有数据（未使用）
 * @param key1 第一个键的指针
 * @param key2 第二个键的指针
 * @return 1 表示相等，0 表示不相等
 */
static int dict_uint64_key_compare(dict_t *privdata, const void *key1, const void *key2) {
    DICT_NOTUSED(privdata);  /* 标记未使用的参数，避免编译警告 */
    const uint64_t *k1 = (const uint64_t *)key1;  /* 转换第一个键 */
    const uint64_t *k2 = (const uint64_t *)key2;  /* 转换第二个键 */
    return (*k1 == *k2) ? 1 : 0;  /* 比较两个值，相等返回 1，否则返回 0 */
}

/**
 * @brief uint64_t 键类型的字典函数表
 * 
 * 定义了使用 uint64_t 作为键的字典所需的所有函数。
 * 用于在 Leader 状态中存储 next_index 和 match_index。
 */
static dict_func_t dict_type_uint64 = {
    dict_uint64_hash,           /* 哈希函数：计算键的哈希值 */
    NULL,                        /* keyDup：键复制函数（不需要，直接使用指针） */
    NULL,                        /* valDup：值复制函数（不需要，直接使用指针） */
    dict_uint64_key_compare,     /* keyCompare：键比较函数 */
    NULL,                        /* keyDestructor：键析构函数（不需要） */
    NULL,                        /* valDestructor：值析构函数（手动管理） */
    NULL,                        /* expandAllowed：扩展允许函数（使用默认） */
    NULL                         /* dictEntryMetadataBytes：元数据字节数（不需要） */
};

/**
 * @brief 获取当前时间（毫秒）
 * 
 * 使用单调时钟获取当前时间戳，转换为毫秒。
 * 如果单调时钟未初始化，则先初始化。
 * 
 * @return 当前时间戳（毫秒）
 */
static uint64_t get_current_time_ms(void) {
    if (getMonotonicUs == NULL) {  /* 检查单调时钟函数指针是否已初始化 */
        monotonicInit();            /* 如果未初始化，则初始化单调时钟 */
    }
    return getMonotonicUs() / 1000;  /* 获取微秒时间戳并转换为毫秒 */
}

/**
 * @brief 生成随机选举超时时间
 * 
 * 在基础超时时间上添加随机抖动，避免多个节点同时发起选举。
 * 随机范围：base_timeout 到 base_timeout * 1.5
 * 
 * @param base_timeout 基础超时时间（毫秒）
 * @return 添加随机抖动后的超时时间（毫秒）
 */
static uint64_t random_election_timeout(uint64_t base_timeout) {
    /* 添加随机抖动：基础超时时间到基础超时时间的 1.5 倍 */
    return base_timeout + (rand() % (base_timeout / 2));  /* 随机数范围是 0 到 base_timeout/2 */
}

/* ==================== 日志条目操作 ==================== */

/**
 * @brief 创建新的日志条目
 * 
 * 分配内存并初始化一个日志条目结构。
 * 如果提供了数据，会复制一份（使用 sds_dup），避免外部修改影响日志。
 * 
 * @param term 任期号：该条目被 Leader 接收时的任期
 * @param index 索引：该条目在日志中的位置
 * @param data 数据：SDS 字符串，会被复制（如果为 NULL 则不复制）
 * @param data_len 数据长度：data 的字节长度
 * @return 成功返回日志条目指针，失败返回 NULL
 */
raft_log_entry_t *raft_log_entry_create(uint64_t term, uint64_t index, 
                                         sds_t data, size_t data_len) {
    /* 分配日志条目内存 */
    raft_log_entry_t *entry = zmalloc(sizeof(raft_log_entry_t));
    if (!entry) return NULL;  /* 内存分配失败，返回 NULL */
    
    /* 设置日志条目的基本字段 */
    entry->term = term;       /* 设置任期号 */
    entry->index = index;     /* 设置索引 */
    entry->data = data ? sds_dup(data) : NULL;  /* 如果提供了数据，则复制一份；否则为 NULL */
    entry->data_len = data_len;  /* 设置数据长度 */
    
    return entry;  /* 返回创建的日志条目 */
}

/* ==================== 节点生命周期管理 ==================== */

/**
 * @brief 创建新的 Raft 节点
 * 
 * 分配内存并初始化一个新的 Raft 节点。
 * 节点初始状态为 FOLLOWER，任期号为 0。
 * 
 * @param node_id 节点 ID：集群中唯一的节点标识符
 * @return 成功返回节点指针，失败返回 NULL
 */
raft_node_t *raft_node_create(uint64_t node_id) {
    /* 分配节点内存 */
    raft_node_t *node = zmalloc(sizeof(raft_node_t));
    if (!node) return NULL;  /* 内存分配失败，返回 NULL */
    
    /* 初始化节点，如果初始化失败则释放内存并返回 NULL */
    if (raft_node_init(node, node_id) != RAFT_OK) {
        zfree(node);  /* 释放已分配的内存 */
        return NULL;  /* 初始化失败，返回 NULL */
    }
    
    return node;  /* 返回成功创建的节点 */
}

/**
 * @brief 删除 Raft 节点并释放所有资源
 * 
 * 递归释放节点及其所有子结构占用的内存：
 * - 持久化状态（包括日志列表中的所有条目）
 * - 易失状态
 * - Leader 状态（如果存在）
 * - 对等节点列表
 * 
 * @param node 要删除的节点指针
 */
void raft_node_delete(raft_node_t *node) {
    if (!node) return;  /* 如果节点指针为空，直接返回 */
    
    /* 删除持久化状态 */
    if (node->persistent) {
        if (node->persistent->log) {
            /* 遍历日志列表，释放每个日志条目 */
            list_iterator_t *iter = list_get_iterator(node->persistent->log, LIST_ITERATOR_OPTION_HEAD);
            list_node_t *ln;
            while ((ln = list_next(iter)) != NULL) {  /* 遍历所有日志条目 */
                raft_log_entry_t *entry = (raft_log_entry_t *)list_node_value(ln);
                if (entry) {
                    if (entry->data) sds_delete(entry->data);  /* 释放日志条目的数据（SDS 字符串） */
                    zfree(entry);  /* 释放日志条目结构本身 */
                }
            }
            list_iterator_delete(iter);  /* 释放迭代器 */
            list_delete(node->persistent->log);  /* 删除日志列表（此时列表已为空） */
        }
        zfree(node->persistent);  /* 释放持久化状态结构 */
    }
    
    /* 删除易失状态 */
    if (node->volatile_state) {
        zfree(node->volatile_state);  /* 释放易失状态结构 */
    }
    
    /* 删除 Leader 状态（仅在 Leader 状态下存在） */
    if (node->leader_state) {
        if (node->leader_state->next_index) {
            dict_delete(node->leader_state->next_index);  /* 删除 next_index 字典（内部会释放所有值） */
        }
        if (node->leader_state->match_index) {
            dict_delete(node->leader_state->match_index);  /* 删除 match_index 字典（内部会释放所有值） */
        }
        zfree(node->leader_state);  /* 释放 Leader 状态结构 */
    }
    
    /* 删除对等节点列表 */
    if (node->peers) {
        list_delete(node->peers);  /* 删除对等节点列表（注意：列表中的节点 ID 指针需要手动释放，但这里简化处理） */
    }
    
    zfree(node);  /* 最后释放节点结构本身 */
}

/**
 * @brief 初始化 Raft 节点
 * 
 * 初始化节点的所有状态和数据结构。
 * 节点初始状态为 FOLLOWER，任期号为 0，日志中有一个初始空条目（索引 0）。
 * 
 * @param node 节点指针（必须已分配内存）
 * @param node_id 节点 ID
 * @return RAFT_OK 成功，RAFT_ERR 失败
 */
int raft_node_init(raft_node_t *node, uint64_t node_id) {
    if (!node) return RAFT_ERR;  /* 节点指针为空，返回错误 */
    
    /* 初始化单调时钟（如果尚未初始化） */
    if (getMonotonicUs == NULL) {  /* 检查单调时钟函数指针 */
        monotonicInit();            /* 初始化单调时钟 */
    }
    
    /* 清零节点内存，确保所有字段初始化为 0 */
    memset(node, 0, sizeof(raft_node_t));
    node->node_id = node_id;              /* 设置节点 ID */
    node->state = RAFT_STATE_FOLLOWER;     /* 初始状态为 FOLLOWER */
    
    /* 初始化持久化状态 */
    node->persistent = zmalloc(sizeof(raft_persistent_state_t));
    if (!node->persistent) return RAFT_ERR;  /* 内存分配失败，返回错误 */
    node->persistent->current_term = 0;      /* 初始任期号为 0 */
    node->persistent->voted_for = 0;         /* 初始未投票给任何候选者 */
    node->persistent->log = list_new();      /* 创建日志列表 */
    if (!node->persistent->log) {            /* 日志列表创建失败 */
        zfree(node->persistent);             /* 释放已分配的持久化状态 */
        return RAFT_ERR;                     /* 返回错误 */
    }
    
    /* 添加初始空日志条目（索引 0，任期 0） */
    /* 这是 Raft 协议的要求：日志从索引 1 开始，索引 0 是一个占位条目 */
    raft_log_entry_t *initial_entry = raft_log_entry_create(0, 0, NULL, 0);
    if (!initial_entry) {                   /* 初始条目创建失败 */
        list_delete(node->persistent->log); /* 删除日志列表 */
        zfree(node->persistent);            /* 释放持久化状态 */
        return RAFT_ERR;                    /* 返回错误 */
    }
    list_add_node_tail(node->persistent->log, initial_entry);  /* 将初始条目添加到日志末尾 */
    
    /* 初始化易失状态 */
    node->volatile_state = zmalloc(sizeof(raft_volatile_state_t));
    if (!node->volatile_state) {            /* 易失状态分配失败 */
        list_delete(node->persistent->log); /* 清理已分配的资源 */
        zfree(node->persistent);
        return RAFT_ERR;
    }
    node->volatile_state->commit_index = 0;  /* 初始提交索引为 0 */
    node->volatile_state->last_applied = 0;   /* 初始最后应用索引为 0 */
    
    /* 初始化对等节点列表 */
    node->peers = list_new();
    if (!node->peers) {                     /* 对等节点列表创建失败 */
        zfree(node->volatile_state);        /* 清理已分配的资源 */
        list_delete(node->persistent->log);
        zfree(node->persistent);
        return RAFT_ERR;
    }
    
    /* 设置默认超时时间 */
    node->election_timeout = 150;    /* 选举超时：150 毫秒（默认值） */
    node->heartbeat_interval = 50;   /* 心跳间隔：50 毫秒（默认值） */
    
    /* 初始化定时器 */
    node->last_heartbeat = get_current_time_ms();  /* 记录当前时间作为最后心跳时间 */
    /* 设置选举截止时间：当前时间 + 随机选举超时（避免多个节点同时选举） */
    node->election_deadline = get_current_time_ms() + random_election_timeout(node->election_timeout);
    
    /* 初始化统计信息 */
    node->votes_received = 0;   /* 收到的投票数初始化为 0 */
    node->election_count = 0;   /* 选举次数初始化为 0 */
    
    return RAFT_OK;  /* 初始化成功 */
}

/* ==================== 状态管理 ==================== */

/**
 * @brief 将节点转换为 Follower 状态
 * 
 * 当节点收到来自更高任期 Leader 的心跳，或发现自己的任期过期时，
 * 需要转换为 Follower 状态。这会清理 Leader 状态并重置选举定时器。
 * 
 * @param node 节点指针
 * @param term 新的任期号（如果大于当前任期则更新）
 * @return RAFT_OK 成功，RAFT_ERR 失败
 */
int raft_become_follower(raft_node_t *node, uint64_t term) {
    if (!node) return RAFT_ERR;  /* 节点指针为空，返回错误 */
    
    /* 日志记录：节点转换为 Follower（已注释，避免依赖 log 模块） */
    /* LATTE_LIB_LOG(LOG_DEBUG, "Node %llu becoming FOLLOWER, term %llu", 
                  node->node_id, term); */
    
    /* 设置节点状态为 FOLLOWER */
    node->state = RAFT_STATE_FOLLOWER;
    
    /* 如果新任期大于当前任期，更新任期并清除投票记录 */
    if (term > node->persistent->current_term) {
        node->persistent->current_term = term;  /* 更新当前任期 */
        node->persistent->voted_for = 0;        /* 清除投票记录（新任期可以重新投票） */
    }
    
    /* 重置选举截止时间 */
    /* 重新计算选举超时，避免立即触发选举 */
    node->election_deadline = get_current_time_ms() + 
                              random_election_timeout(node->election_timeout);
    node->votes_received = 0;  /* 重置收到的投票数 */
    
    /* 清理 Leader 状态（如果存在） */
    /* 从 Leader 或 Candidate 转换为 Follower 时需要清理 Leader 状态 */
    if (node->leader_state) {
        if (node->leader_state->next_index) {
            /* 释放 next_index 字典中的所有值（uint64_t* 指针） */
            latte_iterator_t *iter = dict_get_latte_iterator(node->leader_state->next_index);
            while (protected_dict_iterator_has_next(iter)) {  /* 遍历字典中的所有条目 */
                latte_iterator_pair_t *pair = (latte_iterator_pair_t *)protected_dict_iterator_next(iter);
                if (pair && latte_pair_value(pair)) {  /* 如果值存在 */
                    zfree(latte_pair_value(pair));      /* 释放值（uint64_t*） */
                }
            }
            protected_dict_iterator_delete(iter);  /* 释放迭代器 */
            dict_delete(node->leader_state->next_index);  /* 删除字典 */
        }
        if (node->leader_state->match_index) {
            /* 释放 match_index 字典中的所有值（uint64_t* 指针） */
            latte_iterator_t *iter = dict_get_latte_iterator(node->leader_state->match_index);
            while (protected_dict_iterator_has_next(iter)) {  /* 遍历字典中的所有条目 */
                latte_iterator_pair_t *pair = (latte_iterator_pair_t *)protected_dict_iterator_next(iter);
                if (pair && latte_pair_value(pair)) {  /* 如果值存在 */
                    zfree(latte_pair_value(pair));      /* 释放值（uint64_t*） */
                }
            }
            protected_dict_iterator_delete(iter);  /* 释放迭代器 */
            dict_delete(node->leader_state->match_index);  /* 删除字典 */
        }
        zfree(node->leader_state);      /* 释放 Leader 状态结构 */
        node->leader_state = NULL;      /* 将指针置为 NULL */
    }
    
    return RAFT_OK;  /* 转换成功 */
}

/**
 * @brief 将节点转换为 Candidate 状态并开始选举
 * 
 * 当 Follower 在选举超时内没有收到 Leader 的心跳时，会转换为 Candidate 状态。
 * 节点会：
 * 1. 增加当前任期号
 * 2. 投票给自己
 * 3. 重置选举截止时间
 * 
 * @param node 节点指针
 * @return RAFT_OK 成功，RAFT_ERR 失败
 */
int raft_become_candidate(raft_node_t *node) {
    if (!node) return RAFT_ERR;  /* 节点指针为空，返回错误 */
    
    /* 日志记录：节点转换为 Candidate（已注释） */
    /* LATTE_LIB_LOG(LOG_DEBUG, "Node %llu becoming CANDIDATE, term %llu", 
                  node->node_id, node->persistent->current_term + 1); */
    
    /* 设置节点状态为 CANDIDATE */
    node->state = RAFT_STATE_CANDIDATE;
    
    /* 增加当前任期号（开始新的选举） */
    node->persistent->current_term++;
    
    /* 投票给自己（Raft 协议要求：候选者必须投票给自己） */
    node->persistent->voted_for = node->node_id;
    
    /* 统计收到的投票数（包括自己的一票） */
    node->votes_received = 1;
    
    /* 增加选举计数（用于统计） */
    node->election_count++;
    
    /* 重置选举截止时间 */
    /* 设置新的选举超时，如果在这个时间内没有获得足够投票，会重新选举 */
    node->election_deadline = get_current_time_ms() + 
                              random_election_timeout(node->election_timeout);
    
    return RAFT_OK;  /* 转换成功 */
}

/**
 * @brief 将节点转换为 Leader 状态
 * 
 * 当 Candidate 获得大多数节点的投票后，会转换为 Leader 状态。
 * Leader 需要：
 * 1. 初始化 Leader 状态（next_index 和 match_index）
 * 2. 立即发送心跳（空的 AppendEntries）通知所有 Follower
 * 
 * @param node 节点指针（必须是 Candidate 状态且已获得大多数投票）
 * @return RAFT_OK 成功，RAFT_ERR 失败
 */
int raft_become_leader(raft_node_t *node) {
    if (!node) return RAFT_ERR;  /* 节点指针为空，返回错误 */
    
    /* 日志记录：节点转换为 Leader（已注释） */
    /* LATTE_LIB_LOG(LOG_DEBUG, "Node %llu becoming LEADER, term %llu", 
                  node->node_id, node->persistent->current_term); */
    
    /* 设置节点状态为 LEADER */
    node->state = RAFT_STATE_LEADER;
    node->votes_received = 0;  /* 重置投票计数（选举已完成） */
    
    /* 初始化 Leader 状态 */
    node->leader_state = zmalloc(sizeof(raft_leader_state_t));
    if (!node->leader_state) return RAFT_ERR;  /* 内存分配失败 */
    
    /* 创建字典用于存储每个 Follower 的 next_index 和 match_index */
    node->leader_state->next_index = dict_new(&dict_type_uint64);   /* 创建 next_index 字典 */
    node->leader_state->match_index = dict_new(&dict_type_uint64);  /* 创建 match_index 字典 */
    
    /* 检查字典创建是否成功 */
    if (!node->leader_state->next_index || !node->leader_state->match_index) {
        /* 如果创建失败，清理已创建的资源 */
        if (node->leader_state->next_index) dict_delete(node->leader_state->next_index);
        if (node->leader_state->match_index) dict_delete(node->leader_state->match_index);
        zfree(node->leader_state);
        node->leader_state = NULL;
        return RAFT_ERR;  /* 返回错误 */
    }
    
    /* 为所有对等节点初始化 next_index 和 match_index */
    /* next_index 初始化为 Leader 最后日志索引 + 1（表示要发送的下一条日志） */
    /* match_index 初始化为 0（表示已知在该节点上已复制的最高日志索引） */
    uint64_t last_index = raft_log_get_last_index(node);  /* 获取 Leader 的最后日志索引 */
    list_iterator_t *iter = list_get_iterator(node->peers, LIST_ITERATOR_OPTION_HEAD);
    list_node_t *ln;
    while ((ln = list_next(iter)) != NULL) {  /* 遍历所有对等节点 */
        uint64_t *peer_id = (uint64_t *)list_node_value(ln);
        if (peer_id) {
            /* 为 next_index 创建条目 */
            uint64_t *next_idx = zmalloc(sizeof(uint64_t));
            *next_idx = last_index + 1;  /* 初始化为最后索引 + 1 */
            uint64_t *peer_id_key = zmalloc(sizeof(uint64_t));  /* 创建键的副本 */
            *peer_id_key = *peer_id;
            dict_add(node->leader_state->next_index, peer_id_key, next_idx);  /* 添加到字典 */
            
            /* 为 match_index 创建条目 */
            uint64_t *match_idx = zmalloc(sizeof(uint64_t));
            *match_idx = 0;  /* 初始化为 0 */
            uint64_t *peer_id_key2 = zmalloc(sizeof(uint64_t));  /* 创建键的副本 */
            *peer_id_key2 = *peer_id;
            dict_add(node->leader_state->match_index, peer_id_key2, match_idx);  /* 添加到字典 */
        }
    }
    list_iterator_delete(iter);  /* 释放迭代器 */
    
    /* 发送初始空 AppendEntries（心跳） */
    /* 立即通知所有 Follower 自己已成为 Leader，防止它们发起新的选举 */
    raft_broadcast_append_entries(node);
    
    return RAFT_OK;  /* 转换成功 */
}

/* ==================== 日志操作 ==================== */

/**
 * @brief 追加日志条目到节点日志
 * 
 * 在日志末尾添加新的日志条目。新条目的索引为最后一条日志索引 + 1，
 * 任期号为节点的当前任期。
 * 
 * @param node 节点指针
 * @param data 日志数据（SDS 字符串，会被复制）
 * @param data_len 数据长度
 * @return RAFT_OK 成功，RAFT_ERR 失败
 */
int raft_log_append(raft_node_t *node, sds_t data, size_t data_len) {
    if (!node || !node->persistent || !node->persistent->log) return RAFT_ERR;  /* 参数检查 */
    
    /* 计算新日志条目的索引 */
    uint64_t last_index = raft_log_get_last_index(node);  /* 获取最后一条日志的索引 */
    uint64_t new_index = last_index + 1;                  /* 新条目的索引 = 最后索引 + 1 */
    
    /* 创建新的日志条目 */
    /* 使用当前任期号作为新条目的任期 */
    raft_log_entry_t *entry = raft_log_entry_create(
        node->persistent->current_term, new_index, data, data_len);
    if (!entry) return RAFT_ERR;  /* 创建失败，返回错误 */
    
    /* 将新条目添加到日志列表的末尾 */
    list_add_node_tail(node->persistent->log, entry);
    
    /* 日志记录：已追加日志条目（已注释） */
    /* LATTE_LIB_LOG(LOG_DEBUG, "Node %llu appended log entry at index %llu, term %llu",
                  node->node_id, new_index, node->persistent->current_term); */
    
    return RAFT_OK;  /* 追加成功 */
}

/**
 * @brief 获取指定索引的日志条目
 * 
 * 在日志列表中查找指定索引的日志条目。
 * 索引 0 是初始空条目，位于列表头部。
 * 
 * @param node 节点指针
 * @param index 日志索引（从 0 开始）
 * @return 成功返回日志条目指针，失败返回 NULL
 */
raft_log_entry_t *raft_log_get(raft_node_t *node, uint64_t index) {
    if (!node || !node->persistent || !node->persistent->log) return NULL;  /* 参数检查 */
    
    /* 索引 0 是初始空条目，位于列表头部 */
    if (index == 0) {
        list_node_t *head = list_first(node->persistent->log);  /* 获取列表头部节点 */
        if (head) return (raft_log_entry_t *)list_node_value(head);  /* 返回头部节点的值 */
        return NULL;  /* 列表为空，返回 NULL */
    }
    
    /* 查找指定索引的日志条目 */
    list_iterator_t *iter = list_get_iterator(node->persistent->log, LIST_ITERATOR_OPTION_HEAD);
    list_node_t *ln;
    while ((ln = list_next(iter)) != NULL) {  /* 遍历日志列表 */
        raft_log_entry_t *entry = (raft_log_entry_t *)list_node_value(ln);
        if (entry && entry->index == index) {  /* 找到匹配索引的条目 */
            list_iterator_delete(iter);        /* 释放迭代器 */
            return entry;                       /* 返回找到的条目 */
        }
    }
    list_iterator_delete(iter);  /* 释放迭代器 */
    
    return NULL;  /* 未找到指定索引的条目 */
}

/**
 * @brief 获取最后一条日志条目
 * 
 * 返回日志列表中的最后一条日志条目（列表尾部）。
 * 
 * @param node 节点指针
 * @return 成功返回最后一条日志条目指针，失败返回 NULL
 */
raft_log_entry_t *raft_log_get_last(raft_node_t *node) {
    if (!node || !node->persistent || !node->persistent->log) return NULL;  /* 参数检查 */
    
    list_node_t *tail = list_last(node->persistent->log);  /* 获取列表尾部节点 */
    if (tail) return (raft_log_entry_t *)list_node_value(tail);  /* 返回尾部节点的值 */
    return NULL;  /* 列表为空，返回 NULL */
}

/**
 * @brief 获取最后一条日志的索引
 * 
 * 返回日志中最后一条条目的索引号。
 * 如果日志为空（只有初始空条目），返回 0。
 * 
 * @param node 节点指针
 * @return 最后一条日志的索引（如果没有日志则返回 0）
 */
uint64_t raft_log_get_last_index(raft_node_t *node) {
    raft_log_entry_t *last = raft_log_get_last(node);  /* 获取最后一条日志 */
    if (last) return last->index;  /* 如果存在，返回其索引 */
    return 0;  /* 如果不存在，返回 0 */
}

/**
 * @brief 获取最后一条日志的任期号
 * 
 * 返回日志中最后一条条目的任期号。
 * 如果日志为空（只有初始空条目），返回 0。
 * 
 * @param node 节点指针
 * @return 最后一条日志的任期号（如果没有日志则返回 0）
 */
uint64_t raft_log_get_last_term(raft_node_t *node) {
    raft_log_entry_t *last = raft_log_get_last(node);  /* 获取最后一条日志 */
    if (last) return last->term;  /* 如果存在，返回其任期号 */
    return 0;  /* 如果不存在，返回 0 */
}

/**
 * @brief 从指定索引开始删除日志条目
 * 
 * 删除索引 >= index 的所有日志条目。这在日志冲突时使用：
 * 当 Leader 发现 Follower 的日志与自己的不一致时，会删除冲突的条目。
 * 
 * @param node 节点指针
 * @param index 起始删除索引（包含该索引，即删除 index 及之后的所有条目）
 * @return RAFT_OK 成功，RAFT_ERR 失败
 */
int raft_log_delete_from(raft_node_t *node, uint64_t index) {
    if (!node || !node->persistent || !node->persistent->log) return RAFT_ERR;  /* 参数检查 */
    
    /* 首先收集要删除的节点 */
    list_node_t *current = list_first(node->persistent->log);  /* 从列表头部开始 */
    list_node_t *next;                                         /* 下一个节点的指针 */
    list_node_t **nodes_to_delete = NULL;                      /* 要删除的节点指针数组 */
    int count = 0;                                             /* 要删除的节点数量 */
    int capacity = 0;                                          /* 数组容量 */
    
    /* 第一遍遍历：收集需要删除的节点 */
    while (current) {
        next = list_next_node(current);  /* 保存下一个节点指针（因为删除后无法获取） */
        raft_log_entry_t *entry = (raft_log_entry_t *)list_node_value(current);
        if (entry && entry->index >= index) {  /* 如果条目索引 >= 目标索引，需要删除 */
            /* 如果数组容量不足，扩展数组 */
            if (count >= capacity) {
                capacity = capacity ? capacity * 2 : 16;  /* 容量翻倍，初始为 16 */
                nodes_to_delete = zrealloc(nodes_to_delete, capacity * sizeof(list_node_t *));
            }
            nodes_to_delete[count++] = current;  /* 将节点指针添加到数组 */
        }
        current = next;  /* 移动到下一个节点 */
    }
    
    /* 第二遍遍历：删除节点并释放日志条目 */
    for (int i = 0; i < count; i++) {
        raft_log_entry_t *entry = (raft_log_entry_t *)list_node_value(nodes_to_delete[i]);
        if (entry) {
            if (entry->data) sds_delete(entry->data);  /* 释放日志条目的数据（SDS 字符串） */
            zfree(entry);  /* 释放日志条目结构 */
        }
        list_del_node(node->persistent->log, nodes_to_delete[i]);  /* 从列表中删除节点 */
    }
    
    if (nodes_to_delete) zfree(nodes_to_delete);  /* 释放节点指针数组 */
    
    return RAFT_OK;  /* 删除成功 */
}

/**
 * @brief 删除日志条目并释放资源
 * 
 * 释放日志条目占用的内存，包括数据（SDS 字符串）和结构本身。
 * 
 * @param entry 要删除的日志条目指针
 */
void raft_log_entry_delete(raft_log_entry_t *entry) {
    if (!entry) return;  /* 如果条目为空，直接返回 */
    if (entry->data) sds_delete(entry->data);  /* 如果数据存在，释放 SDS 字符串 */
    zfree(entry);  /* 释放日志条目结构本身 */
}

/* ==================== RPC 处理函数 ==================== */

/**
 * @brief 处理 RequestVote RPC 请求
 * 
 * 当 Candidate 请求投票时，Follower 会调用此函数处理请求。
 * 根据 Raft 协议，节点会授予投票如果：
 * 1. 请求的任期 >= 当前任期
 * 2. 当前任期未投票，或已投票给该候选者
 * 3. 候选者的日志至少和接收者一样新（比较最后一条日志的任期和索引）
 * 
 * @param node 节点指针（接收投票请求的节点）
 * @param args 请求参数（候选者发送的投票请求）
 * @param result 响应结果（输出参数，返回投票决策）
 * @return RAFT_OK 成功，RAFT_ERR 失败
 */
int raft_handle_request_vote(raft_node_t *node, raft_request_vote_args_t *args, 
                              raft_request_vote_result_t *result) {
    if (!node || !args || !result) return RAFT_ERR;  /* 参数检查 */
    
    /* 初始化响应结果 */
    result->term = node->persistent->current_term;  /* 设置当前任期号 */
    result->vote_granted = false;                   /* 默认不授予投票 */
    
    /* 如果 RPC 请求包含的任期 T > currentTerm：设置 currentTerm = T，转换为 Follower */
    /* 这是 Raft 协议的核心规则：发现更大的任期必须更新并转换为 Follower */
    if (args->term > node->persistent->current_term) {
        raft_become_follower(node, args->term);     /* 转换为 Follower 并更新任期 */
        result->term = node->persistent->current_term;  /* 更新响应中的任期号 */
    }
    
    /* 如果 votedFor 为空或等于 candidateId，且候选者的日志至少和接收者一样新，则授予投票 */
    if (args->term == node->persistent->current_term) {  /* 任期必须匹配 */
        bool log_ok = false;  /* 日志是否足够新的标志 */
        uint64_t last_term = raft_log_get_last_term(node);   /* 获取接收者最后一条日志的任期 */
        uint64_t last_index = raft_log_get_last_index(node); /* 获取接收者最后一条日志的索引 */
        
        /* 候选者的日志至少和接收者一样新，如果满足以下任一条件：
         * - last_log_term > last_term（候选者的最后日志任期更大），或
         * - last_log_term == last_term AND last_log_index >= last_index（任期相同但索引更大或相等）
         */
        if (args->last_log_term > last_term ||
            (args->last_log_term == last_term && args->last_log_index >= last_index)) {
            log_ok = true;  /* 候选者的日志足够新 */
        }
        
        /* 如果满足投票条件：未投票或已投票给该候选者，且日志足够新 */
        if ((node->persistent->voted_for == 0 || 
             node->persistent->voted_for == args->candidate_id) && log_ok) {
            node->persistent->voted_for = args->candidate_id;  /* 记录投票给该候选者 */
            result->vote_granted = true;                       /* 授予投票 */
            node->last_heartbeat = get_current_time_ms();     /* 更新心跳时间（重置选举超时） */
            
            /* 日志记录：授予投票（已注释） */
            /* LATTE_LIB_LOG(LOG_DEBUG, "Node %llu granted vote to candidate %llu",
                          node->node_id, args->candidate_id); */
        }
    }
    
    return RAFT_OK;  /* 处理完成 */
}

/**
 * @brief 处理 AppendEntries RPC 请求
 * 
 * 当 Leader 发送日志复制请求或心跳时，Follower 会调用此函数处理。
 * 这是 Raft 协议的核心 RPC，用于：
 * 1. 心跳：保持 Leader 地位（entries 为空）
 * 2. 日志复制：将日志条目复制到 Follower
 * 
 * @param node 节点指针（接收日志复制请求的节点）
 * @param args 请求参数（Leader 发送的日志复制请求）
 * @param result 响应结果（输出参数，返回操作是否成功）
 * @return RAFT_OK 成功，RAFT_ERR 失败
 */
int raft_handle_append_entries(raft_node_t *node, raft_append_entries_args_t *args,
                                raft_append_entries_result_t *result) {
    if (!node || !args || !result) return RAFT_ERR;  /* 参数检查 */
    
    /* 初始化响应结果 */
    result->term = node->persistent->current_term;  /* 设置当前任期号 */
    result->success = false;                        /* 默认操作失败 */
    
    /* 如果 RPC 请求包含的任期 T > currentTerm：设置 currentTerm = T，转换为 Follower */
    /* 发现更大的任期必须更新并转换为 Follower */
    if (args->term > node->persistent->current_term) {
        raft_become_follower(node, args->term);     /* 转换为 Follower 并更新任期 */
        result->term = node->persistent->current_term;  /* 更新响应中的任期号 */
    }
    
    /* 如果请求的任期 < 当前任期，返回 false（不处理过期的请求） */
    if (args->term < node->persistent->current_term) {
        return RAFT_OK;  /* 直接返回，success 保持为 false */
    }
    
    /* 如果收到来自 Leader 的 AppendEntries，重置选举超时 */
    /* 这确保 Follower 在收到心跳时不会发起选举 */
    node->last_heartbeat = get_current_time_ms();  /* 更新最后心跳时间 */
    if (node->state != RAFT_STATE_FOLLOWER) {     /* 如果当前不是 Follower 状态 */
        raft_become_follower(node, args->term);    /* 转换为 Follower */
    }
    
    /* 如果日志不包含 prev_log_index 位置的条目，或该条目的任期不匹配 prev_log_term，返回 false */
    /* 这表示 Follower 的日志与 Leader 不一致，需要 Leader 回退 next_index */
    if (args->prev_log_index > 0) {  /* prev_log_index 为 0 表示从日志开头开始 */
        raft_log_entry_t *prev_entry = raft_log_get(node, args->prev_log_index);
        if (!prev_entry || prev_entry->term != args->prev_log_term) {  /* 条目不存在或任期不匹配 */
            return RAFT_OK;  /* 返回，success 保持为 false */
        }
    }
    
    /* 如果现有条目与新条目冲突（相同索引但不同任期），删除现有条目及其之后的所有条目 */
    /* 这是 Raft 协议处理日志冲突的方式：删除冲突的条目，用 Leader 的条目替换 */
    if (args->entries && list_length(args->entries) > 0) {  /* 如果有新条目 */
        list_iterator_t *iter = list_get_iterator(args->entries, LIST_ITERATOR_OPTION_HEAD);
        list_node_t *ln;
        while ((ln = list_next(iter)) != NULL) {  /* 遍历新条目 */
            raft_log_entry_t *new_entry = (raft_log_entry_t *)list_node_value(ln);
            if (new_entry) {
                raft_log_entry_t *existing = raft_log_get(node, new_entry->index);  /* 查找现有条目 */
                if (existing && existing->term != new_entry->term) {  /* 如果存在且任期不同 */
                    /* 从该索引开始删除所有后续条目 */
                    raft_log_delete_from(node, new_entry->index);
                    break;  /* 找到第一个冲突后停止，因为后续条目也会被删除 */
                }
            }
        }
        list_iterator_delete(iter);  /* 释放迭代器 */
        
        /* 追加任何日志中不存在的新条目 */
        iter = list_get_iterator(args->entries, LIST_ITERATOR_OPTION_HEAD);
        while ((ln = list_next(iter)) != NULL) {  /* 再次遍历新条目 */
            raft_log_entry_t *new_entry = (raft_log_entry_t *)list_node_value(ln);
            if (new_entry) {
                raft_log_entry_t *existing = raft_log_get(node, new_entry->index);  /* 检查是否已存在 */
                if (!existing) {  /* 如果不存在，则添加 */
                    /* 创建条目的副本并追加到日志 */
                    raft_log_entry_t *entry_copy = raft_log_entry_create(
                        new_entry->term, new_entry->index, 
                        new_entry->data, new_entry->data_len);
                    if (entry_copy) {
                        list_add_node_tail(node->persistent->log, entry_copy);  /* 添加到日志末尾 */
                    }
                }
            }
        }
        list_iterator_delete(iter);  /* 释放迭代器 */
    }
    
    /* 如果 leaderCommit > commitIndex，设置 commitIndex = min(leaderCommit, 最后新条目的索引) */
    /* 这确保 Follower 只提交 Leader 已提交的条目 */
    if (args->leader_commit > node->volatile_state->commit_index) {
        uint64_t last_index = raft_log_get_last_index(node);  /* 获取最后一条日志的索引 */
        /* 提交索引不能超过最后一条日志的索引 */
        node->volatile_state->commit_index = 
            (args->leader_commit < last_index) ? args->leader_commit : last_index;
    }
    
    result->success = true;  /* 操作成功 */
    return RAFT_OK;         /* 返回成功 */
}

/* ==================== 选举相关 ==================== */

/**
 * @brief 开始选举过程
 * 
 * Candidate 节点向所有对等节点发送 RequestVote RPC 请求投票。
 * 如果获得大多数节点的投票（包括自己），则转换为 Leader。
 * 
 * @param node 节点指针（必须是 Candidate 状态）
 * @return RAFT_OK 成功，RAFT_ERR 失败
 */
int raft_start_election(raft_node_t *node) {
    if (!node) return RAFT_ERR;  /* 节点指针为空，返回错误 */
    
    /* 如果节点不是 Candidate 状态，先转换为 Candidate */
    if (node->state != RAFT_STATE_CANDIDATE) {
        raft_become_candidate(node);  /* 转换为 Candidate 并增加任期 */
    }
    
    /* 向所有对等节点发送 RequestVote RPC */
    if (node->send_request_vote) {  /* 检查回调函数是否已设置 */
        /* 准备投票请求参数 */
        raft_request_vote_args_t args;
        args.term = node->persistent->current_term;              /* 候选者的当前任期 */
        args.candidate_id = node->node_id;                      /* 候选者的节点 ID */
        args.last_log_index = raft_log_get_last_index(node);    /* 候选者最后一条日志的索引 */
        args.last_log_term = raft_log_get_last_term(node);      /* 候选者最后一条日志的任期 */
        
        /* 遍历所有对等节点，发送投票请求 */
        list_iterator_t *iter = list_get_iterator(node->peers, LIST_ITERATOR_OPTION_HEAD);
        list_node_t *ln;
        while ((ln = list_next(iter)) != NULL) {  /* 遍历对等节点列表 */
            uint64_t *peer_id = (uint64_t *)list_node_value(ln);
            if (peer_id) {
                raft_request_vote_result_t result;  /* 投票响应结果 */
                /* 发送投票请求 */
                if (node->send_request_vote(node, *peer_id, &args, &result) == RAFT_OK) {
                    /* 如果响应的任期 > 当前任期，说明有更新的 Leader，转换为 Follower */
                    if (result.term > node->persistent->current_term) {
                        raft_become_follower(node, result.term);  /* 转换为 Follower */
                        break;  /* 停止发送请求 */
                    }
                    /* 如果获得投票，增加投票计数 */
                    if (result.vote_granted) {
                        node->votes_received++;  /* 增加收到的投票数 */
                    }
                }
            }
        }
        list_iterator_delete(iter);  /* 释放迭代器 */
    }
    
    /* 检查是否赢得了选举 */
    /* 大多数 = (对等节点数 + 1) / 2 + 1（+1 表示包括自己） */
    uint64_t majority = (list_length(node->peers) + 1) / 2 + 1;
    if (node->votes_received >= majority) {  /* 如果获得大多数投票 */
        raft_become_leader(node);  /* 转换为 Leader */
    }
    
    return RAFT_OK;  /* 选举过程完成 */
}

/**
 * @brief 节点时钟滴答函数（需要定期调用）
 * 
 * 这是 Raft 协议的时间驱动函数，需要定期调用（建议每 10-50ms 调用一次）。
 * 根据节点状态执行不同的操作：
 * - Follower/Candidate: 检查选举超时，如果超时则发起选举
 * - Leader: 定期发送心跳（空的 AppendEntries）
 * 
 * @param node 节点指针
 * @param current_time_ms 当前时间（毫秒）
 * @return RAFT_OK 成功，RAFT_ERR 失败
 */
int raft_tick(raft_node_t *node, uint64_t current_time_ms) {
    if (!node) return RAFT_ERR;  /* 节点指针为空，返回错误 */
    
    switch (node->state) {  /* 根据节点状态执行不同操作 */
        case RAFT_STATE_FOLLOWER:   /* Follower 状态 */
        case RAFT_STATE_CANDIDATE:  /* Candidate 状态 */
            /* 检查选举超时是否已过 */
            /* 如果当前时间 >= 选举截止时间，说明超时，需要发起新选举 */
            if (current_time_ms >= node->election_deadline) {
                raft_become_candidate(node);  /* 转换为 Candidate（如果还不是） */
                raft_start_election(node);    /* 开始选举过程 */
            }
            break;
            
        case RAFT_STATE_LEADER:  /* Leader 状态 */
            /* 定期发送心跳 */
            /* 如果距离上次心跳的时间 >= 心跳间隔，发送新的心跳 */
            if (current_time_ms - node->last_heartbeat >= node->heartbeat_interval) {
                raft_broadcast_append_entries(node);  /* 向所有 Follower 广播心跳 */
                node->last_heartbeat = current_time_ms;  /* 更新最后心跳时间 */
            }
            break;
    }
    
    return RAFT_OK;  /* 处理完成 */
}

/* ==================== 日志复制（仅 Leader 使用） ==================== */

/**
 * @brief 复制日志条目到所有 Follower
 * 
 * Leader 收到客户端请求后，先将日志条目追加到本地日志，
 * 然后复制到所有 Follower。这是 Raft 协议中 Leader 处理写请求的标准流程。
 * 
 * @param node 节点指针（必须是 Leader 状态）
 * @param data 日志数据（SDS 字符串）
 * @param data_len 数据长度
 * @return RAFT_OK 成功，RAFT_ERR 失败
 */
int raft_replicate_log(raft_node_t *node, sds_t data, size_t data_len) {
    if (!node || node->state != RAFT_STATE_LEADER) return RAFT_ERR;  /* 必须是 Leader */
    
    /* 先将条目追加到本地日志 */
    if (raft_log_append(node, data, data_len) != RAFT_OK) {  /* 追加失败 */
        return RAFT_ERR;  /* 返回错误 */
    }
    
    /* 复制到所有 Follower */
    raft_broadcast_append_entries(node);  /* 向所有 Follower 广播新日志条目 */
    
    return RAFT_OK;  /* 复制成功 */
}

/**
 * @brief 向所有 Follower 广播 AppendEntries（心跳或日志复制）
 * 
 * Leader 定期调用此函数发送心跳（空的 AppendEntries）或日志复制请求。
 * 这会向所有对等节点发送 AppendEntries RPC。
 * 
 * @param node 节点指针（必须是 Leader 状态）
 * @return RAFT_OK 成功，RAFT_ERR 失败
 */
int raft_broadcast_append_entries(raft_node_t *node) {
    if (!node || node->state != RAFT_STATE_LEADER) return RAFT_ERR;  /* 必须是 Leader */
    
    /* 遍历所有对等节点，向每个节点发送 AppendEntries */
    list_iterator_t *iter = list_get_iterator(node->peers, LIST_ITERATOR_OPTION_HEAD);
    list_node_t *ln;
    while ((ln = list_next(iter)) != NULL) {  /* 遍历对等节点列表 */
        uint64_t *peer_id = (uint64_t *)list_node_value(ln);
        if (peer_id) {
            raft_send_append_entries_to_peer(node, *peer_id);  /* 向该节点发送 AppendEntries */
        }
    }
    list_iterator_delete(iter);  /* 释放迭代器 */
    
    return RAFT_OK;  /* 广播完成 */
}

/**
 * @brief 向指定 Follower 发送 AppendEntries
 * 
 * Leader 向单个 Follower 发送日志复制请求。这是日志复制的核心函数：
 * 1. 根据 next_index 确定要发送的日志条目
 * 2. 发送 AppendEntries RPC
 * 3. 根据响应更新 next_index 和 match_index
 * 4. 更新 commit_index（如果大多数节点已复制）
 * 
 * @param node 节点指针（必须是 Leader 状态）
 * @param peer_id 目标 Follower 的节点 ID
 * @return RAFT_OK 成功，RAFT_ERR 失败
 */
int raft_send_append_entries_to_peer(raft_node_t *node, uint64_t peer_id) {
    if (!node || node->state != RAFT_STATE_LEADER) return RAFT_ERR;  /* 必须是 Leader */
    
    if (!node->send_append_entries) return RAFT_ERR;  /* 回调函数未设置 */
    
    /* 获取该 Follower 的 next_index（要发送的下一条日志索引） */
    uint64_t peer_id_key = peer_id;  /* 创建键的副本（用于字典查找） */
    dict_entry_t *entry = dict_find(node->leader_state->next_index, &peer_id_key);
    if (!entry) return RAFT_ERR;  /* 未找到该 Follower 的 next_index */
    uint64_t *next_idx_ptr = (uint64_t *)dict_get_entry_val(entry);  /* 获取 next_index 的指针 */
    if (!next_idx_ptr) return RAFT_ERR;  /* 指针为空 */
    uint64_t next_index = *next_idx_ptr;  /* 获取 next_index 的值 */
    
    /* 准备 AppendEntries 请求参数 */
    raft_append_entries_args_t args;
    args.term = node->persistent->current_term;              /* Leader 的当前任期 */
    args.leader_id = node->node_id;                          /* Leader 的节点 ID */
    args.prev_log_index = next_index - 1;                    /* 前一条日志的索引 */
    args.prev_log_term = 0;                                  /* 前一条日志的任期（稍后设置） */
    args.entries = list_new();                               /* 要发送的日志条目列表 */
    args.leader_commit = node->volatile_state->commit_index; /* Leader 的提交索引 */
    
    /* 获取 prev_log_term（前一条日志的任期） */
    if (args.prev_log_index > 0) {  /* 如果前一条日志索引 > 0 */
        raft_log_entry_t *prev_entry = raft_log_get(node, args.prev_log_index);
        if (prev_entry) {
            args.prev_log_term = prev_entry->term;  /* 设置前一条日志的任期 */
        }
    }
    
    /* 收集要发送的日志条目 */
    /* 从 next_index 开始到 last_index 的所有条目 */
    uint64_t last_index = raft_log_get_last_index(node);  /* 获取最后一条日志的索引 */
    for (uint64_t i = next_index; i <= last_index; i++) {  /* 遍历要发送的日志条目 */
        raft_log_entry_t *entry = raft_log_get(node, i);
        if (entry) {
            list_add_node_tail(args.entries, entry);  /* 将条目添加到发送列表 */
        }
    }
    
    /* 发送 AppendEntries RPC */
    raft_append_entries_result_t result;
    if (node->send_append_entries(node, peer_id, &args, &result) == RAFT_OK) {
        /* 如果响应的任期 > 当前任期，说明有更新的 Leader，转换为 Follower */
        if (result.term > node->persistent->current_term) {
            raft_become_follower(node, result.term);  /* 转换为 Follower */
            list_delete(args.entries);                /* 清理资源 */
            return RAFT_ERR;                          /* 返回错误 */
        }
        
        /* 如果操作成功 */
        if (result.success) {
            /* 更新 next_index 和 match_index */
            dict_entry_t *match_entry = dict_find(node->leader_state->match_index, &peer_id_key);
            if (match_entry) {
                uint64_t *match_idx_ptr = (uint64_t *)dict_get_entry_val(match_entry);
                if (match_idx_ptr) {
                    *match_idx_ptr = last_index;  /* 更新 match_index 为最后发送的索引 */
                }
            }
            if (next_idx_ptr) {
                *next_idx_ptr = last_index + 1;  /* 更新 next_index 为最后索引 + 1 */
            }
            
            /* 更新 commit_index（提交索引） */
            /* 对于每个日志索引，检查是否已被大多数节点复制 */
            uint64_t majority = (list_length(node->peers) + 1) / 2 + 1;  /* 计算大多数 */
            for (uint64_t i = node->volatile_state->commit_index + 1; 
                 i <= last_index; i++) {  /* 遍历未提交的日志条目 */
                uint64_t count = 1;  /* 计数：包括自己（Leader） */
                /* 统计已复制该条目的节点数 */
                list_iterator_t *iter = list_get_iterator(node->peers, LIST_ITERATOR_OPTION_HEAD);
                list_node_t *ln;
                while ((ln = list_next(iter)) != NULL) {  /* 遍历所有对等节点 */
                    uint64_t *p_id = (uint64_t *)list_node_value(ln);
                    if (p_id) {
                        uint64_t p_id_key = *p_id;
                        dict_entry_t *m_entry = dict_find(node->leader_state->match_index, &p_id_key);
                        if (m_entry) {
                            uint64_t *m_idx = (uint64_t *)dict_get_entry_val(m_entry);
                            if (m_idx && *m_idx >= i) {  /* 如果该节点已复制到索引 i */
                                count++;  /* 增加计数 */
                            }
                        }
                    }
                }
                list_iterator_delete(iter);  /* 释放迭代器 */
                
                /* 如果该条目已被大多数节点复制，且是当前任期的条目，则提交 */
                raft_log_entry_t *entry = raft_log_get(node, i);
                if (entry && entry->term == node->persistent->current_term && 
                    count >= majority) {  /* 大多数节点已复制且是当前任期 */
                    node->volatile_state->commit_index = i;  /* 更新提交索引 */
                }
            }
        } else {
            /* 如果操作失败，递减 next_index 并重试 */
            /* 这表示 Follower 的日志不一致，需要回退 next_index */
            if (next_idx_ptr && *next_idx_ptr > 1) {  /* 如果 next_index > 1 */
                (*next_idx_ptr)--;  /* 递减 next_index，下次发送更早的条目 */
            }
        }
    }
    
    list_delete(args.entries);  /* 清理临时列表（注意：列表中的条目指针指向原始日志，不需要释放） */
    return RAFT_OK;  /* 操作完成 */
}

/* ==================== 工具函数 ==================== */

/**
 * @brief 获取节点的当前任期号
 * 
 * 返回节点持久化状态中的当前任期号。
 * 
 * @param node 节点指针
 * @return 当前任期号（如果节点无效则返回 0）
 */
uint64_t raft_get_current_term(raft_node_t *node) {
    if (!node || !node->persistent) return 0;  /* 节点或持久化状态为空，返回 0 */
    return node->persistent->current_term;     /* 返回当前任期号 */
}

/**
 * @brief 获取节点的当前状态
 * 
 * 返回节点当前的状态（FOLLOWER、CANDIDATE 或 LEADER）。
 * 
 * @param node 节点指针
 * @return 节点状态（如果节点无效则返回 RAFT_STATE_FOLLOWER）
 */
raft_state_t raft_get_state(raft_node_t *node) {
    if (!node) return RAFT_STATE_FOLLOWER;  /* 节点为空，返回默认状态 */
    return node->state;                     /* 返回当前状态 */
}

/**
 * @brief 检查节点是否为 Leader
 * 
 * 判断节点当前是否处于 Leader 状态。
 * 
 * @param node 节点指针
 * @return true 是 Leader，false 不是
 */
bool raft_is_leader(raft_node_t *node) {
    return node && node->state == RAFT_STATE_LEADER;  /* 节点存在且状态为 LEADER */
}

/**
 * @brief 添加对等节点到集群
 * 
 * 将对等节点的 ID 添加到节点的对等节点列表中。
 * 如果节点已存在，则直接返回成功。
 * 
 * @param node 节点指针
 * @param peer_id 要添加的节点 ID
 * @return RAFT_OK 成功，RAFT_ERR 失败
 */
int raft_add_peer(raft_node_t *node, uint64_t peer_id) {
    if (!node || !node->peers) return RAFT_ERR;  /* 节点或对等节点列表为空 */
    
    /* 检查对等节点是否已存在 */
    list_iterator_t *iter = list_get_iterator(node->peers, LIST_ITERATOR_OPTION_HEAD);
    list_node_t *ln;
    while ((ln = list_next(iter)) != NULL) {  /* 遍历对等节点列表 */
        uint64_t *id = (uint64_t *)list_node_value(ln);
        if (id && *id == peer_id) {  /* 如果找到相同的节点 ID */
            list_iterator_delete(iter);  /* 释放迭代器 */
            return RAFT_OK;              /* 已存在，返回成功 */
        }
    }
    list_iterator_delete(iter);  /* 释放迭代器 */
    
    /* 添加对等节点 */
    uint64_t *id = zmalloc(sizeof(uint64_t));  /* 分配节点 ID 的内存 */
    if (!id) return RAFT_ERR;                   /* 内存分配失败 */
    *id = peer_id;                               /* 设置节点 ID */
    list_add_node_tail(node->peers, id);        /* 添加到对等节点列表末尾 */
    
    return RAFT_OK;  /* 添加成功 */
}

/**
 * @brief 从集群中移除对等节点
 * 
 * 从节点的对等节点列表中移除指定的节点 ID。
 * 
 * @param node 节点指针
 * @param peer_id 要移除的节点 ID
 * @return RAFT_OK 成功，RAFT_ERR 失败（节点不存在）
 */
int raft_remove_peer(raft_node_t *node, uint64_t peer_id) {
    if (!node || !node->peers) return RAFT_ERR;  /* 节点或对等节点列表为空 */
    
    /* 遍历对等节点列表，查找要移除的节点 */
    list_iterator_t *iter = list_get_iterator(node->peers, LIST_ITERATOR_OPTION_HEAD);
    list_node_t *ln;
    while ((ln = list_next(iter)) != NULL) {  /* 遍历对等节点列表 */
        uint64_t *id = (uint64_t *)list_node_value(ln);
        if (id && *id == peer_id) {  /* 如果找到匹配的节点 ID */
            list_del_node(node->peers, ln);  /* 从列表中删除节点 */
            zfree(id);                       /* 释放节点 ID 的内存 */
            list_iterator_delete(iter);       /* 释放迭代器 */
            return RAFT_OK;                  /* 移除成功 */
        }
    }
    list_iterator_delete(iter);  /* 释放迭代器 */
    
    return RAFT_ERR;  /* 未找到要移除的节点，返回错误 */
}

/* ==================== 配置函数 ==================== */

/**
 * @brief 设置选举超时时间
 * 
 * 设置 Follower 等待 Leader 心跳的超时时间。
 * 如果在这个时间内没有收到心跳，Follower 会发起选举。
 * 
 * @param node 节点指针
 * @param timeout_ms 超时时间（毫秒）
 */
void raft_set_election_timeout(raft_node_t *node, uint64_t timeout_ms) {
    if (node) {  /* 如果节点存在 */
        node->election_timeout = timeout_ms;  /* 设置选举超时时间 */
    }
}

/**
 * @brief 设置心跳间隔时间
 * 
 * 设置 Leader 发送心跳的间隔时间。
 * Leader 会定期（每 heartbeat_interval 毫秒）向所有 Follower 发送心跳。
 * 
 * @param node 节点指针
 * @param interval_ms 间隔时间（毫秒）
 */
void raft_set_heartbeat_interval(raft_node_t *node, uint64_t interval_ms) {
    if (node) {  /* 如果节点存在 */
        node->heartbeat_interval = interval_ms;  /* 设置心跳间隔时间 */
    }
}
